/* configuration */

#include "config.h"

#if STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#ifndef HAVE_STRNCASECMP
int strncasecmp(const char *str1, const char *str2, int len);
#endif /* HAVE_STRNCASECMP */

#if defined(_ALL_SOURCE) || defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED) && (_XOPEN_SOURCE_EXTENDED >= 1)
# undef EX_OK /* XXX */
#endif
#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#include "sysexits.h"
#endif
#define	EX_YOUHAVEMAIL	2	/* for private use in mailp */


/* common */

#define	NUL		'\0'


/* string.c */

char *strip_crlf(char *buffer);


/* getusername.c */

char *getusername(void);


/* ruserpass.c */

int ruserpass(char *host, char **aname, char **apass);


/* decode.c */

char *decode(char *p);


/* pop.c */

#include <stdio.h>

#define POP_OK		0
#define	POP_ERR		1
#define POP_CONT	2	/* for multi-line response */
#define	POP_EXCEPTION	-1

#define	POP_UNDEFINED	-1
#define	POP_CMD_QUIT	1
#define	POP_CMD_STAT	2
#define	POP_CMD_APOP	3
#define	POP_CMD_USER	4
#define	POP_CMD_RPOP	5
#define	POP_CMD_PASS	6
#define	POP_CMD_RETR	7
#define	POP_CMD_TOP	8

#define	POP_AUTH_NONE	0
#define	POP_AUTH_RPOP	1
#define	POP_AUTH_APOP	2
#define	POP_AUTH_USER	3

#define	POP_OPT_SSL	1

#define POP_ERR_NONE	0
#define POP_ERR_ERRNO	1
#define	POP_ERR_PROTO	2
#define	POP_ERR_SYSTEM	3
#ifdef WITH_OPENSSL
#define	POP_ERR_OPENSSL	4
#endif /* WITH_OPENSSL */

#define __pop_set_error_openssl(psp, _msg)	\
    { psp->error.type = POP_ERR_OPENSSL; psp->error.code = 0; psp->error.msg = _msg; psp->error.where = NULL; }
#define __pop_set_error_errno(psp, _msg)	\
    { psp->error.type = POP_ERR_ERRNO; psp->error.code = errno; psp->error.msg = _msg; psp->error.where = NULL; }
#define __pop_set_error_misc(psp, _type, _msg)	\
    { psp->error.type = _type; psp->error.code = 0; psp->error.msg = _msg; psp->error.where = NULL; }

#define POP_MAXRESPLEN	512

#ifdef WITH_OPENSSL
# include <openssl/bio.h>
# include <openssl/err.h>
# ifdef ENABLE_SSL
#  include <openssl/ssl.h>
# endif /* ENABLE_SSL */
#endif /* WITH_OPENSSL */

typedef struct {
#ifdef WITH_OPENSSL
    BIO *bio;
#else /* WITH_OPENSSL */
    int sr, sw;
    FILE *fr, *fw;
#endif /* WITH_OPENSSL */
    char rbuf[POP_MAXRESPLEN+1];
    char *resp;
    char *rest;
    int restlen;
    int status;
    int cont;
    int eol;
    struct {
	int type;
	int code;
	const char *msg;
	const char *where;
    } error;
} POP_SESSION;

POP_SESSION *pop_session_create(void);
void pop_session_destroy(POP_SESSION *psp);
POP_SESSION *pop_set_socket(POP_SESSION *psp, int s, int opt);
void pop_close(POP_SESSION *psp);
int pop_recvresp(POP_SESSION *psp);
int pop_sendcmd(POP_SESSION *psp, int cmd, ...);
int pop_recvmlresp(POP_SESSION *psp);
char *pop_make_digest(POP_SESSION *psp, char *pass, char *response);


/* pop-session.c */

void pop_session_close(POP_SESSION *psp);
POP_SESSION *pop_session_open(int proto, char *user, char *host, int pop_port, int opt);


/* parse.c */

char *parse_from(char *from);
char *parse_date(char *date);
char *parse_subj(char *subj, int width);


/* socket.c */

int bindpriv(int s, int family);


/* getwidth.c */

int getwidth(void);


/* {from,mailp,prmail}.c */

void do_main(POP_SESSION *psp, int msgs);
