/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_unlink.c,v 11.13 2000/11/30 00:58:42 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "os_jump.h"

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

	ret = __db_jump.j_unlink != NULL ?
	    __db_jump.j_unlink(path) :
#ifdef HAVE_VXWORKS
	    unlink((char *)path);
#else
	    unlink(path);
#endif
	if (ret == -1) {
		ret = __os_get_errno();
		/*
		 * XXX
		 * We really shouldn't be looking at this value ourselves,
		 * but ENOENT usually signals that a file is missing, and
		 * we attempt to unlink things (such as v. 2.x environment
		 * regions, in DB_ENV->remove) that we're expecting not to
		 * be there.  Reporting errors in these cases is annoying.
		 */
#ifdef HAVE_VXWORKS
		/*
		 * XXX
		 * The results of unlink are file system driver specific
		 * on VxWorks.  In the case of removing a file that did
		 * not exist, some, at least, return an error, but with
		 * an errno of 0, not ENOENT.
		 *
		 * Code below falls through to original if-statement only
		 * we didn't get a "successful" error.
		 */
		if (ret != 0)
		/* FALLTHROUGH */
#endif
		if (ret != ENOENT)
			__db_err(dbenv, "Unlink: %s: %s", path, strerror(ret));
	}

	return (ret);
}

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
			__db_err(dbenv, "Shm_unlink: %s: %s",
			    newname, strerror(ret));
	}
err:
	if (newname != NULL)
		__os_free(newname, 0);
	return (ret);
#else
	return (__os_unlink(dbenv, path));
#endif
}
