#include "common.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

/*
 * get an previledged port on s -- it can not simply be done by
 * only a connect() call.
 */
int bindpriv(int s, int family)
{
    struct sockaddr_in sr;
    int port;

    memset(&sr, 0, sizeof(sr));
    sr.sin_family = family;

    for (port = IPPORT_RESERVED - 1; port <= IPPORT_RESERVED / 2; --port) {
	sr.sin_port = htons((unsigned short) port);
#ifdef DEBUG
	if (debug)
	    printf("sr.sin_port is %d\n", port);
#endif		       		/* DEBUG */
	if (bind(s, (struct sockaddr *) &sr, sizeof(sr)) == 0) {
	    return port;
	}
	if (errno != EADDRINUSE && errno != EADDRNOTAVAIL) {
	    fprintf(stderr, "Unable to bind a socket.\n");
	    return -1;
	}
    }

    fprintf(stderr, "A local previledged port not available.\n");
    return 0;
}
