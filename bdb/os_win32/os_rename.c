/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_rename.c,v 1.2 2000/06/13 19:52:19 dda Exp $";
#endif /* not lint */

#include "db_int.h"
#include "os_jump.h"

/*
 * __os_rename --
 *	Rename a file.
 */
int
__os_rename(dbenv, old, new)
	DB_ENV *dbenv;
	const char *old, *new;
{
	int ret;

	ret = 0;
	if (__db_jump.j_rename != NULL) {
		if (__db_jump.j_rename(old, new) == -1)
			ret = __os_get_errno();
	}
	else {
		/* Normally we would use a single MoveFileEx call with
		 * MOVEFILE_REPLACE_EXISTING flag to simulate Unix rename().
		 * But if the target file exists, and the two files' 8.3
		 * names are identical, a Windows bug causes the target file
		 * to be deleted, but the original file will not be renamed,
		 * and an ENOENT error will be returned.  (See MSDN for a
		 * description of the bug).
		 *
		 * After the failed call, a MoveFile seems to perform
		 * the rename correctly (even another call to MoveFileEx
		 * does not)!  The expense of this extra call only occurs
		 * on systems with the bug: Windows/98, for one, but
		 * apparently not Windows/NT and Windows/2000.
		 */
		if (MoveFileEx(old, new, MOVEFILE_REPLACE_EXISTING) != TRUE)
			ret = __os_win32_errno();
		if ((ret == ENOENT || ret == EIO) && MoveFile(old, new) == TRUE)
			ret = 0;
	}
	if (ret != 0)
		__db_err(dbenv, "Rename %s %s: %s", old, new, strerror(ret));

	return (ret);
}
