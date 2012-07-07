#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <pwd.h>

#define	POP_PORT_POP3		"pop3"
#define	POP_PORT_POP3S		"pop3s"
#define	POP_PROTO	"tcp"

void pop_session_close(POP_SESSION *psp)
{
    if (psp == NULL) {
	return;
    }

    pop_close(psp);
    pop_session_destroy(psp);

    return;
}

int pop_session_auth(POP_SESSION *psp, int proto, char *ruser, char *key)
{
    if (pop_recvresp(psp) != POP_OK) {
	__pop_set_error_misc(psp, POP_ERR_PROTO, "Strange server response");
	return POP_EXCEPTION;
    }

    /*
     * now in AUTHORIZATION state
     */

    {
	if (proto == POP_AUTH_APOP) {
	    char *cp;

	    if ((cp = pop_make_digest(psp, key, psp->resp)) == NULL) {
		__pop_set_error_misc(psp, POP_ERR_PROTO, "Fail to get digest");
		return POP_EXCEPTION;
	    }

	    if ((pop_sendcmd(psp, POP_CMD_APOP, ruser, cp)) != POP_OK) {
		__pop_set_error_misc(psp, POP_ERR_PROTO, psp->resp);
		/* fprintf(stderr, "%s: %s\n", ruser, psp->resp); */
	    }
	    return psp->status;
	} else {
	    if (pop_sendcmd(psp, POP_CMD_USER, ruser) != POP_OK) {
		__pop_set_error_misc(psp, POP_ERR_PROTO, psp->resp);
		/* fprintf(stderr, "%s: %s\n", ruser, psp->resp); */
		return psp->status;
	    }

	    if (proto == POP_AUTH_RPOP) {
		pop_sendcmd(psp, POP_CMD_RPOP, key);
	    } else {
		pop_sendcmd(psp, POP_CMD_PASS, key);
	    }
	    if (psp->status != POP_OK) {
		__pop_set_error_misc(psp, POP_ERR_PROTO, psp->resp);
		/* fprintf(stderr, "%s: %s\n", ruser, psp->resp); */
	    }
	    return psp->status;
	}
    }
}

POP_SESSION *pop_session_open(int proto, char *user, char *host, int pop_port, int opt)
{
    struct sockaddr_in so;

    {
	char *desc = strdup(host);
	char *port = NULL;

	if (user == NULL && (host = strrchr(desc, '@')) != NULL) {
	    *(host++) = NUL;
	    user = desc;
	} else {
	    host = desc;
	    if (user == NULL && (user = getusername()) == NULL) {
		/* __pop_set_error_misc(psp, POP_ERR_SYSTEM, "Can't determine user name"); */
		fprintf(stderr, "%s: could not determine user name.\n", host); /* XXX */
		return NULL;
	    }
	}

	if (pop_port < 0 && (port = strrchr(desc, ':')) != NULL) {
	    *(port++) = NUL;
	}

	{
	    struct hostent *hp;

	    if ((hp = gethostbyname(host)) == NULL) {
		/* __pop_set_error_misc(psp, POP_ERR_SYSTEM, "Unknown server name"); */
		fprintf(stderr, "%s: could not be resolved.\n", host); /* XXX */
		return NULL;
	    }

	    if (pop_port < 0) {
		if (port == NULL) {
		    if (opt & POP_OPT_SSL) {
			port = POP_PORT_POP3S;
		    } else {
			port = POP_PORT_POP3;
		    }
		}
		{
		    char *cp;

		    for (cp = port; isdigit((int)*cp); cp++) {
			;
		    }
		    if (*cp == NUL) {
			pop_port = htons(atoi(port));
		    }
		}
	    }

	    if (pop_port < 0) {
		struct servent *sp;

		if ((sp = getservbyname(port, POP_PROTO)) == NULL) {
		/* __pop_set_error_misc(psp, POP_ERR_SYSTEM, "Unknown port"); */
		fprintf(stderr, "%s/%s: service could not be resolved.\n", port, POP_PROTO); /* XXX */
		    return NULL;
		}
		pop_port = sp->s_port;
	    }
	    memset(&so, 0, sizeof(so));
	    so.sin_family = hp->h_addrtype;
	    so.sin_port = pop_port;
	    memcpy(hp->h_addr_list[0], &so.sin_addr, hp->h_length);
	}
    }

    {
	char *key = NULL;
	int s;

	if ((s = socket(so.sin_family, SOCK_STREAM, 0)) < 0) {
	    /* __pop_set_error_errno(psp, "socket()"); */
	    perror("socket"); /* XXX */
	    return NULL;
	}

	if (proto == POP_AUTH_RPOP) {
	    if (bindpriv(s, so.sin_family) <= 0) {
		close(s);
		return NULL;
	    }
	    if ((key = getusername()) == NULL) {
		/* __pop_set_error_misc(psp, POP_ERR_SYSTEM, "Can't determine user name"); */
		fprintf(stderr, "Can't determine user name.\n"); /* XXX */
		return NULL;
	    }
	} else {
	    if (ruserpass(host, &user, &key) != 0) {
		/* __pop_set_error_misc(psp, POP_ERR_SYSTEM, "Can't read secret key"); */
		fprintf(stderr, "Can't read secret key.\n"); /* XXX */
		return NULL;
	    }
	}

	if (connect(s, (struct sockaddr *) &so, sizeof(so)) < 0) {
	    /* __pop_set_error_errno(psp, "connect()"); */
	    perror("connect"); /* XXX */
	    return NULL;
	}

	{
	    POP_SESSION *psp;

	    if ((psp = pop_set_socket(NULL, s, opt)) == NULL) {
		/* __pop_set_error_errno(psp, "pop_set_socket()"); */
		perror("pop_set_socket()"); /* XXX */
		close(s);
		return NULL;
	    }

	    if (pop_session_auth(psp, proto, user, key) != POP_OK) {
		pop_session_close(psp);
		return NULL;
	    }

	    return psp;
	}
    }
}
