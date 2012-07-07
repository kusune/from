/*
 *    `from' replacement using pop protocol; it uses rpop/apop enhancement.
 *
 *      usage: from [-a] [user][@host]
 *             prmail [-a] [user][@host]
 *             mailp [-a] [user][@host]    displays "you have mail" if any
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef	HAVE_STRINGS_H
#include <strings.h>
#endif				/* HAVE_STRINGS_H */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif				/* HAVE_ARPA_INET_H */
#include <netdb.h>
#include <pwd.h>
#include <ctype.h>
#include <signal.h>
#include "md5.h"

#ifndef	POP
#define	POP		"pop3"
#endif				/* POP */
#define	POP_PROTO	"tcp"
#define	PAGER		"less"
#define	NUL		'\0'

#define	MAXSTATES	8
#define	HEAD_NONE		0
#define	HEAD_DATE		1
#define	HEAD_FROM		2
#define	HEAD_TO		3
#define	HEAD_CC		4
#define	HEAD_SUBJ		5
#define HEAD_MSGID         6
#define HEAD_SENDER        7

#define MAXFROMLEN      30	/* characters */
#define MAXMSGIDS       1000

#ifdef DEBUG
int debug = 1;
#else
int debug = 0;
#endif				/* DEBUG */
int terse = 0;

int from = 0;
int prmail = 0;
int mailp = 0;
char heading[MAXSTATES][BUFSIZ * 10];
char *argv0;
char *msgids[MAXMSGIDS];
int nmsgid = 0;

int is_trivial(char *fp);
int is_duplicate(char *buf);
int getline(char *s, int n, FILE * iop);
char *decode(char *p);
char *parse_from(char *from);
char *parse_date(char *date);
char *parse_subj(char *subj);
char *allocpy(char *buf);
char *pop_auth(char *user, char *pass, char *response);
void print_header(void);
void pop_init(int argc, char **argv);
RETSIGTYPE cleanup(int sig);
void pager(void);
void do_from(int msgs);
void initbase64(void);
void putline(FILE * fp, char *line);
void usage(void);

#ifndef HAVE_STRNCASECMP
int strncasecmp(const char *str1, const char *str2, int len);
#endif				/* HAVE_STRNCASECMP */

char *trivial_users[] =
{
    "mailer-daemon",
    "root",
    "uucp",
    "usenet",
    "news",
    "daemon",
    NULL
};

char base[128];

FILE *fr, *fw;

