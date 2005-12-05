/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2004-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_verify.c,v 12.21 2005/10/19 19:06:37 sue Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __rep_dorecovery __P((DB_ENV *, DB_LSN *, DB_LSN *));

/*
 * __rep_verify --
 *	Handle a REP_VERIFY message.
 *
 * PUBLIC: int __rep_verify __P((DB_ENV *, REP_CONTROL *, DBT *, int, time_t));
 */
int
__rep_verify(dbenv, rp, rec, eid, savetime)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	int eid;
	time_t savetime;
{
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN lsn;
	DB_REP *db_rep;
	DBT mylog;
	LOG *lp;
	REP *rep;
	u_int32_t rectype;
	int match, ret, t_ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	if (IS_ZERO_LSN(lp->verify_lsn))
		return (ret);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);
	memset(&mylog, 0, sizeof(mylog));
	if ((ret = __log_c_get(logc, &rp->lsn, &mylog, DB_SET)) != 0)
		goto err;;
	match = 0;
	memcpy(&rectype, mylog.data, sizeof(rectype));
	if (mylog.size == rec->size &&
	    memcmp(mylog.data, rec->data, rec->size) == 0)
		match = 1;
	/*
	 * If we don't have a match, backup to the previous
	 * identification record and try again.
	 */
	if (match == 0) {
		ZERO_LSN(lsn);
		if ((ret = __rep_log_backup(logc, &lsn)) == 0) {
			MUTEX_LOCK(dbenv, rep->mtx_clientdb);
			lp->verify_lsn = lsn;
			lp->rcvd_recs = 0;
			lp->wait_recs = rep->request_gap;
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			(void)__rep_send_message(dbenv, eid, REP_VERIFY_REQ,
			    &lsn, NULL, 0, DB_REP_ANYWHERE);
		} else if (ret == DB_NOTFOUND) {
			/*
			 * We've either run out of records because
			 * logs have been removed or we've rolled back
			 * all the way to the beginning.  In the latter
			 * we don't think these sites were ever part of
			 * the same environment and we'll say so.
			 * In the former, request internal backup.
			 */
			if (rp->lsn.file == 1) {
				__db_err(dbenv,
		"Client was never part of master's environment");
				ret = DB_REP_JOIN_FAILURE;
			} else {
				rep->stat.st_outdated++;

				LOG_SYSTEM_LOCK(dbenv);
				lsn = lp->lsn;
				LOG_SYSTEM_UNLOCK(dbenv);
				REP_SYSTEM_LOCK(dbenv);
				F_CLR(rep, REP_F_RECOVER_VERIFY);
				if (FLD_ISSET(rep->config, REP_C_NOAUTOINIT))
					ret = DB_REP_JOIN_FAILURE;
				else {
					F_SET(rep, REP_F_RECOVER_UPDATE);
					ZERO_LSN(rep->first_lsn);
				}
				REP_SYSTEM_UNLOCK(dbenv);
				if (ret == 0)
					(void)__rep_send_message(dbenv,
					    eid, REP_UPDATE_REQ, NULL,
					    NULL, 0, DB_REP_ANYWHERE);
			}
		}
	} else
		ret = __rep_verify_match(dbenv, &rp->lsn, savetime);

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_verify_fail --
 *	Handle a REP_VERIFY_FAIL message.
 *
 * PUBLIC: int __rep_verify_fail __P((DB_ENV *, REP_CONTROL *, int));
 */
int
__rep_verify_fail(dbenv, rp, eid)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	int eid;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	int ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	/*
	 * If any recovery flags are set, but not VERIFY,
	 * then we ignore this message.  We are already
	 * in the middle of updating.
	 */
	if (F_ISSET(rep, REP_F_RECOVER_MASK) &&
	    !F_ISSET(rep, REP_F_RECOVER_VERIFY))
		return (0);
	rep->stat.st_outdated++;

	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	REP_SYSTEM_LOCK(dbenv);
	/*
	 * We don't want an old or delayed VERIFY_FAIL
	 * message to throw us into internal initialization
	 * when we shouldn't be.
	 *
	 * Only go into internal initialization if:
	 * We are set for AUTOINIT mode.
	 * We are in RECOVER_VERIFY and this LSN == verify_lsn.
	 * We are not in any RECOVERY and we are expecting
	 *    an LSN that no longer exists on the master.
	 * Otherwise, ignore this message.
	 */
	if (FLD_ISSET(rep->config, REP_C_NOAUTOINIT) &&
	    ((F_ISSET(rep, REP_F_RECOVER_VERIFY) &&
	    log_compare(&rp->lsn, &lp->verify_lsn) == 0) ||
	    (F_ISSET(rep, REP_F_RECOVER_MASK) == 0 &&
	    log_compare(&rp->lsn, &lp->ready_lsn) >= 0))) {
		ret = DB_REP_JOIN_FAILURE;
		goto unlock;
	}
	if (((F_ISSET(rep, REP_F_RECOVER_VERIFY)) &&
	    log_compare(&rp->lsn, &lp->verify_lsn) == 0) ||
	    (F_ISSET(rep, REP_F_RECOVER_MASK) == 0 &&
	    log_compare(&rp->lsn, &lp->ready_lsn) >= 0)) {
		F_CLR(rep, REP_F_RECOVER_VERIFY);
		F_SET(rep, REP_F_RECOVER_UPDATE);
		ZERO_LSN(rep->first_lsn);
		lp->wait_recs = rep->request_gap;
		REP_SYSTEM_UNLOCK(dbenv);
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		(void)__rep_send_message(dbenv,
		    eid, REP_UPDATE_REQ, NULL, NULL, 0, 0);
	} else {
unlock:		REP_SYSTEM_UNLOCK(dbenv);
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	}
	return (ret);
}

