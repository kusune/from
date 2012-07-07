#include "config.h"

#include <ctype.h>

/*
 *  Perform a case-insensitive string comparision
 */

int
strncasecmp(register const char *str1,
	    register const char *str2,
	    register int len)
{
    register int i;
    register char a, b;

    for (i = len - 1; i >= 0; i--) {
	a = str1[i];
        b = str2[i];
        if (isupper(a)) a = tolower(str1[i]);
        if (isupper(b)) b = tolower(str2[i]);
        if ((a - b)) return a - b;
    }
    return 0;
}
