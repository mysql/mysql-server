/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: qam_rec.c,v 11.34 2001/01/19 18:01:59 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "lock.h"
#include "db_am.h"
#include "qam.h"
#include "log.h"

/*
 * __qam_inc_recover --
 *	Recovery function for inc.
 *
 * PUBLIC: int __qam_inc_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_inc_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_inc_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	QMETA *meta;
	db_pgno_t metapg;
	int cmp_p, modified, ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__qam_inc_print);
	REC_INTRO(__qam_inc_read, 1);

	metapg = ((QUEUE *)file_dbp->q_internal)->q_meta;

	if ((ret = __db_lget(dbc,
	    LCK_ROLLBACK, metapg,  DB_LOCK_WRITE, 0, &lock)) != 0)
		goto done;
	if ((ret = memp_fget(mpf, &metapg, 0, &meta)) != 0) {
		if (DB_REDO(op)) {
			if ((ret = memp_fget(mpf,
			    &metapg, DB_MPOOL_CREATE, &meta)) != 0) {
				(void)__LPUT(dbc, lock);
				goto out;
			}
			meta->dbmeta.pgno = metapg;
			meta->dbmeta.type = P_QAMMETA;

		} else {
			*lsnp = argp->prev_lsn;
			ret = 0;
			(void)__LPUT(dbc, lock);
			goto out;
		}
	}

	modified = 0;
	cmp_p = log_compare(&LSN(meta), &argp->lsn);
	CHECK_LSN(op, cmp_p, &LSN(meta), &argp->lsn);

	/*
	 * The cur_recno never goes backwards.  It is a point of
	 * contention among appenders.  If one fails cur_recno will
	 * most likely be beyond that one when it aborts.
	 * We move it ahead on either an abort or a commit
	 * and make the LSN reflect that fact.
	 */
	if (cmp_p == 0) {
		modified = 1;
		meta->cur_recno++;
		if (meta->cur_recno == RECNO_OOB)
			meta->cur_recno++;
		meta->dbmeta.lsn = *lsnp;
	}
	if ((ret = memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)))
		goto out;

	(void)__LPUT(dbc, lock);

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __qam_incfirst_recover --
 *	Recovery function for incfirst.
 *
 * PUBLIC: int __qam_incfirst_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_incfirst_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_incfirst_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	QMETA *meta;
	QUEUE_CURSOR *cp;
	db_pgno_t metapg;
	int exact, modified, ret, rec_ext;

	COMPQUIET(info, NULL);
	REC_PRINT(__qam_incfirst_print);
	REC_INTRO(__qam_incfirst_read, 1);

	metapg = ((QUEUE *)file_dbp->q_internal)->q_meta;

	if ((ret = __db_lget(dbc,
	    LCK_ROLLBACK, metapg,  DB_LOCK_WRITE, 0, &lock)) != 0)
		goto done;
	if ((ret = memp_fget(mpf, &metapg, 0, &meta)) != 0) {
		if (DB_REDO(op)) {
			if ((ret = memp_fget(mpf,
			    &metapg, DB_MPOOL_CREATE, &meta)) != 0) {
				(void)__LPUT(dbc, lock);
				goto out;
			}
			meta->dbmeta.pgno = metapg;
			meta->dbmeta.type = P_QAMMETA;
		} else {
			*lsnp = argp->prev_lsn;
			ret = 0;
			(void)__LPUT(dbc, lock);
			goto out;
		}
	}

	modified = 0;

	/*
	 * Only move first_recno backwards so we pick up the aborted delete.
	 * When going forward we need to be careful since
	 * we may have bumped over a locked record.
	 */
	if (DB_UNDO(op)) {
		if (QAM_BEFORE_FIRST(meta, argp->recno)) {
			meta->first_recno = argp->recno;
			modified = 1;
		}
	} else {
		if (log_compare(&LSN(meta), lsnp) < 0) {
			LSN(meta) = *lsnp;
			modified = 1;
		}
		rec_ext = 0;
		if (meta->page_ext != 0)
			rec_ext = meta->page_ext * meta->rec_page;
		cp = (QUEUE_CURSOR *)dbc->internal;
		if (meta->first_recno == RECNO_OOB)
			meta->first_recno++;
		while (meta->first_recno != meta->cur_recno
		    && !QAM_BEFORE_FIRST(meta, argp->recno + 1)) {
			if ((ret = __qam_position(dbc,
			    &meta->first_recno, QAM_READ, &exact)) != 0)
				goto out;
			if (cp->page != NULL)
				__qam_fput(file_dbp, cp->pgno, cp->page, 0);

			if (exact == 1)
				break;
			if (cp->page != NULL &&
			    rec_ext != 0 && meta->first_recno % rec_ext == 0)
				if ((ret =
				    __qam_fremove(file_dbp, cp->pgno)) != 0)
					goto out;
			meta->first_recno++;
			if (meta->first_recno == RECNO_OOB)
				meta->first_recno++;
			modified = 1;
		}
	}

	if ((ret = memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)))
		goto out;

	(void)__LPUT(dbc, lock);

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __qam_mvptr_recover --
 *	Recovery function for mvptr.
 *
 * PUBLIC: int __qam_mvptr_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_mvptr_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_mvptr_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	QMETA *meta;
	db_pgno_t metapg;
	int cmp_p, modified, ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__qam_mvptr_print);
	REC_INTRO(__qam_mvptr_read, 1);

	metapg = ((QUEUE *)file_dbp->q_internal)->q_meta;

	if ((ret = __db_lget(dbc,
	    LCK_ROLLBACK, metapg,  DB_LOCK_WRITE, 0, &lock)) != 0)
		goto done;
	if ((ret = memp_fget(mpf, &metapg, 0, &meta)) != 0) {
		if (DB_REDO(op)) {
			if ((ret = memp_fget(mpf,
			    &metapg, DB_MPOOL_CREATE, &meta)) != 0) {
				(void)__LPUT(dbc, lock);
				goto out;
			}
			meta->dbmeta.pgno = metapg;
			meta->dbmeta.type = P_QAMMETA;
		} else {
			*lsnp = argp->prev_lsn;
			ret = 0;
			(void)__LPUT(dbc, lock);
			goto out;
		}
	}

	modified = 0;
	cmp_p = log_compare(&meta->dbmeta.lsn, &argp->metalsn);

	/*
	 * We never undo a movement of one of the pointers.
	 * Just move them along regardless of abort/commit.
	 */
	if (cmp_p == 0) {
		if (argp->opcode & QAM_SETFIRST)
			meta->first_recno = argp->new_first;

		if (argp->opcode & QAM_SETCUR)
			meta->cur_recno = argp->new_cur;

		modified = 1;
		meta->dbmeta.lsn = *lsnp;
	}

	if ((ret = memp_fput(mpf, meta, modified ? DB_MPOOL_DIRTY : 0)))
		goto out;

	(void)__LPUT(dbc, lock);

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}
/*
 * __qam_del_recover --
 *	Recovery function for del.
 *		Non-extent version or if there is no data (zero len).
 *
 * PUBLIC: int __qam_del_recover
 * PUBLIC:     __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_del_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_del_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	QAMDATA *qp;
	QMETA *meta;
	QPAGE *pagep;
	db_pgno_t metapg;
	int cmp_n, modified, ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__qam_del_print);
	REC_INTRO(__qam_del_read, 1);

	if ((ret = __qam_fget(file_dbp,
	     &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
		goto out;

	modified = 0;
	if (pagep->pgno == PGNO_INVALID) {
		pagep->pgno = argp->pgno;
		pagep->type = P_QAMDATA;
		modified = 1;
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));

	if (DB_UNDO(op)) {
		/* make sure first is behind us */
		metapg = ((QUEUE *)file_dbp->q_internal)->q_meta;
		if ((ret = __db_lget(dbc,
		    LCK_ROLLBACK, metapg, DB_LOCK_WRITE, 0, &lock)) != 0)
			return (ret);
		if ((ret = memp_fget(file_dbp->mpf, &metapg, 0, &meta)) != 0) {
			(void)__LPUT(dbc, lock);
			goto done;
		}
		if (meta->first_recno == RECNO_OOB ||
		    (QAM_BEFORE_FIRST(meta, argp->recno)
		    && (meta->first_recno <= meta->cur_recno
		    || meta->first_recno -
		    argp->recno < argp->recno - meta->cur_recno))) {
			meta->first_recno = argp->recno;
			(void)memp_fput(file_dbp->mpf, meta, DB_MPOOL_DIRTY);
		} else
			(void)memp_fput(file_dbp->mpf, meta, 0);
		(void)__LPUT(dbc, lock);

		/* Need to undo delete - mark the record as present */
		qp = QAM_GET_RECORD(file_dbp, pagep, argp->indx);
		F_SET(qp, QAM_VALID);

		/*
		 * Move the LSN back to this point;  do not move it forward.
		 * Only move it back if we're in recovery.  If we're in
		 * an abort, because we don't hold a page lock, we could
		 * foul up a concurrent put.  Having too late an LSN
		 * is harmless in queue except when we're determining
		 * what we need to roll forward during recovery.  [#2588]
		 */
		if (op == DB_TXN_BACKWARD_ROLL && cmp_n < 0)
			LSN(pagep) = argp->lsn;
		modified = 1;
	} else if (cmp_n > 0 && DB_REDO(op)) {
		/* Need to redo delete - clear the valid bit */
		qp = QAM_GET_RECORD(file_dbp, pagep, argp->indx);
		F_CLR(qp, QAM_VALID);
		LSN(pagep) = *lsnp;
		modified = 1;
	}
	if ((ret = __qam_fput(file_dbp,
	    argp->pgno, pagep, modified ? DB_MPOOL_DIRTY : 0)))
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}
/*
 * __qam_delext_recover --
 *	Recovery function for del in an extent based queue.
 *
 * PUBLIC: int __qam_delext_recover __P((DB_ENV *,
 * PUBLIC:     DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_delext_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_delext_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_LOCK lock;
	DB_MPOOLFILE *mpf;
	QAMDATA *qp;
	QMETA *meta;
	QPAGE *pagep;
	db_pgno_t metapg;
	int cmp_n, modified, ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__qam_delext_print);
	REC_INTRO(__qam_delext_read, 1);

	if ((ret = __qam_fget(file_dbp,
	     &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
		goto out;

	modified = 0;
	if (pagep->pgno == PGNO_INVALID) {
		pagep->pgno = argp->pgno;
		pagep->type = P_QAMDATA;
		modified = 1;
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));

	if (DB_UNDO(op)) {
		/* make sure first is behind us */
		metapg = ((QUEUE *)file_dbp->q_internal)->q_meta;
		if ((ret = __db_lget(dbc,
		    LCK_ROLLBACK, metapg, DB_LOCK_WRITE, 0, &lock)) != 0)
			return (ret);
		if ((ret = memp_fget(file_dbp->mpf, &metapg, 0, &meta)) != 0) {
			(void)__LPUT(dbc, lock);
			goto done;
		}
		if (meta->first_recno == RECNO_OOB ||
		    (QAM_BEFORE_FIRST(meta, argp->recno)
		    && (meta->first_recno <= meta->cur_recno
		    || meta->first_recno -
		    argp->recno < argp->recno - meta->cur_recno))) {
			meta->first_recno = argp->recno;
			(void)memp_fput(file_dbp->mpf, meta, DB_MPOOL_DIRTY);
		} else
			(void)memp_fput(file_dbp->mpf, meta, 0);
		(void)__LPUT(dbc, lock);

		if ((ret = __qam_pitem(dbc, pagep,
		    argp->indx, argp->recno, &argp->data)) != 0)
			goto done;

		/*
		 * Move the LSN back to this point;  do not move it forward.
		 * Only move it back if we're in recovery.  If we're in
		 * an abort, because we don't hold a page lock, we could
		 * foul up a concurrent put.  Having too late an LSN
		 * is harmless in queue except when we're determining
		 * what we need to roll forward during recovery.  [#2588]
		 */
		if (op == DB_TXN_BACKWARD_ROLL && cmp_n < 0)
			LSN(pagep) = argp->lsn;
		modified = 1;
	} else if (cmp_n > 0 && DB_REDO(op)) {
		/* Need to redo delete - clear the valid bit */
		qp = QAM_GET_RECORD(file_dbp, pagep, argp->indx);
		F_CLR(qp, QAM_VALID);
		LSN(pagep) = *lsnp;
		modified = 1;
	}
	if ((ret = __qam_fput(file_dbp,
	    argp->pgno, pagep, modified ? DB_MPOOL_DIRTY : 0)))
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}

