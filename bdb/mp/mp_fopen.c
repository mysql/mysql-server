/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_fopen.c,v 11.41 2001/01/10 04:50:53 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int __memp_mf_open __P((DB_MPOOL *, const char *,
    size_t, db_pgno_t, DB_MPOOL_FINFO *, u_int32_t, MPOOLFILE **));

/*
 * MEMP_FREMOVE --
 *	Discard an MPOOLFILE and any buffers it references: update the flags
 *	so we never try to write buffers associated with the file, nor can we
 *	find it when looking for files to join.  In addition, clear the ftype
 *	field, there's no reason to post-process pages, they can be discarded
 *	by any thread.
 */
#define	MEMP_FREMOVE(mfp) {						\
	mfp->ftype = 0;							\
	F_SET(mfp, MP_DEADFILE);					\
}

/*
 * memp_fopen --
 *	Open a backing file for the memory pool.
 */
int
memp_fopen(dbenv, path, flags, mode, pagesize, finfop, retp)
	DB_ENV *dbenv;
	const char *path;
	u_int32_t flags;
	int mode;
	size_t pagesize;
	DB_MPOOL_FINFO *finfop;
	DB_MPOOLFILE **retp;
{
	DB_MPOOL *dbmp;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_fopen(dbenv, path, flags,
		    mode, pagesize, finfop, retp));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->mp_handle, DB_INIT_MPOOL);

	dbmp = dbenv->mp_handle;

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "memp_fopen", flags,
	    DB_CREATE |
	    DB_NOMMAP | DB_ODDFILESIZE | DB_RDONLY | DB_TRUNCATE)) != 0)
		return (ret);

	/* Require a non-zero pagesize. */
	if (pagesize == 0 ||
	    (finfop != NULL && finfop->clear_len > pagesize)) {
		__db_err(dbenv, "memp_fopen: illegal page size.");
		return (EINVAL);
	}

	return (__memp_fopen(dbmp,
	    NULL, path, flags, mode, pagesize, 1, finfop, retp));
}

/*
 * __memp_set_unlink -- set unlink on last close flag.
 *
 * PUBLIC: void __memp_set_unlink __P((DB_MPOOLFILE *));
 */
void
__memp_set_unlink(dbmpf)
	DB_MPOOLFILE *dbmpf;
{
	DB_MPOOL *dbmp;
	dbmp = dbmpf->dbmp;

	R_LOCK(dbmp->dbenv, dbmp->reginfo);
	F_SET(dbmpf->mfp, MP_UNLINK);
	R_UNLOCK(dbmp->dbenv, dbmp->reginfo);
}

/*
 * __memp_clear_unlink -- clear unlink on last close flag.
 *
 * PUBLIC: void __memp_clear_unlink __P((DB_MPOOLFILE *));
 */
void
__memp_clear_unlink(dbmpf)
	DB_MPOOLFILE *dbmpf;
{
	DB_MPOOL *dbmp;
	dbmp = dbmpf->dbmp;

	/*
	 * This bit is protected in the queue code because the metapage
	 * is locked so we can avoid geting the region lock.
	 * If this gets used from other than the queue code, we cannot.
	 */
	if (!F_ISSET(dbmpf->mfp, MP_UNLINK))
		return;
	R_LOCK(dbmp->dbenv, dbmp->reginfo);
	F_CLR(dbmpf->mfp, MP_UNLINK);
	R_UNLOCK(dbmp->dbenv, dbmp->reginfo);
}

/*
 * __memp_fopen --
 *	Open a backing file for the memory pool; internal version.
 *
 * PUBLIC: int __memp_fopen __P((DB_MPOOL *, MPOOLFILE *, const char *,
 * PUBLIC:    u_int32_t, int, size_t, int, DB_MPOOL_FINFO *, DB_MPOOLFILE **));
 */
