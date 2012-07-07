#include "common.h"

#include <stdio.h>
#include <ctype.h>
#include <strings.h>

static char base_initialized = 0;
static char base[128];

static void initbase64(void)
{
    base['A'] =  0; base['R'] = 17; base['i'] = 34; base['z'] = 51;
    base['B'] =  1; base['S'] = 18; base['j'] = 35; base['0'] = 52;
    base['C'] =  2; base['T'] = 19; base['k'] = 36; base['1'] = 53;
    base['D'] =  3; base['U'] = 20; base['l'] = 37; base['2'] = 54;
    base['E'] =  4; base['V'] = 21; base['m'] = 38; base['3'] = 55;
    base['F'] =  5; base['W'] = 22; base['n'] = 39; base['4'] = 56;
    base['G'] =  6; base['X'] = 23; base['o'] = 40; base['5'] = 57;
    base['H'] =  7; base['Y'] = 24; base['p'] = 41; base['6'] = 58;
    base['I'] =  8; base['Z'] = 25; base['q'] = 42; base['7'] = 59;
    base['J'] =  9; base['a'] = 26; base['r'] = 43; base['8'] = 60;
    base['K'] = 10; base['b'] = 27; base['s'] = 44; base['9'] = 61;
    base['L'] = 11; base['c'] = 28; base['t'] = 45; base['+'] = 62;
    base['M'] = 12; base['d'] = 29; base['u'] = 46; base['/'] = 63;
    base['N'] = 13; base['e'] = 30; base['v'] = 47;
    base['O'] = 14; base['f'] = 31; base['w'] = 48;
    base['P'] = 15; base['g'] = 32; base['x'] = 49;
    base['Q'] = 16; base['h'] = 33; base['y'] = 50;
}

char *decode(char *p)
{
    static char buf[BUFSIZ * 10];
    char *q, *r, ch;
    unsigned long val;

    if (!base_initialized) {
	initbase64();
    }

    q = buf;
    while (*p) {
	if (strncasecmp(p, "=?ISO-2022-JP?B?", 16) == 0) {
	    int len;

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
	    while (isspace(*p))
		p++;
	} else {		/* no "=?ISO-2022-JP?B?" */
	    *q++ = *p++;
	}
    }
    *q = NUL;
    return buf;
}
