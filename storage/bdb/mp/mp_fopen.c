/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_fopen.c,v 11.90 2002/08/26 15:22:01 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

static int  __memp_fclose __P((DB_MPOOLFILE *, u_int32_t));
static int  __memp_fopen __P((DB_MPOOLFILE *,
		const char *, u_int32_t, int, size_t));
static void __memp_get_fileid __P((DB_MPOOLFILE *, u_int8_t *));
static void __memp_last_pgno __P((DB_MPOOLFILE *, db_pgno_t *));
static void __memp_refcnt __P((DB_MPOOLFILE *, db_pgno_t *));
static int  __memp_set_clear_len __P((DB_MPOOLFILE *, u_int32_t));
static int  __memp_set_fileid __P((DB_MPOOLFILE *, u_int8_t *));
static int  __memp_set_ftype __P((DB_MPOOLFILE *, int));
static int  __memp_set_lsn_offset __P((DB_MPOOLFILE *, int32_t));
static int  __memp_set_pgcookie __P((DB_MPOOLFILE *, DBT *));
static int  __memp_set_priority __P((DB_MPOOLFILE *, DB_CACHE_PRIORITY));
static void __memp_set_unlink __P((DB_MPOOLFILE *, int));

/* Initialization methods cannot be called after open is called. */
#define	MPF_ILLEGAL_AFTER_OPEN(dbmfp, name)				\
	if (F_ISSET(dbmfp, MP_OPEN_CALLED))				\
		return (__db_mi_open((dbmfp)->dbmp->dbenv, name, 1));

/*
 * __memp_fcreate --
 *	Create a DB_MPOOLFILE handle.
 *
 * PUBLIC: int __memp_fcreate __P((DB_ENV *, DB_MPOOLFILE **, u_int32_t));
 */
int
__memp_fcreate(dbenv, retp, flags)
	DB_ENV *dbenv;
	DB_MPOOLFILE **retp;
	u_int32_t flags;
{
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_fcreate", DB_INIT_MPOOL);

	dbmp = dbenv->mp_handle;

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "memp_fcreate", flags, 0)) != 0)
		return (ret);

	/* Allocate and initialize the per-process structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_MPOOLFILE), &dbmfp)) != 0)
		return (ret);
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_FH), &dbmfp->fhp)) != 0)
		goto err;

	/* Allocate and initialize a mutex if necessary. */
	if (F_ISSET(dbenv, DB_ENV_THREAD) &&
	    (ret = __db_mutex_setup(dbenv, dbmp->reginfo, &dbmfp->mutexp,
	    MUTEX_ALLOC | MUTEX_THREAD)) != 0)
		goto err;

	dbmfp->ref = 1;
	dbmfp->lsn_offset = -1;
	dbmfp->dbmp = dbmp;
	dbmfp->mfp = INVALID_ROFF;

	dbmfp->close = __memp_fclose;
	dbmfp->get = __memp_fget;
	dbmfp->get_fileid = __memp_get_fileid;
	dbmfp->last_pgno = __memp_last_pgno;
	dbmfp->open = __memp_fopen;
	dbmfp->put = __memp_fput;
	dbmfp->refcnt = __memp_refcnt;
	dbmfp->set = __memp_fset;
	dbmfp->set_clear_len = __memp_set_clear_len;
	dbmfp->set_fileid = __memp_set_fileid;
	dbmfp->set_ftype = __memp_set_ftype;
	dbmfp->set_lsn_offset = __memp_set_lsn_offset;
	dbmfp->set_pgcookie = __memp_set_pgcookie;
	dbmfp->set_priority = __memp_set_priority;
	dbmfp->set_unlink = __memp_set_unlink;
	dbmfp->sync = __memp_fsync;

	*retp = dbmfp;
	return (0);

err:	if (dbmfp != NULL) {
		if (dbmfp->fhp != NULL)
			(void)__os_free(dbenv, dbmfp->fhp);
		(void)__os_free(dbenv, dbmfp);
	}
	return (ret);
}

/*
 * __memp_set_clear_len --
 *	Set the clear length.
 */
static int
__memp_set_clear_len(dbmfp, clear_len)
	DB_MPOOLFILE *dbmfp;
	u_int32_t clear_len;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_clear_len");

	dbmfp->clear_len = clear_len;
	return (0);
}