int
__memp_fopen(dbmp, mfp, path, flags, mode, pagesize, needlock, finfop, retp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
	const char *path;
	u_int32_t flags;
	int mode, needlock;
	size_t pagesize;
	DB_MPOOL_FINFO *finfop;
	DB_MPOOLFILE **retp;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *dbmfp;
	DB_MPOOL_FINFO finfo;
	db_pgno_t last_pgno;
	size_t maxmap;
	u_int32_t mbytes, bytes, oflags;
	int ret;
	u_int8_t idbuf[DB_FILE_ID_LEN];
	char *rpath;

	dbenv = dbmp->dbenv;
	ret = 0;
	rpath = NULL;

	/*
	 * If mfp is provided, we take the DB_MPOOL_FINFO information from
	 * the mfp.  We don't bother initializing everything, because some
	 * of them are expensive to acquire.  If no mfp is provided and the
	 * finfop argument is NULL, we default the values.
	 */
	if (finfop == NULL) {
		memset(&finfo, 0, sizeof(finfo));
		if (mfp != NULL) {
			finfo.ftype = mfp->ftype;
			finfo.pgcookie = NULL;
			finfo.fileid = NULL;
			finfo.lsn_offset = mfp->lsn_off;
			finfo.clear_len = mfp->clear_len;
		} else {
			finfo.ftype = 0;
			finfo.pgcookie = NULL;
			finfo.fileid = NULL;
			finfo.lsn_offset = -1;
			finfo.clear_len = 0;
		}
		finfop = &finfo;
	}

	/* Allocate and initialize the per-process structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_MPOOLFILE), &dbmfp)) != 0)
		return (ret);
	dbmfp->dbmp = dbmp;
	dbmfp->ref = 1;
	if (LF_ISSET(DB_RDONLY))
		F_SET(dbmfp, MP_READONLY);

	if (path == NULL) {
		if (LF_ISSET(DB_RDONLY)) {
			__db_err(dbenv,
			    "memp_fopen: temporary files can't be readonly");
			ret = EINVAL;
			goto err;
		}
		last_pgno = 0;
	} else {
		/* Get the real name for this file and open it. */
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, path, 0, NULL, &rpath)) != 0)
			goto err;
		oflags = 0;
		if (LF_ISSET(DB_CREATE))
			oflags |= DB_OSO_CREATE;
		if (LF_ISSET(DB_RDONLY))
			oflags |= DB_OSO_RDONLY;
		if ((ret =
		   __os_open(dbenv, rpath, oflags, mode, &dbmfp->fh)) != 0) {
			if (!LF_ISSET(DB_EXTENT))
				__db_err(dbenv,
				    "%s: %s", rpath, db_strerror(ret));
			goto err;
		}

		/*
		 * Don't permit files that aren't a multiple of the pagesize,
		 * and find the number of the last page in the file, all the
		 * time being careful not to overflow 32 bits.
		 *
		 * !!!
		 * We can't use off_t's here, or in any code in the mainline
		 * library for that matter.  (We have to use them in the os
		 * stubs, of course, as there are system calls that take them
		 * as arguments.)  The reason is that some customers build in
		 * environments where an off_t is 32-bits, but still run where
		 * offsets are 64-bits, and they pay us a lot of money.
		 */
		if ((ret = __os_ioinfo(dbenv, rpath,
		    &dbmfp->fh, &mbytes, &bytes, NULL)) != 0) {
			__db_err(dbenv, "%s: %s", rpath, db_strerror(ret));
			goto err;
		}

		/*
		 * If we're doing a verify, we might have to cope with
		 * a truncated file;  if the file size is not a multiple
		 * of the page size, round down to a page--we'll
		 * take care of the partial page outside the memp system.
		 */

		/* Page sizes have to be a power-of-two, ignore mbytes. */
		if (bytes % pagesize != 0) {
			if (LF_ISSET(DB_ODDFILESIZE))
				/*
				 * If we're doing a verify, we might
				 * have to cope with a truncated file;
				 * round down, we'll worry about the partial
				 * page outside the memp system.
				 */
				bytes -= (bytes % pagesize);
			else {
				__db_err(dbenv,
		"%s: file size not a multiple of the pagesize",
				    rpath);
				ret = EINVAL;
				goto err;
			}
		}

		last_pgno = mbytes * (MEGABYTE / pagesize);
		last_pgno += bytes / pagesize;

		/* Correction: page numbers are zero-based, not 1-based. */
		if (last_pgno != 0)
			--last_pgno;

		/*
		 * Get the file id if we weren't given one.  Generated file id's
		 * don't use timestamps, otherwise there'd be no chance of any
		 * other process joining the party.
		 */
		if (finfop->fileid == NULL) {
			if ((ret = __os_fileid(dbenv, rpath, 0, idbuf)) != 0)
				goto err;
			finfop->fileid = idbuf;
		}
	}

	/*
	 * If we weren't provided an underlying shared object to join with,
	 * find/allocate the shared file objects.  Also allocate space for
	 * for the per-process thread lock.
	 */
	if (needlock)
		R_LOCK(dbenv, dbmp->reginfo);
	if (mfp == NULL)
		ret = __memp_mf_open(
		    dbmp, path, pagesize, last_pgno, finfop, flags, &mfp);
	else {
		++mfp->mpf_cnt;
		ret = 0;
	}
	if (needlock)
		R_UNLOCK(dbenv, dbmp->reginfo);
	if (ret != 0)
		goto err;

	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, dbmp->reginfo, &dbmfp->mutexp)) != 0)
			goto err;
		if ((ret = __db_mutex_init(
		    dbenv, dbmfp->mutexp, 0, MUTEX_THREAD)) != 0)
			goto err;

		/* XXX: KEITH: CLOSE THE FILE ON FAILURE? */
	}

	dbmfp->mfp = mfp;

	/*
	 * If a file:
	 *	+ is read-only
	 *	+ isn't temporary
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
	 * XXX
	 * We'd like to test to see if the file is too big to mmap.  Since we
	 * don't know what size or type off_t's or size_t's are, or the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way is
	 * flatly impossible.  Hope that mmap fails if the file is too large.
	 */
