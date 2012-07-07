#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef WITH_OPENSSL
# include <openssl/md5.h>
#else /* WITH_OPENSSL */
# include "md5.h"
#endif /* WITH_OPENSSL */

#define	POP_EOL_CONT	0
#define	POP_EOL_LF	1
#define	POP_EOL_CRLF	2

struct pop_cmd {
    int cmd;
    const char *name;
    const char *fmt;
} pop_cmd[] = {
    { POP_CMD_QUIT, "QUIT", "QUIT\r\n" },
    { POP_CMD_STAT, "STAT", "STAT\r\n" },
    { POP_CMD_APOP, "APOP", "APOP %s %s\r\n" },
    { POP_CMD_USER, "USER", "USER %s\r\n" },
    { POP_CMD_RPOP, "RPOP", "RPOP %s\r\n" },
    { POP_CMD_PASS, "PASS", "PASS %s\r\n" },
    { POP_CMD_RETR, "RETR", "RETR %d\r\n" },
    { POP_CMD_TOP, "TOP", "TOP %d %d\r\n" },
    { POP_UNDEFINED, NULL, NULL }
};

static struct pop_cmd *pop_getcmd(int cmd)
{
    int i;

    for (i = 0; pop_cmd[i].cmd > 0; i++) {
	if (pop_cmd[i].cmd == cmd) {
	    return &pop_cmd[i];
	}
    }

    return NULL;
}

POP_SESSION *pop_session_create(void)
{
    POP_SESSION *psp;

    if ((psp = malloc(sizeof(POP_SESSION))) == NULL) {
	return NULL;
    }

    memset(psp, 0, sizeof(POP_SESSION));
#ifdef WITH_OPENSSL
    psp->bio = NULL;
#else /* WITH_OPENSSL */
    psp->sr = psp->sw = -1;
#endif /* WITH_OPENSSL */
    psp->resp = NULL;
    psp->rest = NULL;
    psp->restlen = 0;
    psp->eol = POP_EOL_CRLF;
    psp->error.type = POP_ERR_NONE;
    psp->error.code = 0;
    psp->error.msg = NULL;

    return psp;
}

void pop_session_destroy(POP_SESSION *psp)
{
#ifdef WITH_OPENSSL
    if (psp->bio != NULL) {
	BIO_free_all(psp->bio);
	psp->bio = NULL;
    }
#endif /* WITH_OPENSSL */
    free(psp);

    return;
}

POP_SESSION *pop_set_socket(POP_SESSION *psp, int s, int opt)
{
    if (s < 0) {
	return NULL;
    }

    if (psp == NULL && (psp = pop_session_create()) == NULL) {
	return NULL;
    }

#ifdef WITH_OPENSSL
    if ((psp->bio = BIO_new_socket(s, BIO_NOCLOSE)) == NULL) {
	__pop_set_error_openssl(psp, "BIO_new_socket(): ");
	pop_session_destroy(psp);
	return NULL;
    }

#ifdef ENABLE_SSL
    if (opt & POP_OPT_SSL) {
	SSL_CTX *ctxp;
	BIO *bio_ssl;

	if ((ctxp = SSL_CTX_new(SSLv23_client_method())) == NULL) {
	    __pop_set_error_openssl(psp, "SSL_CTX_new(): ");
	    pop_session_destroy(psp);
	    return NULL;
	}
	if ((bio_ssl = BIO_new_ssl(ctxp, 1)) == NULL) {
	    __pop_set_error_openssl(psp, "BIO_new_ssl(): ");
	    pop_session_destroy(psp);
	    return NULL;
	}
	BIO_push(bio_ssl, psp->bio);
	psp->bio = bio_ssl;
	BIO_do_handshake(psp->bio);
    }
#endif /* ENABLE_SSL */

    {
	BIO *bio_buffer;

	if ((bio_buffer = BIO_new(BIO_f_buffer())) == NULL) {
	    __pop_set_error_openssl(psp, "BIO_new(): ");
	    pop_session_destroy(psp);
	    return NULL;
	}
	BIO_push(bio_buffer, psp->bio);
	psp->bio = bio_buffer;
    }
#else /* WITH_OPENSSL */
    if ((psp->sw = dup(s)) < 0) {
	__pop_set_error_errno(psp, "dup(s): ");
	pop_session_destroy(psp);
	return NULL;
    }
    if ((psp->fw = fdopen(psp->sw, "w")) == NULL) {
	__pop_set_error_errno(psp, "fdopen(fw): ");
	pop_session_destroy(psp);
	return NULL;
    }
    if ((psp->fr = fdopen(s, "r")) == NULL) {
	__pop_set_error_errno(psp, "fdopen(fr): ");
	pop_session_destroy(psp);
	return NULL;
    }
    psp->sr = s;
#endif /* WITH_OPENSSL */

    return psp;
}

