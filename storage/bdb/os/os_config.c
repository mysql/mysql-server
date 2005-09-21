/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_config.c,v 11.15 2004/01/28 03:36:18 bostic Exp $
 */

#include "db_config.h"

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
