/*	$NetBSD: wcsdup.c,v 1.6 2022/03/12 17:31:40 christos Exp $	*/

/*
 * Copyright (C) 2006 Aleksey Cheusov
 *
 * This material is provided "as is", with absolutely no warranty expressed
 * or implied. Any use is at your own risk.
 *
 * Permission to use or copy this software for any purpose is hereby granted 
 * without fee. Permission to modify the code and to distribute modified
 * code is also granted without any restrictions.
 */

#ifndef HAVE_WCSDUP

#include "config.h"

#if defined(LIBC_SCCS) && !defined(lint) 
__RCSID("$NetBSD: wcsdup.c,v 1.6 2022/03/12 17:31:40 christos Exp $"); 
#endif /* LIBC_SCCS and not lint */ 

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <wchar.h>

wchar_t *
wcsdup(const wchar_t *str)
{
	wchar_t *copy;
	size_t len;

	_DIAGASSERT(str != NULL);

	len = wcslen(str) + 1;

	copy = NULL;
	errno = reallocarr(&copy, len, sizeof(*copy));
	if (errno)
		return NULL;

	return wmemcpy(copy, str, len);
}

#endif
