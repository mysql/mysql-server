/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_dir.c,v 11.8 2000/06/27 17:29:52 sue Exp $";
#endif /* not lint */

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
#include "os_jump.h"

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

	if (__db_jump.j_dirlist != NULL)
		return (__db_jump.j_dirlist(dir, namesp, cntp));

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
			    arraysz * sizeof(names[0]), NULL, &names)) != 0)
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
		__os_dirfree(names, cnt);
	if (dirp != NULL)
		(void)closedir(dirp);
	return (ret);
}

/*
 * __os_dirfree --
 *	Free the list of files.
 *
 * PUBLIC: void __os_dirfree __P((char **, int));
 */
void
__os_dirfree(names, cnt)
	char **names;
	int cnt;
{
	if (__db_jump.j_dirfree != NULL)
		__db_jump.j_dirfree(names, cnt);
	else {
		while (cnt > 0)
			__os_free(names[--cnt], 0);
		__os_free(names, 0);
	}
}
