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

void print_header(void)
{
    if (*heading[HEAD_FROM])
	printf("%s\n", decode(heading[HEAD_FROM]));
    if (*heading[HEAD_SENDER])
	printf("%s\n", decode(heading[HEAD_SENDER]));
    if (*heading[HEAD_DATE])
	printf("%s\n", decode(heading[HEAD_DATE]));
    if (*heading[HEAD_TO])
	printf("%s\n", decode(heading[HEAD_TO]));
    if (*heading[HEAD_CC])
	printf("%s\n", decode(heading[HEAD_CC]));
    if (*heading[HEAD_SUBJ])
	printf("%s\n", decode(heading[HEAD_SUBJ]));
}

int prmail(POP_SESSION *psp, int msg)
{
    int header, state;

    if (pop_sendcmd(psp, POP_CMD_RETR, msg) != POP_OK) {
	fprintf(stderr, "Can not retrieve message %d\n", msg);
	if (psp->status != POP_ERR) {
	    return psp->status;
	}
	pop_session_close(psp);
	exit(EX_PROTOCOL);
    }
    header = 1;
    state = 0;
    *heading[HEAD_DATE] = NUL;
    *heading[HEAD_FROM] = NUL;
    *heading[HEAD_TO] = NUL;
    *heading[HEAD_CC] = NUL;
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

	if (*p == NUL || *p == '\n') {	/* null line */
	    if (header) {
		header = 0;
		printf("########\n");
		print_header();
	    }
	    printf("\n");
	    continue;
	}
	if (header) {
	    if (isspace(*p) && state) {
		strcat(heading[state], "\n");
		strcat(heading[state], p);
	    } else if (strncasecmp(p, "from:", 5) == 0)
		strcpy(heading[state = HEAD_FROM], p);
	    else if (strncasecmp(p, "date:", 5) == 0)
		strcpy(heading[state = HEAD_DATE], p);
	    else if (strncasecmp(p, "to:", 3) == 0)
		strcpy(heading[state = HEAD_TO], p);
	    else if (strncasecmp(p, "cc:", 3) == 0)
		strcpy(heading[state = HEAD_CC], p);
	    else if (strncasecmp(p, "subject:", 8) == 0)
		strcpy(heading[state = HEAD_SUBJ], p);
	    else if (strncasecmp(p, "message-id:", 11) == 0)
		strcpy(heading[state = HEAD_MSGID], p);
	    else if (strncasecmp(p, "sender:", 7) == 0)
		strcpy(heading[state = HEAD_SENDER], p);
	    else
		state = 0;
	} else {
	    printf("%s\n", p);
	}
    } while (psp->status == POP_CONT);

    return psp->status;
}

void do_main(POP_SESSION *psp, int msgs)
{
    int i;

    for (i = 1; i <= msgs; i++) {
	prmail(psp, i);
    }
    pop_session_close(psp);

    exit(EX_OK);
}
