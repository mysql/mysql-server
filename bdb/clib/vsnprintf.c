/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: vsnprintf.c,v 11.4 2000/05/18 19:24:59 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#endif

#include "db_int.h"

/*
 * vsnprintf --
 *	Bounded version of vsprintf.
 *
 * PUBLIC: #ifndef HAVE_VSNPRINTF
 * PUBLIC: int vsnprintf();
 * PUBLIC: #endif
 */
#ifndef HAVE_VSNPRINTF
int
vsnprintf(str, n, fmt, ap)
	char *str;
	size_t n;
	const char *fmt;
	va_list ap;
{
	COMPQUIET(n, 0);

#ifdef SPRINTF_RET_CHARPNT
	(void)vsprintf(str, fmt, ap);
	return (strlen(str));
#else
	return (vsprintf(str, fmt, ap));
#endif
}
#endif