/*
 * __rep_verify_req --
 *	Handle a REP_VERIFY_REQ message.
 *
 * PUBLIC: int __rep_verify_req __P((DB_ENV *, REP_CONTROL *, int));
 */
int
__rep_verify_req(dbenv, rp, eid)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	int eid;
{
	DB_LOGC *logc;
	DB_REP *db_rep;
	DBT *d, data_dbt;
	REP *rep;
	u_int32_t type;
	int old, ret;

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	type = REP_VERIFY;
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);
	d = &data_dbt;
	memset(d, 0, sizeof(data_dbt));
	F_SET(logc, DB_LOG_SILENT_ERR);
	ret = __log_c_get(logc, &rp->lsn, d, DB_SET);
	/*
	 * If the LSN was invalid, then we might get a not
	 * found, we might get an EIO, we could get anything.
	 * If we get a DB_NOTFOUND, then there is a chance that
	 * the LSN comes before the first file present in which
	 * case we need to return a fail so that the client can return
	 * a DB_OUTDATED.
	 *
	 * If we're a client servicing this request and we get a
	 * NOTFOUND, return it so the caller can rerequest from
	 * a better source.
	 */
	if (ret == DB_NOTFOUND) {
		if (F_ISSET(rep, REP_F_CLIENT))
			goto notfound;
		else if (__log_is_outdated(dbenv, rp->lsn.file, &old) == 0 &&
		    old != 0)
			type = REP_VERIFY_FAIL;
	}

	if (ret != 0)
		d = NULL;

	(void)__rep_send_message(dbenv, eid, type, &rp->lsn, d, 0, 0);
notfound:
	ret = __log_c_close(logc);
	return (ret);
}

