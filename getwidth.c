#include "common.h"

#include <paths.h>	/* _PATH_TTY */
#include <fcntl.h>	/* open() */
#include <unistd.h>	/* close() */

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
