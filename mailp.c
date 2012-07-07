#include "common.h"

#include <stdio.h>
#include <stdlib.h>

void do_main(POP_SESSION *psp, int msgs)
{
    pop_session_close(psp);
    if (msgs != 0) {
	printf("You have mail.\n");
	exit(EX_OK);
    }
    exit(EX_YOUHAVEMAIL);	/* no mail exists; for Mail Widget */
}
