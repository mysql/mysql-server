/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	The President and Fellows of Harvard University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: log_rec.c,v 11.48 2001/01/11 18:19:53 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_am.h"
#include "log.h"

static int __log_do_open __P((DB_ENV *, DB_LOG *,
    u_int8_t *, char *, DBTYPE, int32_t, db_pgno_t));
static int __log_open_file __P((DB_ENV *, DB_LOG *, __log_register_args *));

/*
 * PUBLIC: int __log_register_recover
 * PUBLIC:     __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__log_register_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	DB_ENTRY *dbe;
	DB_LOG *logp;
	DB *dbp;
	__log_register_args *argp;
	int do_rem, ret, t_ret;

	logp = dbenv->lg_handle;
	dbp = NULL;

#ifdef DEBUG_RECOVER
	REC_PRINT(__log_register_print);
#endif
	COMPQUIET(lsnp, NULL);

	if ((ret = __log_register_read(dbenv, dbtp->data, &argp)) != 0)
		goto out;

	if ((argp->opcode == LOG_OPEN &&
	    (DB_REDO(op) || op == DB_TXN_OPENFILES)) ||
	    (argp->opcode == LOG_CLOSE && DB_UNDO(op))) {
		/*
		 * If we are redoing an open or undoing a close, then we need
		 * to open a file.  We must open the file even if
		 * the meta page is not yet written as we may be creating it.
		 */
		if (op == DB_TXN_OPENFILES)
			F_SET(logp, DBLOG_FORCE_OPEN);
		ret = __log_open_file(dbenv, logp, argp);
		F_CLR(logp, DBLOG_FORCE_OPEN);
		if (ret == ENOENT || ret == EINVAL) {
			if (op == DB_TXN_OPENFILES && argp->name.size != 0 &&
			    (ret = __db_txnlist_delete(dbenv, info,
				argp->name.data, argp->fileid, 0)) != 0)
				goto out;
			ret = 0;
		}
	} else if (argp->opcode != LOG_CHECKPOINT) {
		/*
		 * If we are undoing an open, then we need to close the file.
		 *
		 * If the file is deleted, then we can just ignore this close.
		 * Otherwise, we should usually have a valid dbp we should
		 * close or whose reference count should be decremented.
		 * However, if we shut down without closing a file, we may, in
		 * fact, not have the file open, and that's OK.
		 */
		do_rem = 0;
		MUTEX_THREAD_LOCK(dbenv, logp->mutexp);
		if (argp->fileid < logp->dbentry_cnt) {
			dbe = &logp->dbentry[argp->fileid];

			DB_ASSERT(dbe->refcount == 1);

			ret = __db_txnlist_close(info,
			    argp->fileid, dbe->count);
			if ((dbp = TAILQ_FIRST(&dbe->dblist)) != NULL)
				(void)log_unregister(dbenv, dbp);
			do_rem = 1;
		}
		MUTEX_THREAD_UNLOCK(dbenv, logp->mutexp);
		if (do_rem) {
			(void)__log_rem_logid(logp, dbp, argp->fileid);
			/*
			 * If remove or rename has closed the file, don't
			 * sync.
			 */
			if (dbp != NULL &&
			    (t_ret = dbp->close(dbp,
			    dbp->mpf == NULL ? DB_NOSYNC : 0)) != 0 && ret == 0)
				ret = t_ret;
		}
	} else if (DB_UNDO(op) || op == DB_TXN_OPENFILES) {
		/*
		 * It's a checkpoint and we are rolling backward.  It
		 * is possible that the system was shut down and thus
		 * ended with a stable checkpoint; this file was never
		 * closed and has therefore not been reopened yet.  If
		 * so, we need to try to open it.
		 */
		ret = __log_open_file(dbenv, logp, argp);
		if (ret == ENOENT || ret == EINVAL) {
			if (argp->name.size != 0 && (ret =
			    __db_txnlist_delete(dbenv, info,
				argp->name.data, argp->fileid, 0)) != 0)
				goto out;
			ret = 0;
		}
	}

