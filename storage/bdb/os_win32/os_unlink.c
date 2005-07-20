/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_unlink.c,v 11.5 2004/10/05 14:55:36 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_region_unlink --
 *	Remove a shared memory object file.
 */
int
__os_region_unlink(dbenv, path)
	DB_ENV *dbenv;
	const char *path;
{
	if (F_ISSET(dbenv, DB_ENV_OVERWRITE))
		(void)__db_overwrite(dbenv, path);

	return (__os_unlink(dbenv, path));
}

/*
 * __os_unlink --
 *	Remove a file.
 *
 * PUBLIC: int __os_unlink __P((DB_ENV *, const char *));
 */
int
__os_unlink(dbenv, path)
	DB_ENV *dbenv;
	const char *path;
{
	_TCHAR *tpath;
	int ret;

	if (DB_GLOBAL(j_unlink) != NULL) {
		ret = DB_GLOBAL(j_unlink)(path);
		goto done;
	}

	TO_TSTRING(dbenv, path, tpath, ret);
	if (ret != 0)
		return (ret);
	RETRY_CHK((!DeleteFile(tpath)), ret);
	FREE_STRING(dbenv, tpath);

	/*
	 * XXX
	 * We shouldn't be testing for an errno of ENOENT here, but ENOENT
	 * signals that a file is missing, and we attempt to unlink things
	 * (such as v. 2.x environment regions, in DB_ENV->remove) that we
	 * are expecting not to be there.  Reporting errors in these cases
	 * is annoying.
	 */
done:	if (ret != 0 && ret != ENOENT)
		__db_err(dbenv, "unlink: %s: %s", path, strerror(ret));

	return (ret);
}