void pop_close(POP_SESSION *psp)
{
    if (psp == NULL) {
	return;
    }

#ifdef WITH_OPENSSL
    if (psp->bio != NULL) {
	BIO_flush(psp->bio);
	BIO_set_close(psp->bio, BIO_CLOSE);
    }
#else /* WITH_OPENSSL */
    if (psp->fw != NULL) {
	fclose(psp->fw);
    } else if (psp->sw >= 0) {
	close(psp->sw);
    }
    if (psp->fr != NULL) {
	fclose(psp->fr);
    } else if (psp->sr >= 0) {
	close(psp->sr);
    }
#endif /* WITH_OPENSSL */
}

POP_SESSION *pop_open(const char *servername, int port)
{
    POP_SESSION *psp;

    char *hoststr = NULL, *portstr = NULL;

    if ((hoststr = strdup(servername)) == NULL) {
	/* __pop_set_error_errno(psp, "strdup(): "); */ /* XXX */
	return NULL;
    }

    if (port == 0 && (portstr = strrchr(hoststr, ':')) != NULL) {
	/* XXX */
    }

    return psp = NULL;
}

/* return POP_CONT if line is not end, */
/* return POP_ERR on error,            */
/* otherwise return POP_OK             */
int pop_readline(POP_SESSION *psp)
{
    int len = 0;

#ifdef WITH_OPENSSL
    assert(psp != NULL && psp->bio != NULL);
#else /* WITH_OPENSSL */
    assert(psp != NULL && psp->fr != NULL);
#endif /* WITH_OPENSSL */

    psp->resp = NULL;
    if (psp->rest != NULL && psp->restlen > 0) {
	memmove(psp->rbuf, psp->rest, psp->restlen);
	len = psp->restlen;
    }
    psp->rest = NULL;
    psp->restlen = 0;

    do {
	char *newline = memchr(psp->rbuf, '\n', len);

	if (newline != NULL) {
	    *(newline++) = NUL;
	    if (newline - psp->rbuf >= len) {
		psp->rest = newline;
		psp->restlen = len - (psp->rest - psp->rbuf);
	    }

	    return POP_OK;
	}

	{
	    fd_set fdr;
	    int fd;
	    int c;

#ifdef WITH_OPENSSL
	    fd = BIO_get_fd(psp->bio, NULL);
#else /* WITH_OPENSSL */
	    fd = psp->sr;
#endif /* WITH_OPENSSL */

	    do {
		FD_ZERO(&fdr);
		FD_SET(fd, &fdr);
		c = select(fd + 1, &fdr, NULL, NULL, NULL);
	    } while (!FD_ISSET(fd, &fdr));
	}

	{
	    int n;

#ifdef WITH_OPENSSL
	    n = BIO_gets(psp->bio, psp->rbuf + len, POP_MAXRESPLEN - len);
#else /* WITH_OPENSSL */
	    if (fgets(psp->rbuf + len, POP_MAXRESPLEN - len, psp->fr) == NULL) {
		n = -1;
	    } else {
		n = strlen(psp->rbuf + len);
	    }
#endif /* WITH_OPENSSL */

	    if (n < 0) {
		if (errno == EAGAIN) {
		    continue;
		}
#ifdef WITH_OPENSSL
		__pop_set_error_openssl(psp, "BIO_gets(): ");
#else /* WITH_OPENSSL */
		__pop_set_error_errno(psp, "read(): ");
#endif /* WITH_OPENSSL */
		return POP_ERR;
	    }
	    len += n;
	}
    } while (len < POP_MAXRESPLEN);

    *(psp->rbuf + len) = NUL;
    return POP_CONT;
}

