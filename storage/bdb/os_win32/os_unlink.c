/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_unlink.c,v 12.7 2005/10/20 18:57:08 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
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
		(void)__db_file_multi_write(dbenv, path);

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
	HANDLE h;
	_TCHAR *tpath, *orig_tpath, buf[MAXPATHLEN];
	u_int32_t id;
	int ret;

	if (DB_GLOBAL(j_unlink) != NULL) {
		ret = DB_GLOBAL(j_unlink)(path);
		goto done;
	}

	TO_TSTRING(dbenv, path, tpath, ret);
	if (ret != 0)
		return (ret);
	orig_tpath = tpath;

	/*
	 * Windows NT and its descendents allow removal of open files, but the
	 * DeleteFile Win32 system call isn't equivalent to a POSIX unlink.
	 * Firstly, it only succeeds if FILE_SHARE_DELETE is set when the file
	 * is opened.  Secondly, it leaves the file in a "zombie" state, where
	 * it can't be opened again, but a new file with the same name can't be
	 * created either.
	 *
	 * Since we depend on being able to recreate files (during recovery,
	 * say), we have to first rename the file, and then delete it.  It
	 * still hangs around, but with a name we don't care about.  The rename
	 * will fail if the file doesn't exist, which isn't a problem, but if
	 * it fails for some other reason, we need to know about it or a
	 * subsequent open may fail for no apparent reason.
	 */
	if (__os_is_winnt()) {
		__os_unique_id(dbenv, &id);
		_sntprintf(buf, MAXPATHLEN, _T("%s.del.%010u"), tpath, id);
		if (MoveFile(tpath, buf))
			tpath = buf;
		else if (__os_get_errno() != ENOENT)
			__db_err(dbenv,
			    "unlink: rename %s to temporary file failed", path);

		/*
		 * Try removing the file using the delete-on-close flag.  This
		 * plays nicer with files that are still open than DeleteFile.
		 */
		h = CreateFile(tpath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		    FILE_FLAG_DELETE_ON_CLOSE, 0);
		if (h != INVALID_HANDLE_VALUE) {
			(void)CloseHandle (h);
			if (GetFileAttributes(tpath) == INVALID_FILE_ATTRIBUTES)
				goto skipdel;
		}
	}

	RETRY_CHK((!DeleteFile(tpath)), ret);

skipdel:
	FREE_STRING(dbenv, orig_tpath);

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