#define	DB_MAXMMAPSIZE	(10 * 1024 * 1024)	/* 10 Mb. */
	if (F_ISSET(mfp, MP_CAN_MMAP)) {
		if (!F_ISSET(dbmfp, MP_READONLY))
			F_CLR(mfp, MP_CAN_MMAP);
		if (path == NULL)
			F_CLR(mfp, MP_CAN_MMAP);
		if (finfop->ftype != 0)
			F_CLR(mfp, MP_CAN_MMAP);
		if (LF_ISSET(DB_NOMMAP) || F_ISSET(dbenv, DB_ENV_NOMMAP))
			F_CLR(mfp, MP_CAN_MMAP);
		maxmap = dbenv->mp_mmapsize == 0 ?
		    DB_MAXMMAPSIZE : dbenv->mp_mmapsize;
		if (mbytes > maxmap / MEGABYTE ||
		    (mbytes == maxmap / MEGABYTE && bytes >= maxmap % MEGABYTE))
			F_CLR(mfp, MP_CAN_MMAP);
	}
	dbmfp->addr = NULL;
	if (F_ISSET(mfp, MP_CAN_MMAP)) {
		dbmfp->len = (size_t)mbytes * MEGABYTE + bytes;
		if (__os_mapfile(dbenv, rpath,
		    &dbmfp->fh, dbmfp->len, 1, &dbmfp->addr) != 0) {
			dbmfp->addr = NULL;
			F_CLR(mfp, MP_CAN_MMAP);
		}
	}
	if (rpath != NULL)
		__os_freestr(rpath);

	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	TAILQ_INSERT_TAIL(&dbmp->dbmfq, dbmfp, q);
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	*retp = dbmfp;
	return (0);

err:	/*
	 * Note that we do not have to free the thread mutex, because we
	 * never get to here after we have successfully allocated it.
	 */
	if (rpath != NULL)
		__os_freestr(rpath);
	if (F_ISSET(&dbmfp->fh, DB_FH_VALID))
		(void)__os_closehandle(&dbmfp->fh);
	if (dbmfp != NULL) {
		if (dbmfp->mutexp != NULL)
			__db_mutex_free(dbenv, dbmp->reginfo, dbmfp->mutexp);
		__os_free(dbmfp, sizeof(DB_MPOOLFILE));
	}
	return (ret);
}

/*
 * __memp_mf_open --
 *	Open an MPOOLFILE.
 */