int pop_recvresp(POP_SESSION *psp)
{
    int result;

    assert(psp != NULL);

    psp->resp = NULL;

    result = pop_readline(psp);
    if (result == POP_ERR) {
	return psp->status = POP_EXCEPTION;
    }
    psp->resp = psp->rbuf;
    if (result == POP_CONT) {
	psp->eol = POP_EOL_CONT;
	return psp->status = POP_EXCEPTION;
    }

    {
	size_t len = strlen(psp->resp);

	psp->eol = POP_EOL_LF;
	if (len <= 0 || psp->resp[--len] != '\r') {
	    return psp->status = POP_EXCEPTION;
	}
	psp->eol = POP_EOL_CRLF;
	psp->resp[len] = NUL;
    }

    if (strncmp(psp->resp, "+OK", 3) == 0) {
	return psp->status = POP_OK;
    } else if (strncmp(psp->resp, "-ERR", 4) == 0) {
	return psp->status = POP_ERR;
    } else {
	return psp->status = POP_EXCEPTION;
    }
}

int pop_sendcmd(POP_SESSION *psp, int cmd, ...)
{
    va_list ap;
    struct pop_cmd *cmdp;

#ifdef WITH_OPENSSL
    assert(psp != NULL && psp->bio != NULL);
#else /* WITH_OPENSSL */
    assert(psp != NULL && psp->fw != NULL);
#endif /* WITH_OPENSSL */

    cmdp = pop_getcmd(cmd);
    assert(cmdp != NULL && psp != NULL);

    va_start(ap, cmd);
#ifdef WITH_OPENSSL
    BIO_vprintf(psp->bio, cmdp->fmt, ap);
    BIO_flush(psp->bio);
#else /* WITH_OPENSSL */
    vfprintf(psp->fw, cmdp->fmt, ap);
    fflush(psp->fw);
#endif /* WITH_OPENSSL */
    va_end(ap);

    return pop_recvresp(psp);
}

int pop_recvmlresp(POP_SESSION *psp)
{
#ifdef WITH_OPENSSL
    assert(psp != NULL && psp->bio != NULL);
#else /* WITH_OPENSSL */
    assert(psp != NULL && psp->fr != NULL);
#endif /* WITH_OPENSSL */

    psp->resp = NULL;
#ifdef WITH_OPENSSL
    if (BIO_gets(psp->bio, psp->rbuf, POP_MAXRESPLEN) < 0) {
	return psp->status = POP_EXCEPTION;
    }
#else /* WITH_OPENSSL */
    if (fgets(psp->rbuf, POP_MAXRESPLEN, psp->fr) == NULL) {
	return psp->status = POP_EXCEPTION;
    }
#endif /* WITH_OPENSSL */
    psp->resp = psp->rbuf;

    {
	size_t len = strlen(psp->resp);
	int eot = 0;

	if (len > 0 && psp->eol != POP_EOL_CONT && psp->resp[0] == '.') {
	    psp->resp++;
	    len--;
	    eot++;
	}

	if (len <= 0 || psp->resp[--len] != '\n') {
	    return psp->status = POP_CONT;
	}
	psp->eol = POP_EOL_LF;
	psp->resp[len] = NUL;
	if (len <= 0 || psp->resp[--len] != '\r') {
	    return psp->status = POP_CONT;
	}
	psp->eol = POP_EOL_CRLF;
	psp->rbuf[len] = NUL;

	if (eot && len == 0) {
	    return psp->status = POP_OK;
	} else {
	    return psp->status = POP_CONT;
	}
    }
}

#define DIGESTLEN	16

/* made by MH/uip/popsbr.c */
char *pop_make_digest(POP_SESSION *psp, char *pass, char *response)
{
    char *cp, *lp;
    char digest[DIGESTLEN];
    static char digeststr[DIGESTLEN*2+1];
    MD5_CTX mdContext;
    char *buffer;

    if ((cp = strchr(response, '<')) == NULL || (lp = strchr(cp, '>')) == NULL) {
	__pop_set_error_misc(psp, POP_ERR_PROTO, "APOP is not available");
	return NULL;
    }
    *++lp = NUL;

    if ((buffer = malloc(strlen(cp) + strlen(pass) + 1)) == NULL) {
	__pop_set_error_errno(psp, "malloc()");
	return NULL;
    }
    sprintf(buffer, "%s%s", cp, pass);

    MD5_Init(&mdContext);
    MD5_Update(&mdContext, buffer, strlen(buffer));
    MD5_Final(digest, &mdContext);

    free(buffer);

    {
	int i;

	for (i = 0; i < DIGESTLEN; i++) {
	    sprintf(digeststr + i * 2, "%02x", digest[i] & 0xff);
	}
    }

    return digeststr;
}