/*
 * __memp_set_fileid --
 *	Set the file ID.
 */
static int
__memp_set_fileid(dbmfp, fileid)
	DB_MPOOLFILE *dbmfp;
	u_int8_t *fileid;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_fileid");

	/*
	 * XXX
	 * This is dangerous -- we're saving the caller's pointer instead
	 * of allocating memory and copying the contents.
	 */
	dbmfp->fileid = fileid;
	return (0);
}

/*
 * __memp_set_ftype --
 *	Set the file type (as registered).
 */
static int
__memp_set_ftype(dbmfp, ftype)
	DB_MPOOLFILE *dbmfp;
	int ftype;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_ftype");

	dbmfp->ftype = ftype;
	return (0);
}

/*
 * __memp_set_lsn_offset --
 *	Set the page's LSN offset.
 */
static int
__memp_set_lsn_offset(dbmfp, lsn_offset)
	DB_MPOOLFILE *dbmfp;
	int32_t lsn_offset;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_lsn_offset");

	dbmfp->lsn_offset = lsn_offset;
	return (0);
}

/*
 * __memp_set_pgcookie --
 *	Set the pgin/pgout cookie.
 */
static int
__memp_set_pgcookie(dbmfp, pgcookie)
	DB_MPOOLFILE *dbmfp;
	DBT *pgcookie;
{
	MPF_ILLEGAL_AFTER_OPEN(dbmfp, "set_pgcookie");

	dbmfp->pgcookie = pgcookie;
	return (0);
}

/*
 * __memp_set_priority --
 *	Set the cache priority for pages from this file.
 */
static int
__memp_set_priority(dbmfp, priority)
	DB_MPOOLFILE *dbmfp;
	DB_CACHE_PRIORITY priority;
{
	switch (priority) {
	case DB_PRIORITY_VERY_LOW:
		dbmfp->mfp->priority = MPOOL_PRI_VERY_LOW;
		break;
	case DB_PRIORITY_LOW:
		dbmfp->mfp->priority = MPOOL_PRI_LOW;
		break;
	case DB_PRIORITY_DEFAULT:
		dbmfp->mfp->priority = MPOOL_PRI_DEFAULT;
		break;
	case DB_PRIORITY_HIGH:
		dbmfp->mfp->priority = MPOOL_PRI_HIGH;
		break;
	case DB_PRIORITY_VERY_HIGH:
		dbmfp->mfp->priority = MPOOL_PRI_VERY_HIGH;
		break;
	default:
		__db_err(dbmfp->dbmp->dbenv,
		    "Unknown priority value: %d", priority);
		return (EINVAL);
	}

	return (0);
}

/*
 * __memp_fopen --
 *	Open a backing file for the memory pool.
 */
static int
__memp_fopen(dbmfp, path, flags, mode, pagesize)
	DB_MPOOLFILE *dbmfp;
	const char *path;
	u_int32_t flags;
	int mode;
	size_t pagesize;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

	PANIC_CHECK(dbenv);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "memp_fopen", flags,
	    DB_CREATE | DB_DIRECT | DB_EXTENT |
	    DB_NOMMAP | DB_ODDFILESIZE | DB_RDONLY | DB_TRUNCATE)) != 0)
		return (ret);

	/*
	 * Require a non-zero, power-of-two pagesize, smaller than the
	 * clear length.
	 */
	if (pagesize == 0 || !POWER_OF_TWO(pagesize)) {
		__db_err(dbenv,
		    "memp_fopen: page sizes must be a power-of-2");
		return (EINVAL);
	}
	if (dbmfp->clear_len > pagesize) {
		__db_err(dbenv,
		    "memp_fopen: clear length larger than page size");
		return (EINVAL);
	}

	/* Read-only checks, and local flag. */
	if (LF_ISSET(DB_RDONLY) && path == NULL) {
		__db_err(dbenv,
		    "memp_fopen: temporary files can't be readonly");
		return (EINVAL);
	}

	return (__memp_fopen_int(dbmfp, NULL, path, flags, mode, pagesize));
}

/*
 * __memp_fopen_int --
 *	Open a backing file for the memory pool; internal version.
 *
 * PUBLIC: int __memp_fopen_int __P((DB_MPOOLFILE *,
 * PUBLIC:     MPOOLFILE *, const char *, u_int32_t, int, size_t));
 */
