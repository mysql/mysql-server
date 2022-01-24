/*
 * Public domain
 *
 * File IO compatibility shims
 * Brent Cook <bcook@openbsd.org>
 */

#define NO_REDEF_POSIX_FUNCTIONS

#include <windows.h>

#include <errno.h>
#include <io.h>

#include "posix_win.h"

int
posix_open(const char *path, ...)
{
	va_list ap;
	int mode = 0;
	int flags;

	va_start(ap, path);
	flags = va_arg(ap, int);
	if (flags & O_CREAT)
		mode = va_arg(ap, int);
	va_end(ap);

	flags |= O_BINARY | O_NOINHERIT;

	return (open(path, flags, mode));
}

int
posix_close(int fd)
{
	return (close(fd));
}

ssize_t
posix_read(int fd, void *buf, size_t count)
{
	if (count > INT_MAX) {
		errno = EINVAL;
		return (-1);
	}

	return (read(fd, buf, (unsigned int)count));
}

ssize_t
posix_write(int fd, const void *buf, size_t count)
{
	if (count > INT_MAX) {
		errno = EINVAL;
		return (-1);
	}

	return (write(fd, buf, (unsigned int)count));
}
