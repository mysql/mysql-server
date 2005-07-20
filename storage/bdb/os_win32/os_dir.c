/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_dir.c,v 11.20 2004/10/13 19:12:17 bostic Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_dirlist --
 *	Return a list of the files in a directory.
 */
int
__os_dirlist(dbenv, dir, namesp, cntp)
	DB_ENV *dbenv;
	const char *dir;
	char ***namesp;
	int *cntp;
{
	HANDLE dirhandle;
	WIN32_FIND_DATA fdata;
	int arraysz, cnt, ret;
	char **names, *onename;
	_TCHAR tfilespec[MAXPATHLEN + 1];

	if (DB_GLOBAL(j_dirlist) != NULL)
		return (DB_GLOBAL(j_dirlist)(dir, namesp, cntp));

	(void)_sntprintf(tfilespec, MAXPATHLEN,
	    _T("%hs%hc*"), dir, PATH_SEPARATOR[0]);
	if ((dirhandle = FindFirstFile(tfilespec, &fdata))
	    == INVALID_HANDLE_VALUE)
		return (__os_get_errno());

	names = NULL;
	arraysz = cnt = ret = 0;
	for (;;) {
		if (cnt >= arraysz) {
			arraysz += 100;
			if ((ret = __os_realloc(dbenv,
			    arraysz * sizeof(names[0]), &names)) != 0)
				goto err;
		}
		/*
		 * FROM_TSTRING doesn't necessarily allocate new memory, so we
		 * must do that explicitly.  Unfortunately, when compiled with
		 * UNICODE, we'll copy twice.
		 */
		FROM_TSTRING(dbenv, fdata.cFileName, onename, ret);
		if (ret != 0)
			goto err;
		ret = __os_strdup(dbenv, onename, &names[cnt]);
		FREE_STRING(dbenv, onename);
		if (ret != 0)
			goto err;
		cnt++;
		if (!FindNextFile(dirhandle, &fdata)) {
			if (GetLastError() == ERROR_NO_MORE_FILES)
				break;
			else {
				ret = __os_get_errno();
				goto err;
			}
		}
	}

err:	if (!FindClose(dirhandle) && ret == 0)
		ret = __os_get_errno();

	if (ret == 0) {
		*namesp = names;
		*cntp = cnt;
	} else if (names != NULL)
		__os_dirfree(dbenv, names, cnt);

	return (ret);
}

/*
 * __os_dirfree --
 *	Free the list of files.
 */
void
__os_dirfree(dbenv, names, cnt)
	DB_ENV *dbenv;
	char **names;
	int cnt;
{
	if (DB_GLOBAL(j_dirfree) != NULL) {
		DB_GLOBAL(j_dirfree)(names, cnt);
		return;
	}

	while (cnt > 0)
		__os_free(dbenv, names[--cnt]);
	__os_free(dbenv, names);
}
