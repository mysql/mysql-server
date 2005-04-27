/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_dir.c,v 11.12 2002/07/12 18:56:54 bostic Exp $";
#endif /* not lint */

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
	struct _finddata_t fdata;
#ifdef _WIN64
	intptr_t dirhandle;
#else
	long dirhandle;
#endif
	int arraysz, cnt, finished, ret;
	char **names, filespec[MAXPATHLEN];

	if (DB_GLOBAL(j_dirlist) != NULL)
		return (DB_GLOBAL(j_dirlist)(dir, namesp, cntp));

	(void)snprintf(filespec, sizeof(filespec), "%s/*", dir);
	if ((dirhandle = _findfirst(filespec, &fdata)) == -1)
		return (__os_get_errno());

	names = NULL;
	finished = 0;
	for (arraysz = cnt = 0; finished != 1; ++cnt) {
		if (cnt >= arraysz) {
			arraysz += 100;
			if ((ret = __os_realloc(dbenv,
			    arraysz * sizeof(names[0]), &names)) != 0)
				goto nomem;
		}
		if ((ret = __os_strdup(dbenv, fdata.name, &names[cnt])) != 0)
			goto nomem;
		if (_findnext(dirhandle, &fdata) != 0)
			finished = 1;
	}
	_findclose(dirhandle);

	*namesp = names;
	*cntp = cnt;
	return (0);

nomem:	if (names != NULL)
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
