/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_method.c,v 11.55 2002/08/26 17:52:19 margo Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/fop.h"
#include "dbinc/lock.h"
#include "dbinc/qam.h"
#include "dbinc/txn.h"

static int __qam_set_extentsize __P((DB *, u_int32_t));

struct __qam_cookie {
	DB_LSN lsn;
	QUEUE_FILELIST *filelist;
};

/*
 * __qam_db_create --
 *	Queue specific initialization of the DB structure.
 *
 * PUBLIC: int __qam_db_create __P((DB *));
 */
int
__qam_db_create(dbp)
	DB *dbp;
{
	QUEUE *t;
	int ret;

	/* Allocate and initialize the private queue structure. */
	if ((ret = __os_calloc(dbp->dbenv, 1, sizeof(QUEUE), &t)) != 0)
		return (ret);
	dbp->q_internal = t;
	dbp->set_q_extentsize = __qam_set_extentsize;

	t->re_pad = ' ';

	return (0);
}

/*
 * __qam_db_close --
 *	Queue specific discard of the DB structure.
 *
 * PUBLIC: int __qam_db_close __P((DB *));
 */
int
__qam_db_close(dbp)
	DB *dbp;
{
	DB_MPOOLFILE *mpf;
	MPFARRAY *array;
	QUEUE *t;
	struct __qmpf *mpfp;
	u_int32_t i;
	int ret, t_ret;

	ret = 0;
	if ((t = dbp->q_internal) == NULL)
		return (0);

	array = &t->array1;
again:
	mpfp = array->mpfarray;
	if (mpfp != NULL) {
		for (i = array->low_extent;
		    i <= array->hi_extent; i++, mpfp++) {
			mpf = mpfp->mpf;
			mpfp->mpf = NULL;
			if (mpf != NULL &&
			    (t_ret = mpf->close(mpf, 0)) != 0 && ret == 0)
				ret = t_ret;
		}
		__os_free(dbp->dbenv, array->mpfarray);
	}
	if (t->array2.n_extent != 0) {
		array = &t->array2;
		array->n_extent = 0;
		goto again;
	}

	if (t->path != NULL)
		__os_free(dbp->dbenv, t->path);
	__os_free(dbp->dbenv, t);
	dbp->q_internal = NULL;

	return (ret);
}

static int
__qam_set_extentsize(dbp, extentsize)
	DB *dbp;
	u_int32_t extentsize;
{
	DB_ILLEGAL_AFTER_OPEN(dbp, "set_extentsize");

	if (extentsize < 1) {
		__db_err(dbp->dbenv, "Extent size must be at least 1");
		return (EINVAL);
	}

	((QUEUE*)dbp->q_internal)->page_ext = extentsize;

	return (0);
}

/*
 * __db_prqueue --
 *	Print out a queue
 *
 * PUBLIC: int __db_prqueue __P((DB *, FILE *, u_int32_t));
 */
