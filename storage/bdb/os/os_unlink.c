/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_unlink.c,v 11.28 2004/07/06 13:55:48 bostic Exp $
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
 *
 * PUBLIC: int __os_region_unlink __P((DB_ENV *, const char *));
 */
int
__os_region_unlink(dbenv, path)
	DB_ENV *dbenv;
	const char *path;
{
#ifdef HAVE_QNX
	int ret;
	char *newname;

	if ((ret = __os_shmname(dbenv, path, &newname)) != 0)
		goto err;

	if ((ret = shm_unlink(newname)) != 0) {
		ret = __os_get_errno();
		if (ret != ENOENT)
			__db_err(dbenv, "shm_unlink: %s: %s",
			    newname, strerror(ret));
	}
err:
	if (newname != NULL)
		__os_free(dbenv, newname);
	return (ret);
#else
	if (F_ISSET(dbenv, DB_ENV_OVERWRITE))
		(void)__db_overwrite(dbenv, path);

	return (__os_unlink(dbenv, path));
#endif
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
	int ret;

	if (DB_GLOBAL(j_unlink) != NULL)
		ret = DB_GLOBAL(j_unlink)(path);
	else
#ifdef HAVE_VXWORKS
	    RETRY_CHK((unlink((char *)path)), ret);
#else
	    RETRY_CHK((unlink(path)), ret);
#endif
	/*
	 * !!!
	 * The results of unlink are file system driver specific on VxWorks.
	 * In the case of removing a file that did not exist, some, at least,
	 * return an error, but with an errno of 0, not ENOENT.  We do not
	 * have to test for the explicitly, the RETRY_CHK macro resets "ret"
	 * to be the errno, and so we'll just slide right on through.
	 *
	 * XXX
	 * We shouldn't be testing for an errno of ENOENT here, but ENOENT
	 * signals that a file is missing, and we attempt to unlink things
	 * (such as v. 2.x environment regions, in DB_ENV->remove) that we
	 * are expecting not to be there.  Reporting errors in these cases
	 * is annoying.
	 */
	if (ret != 0 && ret != ENOENT)
		__db_err(dbenv, "unlink: %s: %s", path, strerror(ret));

	return (ret);
}
