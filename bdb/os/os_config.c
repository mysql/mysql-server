/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_config.c,v 11.13 2002/01/31 19:54:12 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * __os_fs_notzero --
 *	Return 1 if allocated filesystem blocks are not zeroed.
 *
 * PUBLIC: int __os_fs_notzero __P((void));
 */
int
__os_fs_notzero()
{
	/* Most filesystems zero out implicitly created pages. */
	return (0);
}
