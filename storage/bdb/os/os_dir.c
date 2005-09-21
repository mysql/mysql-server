/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_dir.c,v 11.17 2004/04/26 18:48:19 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#endif

#include "db_int.h"

/*
 * __os_dirlist --
 *	Return a list of the files in a directory.
 *
 * PUBLIC: int __os_dirlist __P((DB_ENV *, const char *, char ***, int *));
 */
int
__os_dirlist(dbenv, dir, namesp, cntp)
	DB_ENV *dbenv;
	const char *dir;
	char ***namesp;
	int *cntp;
{
	struct dirent *dp;
	DIR *dirp;
	int arraysz, cnt, ret;
	char **names;

	if (DB_GLOBAL(j_dirlist) != NULL)
		return (DB_GLOBAL(j_dirlist)(dir, namesp, cntp));

#ifdef HAVE_VXWORKS
	if ((dirp = opendir((char *)dir)) == NULL)
#else
	if ((dirp = opendir(dir)) == NULL)
#endif
		return (__os_get_errno());
	names = NULL;
	for (arraysz = cnt = 0; (dp = readdir(dirp)) != NULL; ++cnt) {
		if (cnt >= arraysz) {
			arraysz += 100;
			if ((ret = __os_realloc(dbenv,
			    (u_int)arraysz * sizeof(names[0]), &names)) != 0)
				goto nomem;
		}
		if ((ret = __os_strdup(dbenv, dp->d_name, &names[cnt])) != 0)
			goto nomem;
	}
	(void)closedir(dirp);

	*namesp = names;
	*cntp = cnt;
	return (0);

nomem:	if (names != NULL)
		__os_dirfree(dbenv, names, cnt);
	if (dirp != NULL)
		(void)closedir(dirp);
	return (ret);
}

/*
 * __os_dirfree --
 *	Free the list of files.
 *
 * PUBLIC: void __os_dirfree __P((DB_ENV *, char **, int));
 */
void
__os_dirfree(dbenv, names, cnt)
	DB_ENV *dbenv;
	char **names;
	int cnt;
{
	if (DB_GLOBAL(j_dirfree) != NULL)
		DB_GLOBAL(j_dirfree)(names, cnt);
	else {
		while (cnt > 0)
			__os_free(dbenv, names[--cnt]);
		__os_free(dbenv, names);
	}
}