out:	if (argp != NULL)
		__os_free(argp, 0);
	return (ret);
}

/*
 * __log_open_file --
 *	Called during log_register recovery.  Make sure that we have an
 *	entry in the dbentry table for this ndx.  Returns 0 on success,
 *	non-zero on error.
 */
static int
__log_open_file(dbenv, lp, argp)
	DB_ENV *dbenv;
	DB_LOG *lp;
	__log_register_args *argp;
{
	DB_ENTRY *dbe;
	DB *dbp;

	/*
	 * We never re-open temporary files.  Temp files are only
	 * useful during aborts in which case the dbp was entered
	 * when the file was registered.  During recovery, we treat
	 * temp files as properly deleted files, allowing the open to
	 * fail and not reporting any errors when recovery fails to
	 * get a valid dbp from db_fileid_to_db.
	 */
	if (argp->name.size == 0) {
		(void)__log_add_logid(dbenv, lp, NULL, argp->fileid);
		return (ENOENT);
	}

	/*
	 * Because of reference counting, we cannot automatically close files
	 * during recovery, so when we're opening, we have to check that the
	 * name we are opening is what we expect.  If it's not, then we close
	 * the old file and open the new one.
	 */
	MUTEX_THREAD_LOCK(dbenv, lp->mutexp);
	if (argp->fileid < lp->dbentry_cnt)
		dbe = &lp->dbentry[argp->fileid];
	else
		dbe = NULL;

	if (dbe != NULL) {
		dbe->deleted = 0;
		if ((dbp = TAILQ_FIRST(&dbe->dblist)) != NULL) {
			if (dbp->meta_pgno != argp->meta_pgno ||
			    memcmp(dbp->fileid,
			    argp->uid.data, DB_FILE_ID_LEN) != 0) {
				MUTEX_THREAD_UNLOCK(dbenv, lp->mutexp);
				goto reopen;
			}
			if (!F_ISSET(lp, DBLOG_RECOVER))
				dbe->refcount++;
			MUTEX_THREAD_UNLOCK(dbenv, lp->mutexp);
			return (0);
		}
	}

	MUTEX_THREAD_UNLOCK(dbenv, lp->mutexp);
	if (0) {
reopen:		(void)log_unregister(dbp->dbenv, dbp);
		(void)__log_rem_logid(lp, dbp, argp->fileid);
		dbp->close(dbp, 0);
	}

	return (__log_do_open(dbenv, lp,
	    argp->uid.data, argp->name.data,
	    argp->ftype, argp->fileid, argp->meta_pgno));
}

/*
 * log_reopen_file -- close and reopen a db file.
 *	Must be called when a metadata page changes.
 *
 * PUBLIC: int __log_reopen_file __P((DB_ENV *,
 * PUBLIC:     char *, int32_t, u_int8_t *, db_pgno_t));
 *
 */
int
__log_reopen_file(dbenv, name, ndx, fileid, meta_pgno)
	DB_ENV *dbenv;
	char *name;
	int32_t ndx;
	u_int8_t *fileid;
	db_pgno_t meta_pgno;
{
	DB *dbp;
	DB_LOG *logp;
	DBTYPE ftype;
	FNAME *fnp;
	LOG *lp;
	char *tmp_name;
	int ret;

	logp = dbenv->lg_handle;

	if (name == NULL) {
		R_LOCK(dbenv, &logp->reginfo);

		lp = logp->reginfo.primary;

		for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
		    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
			if (fnp->ref == 0)      /* Entry not in use. */
				continue;
			if (memcmp(fnp->ufid, fileid, DB_FILE_ID_LEN) == 0)
				break;
		}

		if (fnp == 0 || fnp->name_off == INVALID_ROFF) {
			__db_err(dbenv,
			    "metasub recover: non-existent file id");
			return (EINVAL);
		}

		name = R_ADDR(&logp->reginfo, fnp->name_off);
		ret = __os_strdup(dbenv, name, &tmp_name);
		R_UNLOCK(dbenv, &logp->reginfo);
		if (ret != 0)
			goto out;
		name = tmp_name;
	} else
		tmp_name = NULL;

	if ((ret = __db_fileid_to_db(dbenv, &dbp, ndx, 0)) != 0)
		goto out;
	ftype = dbp->type;
	(void)log_unregister(dbenv, dbp);
	(void)__log_rem_logid(logp, dbp, ndx);
	(void)dbp->close(dbp, 0);

	ret = __log_do_open(dbenv, logp, fileid, name, ftype, ndx, meta_pgno);

	if (tmp_name != NULL)
		__os_free(tmp_name, 0);