int main(int argc, char **argv)
{
    int msgs, msize;
    int i, non_trivial;
    char buf[BUFSIZ], *p, line[64];
    register int state, header;

    argv0 = *argv;
    if ((p = rindex(argv0, '/')) != NULL)
	argv0 = p + 1;
    if (strcmp(argv0, "from") == 0)
	from = 1;
#ifdef PROG_PRMAIL
    else if (strcmp(argv0, PROG_PRMAIL) == 0)
	prmail = 1;
#endif				/* PROG_PRMAIL */
#ifdef PROG_MAILP
    else if (strcmp(argv0, PROG_MAILP) == 0)
	mailp = 1;
#endif				/* PROG_MAILP */
    else {
	fprintf(stderr, "%s: Unknown command\n", argv0);
	exit(1);
    }
    initbase64();
    pop_init(argc, argv);
    /*
     * this point, the pop connection is in transaction state
     */
    putline(fw, "STAT");
    getline(buf, sizeof(buf), fr);
    if (strncmp(buf, "-ERR", 4) == 0) {
	fprintf(stderr, "Server does not understand STAT command.\n");
	putline(fw, "QUIT");
	exit(1);
    }
    sscanf(buf, "+OK %d %d", &msgs, &msize);
    if (mailp) {
	putline(fw, "QUIT");
	if (msgs != 0) {
	    if (terse == 0)
		printf("You have mail.\n");
	    exit(0);
	}
	exit(2);		/* no mail exists; for Mail Widget */
    }
    if (from)
	do_from(msgs);		/* never return */
    if (prmail)
	pager();
    signal(SIGINT, SIG_IGN);
    for (i = 1; i <= msgs; i++) {
	sprintf(line, "RETR %d", i);
	putline(fw, line);
	header = 1;
	state = 0;
	non_trivial = 1;
	getline(buf, sizeof(buf), fr);
	if (strncmp(buf, "-ERR", 4) == 0) {
	    fprintf(stderr, "Can not retrieve message %d\n", i);
	    putline(fw, "QUIT");
	    exit(1);
	}
	*heading[HEAD_DATE] = NUL;
	*heading[HEAD_FROM] = NUL;
	*heading[HEAD_TO] = NUL;
	*heading[HEAD_CC] = NUL;
	*heading[HEAD_SUBJ] = NUL;
	*heading[HEAD_MSGID] = NUL;
	*heading[HEAD_SENDER] = NUL;
	while (getline(buf, sizeof(buf), fr) >= 0) {
	    p = buf;
	    if (*p == '.') {
		p++;
		if (*p == NUL)
		    break;
	    }
	    if (*p == NUL || *p == '\n') {	/* null line */
		if (header) {
		    header = 0;
		    if (terse) {
			char buf[BUFSIZ];

			strcpy(buf, heading[HEAD_FROM]);
			if (is_trivial(parse_from(buf)))
			    non_trivial = 0;
			strcpy(buf, heading[HEAD_MSGID]);
			if (*buf && is_duplicate(buf))
			    non_trivial = 0;
		    }
		    if (non_trivial) {
			printf("########\n");
			print_header();
		    }
		}
		if (non_trivial)
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
	    } else if (non_trivial)
		printf("%s\n", p);
	}
    }
    putline(fw, "QUIT");
    fclose(fr);
    fclose(fw);
    fflush(stdout);
    fclose(stdout);
    wait(NULL);

    return 0;
}

char *decode(char *p)
{
    static char buf[BUFSIZ * 10];
    char *q, *r, ch;
    int len;
    unsigned long val;

    q = buf;
    while (*p) {
	if (strncasecmp(p, "=?ISO-2022-JP?B?", 16) == 0) {
	    r = p + 16;
	    len = strlen(r);
	    while (*r && *r != '?' && len > 0) {
		r += 4;
		len -= 4;
	    }
	    /* not a valid encoded-word */
	    if (*r++ == NUL || len <= 0) {
		*q++ = *p++;
		continue;
	    }
	    if (*r++ != '=') {
		*q++ = *p++;
		continue;
	    }
	    if (*r != NUL && isspace(*r)) {
		*q++ = *p++;
		continue;
	    }
	    /* examination passed! */
	    p += 16;
	    while (strncmp(p, "?=", 2) != 0) {
		val = base[*p++ & 0x7f];
		val = val * 64 + base[*p++ & 0x7f];
		val = val * 64 + base[*p++ & 0x7f];
		val = val * 64 + base[*p++ & 0x7f];
		ch = (val >> 16) & 0xff;
		if (ch)
		    *q++ = ch;
		ch = (val >> 8) & 0xff;
		if (ch)
		    *q++ = ch;
		ch = val & 0xff;
		if (ch)
		    *q++ = ch;
	    }
	    p += 2;
	    if (*p == '\r' || *p == '\n') {
		while (isspace(*p))
		    p++;
	    } else if (*p == ' ' || *p == '\t')
		p++;
	} else {		/* no "=?ISO-2022-JP?B?" */
	    *q++ = *p++;
	}
    }
    *q = NUL;
    return buf;
}

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

