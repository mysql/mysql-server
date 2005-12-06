/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_truncate.c,v 12.4 2005/10/12 17:57:33 bostic Exp $
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
	} off;
	off_t offset;
	HANDLE dup_handle;
	int ret;

	ret = 0;
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

	/*
	 * Windows doesn't provide truncate directly.  Instead, it has
	 * SetEndOfFile, which truncates to the current position.  To
	 * deal with that, we first duplicate the file handle, then
	 * seek and set the end of file.  This is necessary to avoid
	 * races with {Read,Write}File in other threads.
	 */
	if (!DuplicateHandle(GetCurrentProcess(), fhp->handle,
	    GetCurrentProcess(), &dup_handle, 0, FALSE,
	    DUPLICATE_SAME_ACCESS)) {
		ret = __os_get_errno();
		goto done;
	}

	off.bigint = (__int64)pgsize * pgno;
	RETRY_CHK((SetFilePointer(dup_handle,
	    off.low, &off.high, FILE_BEGIN) == INVALID_SET_FILE_POINTER &&
	    GetLastError() != NO_ERROR) ||
	    !SetEndOfFile(dup_handle), ret);

	if (!CloseHandle(dup_handle) && ret == 0)
		ret = __os_get_errno();

done:	if (ret != 0)
		__db_err(dbenv,
		    "ftruncate: %lu: %s", pgno * pgsize, strerror(ret));

	return (ret);
}
