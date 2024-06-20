/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif

#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "fido.h"

#if defined(_WIN32)
#include <windows.h>

#include <winternl.h>
#include <winerror.h>
#include <stdio.h>
#include <bcrypt.h>
#include <sal.h>

int
fido_get_random(void *buf, size_t len)
{
	NTSTATUS status;

	status = BCryptGenRandom(NULL, buf, (ULONG)len,
	    BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (!NT_SUCCESS(status))
		return (-1);

	return (0);
}
#elif defined(HAVE_ARC4RANDOM_BUF)
int
fido_get_random(void *buf, size_t len)
{
	arc4random_buf(buf, len);
	return (0);
}
#elif defined(HAVE_GETRANDOM)
int
fido_get_random(void *buf, size_t len)
{
	ssize_t	r;

	if ((r = getrandom(buf, len, 0)) < 0 || (size_t)r != len)
		return (-1);

	return (0);
}
#elif defined(HAVE_DEV_URANDOM)
int
fido_get_random(void *buf, size_t len)
{
	int	fd = -1;
	int	ok = -1;
	ssize_t	r;

	if ((fd = open(FIDO_RANDOM_DEV, O_RDONLY)) < 0)
		goto fail;
	if ((r = read(fd, buf, len)) < 0 || (size_t)r != len)
		goto fail;

	ok = 0;
fail:
	if (fd != -1)
		close(fd);

	return (ok);
}
#else
#error "please provide an implementation of fido_get_random() for your platform"
#endif /* _WIN32 */
