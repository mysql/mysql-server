/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_files.c,v 1.52 2002/08/26 17:52:18 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <stdlib.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/qam.h"
#include "dbinc/db_am.h"

/*
 * __qam_fprobe -- calculate and open extent
 *
 * Calculate which extent the page is in, open and create if necessary.
 *
 * PUBLIC: int __qam_fprobe
 * PUBLIC:	   __P((DB *, db_pgno_t, void *, qam_probe_mode, u_int32_t));
 */
int
__qam_fprobe(dbp, pgno, addrp, mode, flags)
	DB *dbp;
	db_pgno_t pgno;
	void *addrp;
	qam_probe_mode mode;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_MPOOLFILE *mpf;
	MPFARRAY *array;
	QUEUE *qp;
	u_int8_t fid[DB_FILE_ID_LEN];
	u_int32_t extid, maxext, openflags;
	char buf[MAXPATHLEN];
	int numext, offset, oldext, ret;

	dbenv = dbp->dbenv;
	qp = (QUEUE *)dbp->q_internal;
	ret = 0;

	if (qp->page_ext == 0) {
		mpf = dbp->mpf;
		return (mode == QAM_PROBE_GET ?
		    mpf->get(mpf, &pgno, flags, addrp) :
		    mpf->put(mpf, addrp, flags));
	}

	mpf = NULL;

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
	 * extents in the array.  This is true by default if there are
	 * no extents here yet.
	 */
	if (offset < 0 || (unsigned) offset >= array->n_extent) {
		oldext = array->n_extent;
		numext = array->hi_extent - array->low_extent  + 1;
		if (offset < 0 &&
		    (unsigned) -offset + numext <= array->n_extent) {
			/*
			 * If we can fit this one into the existing array by
			 * shifting the existing entries then we do not have
			 * to allocate.
			 */
			memmove(&array->mpfarray[-offset],
			    array->mpfarray, numext
			    * sizeof(array->mpfarray[0]));
			memset(array->mpfarray, 0, -offset
			    * sizeof(array->mpfarray[0]));
			offset = 0;
		} else if ((u_int32_t)offset == array->n_extent &&
		    mode != QAM_PROBE_MPF && array->mpfarray[0].pinref == 0) {
			/*
			 * If this is at the end of the array and the file at
			 * the begining has a zero pin count we can close
			 * the bottom extent and put this one at the end.
			 */
			mpf = array->mpfarray[0].mpf;
			if (mpf != NULL && (ret = mpf->close(mpf, 0)) != 0)
				goto err;
			memmove(&array->mpfarray[0], &array->mpfarray[1],
			    (array->n_extent - 1) * sizeof(array->mpfarray[0]));
			array->low_extent++;
			array->hi_extent++;
			offset--;
			array->mpfarray[offset].mpf = NULL;
			array->mpfarray[offset].pinref = 0;
		} else {
			/*
			 * See if we have wrapped around the queue.
			 * If it has then allocate the second array.
			 * Otherwise just expand the one we are using.
			 */
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
			    &array->mpfarray)) != 0)
				goto err;

			if (offset < 0) {
				/*
				 * Move the array up and put the new one
				 * in the first slot.
				 */
				offset = -offset;
				memmove(&array->mpfarray[offset],
				    array->mpfarray,
				    numext * sizeof(array->mpfarray[0]));
				memset(array->mpfarray, 0,
				    offset * sizeof(array->mpfarray[0]));
				memset(&array->mpfarray[numext + offset], 0,
				    (array->n_extent - (numext + offset))
				    * sizeof(array->mpfarray[0]));
				offset = 0;
			}
			else
				/* Clear the new part of the array. */
				memset(&array->mpfarray[oldext], 0,
				    (array->n_extent - oldext) *
				    sizeof(array->mpfarray[0]));
		}
	}

	/* Update the low and hi range of saved extents. */
	if (extid < array->low_extent)
		array->low_extent = extid;
	if (extid > array->hi_extent)
		array->hi_extent = extid;

	/* If the extent file is not yet open, open it. */
	if (array->mpfarray[offset].mpf == NULL) {
		snprintf(buf, sizeof(buf),
		    QUEUE_EXTENT, qp->dir, PATH_SEPARATOR[0], qp->name, extid);
		if ((ret = dbenv->memp_fcreate(
		    dbenv, &array->mpfarray[offset].mpf, 0)) != 0)
			goto err;
		mpf = array->mpfarray[offset].mpf;
		(void)mpf->set_lsn_offset(mpf, 0);
		(void)mpf->set_pgcookie(mpf, &qp->pgcookie);

		/* Set up the fileid for this extent. */
		__qam_exid(dbp, fid, extid);
		(void)mpf->set_fileid(mpf, fid);
		openflags = DB_EXTENT;
		if (LF_ISSET(DB_MPOOL_CREATE))
			openflags |= DB_CREATE;
		if (F_ISSET(dbp, DB_AM_RDONLY))
			openflags |= DB_RDONLY;
		if (F_ISSET(dbenv, DB_ENV_DIRECT_DB))
			openflags |= DB_DIRECT;
		if ((ret = mpf->open(
		    mpf, buf, openflags, qp->mode, dbp->pgsize)) != 0) {
			array->mpfarray[offset].mpf = NULL;
			(void)mpf->close(mpf, 0);
			goto err;
		}
	}

	mpf = array->mpfarray[offset].mpf;
	if (mode == QAM_PROBE_GET)
		array->mpfarray[offset].pinref++;
	if (LF_ISSET(DB_MPOOL_CREATE))
		mpf->set_unlink(mpf, 0);

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
			return (mpf->get(mpf, &pgno, flags, addrp));
		ret = mpf->put(mpf, addrp, flags);
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
	ret = mpf->close(mpf, 0);

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
	char buf[MAXPATHLEN], *real_name;
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
	    QUEUE_EXTENT, qp->dir, PATH_SEPARATOR[0], qp->name, extid);
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, buf, 0, NULL, &real_name)) != 0)
		goto err;
