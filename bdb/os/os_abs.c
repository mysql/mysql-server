/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_abs.c,v 11.3 2000/02/14 03:00:04 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * __os_abspath --
 *	Return if a path is an absolute path.
 *
 * PUBLIC: int __os_abspath __P((const char *));
 */
int
__os_abspath(path)
	const char *path;
{
	return (path[0] == '/');
}