/*
 * __qam_add_recover --
 *	Recovery function for add.
 *
 * PUBLIC: int __qam_add_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_add_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_add_args *argp;
	DB *file_dbp;
	DBC *dbc;
	DB_MPOOLFILE *mpf;
	QAMDATA *qp;
	QMETA *meta;
	QPAGE *pagep;
	db_pgno_t metapg;
	int cmp_n, modified, ret;

	COMPQUIET(info, NULL);
	REC_PRINT(__qam_add_print);
	REC_INTRO(__qam_add_read, 1);

	modified = 0;
	if ((ret = __qam_fget(file_dbp,
	    &argp->pgno, DB_MPOOL_CREATE, &pagep)) != 0)
		goto out;

	if (pagep->pgno == PGNO_INVALID) {
		pagep->pgno = argp->pgno;
		pagep->type = P_QAMDATA;
		modified = 1;
	}

	cmp_n = log_compare(lsnp, &LSN(pagep));

	if (cmp_n > 0 && DB_REDO(op)) {
		/* Need to redo add - put the record on page */
		if ((ret = __qam_pitem(dbc, pagep, argp->indx, argp->recno,
				&argp->data)) != 0)
			goto err;
		LSN(pagep) = *lsnp;
		modified = 1;
		/* Make sure first pointer includes this record. */
		metapg = ((QUEUE *)file_dbp->q_internal)->q_meta;
		if ((ret = memp_fget(mpf, &metapg, 0, &meta)) != 0)
			goto err;
		if (QAM_BEFORE_FIRST(meta, argp->recno)) {
			meta->first_recno = argp->recno;
			if ((ret = memp_fput(mpf, meta, DB_MPOOL_DIRTY)) != 0)
				goto err;
		} else
			if ((ret = memp_fput(mpf, meta, 0)) != 0)
				goto err;

	} else if (DB_UNDO(op)) {
		/*
		 * Need to undo add
		 *	If this was an overwrite, put old record back.
		 *	Otherwise just clear the valid bit
		 */
		if (argp->olddata.size != 0) {
			if ((ret = __qam_pitem(dbc, pagep,
			    argp->indx, argp->recno, &argp->olddata)) != 0)
				goto err;

			if (!(argp->vflag & QAM_VALID)) {
				qp = QAM_GET_RECORD(
				    file_dbp, pagep, argp->indx);
				F_CLR(qp, QAM_VALID);
			}
			modified = 1;
		} else {
			qp = QAM_GET_RECORD(file_dbp, pagep, argp->indx);
			qp->flags = 0;
			modified = 1;
		}

		/*
		 * Move the LSN back to this point;  do not move it forward.
		 * Only move it back if we're in recovery.  If we're in
		 * an abort, because we don't hold a page lock, we could
		 * foul up a concurrent put.  Having too late an LSN
		 * is harmless in queue except when we're determining
		 * what we need to roll forward during recovery.  [#2588]
		 */
		if (op == DB_TXN_BACKWARD_ROLL && cmp_n < 0)
			LSN(pagep) = argp->lsn;
	}

