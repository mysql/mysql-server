/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: dbreg_util.c,v 11.50 2004/10/15 16:59:41 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __dbreg_check_master __P((DB_ENV *, u_int8_t *, char *));

/*
 * __dbreg_add_dbentry --
 *	Adds a DB entry to the dbreg DB entry table.
 *
 * PUBLIC: int __dbreg_add_dbentry __P((DB_ENV *, DB_LOG *, DB *, int32_t));
 */
int
__dbreg_add_dbentry(dbenv, dblp, dbp, ndx)
	DB_ENV *dbenv;
	DB_LOG *dblp;
	DB *dbp;
	int32_t ndx;
{
	int32_t i;
	int ret;

	ret = 0;

	MUTEX_THREAD_LOCK(dbenv, dblp->mutexp);

	/*
	 * Check if we need to grow the table.  Note, ndx is 0-based (the
	 * index into the DB entry table) an dbentry_cnt is 1-based, the
	 * number of available slots.
	 */
	if (dblp->dbentry_cnt <= ndx) {
		if ((ret = __os_realloc(dbenv,
		    (size_t)(ndx + DB_GROW_SIZE) * sizeof(DB_ENTRY),
		    &dblp->dbentry)) != 0)
			goto err;

		/* Initialize the new entries. */
		for (i = dblp->dbentry_cnt; i < ndx + DB_GROW_SIZE; i++) {
			dblp->dbentry[i].dbp = NULL;
			dblp->dbentry[i].deleted = 0;
		}
		dblp->dbentry_cnt = i;
	}

	DB_ASSERT(dblp->dbentry[ndx].dbp == NULL);
	dblp->dbentry[ndx].deleted = dbp == NULL;
	dblp->dbentry[ndx].dbp = dbp;

err:	MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);
	return (ret);
}

/*
 * __dbreg_rem_dbentry
 *	Remove an entry from the DB entry table.
 *
 * PUBLIC: void __dbreg_rem_dbentry __P((DB_LOG *, int32_t));
 */
void
__dbreg_rem_dbentry(dblp, ndx)
	DB_LOG *dblp;
	int32_t ndx;
{
	MUTEX_THREAD_LOCK(dblp->dbenv, dblp->mutexp);
	if (dblp->dbentry_cnt > ndx) {
		dblp->dbentry[ndx].dbp = NULL;
		dblp->dbentry[ndx].deleted = 0;
	}
	MUTEX_THREAD_UNLOCK(dblp->dbenv, dblp->mutexp);
}

/*
 * __dbreg_log_files --
 *	Put a DBREG_CHKPNT/CLOSE log record for each open database.
 *
 * PUBLIC: int __dbreg_log_files __P((DB_ENV *));
 */
int
__dbreg_log_files(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	DB_LSN r_unused;
	DBT *dbtp, fid_dbt, t;
	FNAME *fnp;
	LOG *lp;
	int ret;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	ret = 0;

	MUTEX_LOCK(dbenv, &lp->fq_mutex);

	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {

		if (fnp->name_off == INVALID_ROFF)
			dbtp = NULL;
		else {
			memset(&t, 0, sizeof(t));
			t.data = R_ADDR(&dblp->reginfo, fnp->name_off);
			t.size = (u_int32_t)strlen(t.data) + 1;
			dbtp = &t;
		}
		memset(&fid_dbt, 0, sizeof(fid_dbt));
		fid_dbt.data = fnp->ufid;
		fid_dbt.size = DB_FILE_ID_LEN;
		/*
		 * Output DBREG_CHKPNT records which will be processed during
		 * the OPENFILES pass of recovery.  At the end of recovery we
		 * want to output the files that were open so a future recovery
		 * run will have the correct files open during a backward pass.
		 * For this we output DBREG_RCLOSE records so the files will be
		 * closed on the forward pass.
		 */
		if ((ret = __dbreg_register_log(dbenv,
		    NULL, &r_unused,
		    fnp->is_durable ? 0 : DB_LOG_NOT_DURABLE,
		    F_ISSET(dblp, DBLOG_RECOVER) ? DBREG_RCLOSE : DBREG_CHKPNT,
		    dbtp, &fid_dbt, fnp->id, fnp->s_type, fnp->meta_pgno,
		    TXN_INVALID)) != 0)
			break;
	}

	MUTEX_UNLOCK(dbenv, &lp->fq_mutex);

	return (ret);
}

