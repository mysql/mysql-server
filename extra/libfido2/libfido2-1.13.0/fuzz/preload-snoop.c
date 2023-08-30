/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * cc -fPIC -D_GNU_SOURCE -shared -o preload-snoop.so preload-snoop.c
 * LD_PRELOAD=$(realpath preload-snoop.so)
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

#define SNOOP_DEV_PREFIX	"/dev/hidraw"

struct fd_tuple {
	int snoop_in;
	int snoop_out;
	int real_dev;
};

static struct fd_tuple  *fd_tuple;
static int              (*open_f)(const char *, int, mode_t);
static int              (*close_f)(int);
static ssize_t          (*read_f)(int, void *, size_t);
static ssize_t          (*write_f)(int, const void *, size_t);

static int
get_fd(const char *hid_path, const char *suffix)
{
	char	*s = NULL;
	char	 path[PATH_MAX];
	int	 fd;
	int	 r;

	if ((s = strdup(hid_path)) == NULL) {
		warnx("%s: strdup", __func__);
		return (-1);
	}

	for (size_t i = 0; i < strlen(s); i++)
		if (s[i] == '/')
			s[i] = '_';

	if ((r = snprintf(path, sizeof(path), "%s-%s", s, suffix)) < 0 ||
	    (size_t)r >= sizeof(path)) {
		warnx("%s: snprintf", __func__);
		free(s);
		return (-1);
	}

	free(s);
	s = NULL;

	if ((fd = open_f(path, O_CREAT | O_WRONLY, 0644)) < 0) {
		warn("%s: open", __func__);
		return (-1);
	}

	return (fd);
}

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

	if (strncmp(path, SNOOP_DEV_PREFIX, strlen(SNOOP_DEV_PREFIX)) != 0)
		return (open_f(path, flags, mode));

	if (fd_tuple != NULL) {
		warnx("%s: fd_tuple != NULL", __func__);
		errno = EACCES;
		return (-1);
	}

	if ((fd_tuple = calloc(1, sizeof(*fd_tuple))) == NULL) {
		warn("%s: calloc", __func__);
		errno = ENOMEM;
		return (-1);
	}

	fd_tuple->snoop_in = -1;
	fd_tuple->snoop_out = -1;
	fd_tuple->real_dev = -1;

	if ((fd_tuple->snoop_in = get_fd(path, "in")) < 0 ||
	    (fd_tuple->snoop_out = get_fd(path, "out")) < 0 ||
	    (fd_tuple->real_dev = open_f(path, flags, mode)) < 0) {
		warn("%s: get_fd/open", __func__);
		goto fail;
	}

	return (fd_tuple->real_dev);
fail:
	if (fd_tuple->snoop_in != -1)
		close(fd_tuple->snoop_in);
	if (fd_tuple->snoop_out != -1)
		close(fd_tuple->snoop_out);
	if (fd_tuple->real_dev != -1)
		close(fd_tuple->real_dev);

	free(fd_tuple);
	fd_tuple = NULL;

	errno = EACCES;

	return (-1);
}

int
close(int fd)
{
	if (close_f == NULL) {
		close_f = dlsym(RTLD_NEXT, "close");
		if (close_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EBADF;
			return (-1);
		}
	}

	if (fd_tuple == NULL || fd_tuple->real_dev != fd)
		return (close_f(fd));

	close_f(fd_tuple->snoop_in);
	close_f(fd_tuple->snoop_out);
	close_f(fd_tuple->real_dev);

	free(fd_tuple);
	fd_tuple = NULL;

	return (0);
}

ssize_t
read(int fd, void *buf, size_t nbytes)
{
	ssize_t n;

	if (read_f == NULL) {
		read_f = dlsym(RTLD_NEXT, "read");
		if (read_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EBADF;
			return (-1);
		}
	}

	if (write_f == NULL) {
		write_f = dlsym(RTLD_NEXT, "write");
		if (write_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EBADF;
			return (-1);
		}
	}

	if (fd_tuple == NULL || fd_tuple->real_dev != fd)
		return (read_f(fd, buf, nbytes));

	if ((n = read_f(fd, buf, nbytes)) < 0 ||
	    write_f(fd_tuple->snoop_in, buf, n) != n)
		return (-1);

	return (n);
}

ssize_t
write(int fd, const void *buf, size_t nbytes)
{
	ssize_t n;

	if (write_f == NULL) {
		write_f = dlsym(RTLD_NEXT, "write");
		if (write_f == NULL) {
			warnx("%s: dlsym", __func__);
			errno = EBADF;
			return (-1);
		}
	}

	if (fd_tuple == NULL || fd_tuple->real_dev != fd)
		return (write_f(fd, buf, nbytes));

	if ((n = write_f(fd, buf, nbytes)) < 0 ||
	    write_f(fd_tuple->snoop_out, buf, n) != n)
		return (-1);

	return (n);
}