static int
__memp_mf_open(dbmp, path, pagesize, last_pgno, finfop, flags, retp)
	DB_MPOOL *dbmp;
	const char *path;
	size_t pagesize;
	db_pgno_t last_pgno;
	DB_MPOOL_FINFO *finfop;
	u_int32_t flags;
	MPOOLFILE **retp;
{
	MPOOL *mp;
	MPOOLFILE *mfp;
	int ret;
	void *p;

#define	ISTEMPORARY	(path == NULL)

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
	if (!ISTEMPORARY) {
		mp = dbmp->reginfo[0].primary;
		for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
		    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
			if (F_ISSET(mfp, MP_DEADFILE | MP_TEMP))
				continue;
			if (memcmp(finfop->fileid, R_ADDR(dbmp->reginfo,
			    mfp->fileid_off), DB_FILE_ID_LEN) == 0) {
				if (LF_ISSET(DB_TRUNCATE)) {
					MEMP_FREMOVE(mfp);
					continue;
				}
				if (finfop->clear_len != mfp->clear_len ||
				    pagesize != mfp->stat.st_pagesize) {
					__db_err(dbmp->dbenv,
				    "%s: page size or clear length changed",
					    path);
					return (EINVAL);
				}

				/*
				 * It's possible that our needs for pre- and
				 * post-processing are changing.  For example,
				 * an application created a hash subdatabase
				 * in a database that was previously all btree.
				 */
				if (finfop->ftype != 0)
					mfp->ftype = finfop->ftype;

				++mfp->mpf_cnt;

				*retp = mfp;
				return (0);
			}
		}
	}

	/* Allocate a new MPOOLFILE. */
	if ((ret = __memp_alloc(
	    dbmp, dbmp->reginfo, NULL, sizeof(MPOOLFILE), NULL, &mfp)) != 0)
		goto mem_err;
	*retp = mfp;

	/* Initialize the structure. */
	memset(mfp, 0, sizeof(MPOOLFILE));
	mfp->mpf_cnt = 1;
	mfp->ftype = finfop->ftype;
	mfp->lsn_off = finfop->lsn_offset;
	mfp->clear_len = finfop->clear_len;

	/*
	 * If the user specifies DB_MPOOL_LAST or DB_MPOOL_NEW on a memp_fget,
	 * we have to know the last page in the file.  Figure it out and save
	 * it away.
	 */
	mfp->stat.st_pagesize = pagesize;
	mfp->orig_last_pgno = mfp->last_pgno = last_pgno;

	if (ISTEMPORARY)
		F_SET(mfp, MP_TEMP);
	else {
		/* Copy the file path into shared memory. */
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, strlen(path) + 1, &mfp->path_off, &p)) != 0)
			goto err;
		memcpy(p, path, strlen(path) + 1);

		/* Copy the file identification string into shared memory. */
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, DB_FILE_ID_LEN, &mfp->fileid_off, &p)) != 0)
			goto err;
		memcpy(p, finfop->fileid, DB_FILE_ID_LEN);

		F_SET(mfp, MP_CAN_MMAP);
	}

	/* Copy the page cookie into shared memory. */
	if (finfop->pgcookie == NULL || finfop->pgcookie->size == 0) {
		mfp->pgcookie_len = 0;
		mfp->pgcookie_off = 0;
	} else {
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, finfop->pgcookie->size, &mfp->pgcookie_off, &p)) != 0)
			goto err;
		memcpy(p, finfop->pgcookie->data, finfop->pgcookie->size);
		mfp->pgcookie_len = finfop->pgcookie->size;
	}

	/* Prepend the MPOOLFILE to the list of MPOOLFILE's. */
	mp = dbmp->reginfo[0].primary;
	SH_TAILQ_INSERT_HEAD(&mp->mpfq, mfp, q, __mpoolfile);

	if (0) {
err:		if (mfp->path_off != 0)
			__db_shalloc_free(dbmp->reginfo[0].addr,
			    R_ADDR(dbmp->reginfo, mfp->path_off));
		if (mfp->fileid_off != 0)
			__db_shalloc_free(dbmp->reginfo[0].addr,
			    R_ADDR(dbmp->reginfo, mfp->fileid_off));
		if (mfp != NULL)
			__db_shalloc_free(dbmp->reginfo[0].addr, mfp);
mem_err:	__db_err(dbmp->dbenv,
		    "Unable to allocate memory for mpool file");
	}
	return (ret);
}

/*
 * memp_fclose --
 *	Close a backing file for the memory pool.
 */