out:	return (ret);
}

/*
 * __log_do_open --
 *	Open files referenced in the log.  This is the part of the open that
 * is not protected by the thread mutex.
 */
static int
__log_do_open(dbenv, lp, uid, name, ftype, ndx, meta_pgno)
	DB_ENV *dbenv;
	DB_LOG *lp;
	u_int8_t *uid;
	char *name;
	DBTYPE ftype;
	int32_t ndx;
	db_pgno_t meta_pgno;
{
	DB *dbp;
	int ret;
	u_int8_t zeroid[DB_FILE_ID_LEN];

	if ((ret = db_create(&dbp, lp->dbenv, 0)) != 0)
		return (ret);

	dbp->log_fileid = ndx;

	/*
	 * This is needed to signal to the locking routines called while
	 * opening databases that we are potentially undoing a transaction
	 * from an XA process.  Since the XA process does not share
	 * locks with the aborting transaction this prevents us from
	 * deadlocking during the open during rollback.
	 * Because this routine is called either during recovery or during an
	 * XA_ABORT, we can safely set DB_AM_RECOVER in the dbp since it
	 * will not be shared with other threads.
	 */
	F_SET(dbp, DB_AM_RECOVER);
	if (meta_pgno != PGNO_BASE_MD)
		memcpy(dbp->fileid, uid, DB_FILE_ID_LEN);
	dbp->type = ftype;
	if ((ret =
	    __db_dbopen(dbp, name, 0, __db_omode("rw----"), meta_pgno)) == 0) {
		/*
		 * Verify that we are opening the same file that we were
		 * referring to when we wrote this log record.
		 */
		if (memcmp(uid, dbp->fileid, DB_FILE_ID_LEN) != 0) {
			memset(zeroid, 0, DB_FILE_ID_LEN);
			if (memcmp(dbp->fileid, zeroid, DB_FILE_ID_LEN) != 0)
				goto not_right;
			memcpy(dbp->fileid, uid, DB_FILE_ID_LEN);
		}
		if (IS_RECOVERING(dbenv)) {
			(void)log_register(dbp->dbenv, dbp, name);
			(void)__log_add_logid(dbenv, lp, dbp, ndx);
		}
		return (0);
	}

not_right:
	(void)dbp->close(dbp, 0);
	(void)__log_add_logid(dbenv, lp, NULL, ndx);

	return (ENOENT);
}

/*
 * __log_add_logid --
 *	Adds a DB entry to the log's DB entry table.
 *
 * PUBLIC: int __log_add_logid __P((DB_ENV *, DB_LOG *, DB *, int32_t));
 */