int
__db_prqueue(dbp, fp, flags)
	DB *dbp;
	FILE *fp;
	u_int32_t flags;
{
	DB_MPOOLFILE *mpf;
	PAGE *h;
	QMETA *meta;
	db_pgno_t first, i, last, pg_ext, stop;
	int ret, t_ret;

	mpf = dbp->mpf;

	/* Find out the page number of the last page in the database. */
	i = PGNO_BASE_MD;
	if ((ret = mpf->get(mpf, &i, 0, &meta)) != 0)
		return (ret);

	first = QAM_RECNO_PAGE(dbp, meta->first_recno);
	last = QAM_RECNO_PAGE(dbp, meta->cur_recno);

	ret = __db_prpage(dbp, (PAGE *)meta, fp, flags);
	if ((t_ret = mpf->put(mpf, meta, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (ret != 0)
		return (ret);

	i = first;
	if (first > last)
		stop = QAM_RECNO_PAGE(dbp, UINT32_T_MAX);
	else
		stop = last;

	/* Dump each page. */
begin:
	for (; i <= stop; ++i) {
		if ((ret = __qam_fget(dbp, &i, 0, &h)) != 0) {
			pg_ext = ((QUEUE *)dbp->q_internal)->page_ext;
			if (pg_ext == 0) {
				if (ret == DB_PAGE_NOTFOUND && first == last)
					return (0);
				return (ret);
			}
			if (ret == ENOENT || ret == DB_PAGE_NOTFOUND) {
				i += pg_ext - ((i - 1) % pg_ext) - 1;
				continue;
			}
			return (ret);
		}
		(void)__db_prpage(dbp, h, fp, flags);
		if ((ret = __qam_fput(dbp, i, h, 0)) != 0)
			return (ret);
	}

	if (first > last) {
		i = 1;
		stop = last;
		first = last;
		goto begin;
	}
	return (0);
}

/*
 * __qam_remove
 *	Remove method for a Queue.
 *
 * PUBLIC: int __qam_remove __P((DB *,
 * PUBLIC:     DB_TXN *, const char *, const char *, DB_LSN *));
 */
int
__qam_remove(dbp, txn, name, subdb, lsnp)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	DB_LSN *lsnp;
{
	DB_ENV *dbenv;
	DB *tmpdbp;
	MPFARRAY *ap;
	QUEUE *qp;
	QUEUE_FILELIST *filelist, *fp;
	int ret, needclose, t_ret;
	char buf[MAXPATHLEN];
	u_int8_t fid[DB_FILE_ID_LEN];

	COMPQUIET(lsnp, NULL);

	dbenv = dbp->dbenv;
	ret = 0;
	filelist = NULL;
	needclose = 0;

	PANIC_CHECK(dbenv);

	/*
	 * Subdatabases.
	 */
	if (subdb != NULL) {
		__db_err(dbenv,
		    "Queue does not support multiple databases per file");
		ret = EINVAL;
		goto err;
	}

	/*
	 * Since regular remove no longer opens the database, we may have
	 * to do it here.
	 */
	if (F_ISSET(dbp, DB_AM_OPEN_CALLED))
		tmpdbp = dbp;
	else {
		if ((ret = db_create(&tmpdbp, dbenv, 0)) != 0)
			return (ret);
		/*
		 * We need to make sure we don't self-deadlock, so give
		 * this dbp the same locker as the incoming one.
		 */
		tmpdbp->lid = dbp->lid;

		/*
		 * If this is a transactional dbp and the open fails, then
		 * the transactional abort will close the dbp.  If it's not
		 * a transactional open, then we always have to close it
		 * even if the open fails.  Once the open has succeeded,
		 * then we will always want to close it.
		 */
		if (txn == NULL)
			needclose = 1;
		if ((ret = tmpdbp->open(tmpdbp,
		    txn, name, NULL, DB_QUEUE, 0, 0)) != 0)
			goto err;
		needclose = 1;
	}

	qp = (QUEUE *)tmpdbp->q_internal;

	if (qp->page_ext != 0 &&
	    (ret = __qam_gen_filelist(tmpdbp, &filelist)) != 0)
		goto err;

	if (filelist == NULL)
		goto err;

	for (fp = filelist; fp->mpf != NULL; fp++) {
		snprintf(buf, sizeof(buf),
		    QUEUE_EXTENT, qp->dir, PATH_SEPARATOR[0], qp->name, fp->id);
		if ((ret = fp->mpf->close(fp->mpf, DB_MPOOL_DISCARD)) != 0)
			goto err;
		if (qp->array2.n_extent == 0 || qp->array2.low_extent > fp->id)
			ap = &qp->array1;
		else
			ap = &qp->array2;
		ap->mpfarray[fp->id - ap->low_extent].mpf = NULL;

		/* Take care of object reclamation. */
		__qam_exid(tmpdbp, fid, fp->id);
		if ((ret = __fop_remove(dbenv,
		    txn, fid, buf, DB_APP_DATA)) != 0)
			goto err;
	}

err:	if (filelist != NULL)
		__os_free(dbenv, filelist);
	if (needclose) {
		/*
		 * Since we copied the lid from the dbp, we'd better not
		 * free it here.
		 */
		tmpdbp->lid = DB_LOCK_INVALIDID;

		/* We need to remove the lockevent we associated with this. */
		if (txn != NULL)
			__txn_remlock(dbenv,
			    txn, &tmpdbp->handle_lock, DB_LOCK_INVALIDID);

		if ((t_ret =
		    __db_close_i(tmpdbp, txn, DB_NOSYNC)) != 0 && ret == 0)
			ret = t_ret;
	}

	return (ret);
}

/*
 * __qam_rename
 *	Rename method for Queue.
 *
 * PUBLIC: int __qam_rename __P((DB *, DB_TXN *,
 * PUBLIC:     const char *, const char *, const char *));
 */
int
__qam_rename(dbp, txn, filename, subdb, newname)
	DB *dbp;
	DB_TXN *txn;
	const char *filename, *subdb, *newname;
{
	DB_ENV *dbenv;
	DB *tmpdbp;
	MPFARRAY *ap;
	QUEUE *qp;
	QUEUE_FILELIST *fp, *filelist;
	char buf[MAXPATHLEN], nbuf[MAXPATHLEN];
	char *namep;
	int ret, needclose, t_ret;
	u_int8_t fid[DB_FILE_ID_LEN], *fidp;

	dbenv = dbp->dbenv;
	ret = 0;
	filelist = NULL;
	needclose = 0;

	if (subdb != NULL) {
		__db_err(dbenv,
		    "Queue does not support multiple databases per file");
		ret = EINVAL;
		goto err;
	}

	/*
	 * Since regular rename no longer opens the database, we may have
	 * to do it here.
	 */
	if (F_ISSET(dbp, DB_AM_OPEN_CALLED))
		tmpdbp = dbp;
	else {
		if ((ret = db_create(&tmpdbp, dbenv, 0)) != 0)
			return (ret);
		/* Copy the incoming locker so we don't self-deadlock. */
		tmpdbp->lid = dbp->lid;
		needclose = 1;
		if ((ret = tmpdbp->open(tmpdbp, txn, filename, NULL,
		    DB_QUEUE, 0, 0)) != 0)
			goto err;
	}

	qp = (QUEUE *)tmpdbp->q_internal;

	if (qp->page_ext != 0 &&
	    (ret = __qam_gen_filelist(tmpdbp, &filelist)) != 0)
		goto err;
	if ((namep = __db_rpath(newname)) != NULL)
		newname = namep + 1;

	fidp = fid;
	for (fp = filelist; fp != NULL && fp->mpf != NULL; fp++) {
		fp->mpf->get_fileid(fp->mpf, fidp);
		if ((ret = fp->mpf->close(fp->mpf, DB_MPOOL_DISCARD)) != 0)
			goto err;
		if (qp->array2.n_extent == 0 || qp->array2.low_extent > fp->id)
			ap = &qp->array1;
		else
			ap = &qp->array2;
		ap->mpfarray[fp->id - ap->low_extent].mpf = NULL;
		snprintf(buf, sizeof(buf),
		    QUEUE_EXTENT, qp->dir, PATH_SEPARATOR[0], qp->name, fp->id);
		snprintf(nbuf, sizeof(nbuf),
		    QUEUE_EXTENT, qp->dir, PATH_SEPARATOR[0], newname, fp->id);
		if ((ret = __fop_rename(dbenv,
		    txn, buf, nbuf, fidp, DB_APP_DATA)) != 0)
			goto err;
	}

err:	if (filelist != NULL)
		__os_free(dbenv, filelist);
	if (needclose) {
		/* We copied this, so we mustn't free it. */
		tmpdbp->lid = DB_LOCK_INVALIDID;

		/* We need to remove the lockevent we associated with this. */
		if (txn != NULL)
		    __txn_remlock(dbenv,
			txn, &tmpdbp->handle_lock, DB_LOCK_INVALIDID);

		if ((t_ret =
		    __db_close_i(tmpdbp, txn, DB_NOSYNC)) != 0 && ret == 0)
			ret = t_ret;
	}
	return (ret);
}
