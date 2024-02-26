/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "openbsd-compat.h"

#if !defined(HAVE_CLOCK_GETTIME)

#if _WIN32
int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	ULONGLONG ms;

	if (clock_id != CLOCK_MONOTONIC) {
		errno = EINVAL;
		return (-1);
	}

	ms = GetTickCount64();
	tp->tv_sec = ms / 1000L;
	tp->tv_nsec = (ms % 1000L) * 1000000L;

	return (0);
}
#else
#error "please provide an implementation of clock_gettime() for your platform"
#endif /* _WIN32 */

#endif /* !defined(HAVE_CLOCK_GETTIME) */
