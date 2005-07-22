/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_rename.c,v 1.19 2004/10/05 14:55:36 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_rename --
 *	Rename a file.
 */
int
__os_rename(dbenv, oldname, newname, flags)
	DB_ENV *dbenv;
	const char *oldname, *newname;
	u_int32_t flags;
{
	int ret;
	_TCHAR *toldname, *tnewname;

	ret = 0;
	if (DB_GLOBAL(j_rename) != NULL) {
		if (DB_GLOBAL(j_rename)(oldname, newname) == -1)
			ret = __os_get_errno();
		goto done;
	}

	TO_TSTRING(dbenv, oldname, toldname, ret);
	if (ret != 0)
		goto done;
	TO_TSTRING(dbenv, newname, tnewname, ret);
	if (ret != 0)
		goto err;

	if (!MoveFile(toldname, tnewname))
		ret = __os_get_errno();

	if (ret == EEXIST) {
		ret = 0;
		if (__os_is_winnt()) {
			if (!MoveFileEx(
			    toldname, tnewname, MOVEFILE_REPLACE_EXISTING))
				ret = __os_get_errno();
		} else {
			/*
			 * There is no MoveFileEx for Win9x/Me, so we have to
			 * do the best we can.  Note that the MoveFile call
			 * above would have succeeded if oldname and newname
			 * refer to the same file, so we don't need to check
			 * that here.
			 */
			(void)DeleteFile(tnewname);
			if (!MoveFile(toldname, tnewname))
				ret = __os_get_errno();
		}
	}

	FREE_STRING(dbenv, tnewname);
err:	FREE_STRING(dbenv, toldname);

done:	if (ret != 0 && flags == 0)
		__db_err(dbenv,
		    "Rename %s %s: %s", oldname, newname, strerror(ret));

	return (ret);
}
