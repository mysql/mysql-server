/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_dir.c,v 11.4 2000/03/28 21:50:17 ubell Exp $";
#endif /* not lint */

#include "db_int.h"
#include "os_jump.h"

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
	long dirhandle;
	int arraysz, cnt, finished, ret;
	char **names, filespec[MAXPATHLEN];

	if (__db_jump.j_dirlist != NULL)
		return (__db_jump.j_dirlist(dir, namesp, cntp));

	(void)snprintf(filespec, sizeof(filespec), "%s/*", dir);
	if ((dirhandle = _findfirst(filespec, &fdata)) == -1)
		return (__os_get_errno());

	names = NULL;
	finished = 0;
	for (arraysz = cnt = 0; finished != 1; ++cnt) {
		if (cnt >= arraysz) {
			arraysz += 100;
			if ((ret = __os_realloc(dbenv,
			    arraysz * sizeof(names[0]), NULL, &names)) != 0)
				goto nomem;
		}
		if ((ret = __os_strdup(dbenv, fdata.name, &names[cnt])) != 0)
			goto nomem;
		if (_findnext(dirhandle,&fdata) != 0)
			finished = 1;
	}
	_findclose(dirhandle);

	*namesp = names;
	*cntp = cnt;
	return (0);

nomem:	if (names != NULL)
		__os_dirfree(names, cnt);
	return (ret);
}

/*
 * __os_dirfree --
 *	Free the list of files.
 */
void
__os_dirfree(names, cnt)
	char **names;
	int cnt;
{
	if (__db_jump.j_dirfree != NULL) {
		__db_jump.j_dirfree(names, cnt);
		return;
	}

	while (cnt > 0)
		__os_free(names[--cnt], 0);
	__os_free(names, 0);
}
