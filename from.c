#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define	MAXSTATES	8
#define	HEAD_NONE	0
#define	HEAD_DATE	1
#define	HEAD_FROM	2
#define	HEAD_TO		3
#define	HEAD_CC		4
#define	HEAD_SUBJ	5
#define HEAD_MSGID	6
#define HEAD_SENDER	7

char heading[MAXSTATES][BUFSIZ * 10];

int from(POP_SESSION *psp, int msg, int width)
{
    int header, state;

    if (pop_sendcmd(psp, POP_CMD_TOP, msg, 0) != POP_OK) {
	fprintf(stderr, "Can not retrieve message %d\n", msg);
	if (psp->status != POP_ERR) {
	    return psp->status;
	}
	pop_session_close(psp);
	exit(EX_PROTOCOL);
    }
    header = 1, state = 0;
    *heading[HEAD_DATE] = NUL;
    *heading[HEAD_FROM] = NUL;
    *heading[HEAD_SUBJ] = NUL;
    *heading[HEAD_MSGID] = NUL;
    *heading[HEAD_SENDER] = NUL;
    do {
	char *p;

	if (pop_recvmlresp(psp) == POP_EXCEPTION) {
	    if (psp->resp != NULL) {
		perror("read error");
	    }
	    exit(EX_PROTOCOL);
	}

	p = psp->resp;

	if (*p == NUL || *p == '\n')	/* null line */
	    continue;
	if (header) {
	    if (isspace((int)*p) && state) {
		strcat(heading[state], "\n");
		strcat(heading[state], p);
	    } else if (strncasecmp(p, "from:", 5) == 0)
		strcpy(heading[state = HEAD_FROM], p);
	    else if (strncasecmp(p, "date:", 5) == 0)
		strcpy(heading[state = HEAD_DATE], p);
	    else if (strncasecmp(p, "subject:", 8) == 0)
		strcpy(heading[state = HEAD_SUBJ], p);
	    else if (strncasecmp(p, "message-id:", 11) == 0)
		strcpy(heading[state = HEAD_MSGID], p);
	    else if (strncasecmp(p, "sender:", 7) == 0)
		strcpy(heading[state = HEAD_SENDER], p);
	    else
		state = 0;
	}
    } while (psp->status == POP_CONT);

    printf("%3d %s %-30s %s\n", msg,
	   parse_date(heading[HEAD_DATE]),
	   parse_from(heading[HEAD_FROM]),
	   parse_subj(heading[HEAD_SUBJ], width - 48));

    return psp->status;
}

void do_main(POP_SESSION *psp, int msgs)
{
    int i, width;

    width = getwidth();
    if (width < 0) {
	width = 80;
    }
    width--;

    for (i = 1; i <= msgs; i++) {
	from(psp, i, width);
    }

    pop_session_close(psp);
    exit(EX_OK);
}
