#include "common.h"

#include <ctype.h>
#include <stdio.h>
#include <time.h>

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

#define MAXFROMLEN      30	/* characters */
#define MONTHNAMELEN	3
#define DATESTRLEN	13

char *parse_from(char *from)
{
    char *p;

    if (strlen(from) <= strlen("From:"))
	return "";
    from += strlen("From:");
    while (isspace(*from))
	from++;
    if ((p = strchr(from, '<')) != NULL) {
	from = p + 1;
	if ((p = strchr(from, '>')) != NULL)
	    *p = NUL;
    }
    /* valid only first word -- email address */
    for (p = from; *p && !isspace(*p);)
	p++;
    for (; p < from + MAXFROMLEN;)
	*p++ = ' ';
    *p = NUL;
    /* limit the maximum length */
    if (strlen(from) > MAXFROMLEN)
	*(from + MAXFROMLEN) = NUL;

    return from;
}

char *parse_date(char *date)
{
    char *p, mm[MONTHNAMELEN];
    static char date_buf[DATESTRLEN+1];
    int dd, yy, hour, minute;

    if (strlen(date) <= strlen("Date:"))
	return "";
    date += strlen("Date:");
    while (isspace(*date))
	date++;
    p = strchr(date, ',');
    if (p) {
	date = p + 1;
	while (isspace(*date))
	    date++;
    }
    sscanf(date, "%d %3s %d %d:%d", &dd, mm, &yy, &hour, &minute);
    sprintf(date_buf, "%3s %02d %02d:%02d", mm, dd, hour, minute);
    return date_buf;
}

char *parse_subj(char *subj, int width)
{
    char *p;
    int kanji;

    subj = decode(subj);
    if (strlen(subj) <= strlen("Subject:"))
	return "";

    kanji = 0;
    subj += strlen("Subject:");
    while (isspace(*subj))
	subj++;
    for (p = subj;;) {
	if (strncmp(p, "\033$@", 3) == 0 || strncmp(p, "\033$B", 3) == 0) {
	    kanji = 1;
	    p += 3;
	} else if (strncmp(p, "\033(J", 3) == 0 ||
		   strncmp(p, "\033(B", 3) == 0) {
	    kanji = 0;
	    p += 3;
	} else if (kanji) {
	    if (width < 2) {	/* not enough room */
		strcpy(p, "\033(B");
		break;
	    }
	    p += 2;
	    width -= 2;
	} else if (isspace(*p) && width != 0) {
	    *p++ = ' ';
	    width -= 1;
	    if (isspace(*p) && width != 0) {
		*p++ = ' ';
		width -= 1;
	    }
	    if (width == 0) {
		*p = NUL;
		break;
	    }
	} else {
	    if (width == 0) {
		*p = NUL;
		break;
	    }
	    p++;
	    width -= 1;
	}
    }
    return subj;
}
