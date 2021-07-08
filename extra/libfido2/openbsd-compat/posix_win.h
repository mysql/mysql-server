/*
 * Public domain
 *
 * BSD socket emulation code for Winsock2
 * Brent Cook <bcook@openbsd.org>
 */

#ifndef _COMPAT_POSIX_WIN_H
#define _COMPAT_POSIX_WIN_H

#ifdef _WIN32

#include <windows.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if _MSC_VER >= 1900
#include <../ucrt/fcntl.h>
#else
#include <../include/fcntl.h>
#endif

#include "types.h"

int posix_open(const char *path, ...);

int posix_close(int fd);

ssize_t posix_read(int fd, void *buf, size_t count);

ssize_t posix_write(int fd, const void *buf, size_t count);

#ifndef NO_REDEF_POSIX_FUNCTIONS
#define open(path, ...) posix_open(path, __VA_ARGS__)
#define close(fd) posix_close(fd)
#define read(fd, buf, count) posix_read(fd, buf, count)
#define write(fd, buf, count) posix_write(fd, buf, count)
#endif

#endif /* _WIN32 */

#endif /* !_COMPAT_POSIX_WIN_H */