int
__memp_fopen_int(dbmfp, mfp, path, flags, mode, pagesize)
	DB_MPOOLFILE *dbmfp;
	MPOOLFILE *mfp;
	const char *path;
	u_int32_t flags;
	int mode;
	size_t pagesize;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOL *mp;
	db_pgno_t last_pgno;
	size_t maxmap;
	u_int32_t mbytes, bytes, oflags;
	int mfp_alloc, ret;
	u_int8_t idbuf[DB_FILE_ID_LEN];
	char *rpath;
	void *p;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	mfp_alloc = ret = 0;
	rpath = NULL;

	/*
	 * Set the page size so os_open can decide whether to turn buffering
	 * off if the DB_DIRECT_DB flag is set.
	 */
	dbmfp->fhp->pagesize = (u_int32_t)pagesize;

	/*
	 * If it's a temporary file, delay the open until we actually need
	 * to write the file, and we know we can't join any existing files.
	 */
	if (path == NULL)
		goto alloc;

	/*
	 * Get the real name for this file and open it.  If it's a Queue extent
	 * file, it may not exist, and that's OK.
	 */
	oflags = 0;
	if (LF_ISSET(DB_CREATE))
		oflags |= DB_OSO_CREATE;
	if (LF_ISSET(DB_DIRECT))
		oflags |= DB_OSO_DIRECT;
	if (LF_ISSET(DB_RDONLY)) {
		F_SET(dbmfp, MP_READONLY);
		oflags |= DB_OSO_RDONLY;
	}
	if ((ret =
	    __db_appname(dbenv, DB_APP_DATA, path, 0, NULL, &rpath)) != 0)
		goto err;
	if ((ret = __os_open(dbenv, rpath, oflags, mode, dbmfp->fhp)) != 0) {
		if (!LF_ISSET(DB_EXTENT))
			__db_err(dbenv, "%s: %s", rpath, db_strerror(ret));
		goto err;
	}

	/*
	 * Figure out the file's size.
	 *
	 * !!!
	 * We can't use off_t's here, or in any code in the mainline library
	 * for that matter.  (We have to use them in the os stubs, of course,
	 * as there are system calls that take them as arguments.)  The reason
	 * is some customers build in environments where an off_t is 32-bits,
	 * but still run where offsets are 64-bits, and they pay us a lot of
	 * money.
	 */
	if ((ret = __os_ioinfo(
	    dbenv, rpath, dbmfp->fhp, &mbytes, &bytes, NULL)) != 0) {
		__db_err(dbenv, "%s: %s", rpath, db_strerror(ret));
		goto err;
	}

	/*
	 * Get the file id if we weren't given one.  Generated file id's
	 * don't use timestamps, otherwise there'd be no chance of any
	 * other process joining the party.
	 */
	if (dbmfp->fileid == NULL) {
		if ((ret = __os_fileid(dbenv, rpath, 0, idbuf)) != 0)
			goto err;
		dbmfp->fileid = idbuf;
	}

	/*
	 * If our caller knows what mfp we're using, increment the ref count,
	 * no need to search.
	 *
	 * We don't need to acquire a lock other than the mfp itself, because
	 * we know there's another reference and it's not going away.
	 */
	if (mfp != NULL) {
		MUTEX_LOCK(dbenv, &mfp->mutex);
		++mfp->mpf_cnt;
		MUTEX_UNLOCK(dbenv, &mfp->mutex);
		goto check_map;
	}

	/*
	 * If not creating a temporary file, walk the list of MPOOLFILE's,
	 * looking for a matching file.  Files backed by temporary files
	 * or previously removed files can't match.
	 *
	 * DB_TRUNCATE support.
	 *
	 * The fileID is a filesystem unique number (e.g., a UNIX dev/inode
	 * pair) plus a timestamp.  If files are removed and created in less
	 * than a second, the fileID can be repeated.  The problem with
	 * repetition happens when the file that previously had the fileID
	 * value still has pages in the pool, since we don't want to use them
	 * to satisfy requests for the new file.
	 *
	 * Because the DB_TRUNCATE flag reuses the dev/inode pair, repeated
	 * opens with that flag set guarantees matching fileIDs when the
	 * machine can open a file and then re-open with truncate within a
	 * second.  For this reason, we pass that flag down, and, if we find
	 * a matching entry, we ensure that it's never found again, and we
	 * create a new entry for the current request.
	 */
	R_LOCK(dbenv, dbmp->reginfo);
	for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
		/* Skip dead files and temporary files. */
		if (F_ISSET(mfp, MP_DEADFILE | MP_TEMP))
			continue;

		/* Skip non-matching files. */
		if (memcmp(dbmfp->fileid, R_ADDR(dbmp->reginfo,
		    mfp->fileid_off), DB_FILE_ID_LEN) != 0)
			continue;

		/*
		 * If the file is being truncated, remove it from the system
		 * and create a new entry.
		 *
		 * !!!
		 * We should be able to set mfp to NULL and break out of the
		 * loop, but I like the idea of checking all the entries.
		 */
		if (LF_ISSET(DB_TRUNCATE)) {
			MUTEX_LOCK(dbenv, &mfp->mutex);
			MPOOLFILE_IGNORE(mfp);
			MUTEX_UNLOCK(dbenv, &mfp->mutex);
			continue;
		}

		/*
		 * Some things about a file cannot be changed: the clear length,
		 * page size, or lSN location.
		 *
		 * The file type can change if the application's pre- and post-
		 * processing needs change.  For example, an application that
		 * created a hash subdatabase in a database that was previously
		 * all btree.
		 *
		 * XXX
		 * We do not check to see if the pgcookie information changed,
		 * or update it if it is, this might be a bug.
		 */
		if (dbmfp->clear_len != mfp->clear_len ||
		    pagesize != mfp->stat.st_pagesize ||
		    dbmfp->lsn_offset != mfp->lsn_off) {
			__db_err(dbenv,
		    "%s: clear length, page size or LSN location changed",
			    path);
			R_UNLOCK(dbenv, dbmp->reginfo);
			ret = EINVAL;
			goto err;
		}

		if (dbmfp->ftype != 0)
			mfp->ftype = dbmfp->ftype;

		MUTEX_LOCK(dbenv, &mfp->mutex);
		++mfp->mpf_cnt;
		MUTEX_UNLOCK(dbenv, &mfp->mutex);
		break;
	}
	R_UNLOCK(dbenv, dbmp->reginfo);

	if (mfp != NULL)
		goto check_map;

