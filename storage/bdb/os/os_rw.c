/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: os_rw.c,v 11.39 2004/09/17 22:00:31 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <sys/stat.h>

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
 *
 * PUBLIC: int __os_io __P((DB_ENV *,
 * PUBLIC:     int, DB_FH *, db_pgno_t, u_int32_t, u_int8_t *, size_t *));
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
#if defined(HAVE_PREAD) && defined(HAVE_PWRITE)
	ssize_t nio;
#endif
	int ret;

	/* Check for illegal usage. */
	DB_ASSERT(F_ISSET(fhp, DB_FH_OPENED) && fhp->fd != -1);

#if defined(HAVE_PREAD) && defined(HAVE_PWRITE)
	switch (op) {
	case DB_IO_READ:
		if (DB_GLOBAL(j_read) != NULL)
			goto slow;
		nio = DB_GLOBAL(j_pread) != NULL ? DB_GLOBAL(j_pread)
			(fhp->fd, buf, pagesize, (off_t)pgno * pagesize) :
			pread(fhp->fd, buf, pagesize, (off_t)pgno * pagesize);
		break;
	case DB_IO_WRITE:
		if (DB_GLOBAL(j_write) != NULL)
			goto slow;
#ifdef HAVE_FILESYSTEM_NOTZERO
		if (__os_fs_notzero())
			goto slow;
#endif
		nio = DB_GLOBAL(j_pwrite) != NULL ? DB_GLOBAL(j_pwrite)
			(fhp->fd, buf, pagesize, (off_t)pgno * pagesize) :
			pwrite(fhp->fd, buf, pagesize, (off_t)pgno * pagesize);
		break;
	default:
		return (EINVAL);
	}
	if (nio == (ssize_t)pagesize) {
		*niop = pagesize;
		return (0);
	}
slow:
#endif
	MUTEX_THREAD_LOCK(dbenv, fhp->mutexp);

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
	default:
		ret = EINVAL;
		break;
	}

err:	MUTEX_THREAD_UNLOCK(dbenv, fhp->mutexp);

	return (ret);

}

/*
 * __os_read --
 *	Read from a file handle.
 *
 * PUBLIC: int __os_read __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
 */
int
__os_read(dbenv, fhp, addr, len, nrp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nrp;
{
	size_t offset;
	ssize_t nr;
	int ret;
	u_int8_t *taddr;

	ret = 0;

	/* Check for illegal usage. */
	DB_ASSERT(F_ISSET(fhp, DB_FH_OPENED) && fhp->fd != -1);

	if (DB_GLOBAL(j_read) != NULL) {
		*nrp = len;
		if (DB_GLOBAL(j_read)(fhp->fd, addr, len) != (ssize_t)len) {
			ret = __os_get_errno();
			__db_err(dbenv, "read: %#lx, %lu: %s",
			    P_TO_ULONG(addr), (u_long)len, strerror(ret));
		}
		return (ret);
	}

	for (taddr = addr, offset = 0;
	    offset < len; taddr += nr, offset += (u_int32_t)nr) {
		RETRY_CHK(((nr = read(
		    fhp->fd, taddr, len - offset)) < 0 ? 1 : 0), ret);
		if (nr == 0 || ret != 0)
			break;
	}
	*nrp = (size_t)(taddr - (u_int8_t *)addr);
	if (ret != 0)
		__db_err(dbenv, "read: %#lx, %lu: %s",
		    P_TO_ULONG(taddr), (u_long)len - offset, strerror(ret));
	return (ret);
}

/*
 * __os_write --
 *	Write to a file handle.
 *
 * PUBLIC: int __os_write __P((DB_ENV *, DB_FH *, void *, size_t, size_t *));
 */
int
__os_write(dbenv, fhp, addr, len, nwp)
	DB_ENV *dbenv;
	DB_FH *fhp;
	void *addr;
	size_t len;
	size_t *nwp;
{
	/* Check for illegal usage. */
	DB_ASSERT(F_ISSET(fhp, DB_FH_OPENED) && fhp->fd != -1);

#ifdef HAVE_FILESYSTEM_NOTZERO
	/* Zero-fill as necessary. */
	if (__os_fs_notzero()) {
		int ret;
		if ((ret = __os_zerofill(dbenv, fhp)) != 0)
			return (ret);
	}
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
	size_t offset;
	ssize_t nw;
	int ret;
	u_int8_t *taddr;

	ret = 0;

#if defined(HAVE_FILESYSTEM_NOTZERO) && defined(DIAGNOSTIC)
	if (__os_fs_notzero()) {
		struct stat sb;
		off_t cur_off;

		DB_ASSERT(fstat(fhp->fd, &sb) != -1 &&
		    (cur_off = lseek(fhp->fd, (off_t)0, SEEK_CUR)) != -1 &&
		    cur_off <= sb.st_size);
	}
#endif

	if (DB_GLOBAL(j_write) != NULL) {
		*nwp = len;
		if (DB_GLOBAL(j_write)(fhp->fd, addr, len) != (ssize_t)len) {
			ret = __os_get_errno();
			__db_err(dbenv, "write: %#lx, %lu: %s",
			    P_TO_ULONG(addr), (u_long)len, strerror(ret));
		}
		return (ret);
	}

	for (taddr = addr, offset = 0;
	    offset < len; taddr += nw, offset += (u_int32_t)nw) {
		RETRY_CHK(((nw = write(
		    fhp->fd, taddr, len - offset)) < 0 ? 1 : 0), ret);
		if (ret != 0)
			break;
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
	off_t stat_offset, write_offset;
	size_t blen, nw;
	u_int32_t bytes, mbytes;
	int group_sync, need_free, ret;
	u_int8_t buf[8 * 1024], *bp;

	/* Calculate the byte offset of the next write. */
	write_offset = (off_t)fhp->pgno * fhp->pgsize + fhp->offset;

	/* Stat the file. */
	if ((ret = __os_ioinfo(dbenv, NULL, fhp, &mbytes, &bytes, NULL)) != 0)
		return (ret);
	stat_offset = (off_t)mbytes * MEGABYTE + bytes;

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
