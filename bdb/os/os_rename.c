/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_rename.c,v 11.6 2000/04/14 16:56:33 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "os_jump.h"

/*
 * __os_rename --
 *	Rename a file.
 *
 * PUBLIC: int __os_rename __P((DB_ENV *, const char *, const char *));
 */
int
__os_rename(dbenv, old, new)
	DB_ENV *dbenv;
	const char *old, *new;
{
	int ret;

	ret = __db_jump.j_rename != NULL ?
	    __db_jump.j_rename(old, new) : rename(old, new);

	if (ret == -1) {
		ret = __os_get_errno();
		__db_err(dbenv, "Rename %s %s: %s", old, new, strerror(ret));
	}

	return (ret);
}
