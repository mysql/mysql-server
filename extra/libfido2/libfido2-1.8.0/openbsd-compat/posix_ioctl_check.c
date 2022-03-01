#include <sys/ioctl.h>

int
posix_ioctl_check(int fd)
{
	return ioctl(fd, -1, 0);
}
