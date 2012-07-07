#include "common.h"

#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close() */

#ifdef HAVE_PATHS_H
# include <paths.h>	/* _PATH_TTY */
#endif
#ifndef _PATH_TTY
# define _PATH_TTY	"/dev/tty"
#endif

/* #include <sys/ttold.h> */
#include <sys/ttycom.h>

#if HAVE_TERMIOS_H
# include <termios.h>
#endif

#if GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#endif

int getwidth(void)
{
    struct winsize ws;
    int fd;
    int status;

    if ((fd = open(_PATH_TTY, O_RDONLY)) < 0) {
	status = ioctl(fd, TIOCGWINSZ, &ws);
	close(fd);
    } else {
	status = ioctl(fileno(stderr), TIOCGWINSZ, &ws);
    }

    if (status != 0) {
	return -1;
    } else {
	return ws.ws_col;
    }
}