int
__log_add_logid(dbenv, logp, dbp, ndx)
	DB_ENV *dbenv;
	DB_LOG *logp;
	DB *dbp;
	int32_t ndx;
{
	DB *dbtmp;
	int32_t i;
	int ret;

	ret = 0;

	MUTEX_THREAD_LOCK(dbenv, logp->mutexp);

	/*
	 * Check if we need to grow the table.  Note, ndx is 0-based (the
	 * index into the DB entry table) an dbentry_cnt is 1-based, the
	 * number of available slots.
	 */
	if (logp->dbentry_cnt <= ndx) {
		if ((ret = __os_realloc(dbenv,
		    (ndx + DB_GROW_SIZE) * sizeof(DB_ENTRY),
		    NULL, &logp->dbentry)) != 0)
			goto err;

		/*
		 * We have moved the head of the queue.
		 * Fix up the queue header of an empty queue or the previous
		 * pointer of the first element.
		 */
		for (i = 0; i < logp->dbentry_cnt; i++) {
			if ((dbtmp =
			    TAILQ_FIRST(&logp->dbentry[i].dblist)) == NULL)
				TAILQ_INIT(&logp->dbentry[i].dblist);
			else
				TAILQ_REINSERT_HEAD(
				    &logp->dbentry[i].dblist, dbp, links);
		}

		/* Initialize the new entries. */
		for (i = logp->dbentry_cnt; i < ndx + DB_GROW_SIZE; i++) {
			logp->dbentry[i].count = 0;
			TAILQ_INIT(&logp->dbentry[i].dblist);
			logp->dbentry[i].deleted = 0;
			logp->dbentry[i].refcount = 0;
		}

		logp->dbentry_cnt = i;
	}

	if (logp->dbentry[ndx].deleted == 0 &&
	    TAILQ_FIRST(&logp->dbentry[ndx].dblist) == NULL) {
		logp->dbentry[ndx].count = 0;
		if (dbp != NULL)
			TAILQ_INSERT_HEAD(&logp->dbentry[ndx].dblist,
			    dbp, links);
		logp->dbentry[ndx].deleted = dbp == NULL;
		logp->dbentry[ndx].refcount = 1;
	} else if (!F_ISSET(logp, DBLOG_RECOVER)) {
		if (dbp != NULL)
			TAILQ_INSERT_HEAD(&logp->dbentry[ndx].dblist,
			    dbp, links);
		logp->dbentry[ndx].refcount++;
	}

err:	MUTEX_THREAD_UNLOCK(dbenv, logp->mutexp);
	return (ret);
}

/*
 * __db_fileid_to_db --
 *	Return the DB corresponding to the specified fileid.
 *
 * PUBLIC: int __db_fileid_to_db __P((DB_ENV *, DB **, int32_t, int));
 */
int
__db_fileid_to_db(dbenv, dbpp, ndx, inc)
	DB_ENV *dbenv;
	DB **dbpp;
	int32_t ndx;
	int inc;
{
	DB_LOG *logp;
	DB *dbp;
	FNAME *fname;
	int ret;
	char *name;

	ret = 0;
	logp = dbenv->lg_handle;

	MUTEX_THREAD_LOCK(dbenv, logp->mutexp);

	/*
	 * Under XA, a process different than the one issuing DB operations
	 * may abort a transaction.  In this case, recovery routines are run
	 * by a process that does not necessarily have the file open, so we
	 * we must open the file explicitly.
	 */
	if (ndx >= logp->dbentry_cnt ||
	    (!logp->dbentry[ndx].deleted &&
	    (dbp = TAILQ_FIRST(&logp->dbentry[ndx].dblist)) == NULL)) {
		if (F_ISSET(logp, DBLOG_RECOVER)) {
			ret = ENOENT;
			goto err;
		}
		if (__log_lid_to_fname(logp, ndx, &fname) != 0) {
			/* Couldn't find entry; this is a fatal error. */
			__db_err(dbenv, "Missing log fileid entry");
			ret = EINVAL;
			goto err;
		}
		name = R_ADDR(&logp->reginfo, fname->name_off);

		/*
		 * __log_do_open is called without protection of the
		 * log thread lock.
		 */
		MUTEX_THREAD_UNLOCK(dbenv, logp->mutexp);

		/*
		 * At this point, we are not holding the thread lock, so exit
		 * directly instead of going through the exit code at the
		 * bottom.  If the __log_do_open succeeded, then we don't need
		 * to do any of the remaining error checking at the end of this
		 * routine.
		 */
		if ((ret = __log_do_open(dbenv, logp,
		    fname->ufid, name, fname->s_type,
		    ndx, fname->meta_pgno)) != 0)
			return (ret);

		*dbpp = TAILQ_FIRST(&logp->dbentry[ndx].dblist);
		return (0);
	}

	/*
	 * Return DB_DELETED if the file has been deleted (it's not an error).
	 */
	if (logp->dbentry[ndx].deleted) {
		ret = DB_DELETED;
		if (inc)
			logp->dbentry[ndx].count++;
		goto err;
	}

	/*
	 * Otherwise return 0, but if we don't have a corresponding DB, it's
	 * an error.
	 */
	if ((*dbpp = TAILQ_FIRST(&logp->dbentry[ndx].dblist)) == NULL)
		ret = ENOENT;

err:	MUTEX_THREAD_UNLOCK(dbenv, logp->mutexp);
	return (ret);
}