static int
__rep_dorecovery(dbenv, lsnp, trunclsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp, *trunclsnp;
{
	DB_LSN lsn;
	DB_REP *db_rep;
	DBT mylog;
	DB_LOGC *logc;
	int ret, t_ret, update;
	u_int32_t rectype;
	__txn_regop_args *txnrec;

	db_rep = dbenv->rep_handle;

	/* Figure out if we are backing out any committed transactions. */
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	memset(&mylog, 0, sizeof(mylog));
	update = 0;
	while (update == 0 &&
	    (ret = __log_c_get(logc, &lsn, &mylog, DB_PREV)) == 0 &&
	    log_compare(&lsn, lsnp) > 0) {
		memcpy(&rectype, mylog.data, sizeof(rectype));
		if (rectype == DB___txn_regop) {
			if ((ret =
			    __txn_regop_read(dbenv, mylog.data, &txnrec)) != 0)
				goto err;
			if (txnrec->opcode != TXN_ABORT)
				update = 1;
			__os_free(dbenv, txnrec);
		}
	}

	/*
	 * If we successfully run recovery, we've opened all the necessary
	 * files.  We are guaranteed to be single-threaded here, so no mutex
	 * is necessary.
	 */
	if ((ret = __db_apprec(dbenv, lsnp, trunclsnp, update, 0)) == 0)
		F_SET(db_rep, DBREP_OPENFILES);

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __rep_verify_match --
 *	We have just received a matching log record during verification.
 * Figure out if we're going to need to run recovery. If so, wait until
 * everything else has exited the library.  If not, set up the world
 * correctly and move forward.
 *
 * PUBLIC: int __rep_verify_match __P((DB_ENV *, DB_LSN *, time_t));
 */
int
__rep_verify_match(dbenv, reclsnp, savetime)
	DB_ENV *dbenv;
	DB_LSN *reclsnp;
	time_t savetime;
{
	DB_LOG *dblp;
	DB_LSN trunclsn;
	DB_REP *db_rep;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	int done, master, ret;
	u_int32_t unused;

	dblp = dbenv->lg_handle;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	lp = dblp->reginfo.primary;
	ret = 0;
	infop = dbenv->reginfo;
	renv = infop->primary;

	/*
	 * Check if the savetime is different than our current time stamp.
	 * If it is, then we're racing with another thread trying to recover
	 * and we lost.  We must give up.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	done = savetime != renv->rep_timestamp;
	if (done) {
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		return (0);
	}
	ZERO_LSN(lp->verify_lsn);
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);

	/*
	 * Make sure the world hasn't changed while we tried to get
	 * the lock.  If it hasn't then it's time for us to kick all
	 * operations out of DB and run recovery.
	 */
	REP_SYSTEM_LOCK(dbenv);
	if (!F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    (F_ISSET(rep, REP_F_READY) || rep->in_recovery != 0)) {
		rep->stat.st_msgs_recover++;
		goto errunlock;
	}

	if ((ret = __rep_lockout(dbenv, rep, 1)) != 0)
		goto errunlock;

	/* OK, everyone is out, we can now run recovery. */
	REP_SYSTEM_UNLOCK(dbenv);

	if ((ret = __rep_dorecovery(dbenv, reclsnp, &trunclsn)) != 0) {
		REP_SYSTEM_LOCK(dbenv);
		rep->in_recovery = 0;
		F_CLR(rep, REP_F_READY);
		goto errunlock;
	}

	/*
	 * The log has been truncated (either directly by us or by __db_apprec)
	 * We want to make sure we're waiting for the LSN at the new end-of-log,
	 * not some later point.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lp->ready_lsn = trunclsn;
	ZERO_LSN(lp->waiting_lsn);
	ZERO_LSN(lp->max_wait_lsn);
	lp->max_perm_lsn = *reclsnp;
	lp->wait_recs = 0;
	lp->rcvd_recs = 0;
	ZERO_LSN(lp->verify_lsn);

	/*
	 * Discard any log records we have queued;  we're about to re-request
	 * them, and can't trust the ones in the queue.  We need to set the
	 * DB_AM_RECOVER bit in this handle, so that the operation doesn't
	 * deadlock.
	 */
	F_SET(db_rep->rep_db, DB_AM_RECOVER);
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	ret = __db_truncate(db_rep->rep_db, NULL, &unused);
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	F_CLR(db_rep->rep_db, DB_AM_RECOVER);

	REP_SYSTEM_LOCK(dbenv);
	rep->stat.st_log_queued = 0;
	rep->in_recovery = 0;
	F_CLR(rep, REP_F_NOARCHIVE | REP_F_RECOVER_MASK);

	if (ret != 0)
		goto errunlock2;

	/*
	 * If the master_id is invalid, this means that since
	 * the last record was sent, somebody declared an
	 * election and we may not have a master to request
	 * things of.
	 *
	 * This is not an error;  when we find a new master,
	 * we'll re-negotiate where the end of the log is and
	 * try to bring ourselves up to date again anyway.
	 *
	 * !!!
	 * We cannot assert the election flags though because
	 * somebody may have declared an election and then
	 * got an error, thus clearing the election flags
	 * but we still have an invalid master_id.
	 */
	master = rep->master_id;
	REP_SYSTEM_UNLOCK(dbenv);
	if (master == DB_EID_INVALID) {
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		ret = 0;
	} else {
		/*
		 * We're making an ALL_REQ.  But now that we've
		 * cleared the flags, we're likely receiving new
		 * log records from the master, resulting in a gap
		 * immediately.  So to avoid multiple data streams,
		 * set the wait_recs value high now to give the master
		 * a chance to start sending us these records before
		 * the gap code re-requests the same gap.  Wait_recs
		 * will get reset once we start receiving these
		 * records.
		 */
		lp->wait_recs = rep->max_gap;
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		(void)__rep_send_message(dbenv,
		    master, REP_ALL_REQ, reclsnp, NULL, 0, DB_REP_ANYWHERE);
	}
	if (0) {
errunlock2:	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
errunlock:	REP_SYSTEM_UNLOCK(dbenv);
	}
	return (ret);
}

/*
 * __rep_log_backup --
 *
 * In the verify handshake, we walk backward looking for
 * identification records.  Those are the only record types
 * we verify and match on.
 *
 * PUBLIC: int __rep_log_backup __P((DB_LOGC *, DB_LSN *));
 */
int
__rep_log_backup(logc, lsn)
	DB_LOGC *logc;
	DB_LSN *lsn;
{
	DBT mylog;
	u_int32_t rectype;
	int ret;

	ret = 0;
	memset(&mylog, 0, sizeof(mylog));
	while ((ret = __log_c_get(logc, lsn, &mylog, DB_PREV)) == 0) {
		/*
		 * Look at the record type.  Only txn_regop and txn_ckp
		 * are interesting to us.
		 */
		memcpy(&rectype, mylog.data, sizeof(rectype));
		if (rectype == DB___txn_ckp || rectype == DB___txn_regop)
			break;
	}
	return (ret);
}
