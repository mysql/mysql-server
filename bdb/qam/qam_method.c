/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_method.c,v 11.17 2001/01/10 04:50:54 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_int.h"
#include "db_shash.h"
#include "db_am.h"
#include "qam.h"
#include "db.h"
#include "mp.h"
#include "lock.h"
#include "log.h"

static int __qam_set_extentsize __P((DB *, u_int32_t));
static int __qam_remove_callback __P((DB *, void *));

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
	t = dbp->q_internal;

	array = &t->array1;
again:
	mpfp = array->mpfarray;
	if (mpfp != NULL) {
		for (i = array->low_extent;
		    i <= array->hi_extent; i++, mpfp++) {
			mpf = mpfp->mpf;
			mpfp->mpf = NULL;
			if (mpf != NULL &&
			    (t_ret = memp_fclose(mpf)) != 0 && ret == 0)
				ret = t_ret;
		}
		__os_free(array->mpfarray, 0);
	}
	if (t->array2.n_extent != 0) {
		array = &t->array2;
		array->n_extent = 0;
		goto again;
	}

	if (t->path != NULL)
		__os_free(t->path, 0);
	__os_free(t, sizeof(QUEUE));
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
		__db_err(dbp->dbenv, "Extent size must be at least 1.");
		return (EINVAL);
	}

	((QUEUE*)dbp->q_internal)->page_ext = extentsize;

	return (0);
}

/*
 * __db_prqueue --
 *	Print out a queue
 *
 * PUBLIC: int __db_prqueue __P((DB *, u_int32_t));
 */
