/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_truncate.c,v 1.9 2004/10/05 14:45:30 mjc Exp $
 */

#include "db_config.h"

#include "db_int.h"

/*
 * __os_truncate --
 *	Truncate the file.
 */
int
__os_truncate(dbenv, fhp, pgno, pgsize)
	DB_ENV *dbenv;
	DB_FH *fhp;
	db_pgno_t pgno;
	u_int32_t pgsize;
{
	/* Yes, this really is how Microsoft have designed their API */
	union {
		__int64 bigint;
		struct {
			unsigned long low;
			long high;
		};
	} oldpos;
	off_t offset;
	int ret, retries, t_ret;

	offset = (off_t)pgsize * pgno;

	if (DB_GLOBAL(j_ftruncate) != NULL) {
		ret = DB_GLOBAL(j_ftruncate)(fhp->fd, offset);
		goto done;
	}

#ifdef HAVE_FILESYSTEM_NOTZERO
	/*
	 * If the filesystem doesn't zero fill, it isn't safe to extend the
	 * file, or we end up with junk blocks.  Just return in that case.
	 */
	if (__os_fs_notzero()) {
		off_t stat_offset;
		u_int32_t mbytes, bytes;

		/* Stat the file. */
		if ((ret =
		    __os_ioinfo(dbenv, NULL, fhp, &mbytes, &bytes, NULL)) != 0)
			return (ret);
		stat_offset = (off_t)mbytes * MEGABYTE + bytes;

		if (offset > stat_offset)
			return (0);
	}
#endif

	retries = 0;
	do {
		/*
		 * Windows doesn't provide truncate directly.  Instead,
		 * it has SetEndOfFile, which truncates to the current
		 * position.  So we have to save the current position,
		 * seek to where we want to truncate to, then seek back
		 * to where we were.  To avoid races, all of that needs
		 * to be done while holding the file handle mutex.
		 */
		MUTEX_THREAD_LOCK(dbenv, fhp->mutexp);
		oldpos.bigint = 0;
		if ((oldpos.low = SetFilePointer(fhp->handle,
		    0, &oldpos.high, FILE_CURRENT)) == -1 &&
		    GetLastError() != NO_ERROR) {
			ret = __os_get_errno();
			goto end;
		}
		if ((ret = __os_seek(dbenv, fhp, pgsize, pgno,
		    0, 0, DB_OS_SEEK_SET)) != 0)
			goto end;
		if (!SetEndOfFile(fhp->handle))
			ret = __os_get_errno();
		if ((t_ret = __os_seek(dbenv, fhp, pgsize,
		    (db_pgno_t)(oldpos.bigint / pgsize),
		    0, 0, DB_OS_SEEK_SET)) != 0 && ret == 0)
			ret = t_ret;
end:		MUTEX_THREAD_UNLOCK(dbenv, fhp->mutexp);
	} while ((ret == EAGAIN || ret == EBUSY || ret == EINTR) &&
	    ++retries < DB_RETRY);

done:	if (ret != 0)
		__db_err(dbenv,
		    "ftruncate: %lu: %s", pgno * pgsize, strerror(ret));

	return (ret);
}
