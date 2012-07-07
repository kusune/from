/*
 *    `from' replacement using pop protocol; it uses rpop/apop enhancement.
 *
 *      usage: from [-a] [user][@host]
 *             prmail [-a] [user][@host]
 *             mailp [-a] [user][@host]    displays "you have mail" if any
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef DEBUG
int debug = 1;
#else
int debug = 0;
#endif				/* DEBUG */

char *argv0;

void usage(void)
{
    fprintf(stderr, "Usage: %s [-a] [user][@host[:port]]\n", argv0);
}

POP_SESSION *initialize(int argc, char **argv)
{
    char *ruser = NULL, *host = NULL;
    int auth = POP_AUTH_USER;
    int opt = 0;

#ifdef WITH_OPENSSL
    ERR_load_crypto_strings();
# ifdef ENABLE_SSL
    SSL_load_error_strings();
    SSL_library_init();
# endif /* ENABLE_SSL */
#endif /* WITH_OPENSSL */

    { 
	int c;

	while ((c = getopt(argc, argv, "saRdh")) != -1) {
	    switch (c) {
	    case 'a':
		auth = POP_AUTH_APOP;
		break;
	    case 'R':
		auth = POP_AUTH_RPOP;
		break;
	    case 's':
#ifdef ENABLE_SSL
		opt |= POP_OPT_SSL;
		break;
#else /* ENABLE_SSL */
		fprintf(stderr, "SSL feature is not enabled\n");
		exit(EX_CONFIG);
#endif /* ENABLE_SSL */
	    case 'd':
		debug = 1;
		break;
	    case 'h':
		usage();
		exit(EX_OK);
	    default:
		usage();
		exit(EX_USAGE);
	    }
	}
    }

    if (argc <= optind) {
	usage();
	exit(EX_USAGE);
    }

    if ((host = strrchr(argv[optind], '@')) != NULL) {
	*(host++) = NUL;
    }
    if (*argv[optind] != NUL) {
	ruser = argv[optind];
    }

    if (host == NULL) {
	host = getenv("POPSERVER");
    }
#ifdef DEFAULT_POP_HOST
    if (host == NULL) {
	host = DEFAULT_POP_HOST;
    }
#endif				/* DEFAULT_POP_HOST */
    if (host == NULL) {
	fprintf(stderr, "POP server is not given.\n");
	fprintf(stderr, "You can give POP server's name by argument");
	fprintf(stderr, " or environment POPSERVER.\n");
	usage();
	exit(EX_USAGE);
    }

    if (ruser == NULL) {
	ruser = getusername();
    }

    if (ruser == NULL) {
	fprintf(stderr, "Can't determine your username.\n");
	usage();
	exit(EX_USAGE);
    }

    if ((host = strdup(host)) == NULL || (ruser = strdup(ruser)) == NULL) {
	fprintf(stderr, "Can't allocate enough memory.\n");
	exit(EX_OSERR);
    }

    {
	POP_SESSION *psp;

	psp = pop_session_open(auth, ruser, host, -1, opt);

	free(host);
	free(ruser);

	return psp;
    }
}

int main(int argc, char **argv)
{
    int msgs, msize;
    POP_SESSION *psp;

    if ((argv0 = strrchr(*argv, '/')) != NULL) {
	argv0++;
    } else {
	argv0 = *argv;
    }

    if ((psp = initialize(argc, argv)) == NULL) {
	exit(EX_UNAVAILABLE);
    }

    /*
     * this point, the pop connection is in transaction state
     */
    if (pop_sendcmd(psp, POP_CMD_STAT) != POP_OK) {
	fprintf(stderr, "Server does not understand STAT command.\n");
	pop_session_close(psp);
	exit(EX_PROTOCOL);
    }
    sscanf(psp->resp, "+OK %d %d", &msgs, &msize);
    do_main(psp, msgs);		/* never return */

    exit(EX_OK);
}
