/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_rw.c,v 11.38 2004/09/17 22:00:32 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"

#ifdef HAVE_FILESYSTEM_NOTZERO
static int __os_zerofill __P((DB_ENV *, DB_FH *));
#endif
static int __os_physwrite __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));

/*
 * __os_io --
 *	Do an I/O.
 */
int
__os_io(dbenv, op, fhp, pgno, pagesize, buf, niop)
	DB_ENV *dbenv;
	int op;
	DB_FH *fhp;
	db_pgno_t pgno;
	u_int32_t pagesize;
	u_int8_t *buf;
	size_t *niop;
{
	int ret;

	if (__os_is_winnt()) {
		ULONG64 off = (ULONG64)pagesize * pgno;
		OVERLAPPED over;
		DWORD nbytes;
		over.Offset = (DWORD)(off & 0xffffffff);
		over.OffsetHigh = (DWORD)(off >> 32);
		over.hEvent = 0; /* we don't want asynchronous notifications */

		switch (op) {
		case DB_IO_READ:
			if (DB_GLOBAL(j_read) != NULL)
				goto slow;
			if (!ReadFile(fhp->handle,
			    buf, (DWORD)pagesize, &nbytes, &over))
				goto slow;
			break;
		case DB_IO_WRITE:
			if (DB_GLOBAL(j_write) != NULL)
				goto slow;
#ifdef HAVE_FILESYSTEM_NOTZERO
			if (__os_fs_notzero())
				goto slow;
#endif
			if (!WriteFile(fhp->handle,
			    buf, (DWORD)pagesize, &nbytes, &over))
				goto slow;
			break;
		}
		if (nbytes == pagesize) {
			*niop = (size_t)nbytes;
			return (0);
		}
	}

slow:	MUTEX_THREAD_LOCK(dbenv, fhp->mutexp);

	if ((ret = __os_seek(dbenv, fhp,
	    pagesize, pgno, 0, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	switch (op) {
	case DB_IO_READ:
		ret = __os_read(dbenv, fhp, buf, pagesize, niop);
		break;
	case DB_IO_WRITE:
		ret = __os_write(dbenv, fhp, buf, pagesize, niop);
		break;
	}

err:	MUTEX_THREAD_UNLOCK(dbenv, fhp->mutexp);

	return (ret);
}

/*
 * __os_read --
 *	Read from a file handle.
 */
int
__os_read(dbenv, fhp, addr, len, nrp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nrp;
{
	size_t offset, nr;
	DWORD count;
	int ret;
	u_int8_t *taddr;

	ret = 0;

	if (DB_GLOBAL(j_read) != NULL) {
		*nrp = len;
		if (DB_GLOBAL(j_read)(fhp->fd, addr, len) != (ssize_t)len) {
			ret = __os_get_errno();
			__db_err(dbenv, "read: %#lx, %lu: %s",
			    P_TO_ULONG(addr), (u_long)len, strerror(ret));
		}
		return (ret);
	}

	ret = 0;
	for (taddr = addr,
	    offset = 0; offset < len; taddr += nr, offset += nr) {
		RETRY_CHK((!ReadFile(fhp->handle,
		    taddr, (DWORD)(len - offset), &count, NULL)), ret);
		if (count == 0 || ret != 0)
			break;
		nr = (size_t)count;
	}
	*nrp = taddr - (u_int8_t *)addr;
	if (ret != 0)
		__db_err(dbenv, "read: 0x%lx, %lu: %s",
		    P_TO_ULONG(taddr), (u_long)len - offset, strerror(ret));
	return (ret);
}

/*
 * __os_write --
 *	Write to a file handle.
 */
int
__os_write(dbenv, fhp, addr, len, nwp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nwp;
{
	int ret;

#ifdef HAVE_FILESYSTEM_NOTZERO
	/* Zero-fill as necessary. */
	if (__os_fs_notzero() && (ret = __os_zerofill(dbenv, fhp)) != 0)
		return (ret);
#endif
	return (__os_physwrite(dbenv, fhp, addr, len, nwp));
}

/*
 * __os_physwrite --
 *	Physical write to a file handle.
 */
static int
__os_physwrite(dbenv, fhp, addr, len, nwp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nwp;
{
	size_t offset, nw;
	DWORD count;
	int ret;
	u_int8_t *taddr;

	if (DB_GLOBAL(j_write) != NULL) {
		*nwp = len;
		if (DB_GLOBAL(j_write)(fhp->fd, addr, len) != (ssize_t)len) {
			ret = __os_get_errno();
			__db_err(dbenv, "write: %#lx, %lu: %s",
			    P_TO_ULONG(addr), (u_long)len, strerror(ret));
		}
		return (ret);
	}

	ret = 0;
	for (taddr = addr,
	    offset = 0; offset < len; taddr += nw, offset += nw) {
		RETRY_CHK((!WriteFile(fhp->handle,
		    taddr, (DWORD)(len - offset), &count, NULL)), ret);
		if (ret != 0)
			break;
		nw = (size_t)count;
	}
	*nwp = len;
	if (ret != 0)
		__db_err(dbenv, "write: %#lx, %lu: %s",
		    P_TO_ULONG(taddr), (u_long)len - offset, strerror(ret));
	return (ret);
}

#ifdef HAVE_FILESYSTEM_NOTZERO
/*
 * __os_zerofill --
 *	Zero out bytes in the file.
 *
 *	Pages allocated by writing pages past end-of-file are not zeroed,
 *	on some systems.  Recovery could theoretically be fooled by a page
 *	showing up that contained garbage.  In order to avoid this, we
 *	have to write the pages out to disk, and flush them.  The reason
 *	for the flush is because if we don't sync, the allocation of another
 *	page subsequent to this one might reach the disk first, and if we
 *	crashed at the right moment, leave us with this page as the one
 *	allocated by writing a page past it in the file.
 */
static int
__os_zerofill(dbenv, fhp)
	DB_ENV *dbenv;
	DB_FH *fhp;
{
	unsigned __int64 stat_offset, write_offset;
	size_t blen, nw;
	u_int32_t bytes, mbytes;
	int group_sync, need_free, ret;
	u_int8_t buf[8 * 1024], *bp;

	/* Calculate the byte offset of the next write. */
	write_offset = (unsigned __int64)fhp->pgno * fhp->pgsize + fhp->offset;

	/* Stat the file. */
	if ((ret = __os_ioinfo(dbenv, NULL, fhp, &mbytes, &bytes, NULL)) != 0)
		return (ret);
	stat_offset = (unsigned __int64)mbytes * MEGABYTE + bytes;

	/* Check if the file is large enough. */
	if (stat_offset >= write_offset)
		return (0);

	/* Get a large buffer if we're writing lots of data. */
#undef	ZF_LARGE_WRITE
#define	ZF_LARGE_WRITE	(64 * 1024)
	if (write_offset - stat_offset > ZF_LARGE_WRITE) {
		if ((ret = __os_calloc(dbenv, 1, ZF_LARGE_WRITE, &bp)) != 0)
			    return (ret);
		blen = ZF_LARGE_WRITE;
		need_free = 1;
	} else {
		bp = buf;
		blen = sizeof(buf);
		need_free = 0;
		memset(buf, 0, sizeof(buf));
	}

	/* Seek to the current end of the file. */
	if ((ret = __os_seek(
	    dbenv, fhp, MEGABYTE, mbytes, bytes, 0, DB_OS_SEEK_SET)) != 0)
		goto err;

	/*
	 * Hash is the only access method that allocates groups of pages.  Hash
	 * uses the existence of the last page in a group to signify the entire
	 * group is OK; so, write all the pages but the last one in the group,
	 * flush them to disk, then write the last one to disk and flush it.
	 */
	for (group_sync = 0; stat_offset < write_offset; group_sync = 1) {
		if (write_offset - stat_offset <= blen) {
			blen = (size_t)(write_offset - stat_offset);
			if (group_sync && (ret = __os_fsync(dbenv, fhp)) != 0)
				goto err;
		}
		if ((ret = __os_physwrite(dbenv, fhp, bp, blen, &nw)) != 0)
			goto err;
		stat_offset += blen;
	}
	if ((ret = __os_fsync(dbenv, fhp)) != 0)
		goto err;

	/* Seek back to where we started. */
	mbytes = (u_int32_t)(write_offset / MEGABYTE);
	bytes = (u_int32_t)(write_offset % MEGABYTE);
	ret = __os_seek(dbenv, fhp, MEGABYTE, mbytes, bytes, 0, DB_OS_SEEK_SET);

err:	if (need_free)
		__os_free(dbenv, bp);
	return (ret);
}
#endif
