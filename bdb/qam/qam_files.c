/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_files.c,v 1.16 2001/01/19 18:01:59 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "db_am.h"
#include "lock.h"
#include "btree.h"
#include "qam.h"
#include "mp.h"

/*
 * __qam_fprobe -- calcluate and open extent
 *
 * Calculate which extent the page is in, open and create
 * if necessary.
 *
 * PUBLIC: int __qam_fprobe __P((DB *, db_pgno_t, void *, qam_probe_mode, int));
 */

int
__qam_fprobe(dbp, pgno, addrp, mode, flags)
	DB *dbp;
	db_pgno_t pgno;
	void *addrp;
	qam_probe_mode mode;
	int flags;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	MPFARRAY *array;
	QUEUE *qp;
	u_int32_t extid, maxext;
	char buf[256];
	int numext, offset, oldext, openflags, ret;

	qp = (QUEUE *)dbp->q_internal;
	if (qp->page_ext == 0) {
		mpf = dbp->mpf;
		if (mode == QAM_PROBE_GET)
			return (memp_fget(mpf, &pgno, flags, addrp));
		return (memp_fput(mpf, addrp, flags));
	}

	dbenv = dbp->dbenv;
	mpf = NULL;
	ret = 0;

	/*
	 * Need to lock long enough to find the mpf or create the file.
	 * The file cannot go away because we must have a record locked
	 * in that file.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	extid = (pgno - 1) / qp->page_ext;

	/* Array1 will always be in use if array2 is in use. */
	array = &qp->array1;
	if (array->n_extent == 0) {
		/* Start with 4 extents */
		oldext = 0;
		array->n_extent = 4;
		array->low_extent = extid;
		offset = 0;
		numext = 0;
		goto alloc;
	}

	offset = extid - qp->array1.low_extent;
	if (qp->array2.n_extent != 0 &&
	    abs(offset) > abs(extid - qp->array2.low_extent)) {
		array = &qp->array2;
		offset = extid - array->low_extent;
	}

	/*
	 * Check to see if the requested extent is outside the range of
	 * extents in the array.  This is true by defualt if there are
	 * no extents here yet.
	 */
	if (offset < 0 || (unsigned) offset >= array->n_extent) {
		oldext = array->n_extent;
		numext =  array->hi_extent - array->low_extent  + 1;
		if (offset < 0
		    && (unsigned) -offset + numext <= array->n_extent) {
			/* If we can fit this one in, move the array up */
			memmove(&array->mpfarray[-offset],
			    array->mpfarray, numext
			    * sizeof(array->mpfarray[0]));
			memset(array->mpfarray, 0, -offset
			     * sizeof(array->mpfarray[0]));
			offset = 0;
		} else if ((u_int32_t)offset == array->n_extent &&
		    mode != QAM_PROBE_MPF && array->mpfarray[0].pinref == 0) {
			/* We can close the bottom extent. */
			mpf = array->mpfarray[0].mpf;
			if (mpf != NULL && (ret = memp_fclose(mpf)) != 0)
				goto err;
			memmove(&array->mpfarray[0], &array->mpfarray[1],
			    (array->n_extent - 1) * sizeof (array->mpfarray[0]));
			array->low_extent++;
			array->hi_extent++;
			offset--;
			array->mpfarray[offset].mpf = NULL;
			array->mpfarray[offset].pinref = 0;
		} else {
			/* See if we have wrapped around the queue. */
			maxext = (u_int32_t) UINT32_T_MAX
			    / (qp->page_ext * qp->rec_page);
			if ((u_int32_t) abs(offset) >= maxext/2) {
				array = &qp->array2;
				DB_ASSERT(array->n_extent == 0);
				oldext = 0;
				array->n_extent = 4;
				array->low_extent = extid;
				offset = 0;
				numext = 0;
			} else {
				/*
				 * Increase the size to at least include
				 * the new one and double it.
				 */
				array->n_extent += abs(offset);
				array->n_extent <<= 2;
			}
	alloc:
			if ((ret = __os_realloc(dbenv,
			    array->n_extent * sizeof(struct __qmpf),
			    NULL, &array->mpfarray)) != 0)
				goto err;

			if (offset < 0) {
				offset = -offset;
				memmove(&array->mpfarray[offset], array->mpfarray,
				    numext * sizeof(array->mpfarray[0]));
				memset(array->mpfarray, 0,
				    offset * sizeof(array->mpfarray[0]));
				memset(&array->mpfarray[numext + offset], 0,
				     (array->n_extent - (numext + offset))
				     * sizeof(array->mpfarray[0]));
				offset = 0;
			}
			else
				memset(&array->mpfarray[oldext], 0,
				    (array->n_extent - oldext) *
				    sizeof(array->mpfarray[0]));
		}
	}

	if (extid < array->low_extent)
		array->low_extent = extid;
	if (extid > array->hi_extent)
		array->hi_extent = extid;
	if (array->mpfarray[offset].mpf == NULL) {
		snprintf(buf,
		    sizeof(buf), QUEUE_EXTENT, qp->dir, qp->name, extid);
		openflags = DB_EXTENT;
		if (LF_ISSET(DB_MPOOL_CREATE))
			openflags |= DB_CREATE;
		if (F_ISSET(dbp, DB_AM_RDONLY))
			openflags |= DB_RDONLY;
		qp->finfo.fileid = NULL;
		if ((ret = __memp_fopen(dbenv->mp_handle,
		    NULL, buf, openflags, qp->mode, dbp->pgsize,
		    1, &qp->finfo, &array->mpfarray[offset].mpf)) != 0)
			goto err;
	}

	mpf = array->mpfarray[offset].mpf;
	if (mode == QAM_PROBE_GET)
		array->mpfarray[offset].pinref++;
	if (LF_ISSET(DB_MPOOL_CREATE))
		__memp_clear_unlink(mpf);

