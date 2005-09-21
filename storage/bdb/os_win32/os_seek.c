/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_seek.c,v 11.22 2004/09/17 22:00:32 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_seek --
 *	Seek to a page/byte offset in the file.
 */
int
__os_seek(dbenv, fhp, pgsize, pageno, relative, isrewind, db_whence)
	DB_ENV *dbenv;
	DB_FH *fhp;
	u_int32_t pgsize;
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
	} offbytes;
	off_t offset;
	int ret, whence;
	DWORD from;

	offset = (off_t)pgsize * pageno + relative;
	if (isrewind)
		offset = -offset;

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

		ret = DB_GLOBAL(j_seek)(fhp->fd, offset, whence);
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

		offbytes.bigint = offset;
		ret = (SetFilePointer(fhp->handle,
		    offbytes.low, &offbytes.high, from) == (DWORD) - 1) ?
		    __os_get_errno() : 0;
	}

	if (ret == 0) {
		fhp->pgsize = pgsize;
		fhp->pgno = pageno;
		fhp->offset = relative;
	} else {
		__db_err(dbenv, "seek: %lu %d %d: %s",
		    (u_long)pgsize * pageno + relative,
		    isrewind, db_whence, strerror(ret));
	}

	return (ret);
}