/*
 * __log_close_files --
 *	Close files that were opened by the recovery daemon.  We sync the
 *	file, unless its mpf pointer has been NULLed by a db_remove or
 *	db_rename.  We may not have flushed the log_register record that
 *	closes the file.
 *
 * PUBLIC: void __log_close_files __P((DB_ENV *));
 */
void
__log_close_files(dbenv)
	DB_ENV *dbenv;
{
	DB_ENTRY *dbe;
	DB_LOG *logp;
	DB *dbp;
	int32_t i;

	logp = dbenv->lg_handle;
	MUTEX_THREAD_LOCK(dbenv, logp->mutexp);
	for (i = 0; i < logp->dbentry_cnt; i++) {
		dbe = &logp->dbentry[i];
		while ((dbp = TAILQ_FIRST(&dbe->dblist)) != NULL) {
			(void)log_unregister(dbenv, dbp);
			TAILQ_REMOVE(&dbe->dblist, dbp, links);
			(void)dbp->close(dbp, dbp->mpf == NULL ? DB_NOSYNC : 0);
		}
		dbe->deleted = 0;
		dbe->refcount = 0;
	}
	MUTEX_THREAD_UNLOCK(dbenv, logp->mutexp);
}

/*
 * __log_rem_logid
 *	Remove an entry from the log table.  Find the appropriate DB and
 * unlink it from the linked list off the table.  If the DB is NULL, treat
 * this as a simple refcount decrement.
 *
 * PUBLIC: void __log_rem_logid __P((DB_LOG *, DB *, int32_t));
 */
void
__log_rem_logid(logp, dbp, ndx)
	DB_LOG *logp;
	DB *dbp;
	int32_t ndx;
{
	DB *xdbp;

	MUTEX_THREAD_LOCK(logp->dbenv, logp->mutexp);
	if (--logp->dbentry[ndx].refcount == 0) {
		TAILQ_INIT(&logp->dbentry[ndx].dblist);
		logp->dbentry[ndx].deleted = 0;
	} else if (dbp != NULL)
		for (xdbp = TAILQ_FIRST(&logp->dbentry[ndx].dblist);
		    xdbp != NULL;
		    xdbp = TAILQ_NEXT(xdbp, links))
			if (xdbp == dbp) {
				TAILQ_REMOVE(&logp->dbentry[ndx].dblist,
				    xdbp, links);
				break;
			}

	MUTEX_THREAD_UNLOCK(logp->dbenv, logp->mutexp);
}

/*
 * __log_lid_to_fname --
 *	Traverse the shared-memory region looking for the entry that
 *	matches the passed log fileid.  Returns 0 on success; -1 on error.
 * PUBLIC: int __log_lid_to_fname __P((DB_LOG *, int32_t, FNAME **));
 */
int
__log_lid_to_fname(dblp, lid, fnamep)
	DB_LOG *dblp;
	int32_t lid;
	FNAME **fnamep;
{
	FNAME *fnp;
	LOG *lp;

	lp = dblp->reginfo.primary;

	for (fnp = SH_TAILQ_FIRST(&lp->fq, __fname);
	    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
		if (fnp->ref == 0)	/* Entry not in use. */
			continue;
		if (fnp->id == lid) {
			*fnamep = fnp;
			return (0);
		}
	}
	return (-1);
}