err:
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	if (ret == 0) {
		if (mode == QAM_PROBE_MPF) {
			*(DB_MPOOLFILE **)addrp = mpf;
			return (0);
		}
		pgno--;
		pgno %= qp->page_ext;
		if (mode == QAM_PROBE_GET)
			return (memp_fget(mpf,
			    &pgno, flags | DB_MPOOL_EXTENT, addrp));
		ret = memp_fput(mpf, addrp, flags);
		MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
		array->mpfarray[offset].pinref--;
		MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	}
	return (ret);
}

/*
 * __qam_fclose -- close an extent.
 *
 * Calculate which extent the page is in and close it.
 * We assume the mpf entry is present.
 *
 * PUBLIC: int __qam_fclose __P((DB *, db_pgno_t));
 */

int
__qam_fclose(dbp, pgnoaddr)
	DB *dbp;
	db_pgno_t pgnoaddr;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	MPFARRAY *array;
	QUEUE *qp;
	u_int32_t extid;
	int offset, ret;

	ret = 0;
	dbenv = dbp->dbenv;
	qp = (QUEUE *)dbp->q_internal;

	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);

	extid = (pgnoaddr - 1) / qp->page_ext;
	array = &qp->array1;
	if (array->low_extent > extid || array->hi_extent < extid)
		array = &qp->array2;
	offset = extid - array->low_extent;

	DB_ASSERT(offset >= 0 && (unsigned) offset < array->n_extent);

	/* If other threads are still using this file, leave it. */
	if (array->mpfarray[offset].pinref != 0)
		goto done;

	mpf = array->mpfarray[offset].mpf;
	array->mpfarray[offset].mpf = NULL;
	ret = memp_fclose(mpf);

done:
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	return (ret);
}
/*
 * __qam_fremove -- remove an extent.
 *
 * Calculate which extent the page is in and remove it.  There is no way
 * to remove an extent without probing it first and seeing that is is empty
 * so we assume the mpf entry is present.
 *
 * PUBLIC: int __qam_fremove __P((DB *, db_pgno_t));
 */

int
__qam_fremove(dbp, pgnoaddr)
	DB *dbp;
	db_pgno_t pgnoaddr;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	MPFARRAY *array;
	QUEUE *qp;
	u_int32_t extid;
#if CONFIG_TEST
	char buf[256], *real_name;
#endif
	int offset, ret;

	qp = (QUEUE *)dbp->q_internal;
	dbenv = dbp->dbenv;
	ret = 0;

	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);

	extid = (pgnoaddr - 1) / qp->page_ext;
	array = &qp->array1;
	if (array->low_extent > extid || array->hi_extent < extid)
		array = &qp->array2;
	offset = extid - array->low_extent;

	DB_ASSERT(offset >= 0 && (unsigned) offset < array->n_extent);

#if CONFIG_TEST
	real_name = NULL;
	/* Find the real name of the file. */
	snprintf(buf, sizeof(buf),
	    QUEUE_EXTENT, qp->dir, qp->name, extid);
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, buf, 0, NULL, &real_name)) != 0)
		goto err;
#endif
	mpf = array->mpfarray[offset].mpf;
	array->mpfarray[offset].mpf = NULL;
	__memp_set_unlink(mpf);
	if ((ret = memp_fclose(mpf)) != 0)
		goto err;

	if (offset == 0) {
		memmove(array->mpfarray, &array->mpfarray[1],
		    (array->hi_extent - array->low_extent)
		    * sizeof(array->mpfarray[0]));
		array->mpfarray[array->hi_extent - array->low_extent].mpf = NULL;
		if (array->low_extent != array->hi_extent)
			array->low_extent++;
	} else {
		if (extid == array->hi_extent)
			array->hi_extent--;
	}