alloc:	/* Allocate and initialize a new MPOOLFILE. */
	if ((ret = __memp_alloc(
	    dbmp, dbmp->reginfo, NULL, sizeof(MPOOLFILE), NULL, &mfp)) != 0)
		goto err;
	mfp_alloc = 1;
	memset(mfp, 0, sizeof(MPOOLFILE));
	mfp->mpf_cnt = 1;
	mfp->ftype = dbmfp->ftype;
	mfp->stat.st_pagesize = pagesize;
	mfp->lsn_off = dbmfp->lsn_offset;
	mfp->clear_len = dbmfp->clear_len;

	if (LF_ISSET(DB_DIRECT))
		F_SET(mfp, MP_DIRECT);
	if (LF_ISSET(DB_EXTENT))
		F_SET(mfp, MP_EXTENT);
	F_SET(mfp, MP_CAN_MMAP);

	if (path == NULL)
		F_SET(mfp, MP_TEMP);
	else {
		/*
		 * Don't permit files that aren't a multiple of the pagesize,
		 * and find the number of the last page in the file, all the
		 * time being careful not to overflow 32 bits.
		 *
		 * During verify or recovery, we might have to cope with a
		 * truncated file; if the file size is not a multiple of the
		 * page size, round down to a page, we'll take care of the
		 * partial page outside the mpool system.
		 */
		if (bytes % pagesize != 0) {
			if (LF_ISSET(DB_ODDFILESIZE))
				bytes -= (u_int32_t)(bytes % pagesize);
			else {
				__db_err(dbenv,
		    "%s: file size not a multiple of the pagesize", rpath);
				ret = EINVAL;
				goto err;
			}
		}

		/*
		 * If the user specifies DB_MPOOL_LAST or DB_MPOOL_NEW on a
		 * page get, we have to increment the last page in the file.
		 * Figure it out and save it away.
		 *
		 * Note correction: page numbers are zero-based, not 1-based.
		 */
		last_pgno = (db_pgno_t)(mbytes * (MEGABYTE / pagesize));
		last_pgno += (db_pgno_t)(bytes / pagesize);
		if (last_pgno != 0)
			--last_pgno;
		mfp->orig_last_pgno = mfp->last_pgno = last_pgno;

		/* Copy the file path into shared memory. */
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, strlen(path) + 1, &mfp->path_off, &p)) != 0)
			goto err;
		memcpy(p, path, strlen(path) + 1);

		/* Copy the file identification string into shared memory. */
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, DB_FILE_ID_LEN, &mfp->fileid_off, &p)) != 0)
			goto err;
		memcpy(p, dbmfp->fileid, DB_FILE_ID_LEN);
	}

	/* Copy the page cookie into shared memory. */
	if (dbmfp->pgcookie == NULL || dbmfp->pgcookie->size == 0) {
		mfp->pgcookie_len = 0;
		mfp->pgcookie_off = 0;
	} else {
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, dbmfp->pgcookie->size, &mfp->pgcookie_off, &p)) != 0)
			goto err;
		memcpy(p, dbmfp->pgcookie->data, dbmfp->pgcookie->size);
		mfp->pgcookie_len = dbmfp->pgcookie->size;
	}

	/*
	 * Prepend the MPOOLFILE to the list of MPOOLFILE's.
	 */
	R_LOCK(dbenv, dbmp->reginfo);
	ret = __db_mutex_setup(dbenv, dbmp->reginfo, &mfp->mutex,
	    MUTEX_NO_RLOCK);
	if (ret == 0)
		SH_TAILQ_INSERT_HEAD(&mp->mpfq, mfp, q, __mpoolfile);
	R_UNLOCK(dbenv, dbmp->reginfo);
	if (ret != 0)
		goto err;

