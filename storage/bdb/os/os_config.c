/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_config.c,v 12.2 2005/06/16 20:23:23 bostic Exp $
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

/*
 * __os_support_db_register --
 *	Return 1 if the system supports DB_REGISTER.
 *
 * PUBLIC: int __os_support_db_register __P((void));
 */
int
__os_support_db_register()
{
	return (1);
}

/*
 * __os_support_replication --
 *	Return 1 if the system supports replication.
 *
 * PUBLIC: int __os_support_replication __P((void));
 */
int
__os_support_replication()
{
	return (1);
}
