/* $NetBSD: reallocarr.c,v 1.5 2015/08/20 22:27:49 kamil Exp $ */

/*-
 * Copyright (c) 2015 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !HAVE_REALLOCARR

#include "config.h"

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

__RCSID("$NetBSD: reallocarr.c,v 1.5 2015/08/20 22:27:49 kamil Exp $");

#include <errno.h>
/* Old POSIX has SIZE_MAX in limits.h */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _LIBC
#ifdef __weak_alias
__weak_alias(reallocarr, _reallocarr)
#endif
#endif

#define SQRT_SIZE_MAX (((size_t)1) << (sizeof(size_t) * CHAR_BIT / 2))

int
reallocarr(void *ptr, size_t number, size_t size)
{
	int saved_errno, result;
	void *optr;
	void *nptr;

	saved_errno = errno;
	memcpy(&optr, ptr, sizeof(ptr));
	if (number == 0 || size == 0) {
		free(optr);
		nptr = NULL;
		memcpy(ptr, &nptr, sizeof(ptr));
		errno = saved_errno;
		return 0;
	}

	/*
	 * Try to avoid division here.
	 *
	 * It isn't possible to overflow during multiplication if neither
	 * operand uses any of the most significant half of the bits.
	 */
	if ((number|size) >= SQRT_SIZE_MAX &&
	                    number > SIZE_MAX / size) {
		errno = saved_errno;
		return EOVERFLOW;
	}

	nptr = realloc(optr, number * size);
	if (nptr == NULL) {
		result = errno;
	} else {
		result = 0;
		memcpy(ptr, &nptr, sizeof(ptr));
	}
	errno = saved_errno;
	return result;
}
#endif