check_map:
	/*
	 * If a file:
	 *	+ isn't temporary
	 *	+ is read-only
	 *	+ doesn't require any pgin/pgout support
	 *	+ the DB_NOMMAP flag wasn't set (in either the file open or
	 *	  the environment in which it was opened)
	 *	+ and is less than mp_mmapsize bytes in size
	 *
	 * we can mmap it instead of reading/writing buffers.  Don't do error
	 * checking based on the mmap call failure.  We want to do normal I/O
	 * on the file if the reason we failed was because the file was on an
	 * NFS mounted partition, and we can fail in buffer I/O just as easily
	 * as here.
	 *
	 * We'd like to test to see if the file is too big to mmap.  Since we
	 * don't know what size or type off_t's or size_t's are, or the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way is
	 * flatly impossible.  Hope that mmap fails if the file is too large.
	 */
#define	DB_MAXMMAPSIZE	(10 * 1024 * 1024)	/* 10 MB. */
	if (F_ISSET(mfp, MP_CAN_MMAP)) {
		if (path == NULL)
			F_CLR(mfp, MP_CAN_MMAP);
		if (!F_ISSET(dbmfp, MP_READONLY))
			F_CLR(mfp, MP_CAN_MMAP);
		if (dbmfp->ftype != 0)
			F_CLR(mfp, MP_CAN_MMAP);
		if (LF_ISSET(DB_NOMMAP) || F_ISSET(dbenv, DB_ENV_NOMMAP))
			F_CLR(mfp, MP_CAN_MMAP);
		maxmap = dbenv->mp_mmapsize == 0 ?
		    DB_MAXMMAPSIZE : dbenv->mp_mmapsize;
		if (mbytes > maxmap / MEGABYTE ||
		    (mbytes == maxmap / MEGABYTE && bytes >= maxmap % MEGABYTE))
			F_CLR(mfp, MP_CAN_MMAP);

		dbmfp->addr = NULL;
		if (F_ISSET(mfp, MP_CAN_MMAP)) {
			dbmfp->len = (size_t)mbytes * MEGABYTE + bytes;
			if (__os_mapfile(dbenv, rpath,
			    dbmfp->fhp, dbmfp->len, 1, &dbmfp->addr) != 0) {
				dbmfp->addr = NULL;
				F_CLR(mfp, MP_CAN_MMAP);
			}
		}
	}

	dbmfp->mfp = mfp;

	F_SET(dbmfp, MP_OPEN_CALLED);

	/* Add the file to the process' list of DB_MPOOLFILEs. */
	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	TAILQ_INSERT_TAIL(&dbmp->dbmfq, dbmfp, q);
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	if (0) {
err:		if (F_ISSET(dbmfp->fhp, DB_FH_VALID))
			(void)__os_closehandle(dbenv, dbmfp->fhp);

		if (mfp_alloc) {
			R_LOCK(dbenv, dbmp->reginfo);
			if (mfp->path_off != 0)
				__db_shalloc_free(dbmp->reginfo[0].addr,
				    R_ADDR(dbmp->reginfo, mfp->path_off));
			if (mfp->fileid_off != 0)
				__db_shalloc_free(dbmp->reginfo[0].addr,
				    R_ADDR(dbmp->reginfo, mfp->fileid_off));
			__db_shalloc_free(dbmp->reginfo[0].addr, mfp);
			R_UNLOCK(dbenv, dbmp->reginfo);
		}

	}
	if (rpath != NULL)
		__os_free(dbenv, rpath);
	return (ret);
}

