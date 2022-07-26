/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

/*
 * cc -fPIC -D_GNU_SOURCE -shared -o preload-fuzz.so preload-fuzz.c
 * LD_PRELOAD=$(realpath preload-fuzz.so)
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FUZZ_DEV_PREFIX	"nodev"

static int               fd_fuzz = -1;
static int              (*open_f)(const char *, int, mode_t);
static int              (*close_f)(int);
static ssize_t          (*write_f)(int, const void *, size_t);

int
open(const char *path, int flags, ...)
{
	va_list	ap;
	mode_t	mode;

	va_start(ap, flags);
	mode = va_arg(ap, mode_t);
	va_end(ap);

	if (open_f == NULL) {
		open_f = dlsym(RTLD_NEXT, "open");
		if (open_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EACCES;
			return (-1);
		}
	}

	if (strncmp(path, FUZZ_DEV_PREFIX, strlen(FUZZ_DEV_PREFIX)) != 0)
		return (open_f(path, flags, mode));

	if (fd_fuzz != -1) {
		warnx("%s: fd_fuzz != -1", __func__);
		errno = EACCES;
		return (-1);
	}

	if ((fd_fuzz = dup(STDIN_FILENO)) < 0) {
		warn("%s: dup", __func__);
		errno = EACCES;
		return (-1);
	}

	return (fd_fuzz);
}

int
close(int fd)
{
	if (close_f == NULL) {
		close_f = dlsym(RTLD_NEXT, "close");
		if (close_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EACCES;
			return (-1);
		}
	}

	if (fd == fd_fuzz)
		fd_fuzz = -1;

	return (close_f(fd));
}

ssize_t
write(int fd, const void *buf, size_t nbytes)
{
	if (write_f == NULL) {
		write_f = dlsym(RTLD_NEXT, "write");
		if (write_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EBADF;
			return (-1);
		}
	}

	if (fd != fd_fuzz)
		return (write_f(fd, buf, nbytes));

	return (nbytes);
}