err:	if ((ret = __qam_fput(file_dbp,
	    argp->pgno, pagep, modified ? DB_MPOOL_DIRTY : 0)))
		goto out;

done:	*lsnp = argp->prev_lsn;
	ret = 0;

out:	REC_CLOSE;
}
/*
 * __qam_delete_recover --
 *	Recovery function for delete of an extent.
 *
 * PUBLIC: int __qam_delete_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_delete_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_delete_args *argp;
	int ret;
	char *backup, *real_back, *real_name;

	COMPQUIET(info, NULL);

	REC_PRINT(__qam_delete_print);

	backup = real_back = real_name = NULL;
	if ((ret = __qam_delete_read(dbenv, dbtp->data, &argp)) != 0)
		goto out;

	if (DB_REDO(op)) {
		/*
		 * On a recovery, as we recreate what was going on, we
		 * recreate the creation of the file.  And so, even though
		 * it committed, we need to delete it.  Try to delete it,
		 * but it is not an error if that delete fails.
		 */
		if ((ret = __db_appname(dbenv, DB_APP_DATA,
		    NULL, argp->name.data, 0, NULL, &real_name)) != 0)
			goto out;
		if (__os_exists(real_name, NULL) == 0) {
			if ((ret = __os_unlink(dbenv, real_name)) != 0)
				goto out;
		}
	} else if (DB_UNDO(op)) {
		/*
		 * Trying to undo.  File may or may not have been deleted.
		 * Try to move the backup to the original.  If the backup
		 * exists, then this is right.  If it doesn't exist, then
		 * nothing will happen and that's OK.
		 */
		if ((ret =  __db_backup_name(dbenv, argp->name.data,
		    &backup, &argp->lsn)) != 0)
			goto out;
		if ((ret = __db_appname(dbenv,
		    DB_APP_DATA, NULL, backup, 0, NULL, &real_back)) != 0)
			goto out;
		if ((ret = __db_appname(dbenv, DB_APP_DATA,
		    NULL, argp->name.data, 0, NULL, &real_name)) != 0)
			goto out;
		if (__os_exists(real_back, NULL) == 0)
			if ((ret =
			     __os_rename(dbenv, real_back, real_name)) != 0)
				goto out;
	}
	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (argp != NULL)
		__os_free(argp, 0);
	if (backup != NULL)
		__os_freestr(backup);
	if (real_back != NULL)
		__os_freestr(real_back);
	if (real_name != NULL)
		__os_freestr(real_name);
	return (ret);
}
/*
 * __qam_rename_recover --
 *	Recovery function for rename.
 *
 * PUBLIC: int __qam_rename_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__qam_rename_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__qam_rename_args *argp;
	char *new_name, *real_name;
	int ret;

	COMPQUIET(info, NULL);

	REC_PRINT(__qam_rename_print);

	new_name = real_name = NULL;

	if ((ret = __qam_rename_read(dbenv, dbtp->data, &argp)) != 0)
		goto out;

	if (DB_REDO(op)) {
		if ((ret = __db_appname(dbenv, DB_APP_DATA,
		    NULL, argp->name.data, 0, NULL, &real_name)) != 0)
			goto out;
		if (__os_exists(real_name, NULL) == 0) {
			if ((ret = __db_appname(dbenv,
			    DB_APP_DATA, NULL, argp->newname.data,
			    0, NULL, &new_name)) != 0)
				goto out;
			if ((ret = __os_rename(dbenv,
			    real_name, new_name)) != 0)
				goto out;
		}
	} else {
		if ((ret = __db_appname(dbenv, DB_APP_DATA,
		    NULL, argp->newname.data, 0, NULL, &new_name)) != 0)
			goto out;
		if (__os_exists(new_name, NULL) == 0) {
			if ((ret = __db_appname(dbenv,
			    DB_APP_DATA, NULL, argp->name.data,
			    0, NULL, &real_name)) != 0)
				goto out;
			if ((ret = __os_rename(dbenv,
			    new_name, real_name)) != 0)
				goto out;
		}
	}

	*lsnp = argp->prev_lsn;
	ret = 0;

out:	if (argp != NULL)
		__os_free(argp, 0);

	if (new_name != NULL)
		__os_free(new_name, 0);

	if (real_name != NULL)
		__os_free(real_name, 0);

	return (ret);
}