/*
 * __memp_get_fileid --
 *	Return the file ID.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_get_fileid(dbmfp, fidp)
	DB_MPOOLFILE *dbmfp;
	u_int8_t *fidp;
{
	/*
	 * No lock needed -- we're using the handle, it had better not
	 * be going away.
	 *
	 * !!!
	 * Get the fileID out of the region, not out of the DB_MPOOLFILE
	 * structure because the DB_MPOOLFILE reference is possibly short
	 * lived, and isn't to be trusted.
	 */
	memcpy(fidp, R_ADDR(
	    dbmfp->dbmp->reginfo, dbmfp->mfp->fileid_off), DB_FILE_ID_LEN);
}

/*
 * __memp_last_pgno --
 *	Return the page number of the last page in the file.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_last_pgno(dbmfp, pgnoaddr)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *pgnoaddr;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;

	R_LOCK(dbenv, dbmp->reginfo);
	*pgnoaddr = dbmfp->mfp->last_pgno;
	R_UNLOCK(dbenv, dbmp->reginfo);
}

/*
 * __memp_refcnt --
 *	Return the current reference count.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_refcnt(dbmfp, cntp)
	DB_MPOOLFILE *dbmfp;
	db_pgno_t *cntp;
{
	DB_ENV *dbenv;

	dbenv = dbmfp->dbmp->dbenv;

	MUTEX_LOCK(dbenv, &dbmfp->mfp->mutex);
	*cntp = dbmfp->mfp->mpf_cnt;
	MUTEX_UNLOCK(dbenv, &dbmfp->mfp->mutex);
}

/*
 * __memp_set_unlink --
 *	Set unlink on last close flag.
 *
 * XXX
 * Undocumented interface: DB private.
 */
static void
__memp_set_unlink(dbmpf, set)
	DB_MPOOLFILE *dbmpf;
	int set;
{
	DB_ENV *dbenv;

	dbenv = dbmpf->dbmp->dbenv;

	MUTEX_LOCK(dbenv, &dbmpf->mfp->mutex);
	if (set)
		F_SET(dbmpf->mfp, MP_UNLINK);
	else
		F_CLR(dbmpf->mfp, MP_UNLINK);
	MUTEX_UNLOCK(dbenv, &dbmpf->mfp->mutex);
}

/*
 * memp_fclose --
 *	Close a backing file for the memory pool.
 */
