/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_rename.c,v 12.2 2005/07/29 14:21:51 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"

/*
 * __os_rename --
 *	Rename a file.
 *
 * PUBLIC: int __os_rename __P((DB_ENV *,
 * PUBLIC:    const char *, const char *, u_int32_t));
 */
int
__os_rename(dbenv, old, new, silent)
	DB_ENV *dbenv;
	const char *old, *new;
	u_int32_t silent;
{
	int ret;

	if (DB_GLOBAL(j_rename) != NULL)
		ret = DB_GLOBAL(j_rename)(old, new);
	else
		RETRY_CHK((rename(old, new)), ret);

	/*
	 * If "silent" is not set, then errors are OK and we should not output
	 * an error message.
	 */
	if (!silent && ret != 0)
		__db_err(dbenv, "rename %s %s: %s", old, new, strerror(ret));
	return (ret);
}