/*
 * __dbreg_close_files --
 *	Remove the id's of open files and actually close those
 *	files that were opened by the recovery daemon.  We sync the
 *	file, unless its mpf pointer has been NULLed by a db_remove or
 *	db_rename.  We may not have flushed the log_register record that
 *	closes the file.
 *
 * PUBLIC: int __dbreg_close_files __P((DB_ENV *));
 */
int
__dbreg_close_files(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG *dblp;
	DB *dbp;
	int ret, t_ret;
	int32_t i;

	/* If we haven't initialized logging, we have nothing to do. */
	if (!LOGGING_ON(dbenv))
		return (0);

	dblp = dbenv->lg_handle;
	ret = 0;
	MUTEX_THREAD_LOCK(dbenv, dblp->mutexp);
	for (i = 0; i < dblp->dbentry_cnt; i++) {
		/*
		 * We only want to close dbps that recovery opened.  Any
		 * dbps that weren't opened by recovery but show up here
		 * are about to be unconditionally removed from the table.
		 * Before doing so, we need to revoke their log fileids
		 * so that we don't end up leaving around FNAME entries
		 * for dbps that shouldn't have them.
		 */
		if ((dbp = dblp->dbentry[i].dbp) != NULL) {
			/*
			 * It's unsafe to call DB->close or revoke_id
			 * while holding the thread lock, because
			 * we'll call __dbreg_rem_dbentry and grab it again.
			 *
			 * Just drop it.  Since dbreg ids go monotonically
			 * upward, concurrent opens should be safe, and the
			 * user should have no business closing files while
			 * we're in this loop anyway--we're in the process of
			 * making all outstanding dbps invalid.
			 */
			MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);
			if (F_ISSET(dbp, DB_AM_RECOVER))
				t_ret = __db_close(dbp,
				     NULL, dbp->mpf == NULL ? DB_NOSYNC : 0);
			else
				t_ret = __dbreg_revoke_id(
				     dbp, 0, DB_LOGFILEID_INVALID);
			if (ret == 0)
				ret = t_ret;
			MUTEX_THREAD_LOCK(dbenv, dblp->mutexp);
		}

		dblp->dbentry[i].deleted = 0;
		dblp->dbentry[i].dbp = NULL;
	}
	MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);
	return (ret);
}

/*
 * __dbreg_id_to_db --
 *	Return the DB corresponding to the specified dbreg id.
 *
 * PUBLIC: int __dbreg_id_to_db __P((DB_ENV *, DB_TXN *, DB **, int32_t, int));
 */
int
__dbreg_id_to_db(dbenv, txn, dbpp, ndx, inc)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB **dbpp;
	int32_t ndx;
	int inc;
{
	return (__dbreg_id_to_db_int(dbenv, txn, dbpp, ndx, inc, 1));
}

/*
 * __dbreg_id_to_db_int --
 *	Return the DB corresponding to the specified dbreg id.  The internal
 * version takes a final parameter that indicates whether we should attempt
 * to open the file if no mapping is found.  During recovery, the recovery
 * routines all want to try to open the file (and this is called from
 * __dbreg_id_to_db), however, if we have a multi-process environment where
 * some processes may not have the files open (e.g., XA), then we also get
 * called from __dbreg_assign_id and it's OK if there is no mapping.
 *
 * PUBLIC: int __dbreg_id_to_db_int __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, DB **, int32_t, int, int));
 */
