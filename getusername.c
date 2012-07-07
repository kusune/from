#include "common.h"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

char *getusername(void)
{
    struct passwd *pwd;

    if ((pwd = getpwuid(getuid())) != NULL) {
	return pwd->pw_name;
    }

    return NULL;
}