int
__db_prqueue(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	PAGE *h;
	QMETA *meta;
	db_pgno_t first, i, last, pg_ext, stop;
	int ret;

	/* Find out the page number of the last page in the database. */
	i = PGNO_BASE_MD;
	if ((ret = memp_fget(dbp->mpf, &i, 0, &meta)) != 0)
		return (ret);

	first = QAM_RECNO_PAGE(dbp, meta->first_recno);
	last = QAM_RECNO_PAGE(dbp, meta->cur_recno);

	if ((ret = __db_prpage(dbp, (PAGE *)meta, flags)) != 0)
		return (ret);
	if ((ret = memp_fput(dbp->mpf, meta, 0)) != 0)
		return (ret);

	i = first;
	if (first > last)
		stop = QAM_RECNO_PAGE(dbp, UINT32_T_MAX);
	else
		stop = last;

	/* Dump each page. */
begin:
	for (; i <= stop; ++i) {
		if ((ret = __qam_fget(dbp, &i, DB_MPOOL_EXTENT, &h)) != 0) {
			pg_ext = ((QUEUE *)dbp->q_internal)->page_ext;
			if (pg_ext == 0) {
				if (ret == EINVAL && first == last)
					return (0);
				return (ret);
			}
			if (ret == ENOENT || ret == EINVAL) {
				i += pg_ext - ((i - 1) % pg_ext) - 1;
				continue;
			}
			return (ret);
		}
		(void)__db_prpage(dbp, h, flags);
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
 * PUBLIC: int __qam_remove __P((DB *, const char *,
 * PUBLIC:      const char *, DB_LSN *, int (**)(DB *, void*), void **));
 */
int
__qam_remove(dbp, name, subdb, lsnp, callbackp, cookiep)
	DB *dbp;
	const char *name, *subdb;
	DB_LSN *lsnp;
	int (**callbackp) __P((DB *, void *));
	void **cookiep;
{
	DBT namedbt;
	DB_ENV *dbenv;
	DB_LSN lsn;
	MPFARRAY *ap;
	QUEUE *qp;
	int ret;
	char *backup, buf[256], *real_back, *real_name;
	QUEUE_FILELIST *filelist, *fp;
	struct __qam_cookie *qam_cookie;

	dbenv = dbp->dbenv;
	ret = 0;
	backup = real_back = real_name = NULL;
	filelist = NULL;

	PANIC_CHECK(dbenv);

	/*
	 * Subdatabases.
	 */
	if (subdb != NULL) {
		__db_err(dbenv,
		    "Queue does not support multiple databases per file.");
		ret = EINVAL;
		goto done;
	}

	qp = (QUEUE *)dbp->q_internal;

	if (qp->page_ext != 0 &&
	    (ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		goto done;

	if (filelist == NULL)
		goto done;

	for (fp = filelist; fp->mpf != NULL; fp++) {
		snprintf(buf,
		    sizeof(buf), QUEUE_EXTENT, qp->dir, qp->name, fp->id);
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, buf, 0, NULL, &real_name)) != 0)
			goto done;
		if (LOGGING_ON(dbenv)) {
			memset(&namedbt, 0, sizeof(namedbt));
			namedbt.data = (char *)buf;
			namedbt.size = strlen(buf) + 1;

			if ((ret =
			    __qam_delete_log(dbenv, dbp->open_txn,
			    &lsn, DB_FLUSH, &namedbt, lsnp)) != 0) {
				__db_err(dbenv,
				    "%s: %s", name, db_strerror(ret));
				goto done;
			}
		}
		(void)__memp_fremove(fp->mpf);
		if ((ret = memp_fclose(fp->mpf)) != 0)
			goto done;
		if (qp->array2.n_extent == 0 || qp->array2.low_extent > fp->id)
			ap = &qp->array1;
		else
			ap = &qp->array2;
		ap->mpfarray[fp->id - ap->low_extent].mpf = NULL;

		/* Create name for backup file. */
		if (TXN_ON(dbenv)) {
			if ((ret = __db_backup_name(dbenv,
			    buf, &backup, lsnp)) != 0)
				goto done;
			if ((ret = __db_appname(dbenv, DB_APP_DATA,
			     NULL, backup, 0, NULL, &real_back)) != 0)
				goto done;
			if ((ret = __os_rename(dbenv,
			     real_name, real_back)) != 0)
				goto done;
			__os_freestr(real_back);
			real_back = NULL;
		}
		else
			if ((ret = __os_unlink(dbenv, real_name)) != 0)
				goto done;
		__os_freestr(real_name);
		real_name = NULL;
	}
	if ((ret= __os_malloc(dbenv,
	    sizeof(struct __qam_cookie), NULL, &qam_cookie)) != 0)
		goto done;
	qam_cookie->lsn = *lsnp;
	qam_cookie->filelist = filelist;
	*cookiep = qam_cookie;
	*callbackp = __qam_remove_callback;

done:
	if (ret != 0 && filelist != NULL)
		__os_free(filelist, 0);
	if (real_back != NULL)
		__os_freestr(real_back);
	if (real_name != NULL)
		__os_freestr(real_name);
	if (backup != NULL)
		__os_freestr(backup);

	return (ret);
}

static int
__qam_remove_callback(dbp, cookie)
	DB *dbp;
	void *cookie;
{
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	QUEUE *qp;
	QUEUE_FILELIST *filelist, *fp;
	char *backup, buf[256], *real_back;
	int ret;

	qp = (QUEUE *)dbp->q_internal;
	if (qp->page_ext == 0)
		return (__os_unlink(dbp->dbenv, cookie));

	dbenv = dbp->dbenv;
	lsnp = &((struct __qam_cookie *)cookie)->lsn;
	filelist = fp = ((struct __qam_cookie *)cookie)->filelist;
	real_back = backup = NULL;
	if ((ret =
	    __db_backup_name(dbenv, qp->name, &backup, lsnp)) != 0)
		goto err;
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, NULL, backup, 0, NULL, &real_back)) != 0)
		goto err;
	if ((ret = __os_unlink(dbp->dbenv, real_back)) != 0)
		goto err;

	__os_freestr(backup);
	__os_freestr(real_back);

	if (fp == NULL)
		return (0);

	for (; fp->mpf != NULL; fp++) {
		snprintf(buf,
		    sizeof(buf), QUEUE_EXTENT, qp->dir, qp->name, fp->id);
		real_back = backup = NULL;
		if ((ret = __db_backup_name(dbenv, buf, &backup, lsnp)) != 0)
			goto err;
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, backup, 0, NULL, &real_back)) != 0)
			goto err;
		ret = __os_unlink(dbenv, real_back);
		__os_freestr(real_back);
		__os_freestr(backup);
	}
	__os_free(filelist, 0);
	__os_free(cookie, sizeof (struct __qam_cookie));

	return (0);