#endif
	/*
	 * The log must be flushed before the file is deleted.  We depend on
	 * the log record of the last delete to recreate the file if we crash.
	 */
	if (LOGGING_ON(dbenv) && (ret = dbenv->log_flush(dbenv, NULL)) != 0)
		goto err;

	mpf = array->mpfarray[offset].mpf;
	array->mpfarray[offset].mpf = NULL;
	mpf->set_unlink(mpf, 1);
	if ((ret = mpf->close(mpf, 0)) != 0)
		goto err;

	/*
	 * If the file is at the bottom of the array
	 * shift things down and adjust the end points.
	 */
	if (offset == 0) {
		memmove(array->mpfarray, &array->mpfarray[1],
		    (array->hi_extent - array->low_extent)
		    * sizeof(array->mpfarray[0]));
		array->mpfarray[
		    array->hi_extent - array->low_extent].mpf = NULL;
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
		__os_free(dbenv, real_name);
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
	mpf = dbp->mpf;

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
	if ((ret = mpf->sync(dbp->mpf)) != 0)
		return (ret);

	qp = (QUEUE *)dbp->q_internal;
	if (qp->page_ext == 0)
		return (0);

	/* We do this for the side effect of opening all active extents. */
	if ((ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		return (ret);

	if (filelist == NULL)
		return (0);

	__os_free(dbp->dbenv, filelist);

	done = 0;
	qp = (QUEUE *)dbp->q_internal;
	array = &qp->array1;

	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
again:
	mpfp = array->mpfarray;
	for (i = array->low_extent; i <= array->hi_extent; i++, mpfp++)
		if ((mpf = mpfp->mpf) != NULL) {
			if ((ret = mpf->sync(mpf)) != 0)
				goto err;
			/*
			 * If we are the only ones with this file open
			 * then close it so it might be removed.
			 */
			if (mpfp->pinref == 0) {
				mpfp->mpf = NULL;
				if ((ret = mpf->close(mpf, 0)) != 0)
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
	DB_MPOOLFILE *mpf;
	QUEUE *qp;
	QMETA *meta;
	db_pgno_t i, last, start;
	db_recno_t current, first;
	QUEUE_FILELIST *fp;
	int ret;

	dbenv = dbp->dbenv;
	mpf = dbp->mpf;
	qp = (QUEUE *)dbp->q_internal;
	*filelistp = NULL;

	if (qp->page_ext == 0)
		return (0);

	/* This may happen during metapage recovery. */
	if (qp->name == NULL)
		return (0);

	/* Find out the page number of the last page in the database. */
	i = PGNO_BASE_MD;
	if ((ret = mpf->get(mpf, &i, 0, &meta)) != 0)
		return (ret);

	current = meta->cur_recno;
	first = meta->first_recno;

	if ((ret = mpf->put(mpf, meta, 0)) != 0)
		return (ret);

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

again:	for (; i <= last; i += qp->page_ext) {
		if ((ret =
		    __qam_fprobe(dbp, i, &fp->mpf, QAM_PROBE_MPF, 0)) != 0) {
			if (ret == ENOENT)
				continue;
			return (ret);
		}
		fp->id = (i - 1) / qp->page_ext;
		fp++;
	}

	if (last < start) {
		i = 1;
		start = 0;
		goto again;
	}

	return (0);
}

/*
 * __qam_extent_names -- generate a list of extent files names.
 *
 * PUBLIC: int __qam_extent_names __P((DB_ENV *, char *, char ***));
 */
int
__qam_extent_names(dbenv, name, namelistp)
	DB_ENV *dbenv;
	char *name;
	char ***namelistp;
{
	DB *dbp;
	QUEUE *qp;
	QUEUE_FILELIST *filelist, *fp;
	char buf[MAXPATHLEN], *dir, **cp, *freep;
	int cnt, len, ret;

	*namelistp = NULL;
	filelist = NULL;
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		return (ret);
	if ((ret =
	    __db_open(dbp, NULL, name, NULL, DB_QUEUE, DB_RDONLY, 0)) != 0)
		return (ret);
	qp = dbp->q_internal;
	if (qp->page_ext == 0)
		goto done;

	if ((ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		goto done;

	if (filelist == NULL)
		goto done;

	cnt = 0;
	for (fp = filelist; fp->mpf != NULL; fp++)
		cnt++;
	dir = ((QUEUE *)dbp->q_internal)->dir;
	name = ((QUEUE *)dbp->q_internal)->name;

	/* QUEUE_EXTENT contains extra chars, but add 6 anyway for the int. */
	len = (u_int32_t)(cnt * (sizeof(**namelistp)
	    + strlen(QUEUE_EXTENT) + strlen(dir) + strlen(name) + 6));

	if ((ret =
	    __os_malloc(dbp->dbenv, len, namelistp)) != 0)
		goto done;
	cp = *namelistp;
	freep = (char *)(cp + cnt + 1);
	for (fp = filelist; fp->mpf != NULL; fp++) {
		snprintf(buf, sizeof(buf),
		    QUEUE_EXTENT, dir, PATH_SEPARATOR[0], name, fp->id);
		len = (u_int32_t)strlen(buf);
		*cp++ = freep;
		strcpy(freep, buf);
		freep += len + 1;
	}
	*cp = NULL;

done:
	if (filelist != NULL)
		__os_free(dbp->dbenv, filelist);
	(void)dbp->close(dbp, DB_NOSYNC);

	return (ret);
}

/*
 * __qam_exid --
 *	Generate a fileid for an extent based on the fileid of the main
 * file.  Since we do not log schema creates/deletes explicitly, the log
 * never captures the fileid of an extent file.  In order that masters and
 * replicas have the same fileids (so they can explicitly delete them), we
 * use computed fileids for the extent files of Queue files.
 *
 * An extent file id retains the low order 12 bytes of the file id and
 * overwrites the dev/inode fields, placing a 0 in the inode field, and
 * the extent number in the dev field.
 *
 * PUBLIC: void __qam_exid __P((DB *, u_int8_t *, u_int32_t));
 */
void
__qam_exid(dbp, fidp, exnum)
	DB *dbp;
	u_int8_t *fidp;
	u_int32_t exnum;
{
	int i;
	u_int8_t *p;

	/* Copy the fileid from the master. */
	memcpy(fidp, dbp->fileid, DB_FILE_ID_LEN);

	/* The first four bytes are the inode or the FileIndexLow; 0 it. */
	for (i = sizeof(u_int32_t); i > 0; --i)
		*fidp++ = 0;

	/* The next four bytes are the dev/FileIndexHigh; insert the exnum . */
	for (p = (u_int8_t *)&exnum, i = sizeof(u_int32_t); i > 0; --i)
		*fidp++ = *p++;
}
