/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: snprintf.c,v 11.10 2002/01/11 15:51:28 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#endif

#include "db_int.h"

/*
 * snprintf --
 *	Bounded version of sprintf.
 *
 * PUBLIC: #ifndef HAVE_SNPRINTF
 * PUBLIC: int snprintf __P((char *, size_t, const char *, ...));
 * PUBLIC: #endif
 */
#ifndef HAVE_SNPRINTF
int
#ifdef __STDC__
snprintf(char *str, size_t n, const char *fmt, ...)
#else
snprintf(str, n, fmt, va_alist)
	char *str;
	size_t n;
	const char *fmt;
	va_dcl
#endif
{
	static int ret_charpnt = -1;
	va_list ap;
	int len;

	COMPQUIET(n, 0);

	/*
	 * Some old versions of sprintf return a pointer to the first argument
	 * instead of a character count.  Assume the return value of snprintf,
	 * vsprintf, etc. will be the same as sprintf, and check the easy one.
	 *
	 * We do this test at run-time because it's not a test we can do in a
	 * cross-compilation environment.
	 */
	if (ret_charpnt == -1) {
		char buf[10];

		ret_charpnt =
		    sprintf(buf, "123") != 3 ||
		    sprintf(buf, "123456789") != 9 ||
		    sprintf(buf, "1234") != 4;
	}

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	len = vsprintf(str, fmt, ap);
	va_end(ap);
	return (ret_charpnt ? (int)strlen(str) : len);
}
#endif