int
__dbreg_id_to_db_int(dbenv, txn, dbpp, ndx, inc, tryopen)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB **dbpp;
	int32_t ndx;
	int inc, tryopen;
{
	DB_LOG *dblp;
	FNAME *fname;
	int ret;
	char *name;

	ret = 0;
	dblp = dbenv->lg_handle;
	COMPQUIET(inc, 0);

	MUTEX_THREAD_LOCK(dbenv, dblp->mutexp);

	/*
	 * Under XA, a process different than the one issuing DB operations
	 * may abort a transaction.  In this case, the "recovery" routines
	 * are run by a process that does not necessarily have the file open,
	 * so we we must open the file explicitly.
	 */
	if (ndx >= dblp->dbentry_cnt ||
	    (!dblp->dbentry[ndx].deleted && dblp->dbentry[ndx].dbp == NULL)) {
		if (!tryopen || F_ISSET(dblp, DBLOG_RECOVER)) {
			ret = ENOENT;
			goto err;
		}

		/*
		 * __dbreg_id_to_fname acquires the region's fq_mutex,
		 * which we can't safely acquire while we hold the thread lock.
		 * We no longer need it anyway--the dbentry table didn't
		 * have what we needed.
		 */
		MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);

		if (__dbreg_id_to_fname(dblp, ndx, 0, &fname) != 0)
			/*
			 * With transactional opens, we may actually have
			 * closed this file in the transaction in which
			 * case this will fail too.  Then it's up to the
			 * caller to reopen the file.
			 */
			return (ENOENT);

		/*
		 * Note that we're relying on fname not to change, even
		 * though we released the mutex that protects it (fq_mutex)
		 * inside __dbreg_id_to_fname.  This should be a safe
		 * assumption, because the other process that has the file
		 * open shouldn't be closing it while we're trying to abort.
		 */
		name = R_ADDR(&dblp->reginfo, fname->name_off);

		/*
		 * At this point, we are not holding the thread lock, so exit
		 * directly instead of going through the exit code at the
		 * bottom.  If the __dbreg_do_open succeeded, then we don't need
		 * to do any of the remaining error checking at the end of this
		 * routine.
		 * XXX I am sending a NULL txnlist and 0 txnid which may be
		 * completely broken ;(
		 */
		if ((ret = __dbreg_do_open(dbenv, txn, dblp,
		    fname->ufid, name, fname->s_type,
		    ndx, fname->meta_pgno, NULL, 0)) != 0)
			return (ret);

		*dbpp = dblp->dbentry[ndx].dbp;
		return (0);
	}

	/*
	 * Return DB_DELETED if the file has been deleted (it's not an error).
	 */
	if (dblp->dbentry[ndx].deleted) {
		ret = DB_DELETED;
		goto err;
	}

	/* It's an error if we don't have a corresponding writeable DB. */
	if ((*dbpp = dblp->dbentry[ndx].dbp) == NULL)
		ret = ENOENT;

err:	MUTEX_THREAD_UNLOCK(dbenv, dblp->mutexp);
	return (ret);
}

/*
 * __dbreg_id_to_fname --
 *	Traverse the shared-memory region looking for the entry that
 *	matches the passed dbreg id.  Returns 0 on success; -1 on error.
 *
 * PUBLIC: int __dbreg_id_to_fname __P((DB_LOG *, int32_t, int, FNAME **));
 */
int
__dbreg_id_to_fname(dblp, id, have_lock, fnamep)
	DB_LOG *dblp;
	int32_t id;
	int have_lock;
	FNAME **fnamep;
{
	DB_ENV *dbenv;
	FNAME *fnp;
	LOG *lp;
	int ret;

	dbenv = dblp->dbenv;
	lp = dblp->reginfo.primary;

	ret = -1;

	if (!have_lock)
		MUTEX_LOCK(dbenv, &lp->fq_mutex);
	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (fnp->id == id) {
			*fnamep = fnp;
			ret = 0;
			break;
		}
	}
	if (!have_lock)
		MUTEX_UNLOCK(dbenv, &lp->fq_mutex);

	return (ret);
}
/*
 * __dbreg_fid_to_fname --
 *	Traverse the shared-memory region looking for the entry that
 *	matches the passed file unique id.  Returns 0 on success; -1 on error.
 *
 * PUBLIC: int __dbreg_fid_to_fname __P((DB_LOG *, u_int8_t *, int, FNAME **));
 */
