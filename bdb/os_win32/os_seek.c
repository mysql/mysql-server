/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_seek.c,v 11.17 2002/08/06 04:56:20 bostic Exp $";
#endif /* not lint */

#include "db_int.h"

/*
 * __os_seek --
 *	Seek to a page/byte offset in the file.
 */
int
__os_seek(dbenv, fhp, pgsize, pageno, relative, isrewind, db_whence)
	DB_ENV *dbenv;
	DB_FH *fhp;
	size_t pgsize;
	db_pgno_t pageno;
	u_int32_t relative;
	int isrewind;
	DB_OS_SEEK db_whence;
{
	/* Yes, this really is how Microsoft have designed their API */
	union {
		__int64 bigint;
		struct {
			unsigned long low;
			long high;
		};
	} offset;
	int ret, whence;
	DWORD from;

	if (DB_GLOBAL(j_seek) != NULL) {
		switch (db_whence) {
		case DB_OS_SEEK_CUR:
			whence = SEEK_CUR;
			break;
		case DB_OS_SEEK_END:
			whence = SEEK_END;
			break;
		case DB_OS_SEEK_SET:
			whence = SEEK_SET;
			break;
		default:
			return (EINVAL);
		}

		ret = DB_GLOBAL(j_seek)(fhp->fd, pgsize, pageno,
		    relative, isrewind, whence);
	} else {
		switch (db_whence) {
		case DB_OS_SEEK_CUR:
			from = FILE_CURRENT;
			break;
		case DB_OS_SEEK_END:
			from = FILE_END;
			break;
		case DB_OS_SEEK_SET:
			from = FILE_BEGIN;
			break;
		default:
			return (EINVAL);
		}

		offset.bigint = (__int64)pgsize * pageno + relative;
		if (isrewind)
			offset.bigint = -offset.bigint;

		ret = (SetFilePointer(fhp->handle,
		    offset.low, &offset.high, from) == (DWORD) - 1) ?
		    __os_win32_errno() : 0;
	}

	if (ret != 0)
		__db_err(dbenv, "seek: %lu %d %d: %s",
		    (u_long)pgsize * pageno + relative,
		    isrewind, db_whence, strerror(ret));

	return (ret);
}