void do_from(int msgs)
{
    int i, header, state;
    char buf[BUFSIZ], *p, *fp, line[64];

    for (i = 1; i <= msgs; i++) {
	sprintf(line, "TOP %d 0", i);
	putline(fw, line);
	header = 1, state = 0;
	getline(buf, sizeof(buf), fr);
	if (strncmp(buf, "-ERR", 4) == 0) {
	    fprintf(stderr, "Can not retrieve message %d\n", i);
	    putline(fw, "QUIT");
	    exit(1);
	}
	*heading[HEAD_DATE] = NUL;
	*heading[HEAD_FROM] = NUL;
	*heading[HEAD_SUBJ] = NUL;
	*heading[HEAD_MSGID] = NUL;
	*heading[HEAD_SENDER] = NUL;
	while (getline(buf, sizeof(buf), fr) >= 0) {
	    p = buf;
	    if (*p == '.') {
		p++;
		if (*p == NUL)
		    break;
	    }
	    if (*p == NUL || *p == '\n')	/* null line */
		continue;
	    if (header) {
		if (isspace(*p) && state) {
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
	}
	fp = parse_from(heading[HEAD_FROM]);
	/* skip if this is not interesting */
	if (terse && (is_trivial(fp) || is_duplicate(heading[HEAD_MSGID])))
	    continue;
	printf("%3d %s %-30s %s\n", i,
	       parse_date(heading[HEAD_DATE]),
	       fp,
	       parse_subj(heading[HEAD_SUBJ]));
    }

    putline(fw, "QUIT");
    exit(0);
}

int is_trivial(char *fp)
{
    char **tp;
    int tl;

    for (tp = trivial_users; *tp; tp++) {
	tl = strlen(*tp);
	if (strncasecmp(fp, *tp, tl) == 0)
	    switch (*(fp + tl)) {
	    case '@':
	    case '%':
	    case '!':
	    case ':':
		return 1;
	    default:
		return 0;
	    }
    }
    return 0;
}

char *parse_from(char *from)
{
    char *p;

    if (strlen(from) <= strlen("From:"))
	return "";
    from += strlen("From:");
    while (isspace(*from))
	from++;
    if ((p = index(from, '<')) != NULL) {
	from = p + 1;
	if ((p = index(from, '>')) != NULL)
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
    char *p, mm[BUFSIZ];
    static char date_buf[BUFSIZ];
    int dd, yy, hour, minute;

    if (strlen(date) <= strlen("Date:"))
	return "";
    date += strlen("Date:");
    while (isspace(*date))
	date++;
    p = index(date, ',');
    if (p) {
	date = p + 1;
	while (isspace(*date))
	    date++;
    }
    sscanf(date, "%d %3s %d %d:%d", &dd, mm, &yy, &hour, &minute);
    sprintf(date_buf, "%3s %02d %02d:%02d", mm, dd, hour, minute);
    return date_buf;
}

char *parse_subj(char *subj)
{
    char *p;
    int width, kanji;

    subj = decode(subj);
    if (strlen(subj) <= strlen("Subject:"))
	return "";
    width = 31, kanji = 0;
    subj += strlen("Subject:");
    while (isspace(*subj))
	subj++;
    for (p = subj;;) {
	if (strncmp(p, "\033$@", 3) == 0 ||
	    strncmp(p, "\033$B", 3) == 0) {
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
	} else if (*p == '\n' && width != 0) {
	    *p++ = '\\';
	    width -= 1;
	    if (isspace(*p) && width != 0) {
		*p++ = '\\';
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

void pop_init(int argc, char **argv)
{
    struct passwd *pwd;
    struct hostent *hp;
    struct servent *sp;
    struct sockaddr_in so, sr;
    char *user, *ruser, *host, *p;
    int pop_port = 0;
    int s, s2, port;
    char buf[BUFSIZ], line[64];
    int apop = 0;
    char *pass = NULL;
    char *cp;

#ifdef DEFAULT_POP_HOST
    host = DEFAULT_POP_HOST;
#else				/* DEFAULT_POP_HOST */
    host = NULL;
#endif				/* DEFAULT_POP_HOST */
    pwd = getpwuid(getuid());
    ruser = user = pwd->pw_name;
    if ((p = getenv("POPSERVER")) != NULL)
	host = p;

    while (argc > 1 && *argv[1] == '-') {
	argc--;
	argv++;
	p = *argv;
	while (*++p) {
	    switch (*p) {
	    case 'a':
		apop = 1;
		break;
	    case 'd':
		debug = 1;
		break;
	    case 't':
		terse = 1;
		break;
	    case 'v':
		terse = 0;
		break;
	    case 'h':
		usage();
		exit(0);
	    default:
		fprintf(stderr, "-%c: Unknown option\n", *p);
		break;
	    }
	}
    }

    if (argc > 2) {
	usage();
	exit(1);
    }
    if (argc == 2) {
	if (*argv[1] != '@') {	/* username */
	    ruser = argv[1];
	    if ((p = index(argv[1], '@')) != NULL) {
		*(p++) = NUL;
		host = p;
	    }
	} else			/* hostname only */
	    host = argv[1] + 1;
    }
    if (host == NULL) {
	fprintf(stderr, "POP server is not given.\n");
	fprintf(stderr, "You can give POP server's name by argument");
	fprintf(stderr, " or environment POPSERVER.\n");
	usage();
	exit(1);
    }
    if ((host = strdup(host)) == NULL) {
	fprintf(stderr, "Can't allocate enough memory.\n");
    }
    if ((p = index(host, ':')) != NULL) {
	*(p++) = NUL;
	pop_port = htons(atoi(p));
    }
#ifdef DEBUG
    if (debug)
	printf("user %s ruser %s host %s port %d\n",
	       user, ruser, host, pop_port);
#endif				/* DEBUG */
    if ((hp = gethostbyname(host)) == NULL) {
	fprintf(stderr, "%s: can not be resolved.\n", host);
	exit(1);
    }
    if (pop_port == 0) {
	if ((sp = getservbyname(POP, POP_PROTO)) == NULL) {
	    fprintf(stderr, "%s/%s: service can not be resolved.\n",
		    POP, POP_PROTO);
	    exit(1);
	}
	pop_port = sp->s_port;
    }
    bzero(&so, sizeof(so));
    so.sin_family = hp->h_addrtype;
    so.sin_port = pop_port;
    bcopy(hp->h_addr_list[0], &so.sin_addr, hp->h_length);
#ifdef DEBUG
    if (debug)
	printf("host %s address %s port %d\n",
	       host, inet_ntoa(so.sin_addr), ntohs(so.sin_port));
#endif				/* DEBUG */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }
    /*
     * get an previledged port on s -- it can not simply be done by
     * only a connect() call.
     */
    bzero(&sr, sizeof(sr));
    sr.sin_family = hp->h_addrtype;
    if (!apop) {
	for (port = IPPORT_RESERVED - 1;;) {
	    sr.sin_port = htons((u_short) port);
#ifdef DEBUG
	    if (debug)
		printf("sr.sin_port is %d\n", port);
#endif				/* DEBUG */
	    if (bind(s, (struct sockaddr *) &sr, sizeof(sr)) == 0)
		break;
	    if (errno == EADDRINUSE || errno == EADDRNOTAVAIL) {
		if (--port <= IPPORT_RESERVED / 2) {
		    fprintf(stderr, "A local previledged port not available.\n");
		    exit(1);
		}
	    } else {
		fprintf(stderr, "Unable to bind a socket.\n");
		exit(1);
	    }
	}
    }
    if (connect(s, (struct sockaddr *) &so, sizeof(so)) < 0) {
	perror("connect");
	exit(1);
    }
    if ((s2 = dup(s)) < 0) {
	perror("dup");
	exit(1);
    }
    if ((fr = fdopen(s, "r")) == NULL) {
	perror("fdopen (fr)");
	exit(1);
    }
    if ((fw = fdopen(s2, "w")) == NULL) {
	perror("fdopen (fw)");
	exit(1);
    }
    signal(SIGHUP, cleanup);
    signal(SIGINT, cleanup);
    signal(SIGQUIT, cleanup);
    signal(SIGPIPE, cleanup);
    signal(SIGTERM, cleanup);
    if (getline(buf, sizeof(buf), fr) < 0 || strncmp(buf, "+OK", 3) != 0) {
	fprintf(stderr, "Strange server response.\n");
	exit(1);
    }
    /*
     * now in AUTHORIZATION state
     */
    if (apop) {
	if (user != ruser) {
	    ruserpass(host, &ruser, &pass);
	    cp = pop_auth(ruser, pass, buf);
	} else {
	    ruserpass(host, &user, &pass);
	    cp = pop_auth(user, pass, buf);
	}
	if (cp) {
	    sprintf(line, "APOP %s", cp);
	    putline(fw, line);
	} else {
	    fprintf(stderr, "Fail to get digest.\n");
	    putline(fw, "QUIT");
	    exit(1);
	}
	getline(buf, sizeof(buf), fr);
	if (strncmp(buf, "-ERR", 4) == 0) {
	    fprintf(stderr, "%s\n", buf);
	    putline(fw, "QUIT");
	    exit(1);
	}
    } else {
	sprintf(line, "USER %s", ruser);
	putline(fw, line);
	getline(buf, sizeof(buf), fr);
	if (strncmp(buf, "-ERR", 4) == 0) {
	    if (strstr(buf, "APOP") != NULL)
		fprintf(stderr, "%s.\n", buf + 5);
	    else
		fprintf(stderr, "User \"%s\" not found at the server.\n", ruser);
	    putline(fw, "QUIT");
	    exit(1);
	}
	sprintf(line, "RPOP %s", user);
	putline(fw, line);
	getline(buf, sizeof(buf), fr);
	if (strncmp(buf, "-ERR", 4) == 0) {
	    fprintf(stderr, "Server denied the RPOP access.\n");
	    putline(fw, "QUIT");
	    exit(1);
	}
    }
}

void usage(void)
{
    fprintf(stderr, "Usage: %s [-a] [user][@host[:port]]\n", argv0);
}

/* this routine comes from mh-6.6/uip/popsbr.c */
int getline(char *s, int n, FILE * iop)
{
    int c = NUL;
    char *p;

    p = s;
    while (--n > 0 && (c = fgetc(iop)) != EOF)
	if ((*p++ = c) == '\n')
	    break;
    if (ferror(iop) && c != EOF) {
#ifdef DEBUG
	if (debug)
	    printf("Strange error status on reading.\n");
#endif				/* DEBUG */
	return -1;
    }
    if (c == EOF && p == s) {
#ifdef DEBUG
	if (debug)
	    printf("EOF encountered on reading.\n");
#endif				/* DEBUG */
	return -1;
    }
    *p = NUL;
    if (*--p == '\n')
	*p = NUL;
    if (*--p == '\r')
	*p = NUL;
#ifdef DEBUG
    if (debug)
	printf(">>> \"%s\"\n", s);
#endif				/* DEBUG */
    return 0;
}

void putline(FILE * fp, char *line)
{
#ifdef DEBUG
    if (debug)
	printf("<<< %s\n", line);
#endif				/* DEBUG */
    fprintf(fp, "%s\r\n", line);
    fflush(fp);
    if (ferror(fp)) {
	fprintf(stderr, "Lost connection to the server.\n");
	exit(1);
    }
}

void pager(void)
{
    char *pagername, *p;
    char *argv[2];
    int pid, pp[2];

    pagername = PAGER;
    if ((p = getenv("PAGER")) != NULL)
	pagername = p;
#ifdef DEBUG
    if (debug)
	printf("Pager: %s\n", pagername);
#endif				/* DEBUG */
    if (pipe(pp) < 0) {
	perror("pipe");
	exit(1);
    }
    if ((pid = fork()) < 0) {
	perror("fork");
	exit(1);
    }
    if (pid == 0) {		/* child process : exec the pager */
	close(pp[1]);
	if (dup2(pp[0], 0) < 0) {
	    perror("dup2");
	    exit(1);
	}
	close(pp[0]);
	argv[0] = pagername;
	argv[1] = NULL;
	execvp(pagername, argv);
	/* not reached if no error happens */
	perror("execv the pager");
	exit(1);
    } else {			/* parent side */
	close(pp[0]);
	if (dup2(pp[1], 1) < 0) {
	    perror("dup2");
	    exit(1);
	}
	close(pp[1]);
    }
}

char *
 allocpy(char *buf)
{
    char *p;

    p = malloc(strlen(buf) + 1);
    strcpy(p, buf);
    return p;
}

int is_duplicate(char *buf)
{
    int i;
    char *p;

    if (strlen(buf) < 11)
	return 0;
    buf += 11;
    for (i = 0; i < nmsgid; i++) {
	p = msgids[i];
	if (strcmp(p, buf) == 0)
	    return 1;
    }
    if (nmsgid + 1 < MAXMSGIDS)
	msgids[nmsgid++] = allocpy(buf);
    return 0;
}

RETSIGTYPE
cleanup(int sig)
{
#ifdef DEBUG
    fprintf(stderr, "Signal %d\n", sig);
#endif				/* DEBUG */
    putline(fw, "QUIT");
    fclose(fr);
    fclose(fw);
    fflush(stdout);
    fclose(stdout);
    exit(1);
}


void initbase64(void)
{
    base['A'] = 0;
    base['R'] = 17;
    base['i'] = 34;
    base['z'] = 51;
    base['B'] = 1;
    base['S'] = 18;
    base['j'] = 35;
    base['0'] = 52;
    base['C'] = 2;
    base['T'] = 19;
    base['k'] = 36;
    base['1'] = 53;
    base['D'] = 3;
    base['U'] = 20;
    base['l'] = 37;
    base['2'] = 54;
    base['E'] = 4;
    base['V'] = 21;
    base['m'] = 38;
    base['3'] = 55;
    base['F'] = 5;
    base['W'] = 22;
    base['n'] = 39;
    base['4'] = 56;
    base['G'] = 6;
    base['X'] = 23;
    base['o'] = 40;
    base['5'] = 57;
    base['H'] = 7;
    base['Y'] = 24;
    base['p'] = 41;
    base['6'] = 58;
    base['I'] = 8;
    base['Z'] = 25;
    base['q'] = 42;
    base['7'] = 59;
    base['J'] = 9;
    base['a'] = 26;
    base['r'] = 43;
    base['8'] = 60;
    base['K'] = 10;
    base['b'] = 27;
    base['s'] = 44;
    base['9'] = 61;
    base['L'] = 11;
    base['c'] = 28;
    base['t'] = 45;
    base['+'] = 62;
    base['M'] = 12;
    base['d'] = 29;
    base['u'] = 46;
    base['/'] = 63;
    base['N'] = 13;
    base['e'] = 30;
    base['v'] = 47;
    base['O'] = 14;
    base['f'] = 31;
    base['w'] = 48;
    base['P'] = 15;
    base['g'] = 32;
    base['x'] = 49;
    base['Q'] = 16;
    base['h'] = 33;
    base['y'] = 50;
}

/* cut from MH/uip/popsbr.c */
char *
 pop_auth(char *user, char *pass, char *response)
{
    register char *cp, *lp;
    register unsigned char *dp;
    unsigned char *ep, digest[16];
    MD5_CTX mdContext;
    static char buffer[BUFSIZ];

    if ((cp = index(response, '<')) == NULL
	|| (lp = index(cp, '>')) == NULL) {
	(void) sprintf(buffer, "APOP not available: %s", response);
	(void) strcpy(response, buffer);
	return NULL;
    }
    *++lp = NUL;
    (void) sprintf(buffer, "%s%s", cp, pass);

    MD5Init(&mdContext);
    MD5Update(&mdContext, (unsigned char *) buffer,
	      (unsigned int) strlen(buffer));
    MD5Final(digest, &mdContext);

    (void) sprintf(cp = buffer, "%s ", user);
    cp += strlen(cp);
    for (ep = (dp = digest) + sizeof digest / sizeof digest[0];
	 dp < ep;
	 cp += 2)
	(void) sprintf(cp, "%02x", *dp++ & 0xff);
    *cp = NUL;

    return buffer;
}