int
__dbreg_fid_to_fname(dblp, fid, have_lock, fnamep)
	DB_LOG *dblp;
	u_int8_t *fid;
	int have_lock;
	FNAME **fnamep;
{
	DB_ENV *dbenv;
	FNAME *fnp;
	LOG *lp;
	int ret;

	dbenv = dblp->dbenv;
	lp = dblp->reginfo.primary;

	ret = -1;

	if (!have_lock)
		MUTEX_LOCK(dbenv, &lp->fq_mutex);
	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (memcmp(fnp->ufid, fid, DB_FILE_ID_LEN) == 0) {
			*fnamep = fnp;
			ret = 0;
			break;
		}
	}
	if (!have_lock)
		MUTEX_UNLOCK(dbenv, &lp->fq_mutex);

	return (ret);
}

/*
 * __dbreg_get_name
 *
 * Interface to get name of registered files.  This is mainly diagnostic
 * and the name passed could be transient unless there is something
 * ensuring that the file cannot be closed.
 *
 * PUBLIC: int __dbreg_get_name __P((DB_ENV *, u_int8_t *, char **));
 */
int
__dbreg_get_name(dbenv, fid, namep)
	DB_ENV *dbenv;
	u_int8_t *fid;
	char **namep;
{
	DB_LOG *dblp;
	FNAME *fnp;

	dblp = dbenv->lg_handle;

	if (dblp != NULL && __dbreg_fid_to_fname(dblp, fid, 0, &fnp) == 0) {
		*namep = R_ADDR(&dblp->reginfo, fnp->name_off);
		return (0);
	}

	return (-1);
}

/*
 * __dbreg_do_open --
 *	Open files referenced in the log.  This is the part of the open that
 * is not protected by the thread mutex.
 * PUBLIC: int __dbreg_do_open __P((DB_ENV *, DB_TXN *, DB_LOG *, u_int8_t *,
 * PUBLIC:     char *, DBTYPE, int32_t, db_pgno_t, void *, u_int32_t));
 */
int
__dbreg_do_open(dbenv,
    txn, lp, uid, name, ftype, ndx, meta_pgno, info, id)
	DB_ENV *dbenv;
	DB_TXN *txn;
	DB_LOG *lp;
	u_int8_t *uid;
	char *name;
	DBTYPE ftype;
	int32_t ndx;
	db_pgno_t meta_pgno;
	void *info;
	u_int32_t id;
{
	DB *dbp;
	u_int32_t cstat, ret_stat;
	int ret;

	if ((ret = db_create(&dbp, lp->dbenv, 0)) != 0)
		return (ret);

	/*
	 * We can open files under a number of different scenarios.
	 * First, we can open a file during a normal txn_abort, if that file
	 * was opened and closed during the transaction (as is the master
	 * database of a sub-database).
	 * Second, we might be aborting a transaction in XA and not have
	 * it open in the process that is actually doing the abort.
	 * Third, we might be in recovery.
	 * In case 3, there is no locking, so there is no issue.
	 * In cases 1 and 2, we are guaranteed to already hold any locks
	 * that we need, since we're still in the same transaction, so by
	 * setting DB_AM_RECOVER, we guarantee that we don't log and that
	 * we don't try to acquire locks on behalf of a different locker id.
	 */
	F_SET(dbp, DB_AM_RECOVER);
	if (meta_pgno != PGNO_BASE_MD) {
		memcpy(dbp->fileid, uid, DB_FILE_ID_LEN);
		dbp->meta_pgno = meta_pgno;
	}
	if ((ret = __db_open(dbp, txn, name, NULL,
	    ftype, DB_ODDFILESIZE, __db_omode("rw----"), meta_pgno)) == 0) {

		/*
		 * Verify that we are opening the same file that we were
		 * referring to when we wrote this log record.
		 */
		if ((meta_pgno != PGNO_BASE_MD &&
		    __dbreg_check_master(dbenv, uid, name) != 0) ||
		    memcmp(uid, dbp->fileid, DB_FILE_ID_LEN) != 0)
			cstat = TXN_IGNORE;
		else
			cstat = TXN_EXPECTED;

		/* Assign the specific dbreg id to this dbp. */
		if ((ret = __dbreg_assign_id(dbp, ndx)) != 0)
			goto err;

		/*
		 * If we successfully opened this file, then we need to
		 * convey that information to the txnlist so that we
		 * know how to handle the subtransaction that created
		 * the file system object.
		 */
		if (id != TXN_INVALID)
			ret = __db_txnlist_update(dbenv,
			    info, id, cstat, NULL, &ret_stat, 1);

err:		if (cstat == TXN_IGNORE)
			goto not_right;
		return (ret);
	} else if (ret == ENOENT) {
		/* Record that the open failed in the txnlist. */
		if (id != TXN_INVALID)
			ret = __db_txnlist_update(dbenv, info,
			    id, TXN_UNEXPECTED, NULL, &ret_stat, 1);
	}
not_right:
	(void)__db_close(dbp, NULL, DB_NOSYNC);
	/* Add this file as deleted. */
	(void)__dbreg_add_dbentry(dbenv, lp, NULL, ndx);
	return (ret);
}

