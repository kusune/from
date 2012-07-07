#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

char *strip_crlf(char *buffer)
{
    size_t len;

    if ((len = strlen(buffer)) > 0) {
	if (buffer[--len] == '\n') {
	    buffer[len] = '\0';
	    if (buffer[--len] == '\r') {
		buffer[len] = '\0';
	    }
	}
    }

    return buffer;
}