static int
__memp_fclose(dbmfp, flags)
	DB_MPOOLFILE *dbmfp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret, t_ret;

	dbenv = dbmfp->dbmp->dbenv;

	PANIC_CHECK(dbenv);

	/*
	 * XXX
	 * DB_MPOOL_DISCARD: Undocumented flag: DB private.
	 */
	ret = __db_fchk(dbenv, "DB_MPOOLFILE->close", flags, DB_MPOOL_DISCARD);

	if ((t_ret = __memp_fclose_int(dbmfp, flags)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __memp_fclose_int --
 *	Internal version of __memp_fclose.
 *
 * PUBLIC: int __memp_fclose_int __P((DB_MPOOLFILE *, u_int32_t));
 */
int
__memp_fclose_int(dbmfp, flags)
	DB_MPOOLFILE *dbmfp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
	char *rpath;
	int deleted, ret, t_ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	ret = 0;

	/*
	 * We have to reference count DB_MPOOLFILE structures as other threads
	 * in the process may be using them.  Here's the problem:
	 *
	 * Thread A opens a database.
	 * Thread B uses thread A's DB_MPOOLFILE to write a buffer
	 *    in order to free up memory in the mpool cache.
	 * Thread A closes the database while thread B is using the
	 *    DB_MPOOLFILE structure.
	 *
	 * By opening all databases before creating any threads, and closing
	 * the databases after all the threads have exited, applications get
	 * better performance and avoid the problem path entirely.
	 *
	 * Regardless, holding the DB_MPOOLFILE to flush a dirty buffer is a
	 * short-term lock, even in worst case, since we better be the only
	 * thread of control using the DB_MPOOLFILE structure to read pages
	 * *into* the cache.  Wait until we're the only reference holder and
	 * remove the DB_MPOOLFILE structure from the list, so nobody else can
	 * find it.  We do this, rather than have the last reference holder
	 * (whoever that might be) discard the DB_MPOOLFILE structure, because
	 * we'd rather write error messages to the application in the close
	 * routine, not in the checkpoint/sync routine.
	 *
	 * !!!
	 * It's possible the DB_MPOOLFILE was never added to the DB_MPOOLFILE
	 * file list, check the DB_OPEN_CALLED flag to be sure.
	 */
	for (deleted = 0;;) {
		MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
		if (dbmfp->ref == 1) {
			if (F_ISSET(dbmfp, MP_OPEN_CALLED))
				TAILQ_REMOVE(&dbmp->dbmfq, dbmfp, q);
			deleted = 1;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

		if (deleted)
			break;
		__os_sleep(dbenv, 1, 0);
	}

	/* Complain if pinned blocks never returned. */
	if (dbmfp->pinref != 0) {
		__db_err(dbenv, "%s: close: %lu blocks left pinned",
		    __memp_fn(dbmfp), (u_long)dbmfp->pinref);
		ret = __db_panic(dbenv, DB_RUNRECOVERY);
	}

	/* Discard any mmap information. */
	if (dbmfp->addr != NULL &&
	    (ret = __os_unmapfile(dbenv, dbmfp->addr, dbmfp->len)) != 0)
		__db_err(dbenv, "%s: %s", __memp_fn(dbmfp), db_strerror(ret));

	/* Close the file; temporary files may not yet have been created. */
	if (F_ISSET(dbmfp->fhp, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(dbenv, dbmfp->fhp)) != 0) {
		__db_err(dbenv, "%s: %s", __memp_fn(dbmfp), db_strerror(t_ret));
		if (ret == 0)
			ret = t_ret;
	}

	/* Discard the thread mutex. */
	if (dbmfp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmfp->mutexp);

	/*
	 * Discard our reference on the the underlying MPOOLFILE, and close
	 * it if it's no longer useful to anyone.  It possible the open of
	 * the file never happened or wasn't successful, in which case, mpf
	 * will be NULL;
	 */
	if ((mfp = dbmfp->mfp) == NULL)
		goto done;

	/*
	 * If it's a temp file, all outstanding references belong to unflushed
	 * buffers.  (A temp file can only be referenced by one DB_MPOOLFILE).
	 * We don't care about preserving any of those buffers, so mark the
	 * MPOOLFILE as dead so that even the dirty ones just get discarded
	 * when we try to flush them.
	 */
	deleted = 0;
	MUTEX_LOCK(dbenv, &mfp->mutex);
	if (--mfp->mpf_cnt == 0 || LF_ISSET(DB_MPOOL_DISCARD)) {
		if (LF_ISSET(DB_MPOOL_DISCARD) ||
		    F_ISSET(mfp, MP_TEMP | MP_UNLINK))
			MPOOLFILE_IGNORE(mfp);
		if (F_ISSET(mfp, MP_UNLINK)) {
			if ((t_ret = __db_appname(dbmp->dbenv,
			    DB_APP_DATA, R_ADDR(dbmp->reginfo,
			    mfp->path_off), 0, NULL, &rpath)) != 0 && ret == 0)
				ret = t_ret;
			if (t_ret == 0) {
				if ((t_ret = __os_unlink(
				    dbmp->dbenv, rpath) != 0) && ret == 0)
					ret = t_ret;
				__os_free(dbenv, rpath);
			}
		}
		if (mfp->block_cnt == 0) {
			if ((t_ret =
			    __memp_mf_discard(dbmp, mfp)) != 0 && ret == 0)
				ret = t_ret;
			deleted = 1;
		}
	}
	if (deleted == 0)
		MUTEX_UNLOCK(dbenv, &mfp->mutex);

	/* Discard the DB_MPOOLFILE structure. */
done:	__os_free(dbenv, dbmfp->fhp);
	__os_free(dbenv, dbmfp);

	return (ret);
}

/*
 * __memp_mf_discard --
 *	Discard an MPOOLFILE.
 *
 * PUBLIC: int __memp_mf_discard __P((DB_MPOOL *, MPOOLFILE *));
 */
int
__memp_mf_discard(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	DB_ENV *dbenv;
	DB_FH fh;
	DB_MPOOL_STAT *sp;
	MPOOL *mp;
	char *rpath;
	int ret;

	dbenv = dbmp->dbenv;
	mp = dbmp->reginfo[0].primary;
	ret = 0;

	/*
	 * Expects caller to be holding the MPOOLFILE mutex.
	 *
	 * When discarding a file, we have to flush writes from it to disk.
	 * The scenario is that dirty buffers from this file need to be
	 * flushed to satisfy a future checkpoint, but when the checkpoint
	 * calls mpool sync, the sync code won't know anything about them.
	 */
	if (!F_ISSET(mfp, MP_DEADFILE) &&
	    (ret = __db_appname(dbenv, DB_APP_DATA,
	    R_ADDR(dbmp->reginfo, mfp->path_off), 0, NULL, &rpath)) == 0) {
		if ((ret = __os_open(dbenv, rpath, 0, 0, &fh)) == 0) {
			ret = __os_fsync(dbenv, &fh);
			(void)__os_closehandle(dbenv, &fh);
		}
		__os_free(dbenv, rpath);
	}

	/*
	 * We have to release the MPOOLFILE lock before acquiring the region
	 * lock so that we don't deadlock.  Make sure nobody ever looks at
	 * this structure again.
	 */
	MPOOLFILE_IGNORE(mfp);

	/* Discard the mutex we're holding. */
	MUTEX_UNLOCK(dbenv, &mfp->mutex);

	/* Delete from the list of MPOOLFILEs. */
	R_LOCK(dbenv, dbmp->reginfo);
	SH_TAILQ_REMOVE(&mp->mpfq, mfp, q, __mpoolfile);

	/* Copy the statistics into the region. */
	sp = &mp->stat;
	sp->st_cache_hit += mfp->stat.st_cache_hit;
	sp->st_cache_miss += mfp->stat.st_cache_miss;
	sp->st_map += mfp->stat.st_map;
	sp->st_page_create += mfp->stat.st_page_create;
	sp->st_page_in += mfp->stat.st_page_in;
	sp->st_page_out += mfp->stat.st_page_out;

	/* Clear the mutex this MPOOLFILE recorded. */
	__db_shlocks_clear(&mfp->mutex, dbmp->reginfo,
	    (REGMAINT *)R_ADDR(dbmp->reginfo, mp->maint_off));

	/* Free the space. */
	if (mfp->path_off != 0)
		__db_shalloc_free(dbmp->reginfo[0].addr,
		    R_ADDR(dbmp->reginfo, mfp->path_off));
	if (mfp->fileid_off != 0)
		__db_shalloc_free(dbmp->reginfo[0].addr,
		    R_ADDR(dbmp->reginfo, mfp->fileid_off));
	if (mfp->pgcookie_off != 0)
		__db_shalloc_free(dbmp->reginfo[0].addr,
		    R_ADDR(dbmp->reginfo, mfp->pgcookie_off));
	__db_shalloc_free(dbmp->reginfo[0].addr, mfp);

	R_UNLOCK(dbenv, dbmp->reginfo);

	return (ret);
}

/*
 * __memp_fn --
 *	On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * __memp_fn __P((DB_MPOOLFILE *));
 */
char *
__memp_fn(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	return (__memp_fns(dbmfp->dbmp, dbmfp->mfp));
}

/*
 * __memp_fns --
 *	On errors we print whatever is available as the file name.
 *
 * PUBLIC: char * __memp_fns __P((DB_MPOOL *, MPOOLFILE *));
 *
 */
char *
__memp_fns(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	if (mfp->path_off == 0)
		return ((char *)"temporary");

	return ((char *)R_ADDR(dbmp->reginfo, mfp->path_off));
}