err:
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
#if CONFIG_TEST
	if (real_name != NULL)
		__os_freestr(real_name);
#endif
	return (ret);
}

/*
 * __qam_sync --
 *	Flush the database cache.
 *
 * PUBLIC: int __qam_sync __P((DB *, u_int32_t));
 */
int
__qam_sync(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	MPFARRAY *array;
	QUEUE *qp;
	QUEUE_FILELIST *filelist;
	struct __qmpf *mpfp;
	u_int32_t i;
	int done, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->sync");

	if ((ret = __db_syncchk(dbp, flags)) != 0)
		return (ret);

	/* Read-only trees never need to be sync'd. */
	if (F_ISSET(dbp, DB_AM_RDONLY))
		return (0);

	/* If the tree was never backed by a database file, we're done. */
	if (F_ISSET(dbp, DB_AM_INMEM))
		return (0);

	/* Flush any dirty pages from the cache to the backing file. */
	if ((ret = memp_fsync(dbp->mpf)) != 0)
		return (ret);

	qp = (QUEUE *)dbp->q_internal;
	if (qp->page_ext == 0)
		return (0);

	/* We do this for the side effect of opening all active extents. */
	if ((ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		return (ret);

	if (filelist == NULL)
		return (0);

	__os_free(filelist, 0);

	done = 0;
	qp = (QUEUE *)dbp->q_internal;
	array = &qp->array1;

	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
again:
	mpfp = array->mpfarray;
	for (i = array->low_extent; i <= array->hi_extent; i++, mpfp++)
		if ((mpf = mpfp->mpf) != NULL) {
			if ((ret = memp_fsync(mpf)) != 0)
				goto err;
			/*
			 * If we are the only ones with this file open
			 * then close it so it might be removed.
			 */
			if (mpfp->pinref == 0) {
				mpfp->mpf = NULL;
				if ((ret = memp_fclose(mpf)) != 0)
					goto err;
			}
		}

	if (done == 0 && qp->array2.n_extent != 0) {
		array = &qp->array2;
		done = 1;
		goto again;
	}

err:
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);
	return (ret);
}

/*
 * __qam_gen_filelist -- generate a list of extent files.
 *	Another thread may close the handle so this should only
 *	be used single threaded or with care.
 *
 * PUBLIC: int __qam_gen_filelist __P(( DB *, QUEUE_FILELIST **));
 */
int
__qam_gen_filelist(dbp, filelistp)
	DB *dbp;
	QUEUE_FILELIST **filelistp;
{
	DB_ENV *dbenv;
	QUEUE *qp;
	QMETA *meta;
	db_pgno_t i, last, start, stop;
	db_recno_t current, first;
	QUEUE_FILELIST *fp;
	int ret;

	dbenv = dbp->dbenv;
	qp = (QUEUE *)dbp->q_internal;
	*filelistp = NULL;
	if (qp->page_ext == 0)
		return (0);

	/* This may happen during metapage recovery. */
	if (qp->name == NULL)
		return (0);

	/* Find out the page number of the last page in the database. */
	i = PGNO_BASE_MD;
	if ((ret = memp_fget(dbp->mpf, &i, 0, &meta)) != 0) {
		(void)dbp->close(dbp, 0);
		return (ret);
	}

	current = meta->cur_recno;
	first = meta->first_recno;

	if ((ret = memp_fput(dbp->mpf, meta, 0)) != 0) {
		(void)dbp->close(dbp, 0);
		return (ret);
	}

	last = QAM_RECNO_PAGE(dbp, current);
	start = QAM_RECNO_PAGE(dbp, first);

	/* Allocate the worst case plus 1 for null termination. */
	if (last >= start)
		ret = last - start + 2;
	else
		ret = last + (QAM_RECNO_PAGE(dbp, UINT32_T_MAX) - start) + 1;
	if ((ret = __os_calloc(dbenv,
	    ret, sizeof(QUEUE_FILELIST), filelistp)) != 0)
		return (ret);
	fp = *filelistp;
	i = start;
	if (last >= start)
		stop = last;
	else
		stop = QAM_RECNO_PAGE(dbp, UINT32_T_MAX);
again:
	for (; i <= last; i += qp->page_ext) {
		if ((ret = __qam_fprobe(dbp,
		    i, &fp->mpf, QAM_PROBE_MPF, 0)) != 0) {
			if (ret == ENOENT)
				continue;
			return (ret);
		}
		fp->id = (i - 1) / qp->page_ext;
		fp++;
	}

	if (last < start) {
		i = 1;
		stop = last;
		start = 0;
		goto again;
	}

	return (0);
}