static int
__dbreg_check_master(dbenv, uid, name)
	DB_ENV *dbenv;
	u_int8_t *uid;
	char *name;
{
	DB *dbp;
	int ret;

	ret = 0;
	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		return (ret);
	F_SET(dbp, DB_AM_RECOVER);
	ret = __db_open(dbp,
	    NULL, name, NULL, DB_BTREE, 0, __db_omode("rw----"), PGNO_BASE_MD);

	if (ret == 0 && memcmp(uid, dbp->fileid, DB_FILE_ID_LEN) != 0)
		ret = EINVAL;

	(void)__db_close(dbp, NULL, 0);
	return (ret);
}

/*
 * __dbreg_lazy_id --
 *	When a replication client gets upgraded to being a replication master,
 * it may have database handles open that have not been assigned an ID, but
 * which have become legal to use for logging.
 *
 *	This function lazily allocates a new ID for such a function, in a
 * new transaction created for the purpose.  We need to do this in a new
 * transaction because we definitely wish to commit the dbreg_register, but
 * at this point we have no way of knowing whether the log record that incited
 * us to call this will be part of a committed transaction.
 *
 * PUBLIC: int __dbreg_lazy_id __P((DB *));
 */
int
__dbreg_lazy_id(dbp)
	DB *dbp;
{
	DB_ENV *dbenv;
	DB_LOG *dblp;
	DB_TXN *txn;
	FNAME *fnp;
	LOG *lp;
	int32_t id;
	int ret;

	dbenv = dbp->dbenv;

	DB_ASSERT(IS_REP_MASTER(dbenv));

	dbenv = dbp->dbenv;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	fnp = dbp->log_filename;

	/* The fq_mutex protects the FNAME list and id management. */
	MUTEX_LOCK(dbenv, &lp->fq_mutex);
	if (fnp->id != DB_LOGFILEID_INVALID) {
		MUTEX_UNLOCK(dbenv, &lp->fq_mutex);
		return (0);
	}
	id = DB_LOGFILEID_INVALID;
	if ((ret = __txn_begin(dbenv, NULL, &txn, 0)) != 0)
		goto err;

	if ((ret = __dbreg_get_id(dbp, txn, &id)) != 0) {
		(void)__txn_abort(txn);
		goto err;
	}

	if ((ret = __txn_commit(txn, DB_TXN_NOSYNC)) != 0)
		goto err;

	/*
	 * All DB related logging routines check the id value *without*
	 * holding the fq_mutex to know whether we need to call
	 * dbreg_lazy_id to begin with.  We must set the ID after a
	 * *successful* commit so that there is no possibility of a second
	 * modification call finding a valid ID in the dbp before the
	 * dbreg_register and commit records are in the log.
	 * If there was an error, then we call __dbreg_revoke_id to
	 * remove the entry from the lists.
	 */
	fnp->id = id;
err:
	if (ret != 0 && id != DB_LOGFILEID_INVALID)
		(void)__dbreg_revoke_id(dbp, 1, id);
	MUTEX_UNLOCK(dbenv, &lp->fq_mutex);
	return (ret);
}