int
memp_fclose(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
	char *rpath;
	int ret, t_ret;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	ret = 0;

	PANIC_CHECK(dbenv);

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_memp_fclose(dbmfp));
#endif

	/*
	 * Remove the DB_MPOOLFILE from the queue.  This has to happen before
	 * we perform any action that can fail, otherwise __memp_close may
	 * loop infinitely when calling us to discard all of the DB_MPOOLFILEs.
	 */
	for (;;) {
		MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);

		/*
		 * We have to reference count DB_MPOOLFILE structures as other
		 * threads may be using them.  The problem only happens if the
		 * application makes a bad design choice.  Here's the path:
		 *
		 * Thread A opens a database.
		 * Thread B uses thread A's DB_MPOOLFILE to write a buffer
		 *    in order to free up memory in the mpool cache.
		 * Thread A closes the database while thread B is using the
		 *    DB_MPOOLFILE structure.
		 *
		 * By opening all databases before creating the threads, and
		 * closing them after the threads have exited, applications
		 * get better performance and avoid the problem path entirely.
		 *
		 * Regardless, holding the DB_MPOOLFILE to flush a dirty buffer
		 * is a short-term lock, even in worst case, since we better be
		 * the only thread of control using the DB_MPOOLFILE structure
		 * to read pages *into* the cache.  Wait until we're the only
		 * reference holder and remove the DB_MPOOLFILE structure from
		 * the list, so nobody else can even find it.
		 */
		if (dbmfp->ref == 1) {
			TAILQ_REMOVE(&dbmp->dbmfq, dbmfp, q);
			break;
		}
		MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

		(void)__os_sleep(dbenv, 1, 0);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);

	/* Complain if pinned blocks never returned. */
	if (dbmfp->pinref != 0)
		__db_err(dbenv, "%s: close: %lu blocks left pinned",
		    __memp_fn(dbmfp), (u_long)dbmfp->pinref);

	/* Discard any mmap information. */
	if (dbmfp->addr != NULL &&
	    (ret = __os_unmapfile(dbenv, dbmfp->addr, dbmfp->len)) != 0)
		__db_err(dbenv, "%s: %s", __memp_fn(dbmfp), db_strerror(ret));

	/* Close the file; temporary files may not yet have been created. */
	if (F_ISSET(&dbmfp->fh, DB_FH_VALID) &&
	    (t_ret = __os_closehandle(&dbmfp->fh)) != 0) {
		__db_err(dbenv, "%s: %s", __memp_fn(dbmfp), db_strerror(t_ret));
		if (ret != 0)
			t_ret = ret;
	}

	/* Discard the thread mutex. */
	if (dbmfp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmfp->mutexp);

	/*
	 * Discard our reference on the the underlying MPOOLFILE, and close
	 * it if it's no longer useful to anyone.
	 *
	 * If we're not discarding it, and it's a temp file, this means
	 * all the outstanding references belong to unflushed buffers.
	 * (A temp file can only be referenced by one DB_MPOOLFILE).
	 * We don't care about preserving any of those buffers, so mark
	 * the MPOOLFILE as dead so that when we try to flush them,
	 * even the dirty ones just get discarded.
	 */
	R_LOCK(dbenv, dbmp->reginfo);
	mfp = dbmfp->mfp;
	if (--mfp->mpf_cnt == 0) {
		if (F_ISSET(mfp, MP_UNLINK)) {
			MEMP_FREMOVE(mfp);
			if ((t_ret = __db_appname(dbmp->dbenv,
			    DB_APP_DATA, NULL, R_ADDR(dbmp->reginfo,
			    mfp->path_off), 0, NULL, &rpath)) != 0 && ret == 0)
				ret = t_ret;
			if (t_ret == 0 && (t_ret =
			    __os_unlink(dbmp->dbenv, rpath) != 0 && ret == 0))
				ret = t_ret;
			__os_free(rpath, 0);
		}
		if (mfp->block_cnt == 0)
			__memp_mf_discard(dbmp, mfp);
	}
	else if (F_ISSET(mfp, MP_TEMP))
		MEMP_FREMOVE(mfp);
	R_UNLOCK(dbenv, dbmp->reginfo);

	/* Discard the DB_MPOOLFILE structure. */
	__os_free(dbmfp, sizeof(DB_MPOOLFILE));

	return (ret);
}

/*
 * __memp_mf_discard --
 *	Discard an MPOOLFILE.
 *
 * PUBLIC: void __memp_mf_discard __P((DB_MPOOL *, MPOOLFILE *));
 */
void
__memp_mf_discard(dbmp, mfp)
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;
{
	MPOOL *mp;

	mp = dbmp->reginfo[0].primary;

	/* Delete from the list of MPOOLFILEs. */
	SH_TAILQ_REMOVE(&mp->mpfq, mfp, q, __mpoolfile);

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
}

/*
 * __memp_fremove --
 *	Remove an underlying file from the system.
 *
 * PUBLIC: int __memp_fremove __P((DB_MPOOLFILE *));
 */
int
__memp_fremove(dbmfp)
	DB_MPOOLFILE *dbmfp;
{
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	MPOOLFILE *mfp;

	dbmp = dbmfp->dbmp;
	dbenv = dbmp->dbenv;
	mfp = dbmfp->mfp;

	PANIC_CHECK(dbenv);

	R_LOCK(dbenv, dbmp->reginfo);

	MEMP_FREMOVE(mfp);

	R_UNLOCK(dbenv, dbmp->reginfo);

	return (0);
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