err:
	if (backup != NULL)
		__os_freestr(backup);

	if (real_back != NULL)
		__os_freestr(real_back);

	return (ret);
}

/*
 * __qam_rename
 *	Rename method for Queue.
 *
 * PUBLIC: int __qam_rename __P((DB *,
 * PUBLIC:     const char *, const char *, const char *));
 */
int
__qam_rename(dbp, filename, subdb, newname)
	DB *dbp;
	const char *filename, *subdb, *newname;
{
	DBT namedbt, newnamedbt;
	DB_ENV *dbenv;
	DB_LSN newlsn;
	MPFARRAY *ap;
	QUEUE *qp;
	QUEUE_FILELIST *fp, *filelist;
	char buf[256], nbuf[256], *namep, *real_name, *real_newname;
	int ret;

	dbenv = dbp->dbenv;
	ret = 0;
	real_name = real_newname = NULL;
	filelist = NULL;

	qp = (QUEUE *)dbp->q_internal;

	if (subdb != NULL) {
		__db_err(dbenv,
		    "Queue does not support multiple databases per file.");
		ret = EINVAL;
		goto err;
	}
	if (qp->page_ext != 0 &&
	    (ret = __qam_gen_filelist(dbp, &filelist)) != 0)
		goto err;
	if ((namep = __db_rpath(newname)) != NULL)
		newname = namep + 1;

	for (fp = filelist; fp != NULL && fp->mpf != NULL; fp++) {
		if ((ret = __memp_fremove(fp->mpf)) != 0)
			goto err;
		if ((ret = memp_fclose(fp->mpf)) != 0)
			goto err;
		if (qp->array2.n_extent == 0 || qp->array2.low_extent > fp->id)
			ap = &qp->array1;
		else
			ap = &qp->array2;
		ap->mpfarray[fp->id - ap->low_extent].mpf = NULL;
		snprintf(buf,
		    sizeof(buf), QUEUE_EXTENT, qp->dir, qp->name, fp->id);
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, buf, 0, NULL, &real_name)) != 0)
			goto err;
		snprintf(nbuf,
		     sizeof(nbuf), QUEUE_EXTENT, qp->dir, newname, fp->id);
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, nbuf, 0, NULL, &real_newname)) != 0)
			goto err;
		if (LOGGING_ON(dbenv)) {
			memset(&namedbt, 0, sizeof(namedbt));
			namedbt.data = (char *)buf;
			namedbt.size = strlen(buf) + 1;

			memset(&newnamedbt, 0, sizeof(namedbt));
			newnamedbt.data = (char *)nbuf;
			newnamedbt.size = strlen(nbuf) + 1;

			if ((ret =
			    __qam_rename_log(dbenv,
			    dbp->open_txn, &newlsn, 0,
			    &namedbt, &newnamedbt)) != 0) {
				__db_err(dbenv, "%s: %s", filename, db_strerror(ret));
				goto err;
			}

			if ((ret = __log_filelist_update(dbenv, dbp,
			    dbp->log_fileid, newname, NULL)) != 0)
				goto err;
		}
		if ((ret = __os_rename(dbenv, real_name, real_newname)) != 0)
			goto err;
		__os_freestr(real_name);
		__os_freestr(real_newname);
		real_name = real_newname = NULL;
	}

err:
	if (real_name != NULL)
		__os_freestr(real_name);
	if (real_newname != NULL)
		__os_freestr(real_newname);
	if (filelist != NULL)
		__os_free(filelist, 0);

	return (ret);
}
