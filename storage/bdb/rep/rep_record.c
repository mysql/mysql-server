/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_record.c,v 12.25 2005/10/20 18:57:13 bostic Exp $
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
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __rep_collect_txn __P((DB_ENV *, DB_LSN *, LSN_COLLECTION *));
static int __rep_do_ckp __P((DB_ENV *, DBT *, REP_CONTROL *));
static int __rep_getnext __P((DB_ENV *));
static int __rep_lsn_cmp __P((const void *, const void *));
static int __rep_newfile __P((DB_ENV *, REP_CONTROL *, DB_LSN *));
static int __rep_process_rec __P((DB_ENV *,
    REP_CONTROL *, DBT *, u_int32_t *, DB_LSN *));
static int __rep_remfirst __P((DB_ENV *, DBT *, DBT *));
static int __rep_resend_req __P((DB_ENV *, int));
static int __rep_skip_msg __P((DB_ENV *, REP *, int, u_int32_t));

/* Used to consistently designate which messages ought to be received where. */

#define	MASTER_ONLY(rep, rp) do {					\
	if (!F_ISSET(rep, REP_F_MASTER)) {				\
		RPRINT(dbenv, rep,					\
		(dbenv, &mb, "Master record received on client"));	\
		REP_PRINT_MESSAGE(dbenv,				\
		    *eidp, rp, "rep_process_message");			\
		ret = EINVAL;						\
		goto errlock;						\
	}								\
} while (0)

#define	CLIENT_ONLY(rep, rp) do {					\
	if (!F_ISSET(rep, REP_F_CLIENT)) {				\
		RPRINT(dbenv, rep,					\
		    (dbenv, &mb, "Client record received on master"));	\
		REP_PRINT_MESSAGE(dbenv,				\
		    *eidp, rp, "rep_process_message");			\
		(void)__rep_send_message(dbenv,				\
		    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0, 0);	\
		ret = DB_REP_DUPMASTER;					\
		goto errlock;						\
	}								\
} while (0)

/*
 * If a client is attempting to service a request it does not have,
 * call rep_skip_msg to skip this message and force a rerequest to the
 * sender.  We don't hold the mutex for the stats and may miscount.
 */
#define	CLIENT_REREQ do {						\
	if (F_ISSET(rep, REP_F_CLIENT)) {				\
		rep->stat.st_client_svc_req++;				\
		if (ret == DB_NOTFOUND) {				\
			rep->stat.st_client_svc_miss++;			\
			ret = __rep_skip_msg(dbenv, rep, *eidp, rp->rectype);\
		}							\
	}								\
} while (0)

#define	MASTER_UPDATE(dbenv, renv) do {					\
	REP_SYSTEM_LOCK(dbenv);						\
	F_SET((renv), DB_REGENV_REPLOCKED);				\
	(void)time(&(renv)->op_timestamp);				\
	REP_SYSTEM_UNLOCK(dbenv);					\
} while (0)

#define	RECOVERING_SKIP do {						\
	if (recovering) {						\
		/* Not holding region mutex, may miscount */		\
		rep->stat.st_msgs_recover++;				\
		ret = __rep_skip_msg(dbenv, rep, *eidp, rp->rectype);	\
		goto errlock;						\
	}								\
} while (0)

/*
 * If we're recovering the log we only want log records that are in the
 * range we need to recover.  Otherwise we can end up storing a huge
 * number of "new" records, only to truncate the temp database later after
 * we run recovery.  If we are actively delaying a sync-up, we also skip
 * all incoming log records until the application requests sync-up.
 */
#define	RECOVERING_LOG_SKIP do {					\
	if (F_ISSET(rep, REP_F_DELAY) ||				\
	    (recovering &&						\
	    (!F_ISSET(rep, REP_F_RECOVER_LOG) ||			\
	     log_compare(&rp->lsn, &rep->last_lsn) > 0))) {		\
		/* Not holding region mutex, may miscount */		\
		rep->stat.st_msgs_recover++;				\
		ret = __rep_skip_msg(dbenv, rep, *eidp, rp->rectype);	\
		goto errlock;						\
	}								\
} while (0)

#define	ANYSITE(rep)

/*
 * __rep_process_message --
 *
 * This routine takes an incoming message and processes it.
 *
 * control: contains the control fields from the record
 * rec: contains the actual record
 * eidp: contains the machine id of the sender of the message;
 *	in the case of a DB_NEWMASTER message, returns the eid
 *	of the new master.
 * ret_lsnp: On DB_REP_ISPERM and DB_REP_NOTPERM returns, contains the
 *	lsn of the maximum permanent or current not permanent log record
 *	(respectively).
 *
 * PUBLIC: int __rep_process_message __P((DB_ENV *, DBT *, DBT *, int *,
 * PUBLIC:     DB_LSN *));
 */
int
__rep_process_message(dbenv, control, rec, eidp, ret_lsnp)
	DB_ENV *dbenv;
	DBT *control, *rec;
	int *eidp;
	DB_LSN *ret_lsnp;
{
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_REP *db_rep;
	DBT data_dbt;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	REP_CONTROL *rp;
	u_int32_t egen, gen;
	int cmp, recovering, ret;
	time_t savetime;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_process_message",
	    DB_INIT_REP);

	/* Control argument must be non-Null. */
	if (control == NULL || control->size == 0) {
		__db_err(dbenv,
	"DB_ENV->rep_process_message: control argument must be specified");
		return (EINVAL);
	}

	if (!IS_REP_MASTER(dbenv) && !IS_REP_CLIENT(dbenv)) {
		__db_err(dbenv,
	"Environment not configured as replication master or client");
		return (EINVAL);
	}

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	infop = dbenv->reginfo;
	renv = infop->primary;
	rp = (REP_CONTROL *)control->data;
	if (ret_lsnp != NULL)
		ZERO_LSN(*ret_lsnp);

	/*
	 * Acquire the replication lock.
	 */
	REP_SYSTEM_LOCK(dbenv);
	if (rep->start_th != 0) {
		/*
		 * If we're racing with a thread in rep_start, then
		 * just ignore the message and return.
		 */
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Racing rep_start, ignore message."));
		if (F_ISSET(rp, DB_LOG_PERM))
			ret = DB_REP_IGNORE;
		REP_SYSTEM_UNLOCK(dbenv);
		goto out;
	}
	rep->msg_th++;
	gen = rep->gen;
	recovering = rep->in_recovery || F_ISSET(rep, REP_F_RECOVER_MASK);
	savetime = renv->rep_timestamp;

	rep->stat.st_msgs_processed++;
	REP_SYSTEM_UNLOCK(dbenv);

	REP_PRINT_MESSAGE(dbenv, *eidp, rp, "rep_process_message");

	/* Complain if we see an improper version number. */
	if (rp->rep_version != DB_REPVERSION) {
		__db_err(dbenv,
		    "unexpected replication message version %lu, expected %d",
		    (u_long)rp->rep_version, DB_REPVERSION);
		ret = EINVAL;
		goto errlock;
	}
	if (rp->log_version != DB_LOGVERSION) {
		__db_err(dbenv,
		    "unexpected log record version %lu, expected %d",
		    (u_long)rp->log_version, DB_LOGVERSION);
		ret = EINVAL;
		goto errlock;
	}

	/*
	 * Check for generation number matching.  Ignore any old messages
	 * except requests that are indicative of a new client that needs
	 * to get in sync.
	 */
	if (rp->gen < gen && rp->rectype != REP_ALIVE_REQ &&
	    rp->rectype != REP_NEWCLIENT && rp->rectype != REP_MASTER_REQ &&
	    rp->rectype != REP_DUPMASTER) {
		/*
		 * We don't hold the rep mutex, and could miscount if we race.
		 */
		rep->stat.st_msgs_badgen++;
		if (F_ISSET(rp, DB_LOG_PERM))
			ret = DB_REP_IGNORE;
		goto errlock;
	}

	if (rp->gen > gen) {
		/*
		 * If I am a master and am out of date with a lower generation
		 * number, I am in bad shape and should downgrade.
		 */
		if (F_ISSET(rep, REP_F_MASTER)) {
			rep->stat.st_dupmasters++;
			ret = DB_REP_DUPMASTER;
			if (rp->rectype != REP_DUPMASTER)
				(void)__rep_send_message(dbenv,
				    DB_EID_BROADCAST, REP_DUPMASTER,
				    NULL, NULL, 0, 0);
			goto errlock;
		}

		/*
		 * I am a client and am out of date.  If this is an election,
		 * or a response from the first site I contacted, then I can
		 * accept the generation number and participate in future
		 * elections and communication. Otherwise, I need to hear about
		 * a new master and sync up.
		 */
		if (rp->rectype == REP_ALIVE ||
		    rp->rectype == REP_VOTE1 || rp->rectype == REP_VOTE2) {
			REP_SYSTEM_LOCK(dbenv);
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Updating gen from %lu to %lu",
			    (u_long)gen, (u_long)rp->gen));
			rep->master_id = DB_EID_INVALID;
			gen = rep->gen = rp->gen;
			/*
			 * Updating of egen will happen when we process the
			 * message below for each message type.
			 */
			REP_SYSTEM_UNLOCK(dbenv);
			if (rp->rectype == REP_ALIVE)
				(void)__rep_send_message(dbenv,
				    DB_EID_BROADCAST, REP_MASTER_REQ, NULL,
				    NULL, 0, 0);
		} else if (rp->rectype != REP_NEWMASTER) {
			/*
			 * Ignore this message, retransmit if needed.
			 */
			if (__rep_check_doreq(dbenv, rep))
				(void)__rep_send_message(dbenv,
				    DB_EID_BROADCAST, REP_MASTER_REQ,
				    NULL, NULL, 0, 0);
			goto errlock;
		}
		/*
		 * If you get here, then you're a client and either you're
		 * in an election or you have a NEWMASTER or an ALIVE message
		 * whose processing will do the right thing below.
		 */
	}

	/*
	 * We need to check if we're in recovery and if we are
	 * then we need to ignore any messages except VERIFY*, VOTE*,
	 * NEW* and ALIVE_REQ, or backup related messages: UPDATE*,
	 * PAGE* and FILE*.  We need to also accept LOG messages
	 * if we're copying the log for recovery/backup.
	 */
	switch (rp->rectype) {
	case REP_ALIVE:
		/*
		 * Handle even if we're recovering.
		 */
		ANYSITE(rep);
		egen = *(u_int32_t *)rec->data;
		REP_SYSTEM_LOCK(dbenv);
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Received ALIVE egen of %lu, mine %lu",
		    (u_long)egen, (u_long)rep->egen));
		if (egen > rep->egen) {
			/*
			 * We're changing egen, need to clear out any old
			 * election information.
			 */
			__rep_elect_done(dbenv, rep);
			rep->egen = egen;
		}
		REP_SYSTEM_UNLOCK(dbenv);
		break;
	case REP_ALIVE_REQ:
		/*
		 * Handle even if we're recovering.
		 */
		ANYSITE(rep);
		dblp = dbenv->lg_handle;
		LOG_SYSTEM_LOCK(dbenv);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		LOG_SYSTEM_UNLOCK(dbenv);
		REP_SYSTEM_LOCK(dbenv);
		egen = rep->egen;
		REP_SYSTEM_UNLOCK(dbenv);
		data_dbt.data = &egen;
		data_dbt.size = sizeof(egen);
		(void)__rep_send_message(dbenv,
		    *eidp, REP_ALIVE, &lsn, &data_dbt, 0, 0);
		break;
	case REP_ALL_REQ:
		RECOVERING_SKIP;
		ret = __rep_allreq(dbenv, rp, *eidp);
		CLIENT_REREQ;
		break;
	case REP_BULK_LOG:
		RECOVERING_LOG_SKIP;
		CLIENT_ONLY(rep, rp);
		ret = __rep_bulk_log(dbenv, rp, rec, savetime, ret_lsnp);
		break;
	case REP_BULK_PAGE:
		/*
		 * Handle even if we're recovering.
		 */
		CLIENT_ONLY(rep, rp);
		ret = __rep_bulk_page(dbenv, *eidp, rp, rec);
		break;
	case REP_DUPMASTER:
		/*
		 * Handle even if we're recovering.
		 */
		if (F_ISSET(rep, REP_F_MASTER))
			ret = DB_REP_DUPMASTER;
		break;
#ifdef NOTYET
	case REP_FILE: /* TODO */
		CLIENT_ONLY(rep, rp);
		break;
	case REP_FILE_REQ:
		ret = __rep_send_file(dbenv, rec, *eidp);
		break;
#endif
	case REP_FILE_FAIL:
		/*
		 * Handle even if we're recovering.
		 */
		CLIENT_ONLY(rep, rp);
		/*
		 * XXX
		 */
		break;
	case REP_LOG:
	case REP_LOG_MORE:
		RECOVERING_LOG_SKIP;
		CLIENT_ONLY(rep, rp);
		ret = __rep_log(dbenv, rp, rec, savetime, ret_lsnp);
		break;
	case REP_LOG_REQ:
		RECOVERING_SKIP;
		ret = __rep_logreq(dbenv, rp, rec, *eidp);
		CLIENT_REREQ;
		break;
	case REP_NEWSITE:
		/*
		 * Handle even if we're recovering.
		 */
		/* We don't hold the rep mutex, and may miscount. */
		rep->stat.st_newsites++;

		/* This is a rebroadcast; simply tell the application. */
		if (F_ISSET(rep, REP_F_MASTER)) {
			dblp = dbenv->lg_handle;
			lp = dblp->reginfo.primary;
			LOG_SYSTEM_LOCK(dbenv);
			lsn = lp->lsn;
			LOG_SYSTEM_UNLOCK(dbenv);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0, 0);
		}
		ret = DB_REP_NEWSITE;
		break;
	case REP_NEWCLIENT:
		/*
		 * Handle even if we're recovering.
		 */
		/*
		 * This message was received and should have resulted in the
		 * application entering the machine ID in its machine table.
		 * We respond to this with an ALIVE to send relevant information
		 * to the new client (if we are a master, we'll send a
		 * NEWMASTER, so we only need to send the ALIVE if we're a
		 * client).  But first, broadcast the new client's record to
		 * all the clients.
		 */
		(void)__rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_NEWSITE, &rp->lsn, rec, 0, 0);

		ret = DB_REP_NEWSITE;

		if (F_ISSET(rep, REP_F_CLIENT)) {
			REP_SYSTEM_LOCK(dbenv);
			egen = rep->egen;
			if (*eidp == rep->master_id)
				rep->master_id = DB_EID_INVALID;
			REP_SYSTEM_UNLOCK(dbenv);
			data_dbt.data = &egen;
			data_dbt.size = sizeof(egen);
			(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
			    REP_ALIVE, &rp->lsn, &data_dbt, 0, 0);
			break;
		}
		/* FALLTHROUGH */
	case REP_MASTER_REQ:
		RECOVERING_SKIP;
		if (F_ISSET(rep, REP_F_MASTER)) {
			LOG_SYSTEM_LOCK(dbenv);
			lsn = lp->lsn;
			LOG_SYSTEM_UNLOCK(dbenv);
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_NEWMASTER, &lsn, NULL, 0, 0);
		}
		/*
		 * If there is no master, then we could get into a state
		 * where an old client lost the initial ALIVE message and
		 * is calling an election under an old gen and can
		 * never get to the current gen.
		 */
		if (F_ISSET(rep, REP_F_CLIENT) && rp->gen < gen) {
			REP_SYSTEM_LOCK(dbenv);
			egen = rep->egen;
			if (*eidp == rep->master_id)
				rep->master_id = DB_EID_INVALID;
			REP_SYSTEM_UNLOCK(dbenv);
			data_dbt.data = &egen;
			data_dbt.size = sizeof(egen);
			(void)__rep_send_message(dbenv, *eidp,
			    REP_ALIVE, &rp->lsn, &data_dbt, 0, 0);
		}
		break;
	case REP_NEWFILE:
		RECOVERING_LOG_SKIP;
		CLIENT_ONLY(rep, rp);
		ret = __rep_apply(dbenv, rp, rec, ret_lsnp, NULL);
		break;
	case REP_NEWMASTER:
		/*
		 * Handle even if we're recovering.
		 */
		ANYSITE(rep);
		if (F_ISSET(rep, REP_F_MASTER) &&
		    *eidp != dbenv->rep_eid) {
			/* We don't hold the rep mutex, and may miscount. */
			rep->stat.st_dupmasters++;
			ret = DB_REP_DUPMASTER;
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0, 0);
			break;
		}
		ret = __rep_new_master(dbenv, rp, *eidp);
		break;
	case REP_PAGE:
	case REP_PAGE_MORE:
		/*
		 * Handle even if we're recovering.
		 */
		CLIENT_ONLY(rep, rp);
		ret = __rep_page(dbenv, *eidp, rp, rec);
		break;
	case REP_PAGE_FAIL:
		/*
		 * Handle even if we're recovering.
		 */
		CLIENT_ONLY(rep, rp);
		ret = __rep_page_fail(dbenv, *eidp, rec);
		break;
	case REP_PAGE_REQ:
		/*
		 * Handle even if we're recovering.
		 */
		MASTER_UPDATE(dbenv, renv);
		ret = __rep_page_req(dbenv, *eidp, rec);
		CLIENT_REREQ;
		break;
	case REP_REREQUEST:
		/*
		 * Handle even if we're recovering.  Don't do a master
		 * check.
		 */
		CLIENT_ONLY(rep, rp);
		/*
		 * Don't hold any mutex, may miscount.
		 */
		rep->stat.st_client_rerequests++;
		ret = __rep_resend_req(dbenv, 1);
		break;
	case REP_UPDATE:
		/*
		 * Handle even if we're recovering.
		 */
		CLIENT_ONLY(rep, rp);
		ret = __rep_update_setup(dbenv, *eidp, rp, rec);
		break;
	case REP_UPDATE_REQ:
		/*
		 * Handle even if we're recovering.
		 */
		MASTER_ONLY(rep, rp);
		infop = dbenv->reginfo;
		renv = infop->primary;
		MASTER_UPDATE(dbenv, renv);
		ret = __rep_update_req(dbenv, *eidp);
		break;
	case REP_VERIFY:
		if (recovering) {
			MUTEX_LOCK(dbenv, rep->mtx_clientdb);
			cmp = log_compare(&lp->verify_lsn, &rp->lsn);
			MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
			/*
			 * If this is not the verify record I want, skip it.
			 */
			if (cmp != 0) {
				ret = __rep_skip_msg(
				    dbenv, rep, *eidp, rp->rectype);
				break;
			}
		}
		CLIENT_ONLY(rep, rp);
		ret = __rep_verify(dbenv, rp, rec, *eidp, savetime);
		break;
	case REP_VERIFY_FAIL:
		/*
		 * Handle even if we're recovering.
		 */
		CLIENT_ONLY(rep, rp);
		ret = __rep_verify_fail(dbenv, rp, *eidp);
		break;
	case REP_VERIFY_REQ:
		RECOVERING_SKIP;
		ret = __rep_verify_req(dbenv, rp, *eidp);
		CLIENT_REREQ;
		break;
	case REP_VOTE1:
		/*
		 * Handle even if we're recovering.
		 */
		ret = __rep_vote1(dbenv, rp, rec, *eidp);
		break;
	case REP_VOTE2:
		/*
		 * Handle even if we're recovering.
		 */
		ret = __rep_vote2(dbenv, rec, eidp);
		break;
	default:
		__db_err(dbenv,
	"DB_ENV->rep_process_message: unknown replication message: type %lu",
		   (u_long)rp->rectype);
		ret = EINVAL;
		break;
	}

errlock:
	REP_SYSTEM_LOCK(dbenv);
	rep->msg_th--;
	REP_SYSTEM_UNLOCK(dbenv);
out:
	if (ret == 0 && F_ISSET(rp, DB_LOG_PERM)) {
		if (ret_lsnp != NULL)
			*ret_lsnp = rp->lsn;
		ret = DB_REP_NOTPERM;
	}
	return (ret);
}

/*
 * __rep_apply --
 *
 * Handle incoming log records on a client, applying when possible and
 * entering into the bookkeeping table otherwise.  This routine manages
 * the state of the incoming message stream -- processing records, via
 * __rep_process_rec, when possible and enqueuing in the __db.rep.db
 * when necessary.  As gaps in the stream are filled in, this is where
 * we try to process as much as possible from __db.rep.db to catch up.
 *
 * PUBLIC: int __rep_apply __P((DB_ENV *, REP_CONTROL *,
 * PUBLIC:     DBT *, DB_LSN *, int *));
 */
int
__rep_apply(dbenv, rp, rec, ret_lsnp, is_dupp)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	DB_LSN *ret_lsnp;
	int *is_dupp;
{
	DB_REP *db_rep;
	DBT control_dbt, key_dbt;
	DBT rec_dbt;
	DB *dbp;
	DB_LOG *dblp;
	DB_LSN max_lsn;
	LOG *lp;
	REP *rep;
	u_int32_t rectype;
	int cmp, ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dbp = db_rep->rep_db;
	rectype = 0;
	ret = 0;
	memset(&control_dbt, 0, sizeof(control_dbt));
	memset(&rec_dbt, 0, sizeof(rec_dbt));
	ZERO_LSN(max_lsn);

	dblp = dbenv->lg_handle;
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lp = dblp->reginfo.primary;
	REP_SYSTEM_LOCK(dbenv);
	if (F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    log_compare(&lp->ready_lsn, &rep->first_lsn) < 0)
		lp->ready_lsn = rep->first_lsn;
	REP_SYSTEM_UNLOCK(dbenv);
	cmp = log_compare(&rp->lsn, &lp->ready_lsn);

	if (cmp == 0) {
		if ((ret =
		    __rep_process_rec(dbenv, rp, rec, &rectype, &max_lsn)) != 0)
			goto err;
		/*
		 * If we get the record we are expecting, reset
		 * the count of records we've received and are applying
		 * towards the request interval.
		 */
		lp->rcvd_recs = 0;
		ZERO_LSN(lp->max_wait_lsn);

		while (ret == 0 &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0) {
			/*
			 * We just filled in a gap in the log record stream.
			 * Write subsequent records to the log.
			 */
gap_check:
			if ((ret =
			    __rep_remfirst(dbenv, &control_dbt, &rec_dbt)) != 0)
				goto err;

			rp = (REP_CONTROL *)control_dbt.data;
			rec = &rec_dbt;
			if ((ret = __rep_process_rec(dbenv,
			    rp, rec, &rectype, &max_lsn)) != 0)
				goto err;

			/*
			 * We may miscount, as we don't hold the rep mutex.
			 */
			--rep->stat.st_log_queued;

			/*
			 * Since we just filled a gap in the log stream, and
			 * we're writing subsequent records to the log, we want
			 * to use rcvd_recs and wait_recs so that we will
			 * request the next gap if we end up with a gap and
			 * a lot of records still in the temp db, but not
			 * request if it is near the end of the temp db and
			 * likely to arrive on its own shortly.  We want to
			 * avoid requesting the record in that case.  Also
			 * reset max_wait_lsn because the next gap is a
			 * fresh gap.
			 */
			lp->rcvd_recs = rep->stat.st_log_queued;
			lp->wait_recs = rep->request_gap;

			if ((ret = __rep_getnext(dbenv)) == DB_NOTFOUND) {
				lp->rcvd_recs = 0;
				ret = 0;
				break;
			} else if (ret != 0)
				goto err;
		}

		/*
		 * Check if we're at a gap in the table and if so, whether we
		 * need to ask for any records.
		 */
		if (!IS_ZERO_LSN(lp->waiting_lsn) &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) != 0) {
			/*
			 * We got a record and processed it, but we may
			 * still be waiting for more records.  If we
			 * filled a gap we keep a count of how many other
			 * records are in the temp database and if we should
			 * request the next gap at this time.
			 */
			if (__rep_check_doreq(dbenv, rep) && (ret =
			    __rep_loggap_req(dbenv, rep, &rp->lsn, 0)) != 0)
				goto err;
		} else {
			lp->wait_recs = 0;
			ZERO_LSN(lp->max_wait_lsn);
		}

	} else if (cmp > 0) {
		/*
		 * The LSN is higher than the one we were waiting for.
		 * This record isn't in sequence; add it to the temporary
		 * database, update waiting_lsn if necessary, and perform
		 * calculations to determine if we should issue requests
		 * for new records.
		 */
		memset(&key_dbt, 0, sizeof(key_dbt));
		key_dbt.data = rp;
		key_dbt.size = sizeof(*rp);
		if (lp->wait_recs == 0) {
			/*
			 * This is a new gap. Initialize the number of
			 * records that we should wait before requesting
			 * that it be resent.  We grab the limits out of
			 * the rep without the mutex.
			 */
			lp->wait_recs = rep->request_gap;
			lp->rcvd_recs = 0;
			ZERO_LSN(lp->max_wait_lsn);
		}
		if (__rep_check_doreq(dbenv, rep) &&
		    (ret = __rep_loggap_req(dbenv, rep, &rp->lsn, 0) != 0))
			goto err;

		ret = __db_put(dbp, NULL, &key_dbt, rec, DB_NOOVERWRITE);
		rep->stat.st_log_queued++;
		rep->stat.st_log_queued_total++;
		if (rep->stat.st_log_queued_max < rep->stat.st_log_queued)
			rep->stat.st_log_queued_max = rep->stat.st_log_queued;

		if (ret == DB_KEYEXIST)
			ret = 0;
		if (ret != 0)
			goto done;

		if (IS_ZERO_LSN(lp->waiting_lsn) ||
		    log_compare(&rp->lsn, &lp->waiting_lsn) < 0)
			lp->waiting_lsn = rp->lsn;

		/*
		 * If this is permanent; let the caller know that we have
		 * not yet written it to disk, but we've accepted it.
		 */
		if (ret == 0 && F_ISSET(rp, DB_LOG_PERM)) {
			max_lsn = rp->lsn;
			ret = DB_REP_NOTPERM;
		}
		goto done;
	} else {
		/*
		 * We may miscount if we race, since we
		 * don't currently hold the rep mutex.
		 */
		rep->stat.st_log_duplicated++;
		if (is_dupp != NULL)
			*is_dupp = 1;
		if (F_ISSET(rp, DB_LOG_PERM))
			max_lsn = lp->max_perm_lsn;
		goto done;
	}

	/* Check if we need to go back into the table. */
	if (ret == 0 && log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0)
		goto gap_check;

done:
err:	/* Check if we need to go back into the table. */
	REP_SYSTEM_LOCK(dbenv);
	if (ret == 0 &&
	    F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    log_compare(&lp->ready_lsn, &rep->last_lsn) >= 0) {
		rep->last_lsn = max_lsn;
		ZERO_LSN(max_lsn);
		ret = DB_REP_LOGREADY;
	}
	REP_SYSTEM_UNLOCK(dbenv);

	if (ret == 0 && !F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    !IS_ZERO_LSN(max_lsn)) {
		if (ret_lsnp != NULL)
			*ret_lsnp = max_lsn;
		ret = DB_REP_ISPERM;
		DB_ASSERT(log_compare(&max_lsn, &lp->max_perm_lsn) >= 0);
		lp->max_perm_lsn = max_lsn;
	}
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);

	/*
	 * Startup is complete when we process our first live record.  However,
	 * we want to return DB_REP_STARTUPDONE on the first record we can --
	 * but other return values trump this one.  We know we've processed at
	 * least one record when rectype is non-zero.
	 */
	if (ret == 0 && !F_ISSET(rp, DB_LOG_RESEND) &&
	    rectype != 0 && rep->stat.st_startup_complete == 0) {
		rep->stat.st_startup_complete = 1;
		ret = DB_REP_STARTUPDONE;
	}
	if (ret == 0 && rp->rectype == REP_NEWFILE && lp->db_log_autoremove)
		__log_autoremove(dbenv);
	if (control_dbt.data != NULL)
		__os_ufree(dbenv, control_dbt.data);
	if (rec_dbt.data != NULL)
		__os_ufree(dbenv, rec_dbt.data);

	if (ret == DB_REP_NOTPERM && !F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    !IS_ZERO_LSN(max_lsn) && ret_lsnp != NULL)
		*ret_lsnp = max_lsn;

#ifdef DIAGNOSTIC
	if (ret == DB_REP_ISPERM)
		RPRINT(dbenv, rep, (dbenv, &mb, "Returning ISPERM [%lu][%lu]",
		    (u_long)max_lsn.file, (u_long)max_lsn.offset));
	else if (ret == DB_REP_LOGREADY)
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Returning LOGREADY up to [%lu][%lu]",
		    (u_long)rep->last_lsn.file,
		    (u_long)rep->last_lsn.offset));
	else if (ret == DB_REP_NOTPERM)
		RPRINT(dbenv, rep, (dbenv, &mb, "Returning NOTPERM [%lu][%lu]",
		    (u_long)max_lsn.file, (u_long)max_lsn.offset));
	else if (ret == DB_REP_STARTUPDONE)
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Returning STARTUPDONE [%lu][%lu]",
		    (u_long)rp->lsn.file, (u_long)rp->lsn.offset));
	else if (ret != 0)
		RPRINT(dbenv, rep, (dbenv, &mb, "Returning %d [%lu][%lu]", ret,
		    (u_long)max_lsn.file, (u_long)max_lsn.offset));
#endif
	return (ret);
}

/*
 * __rep_process_txn --
 *
 * This is the routine that actually gets a transaction ready for
 * processing.
 *
 * PUBLIC: int __rep_process_txn __P((DB_ENV *, DBT *));
 */
int
__rep_process_txn(dbenv, rec)
	DB_ENV *dbenv;
	DBT *rec;
{
	DBT data_dbt, *lock_dbt;
	DB_LOCKREQ req, *lvp;
	DB_LOGC *logc;
	DB_LSN prev_lsn, *lsnp;
	DB_REP *db_rep;
	DB_TXNHEAD *txninfo;
	LSN_COLLECTION lc;
	REP *rep;
	__txn_regop_args *txn_args;
	__txn_xa_regop_args *prep_args;
	u_int32_t lockid, rectype;
	u_int i;
	int ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	logc = NULL;
	txn_args = NULL;
	prep_args = NULL;
	txninfo = NULL;

	memset(&data_dbt, 0, sizeof(data_dbt));
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		F_SET(&data_dbt, DB_DBT_REALLOC);

	/*
	 * There are two phases:  First, we have to traverse backwards through
	 * the log records gathering the list of all LSNs in the transaction.
	 * Once we have this information, we can loop through and then apply it.
	 *
	 * We may be passed a prepare (if we're restoring a prepare on upgrade)
	 * instead of a commit (the common case).  Check which it is and behave
	 * appropriately.
	 */
	memcpy(&rectype, rec->data, sizeof(rectype));
	memset(&lc, 0, sizeof(lc));
	if (rectype == DB___txn_regop) {
		/*
		 * We're the end of a transaction.  Make sure this is
		 * really a commit and not an abort!
		 */
		if ((ret = __txn_regop_read(dbenv, rec->data, &txn_args)) != 0)
			return (ret);
		if (txn_args->opcode != TXN_COMMIT) {
			__os_free(dbenv, txn_args);
			return (0);
		}
		prev_lsn = txn_args->prev_lsn;
		lock_dbt = &txn_args->locks;
	} else {
		/* We're a prepare. */
		DB_ASSERT(rectype == DB___txn_xa_regop);

		if ((ret =
		    __txn_xa_regop_read(dbenv, rec->data, &prep_args)) != 0)
			return (ret);
		prev_lsn = prep_args->prev_lsn;
		lock_dbt = &prep_args->locks;
	}

	/* Get locks. */
	if ((ret = __lock_id(dbenv, &lockid, NULL)) != 0)
		goto err1;

	if ((ret =
	      __lock_get_list(dbenv, lockid, 0, DB_LOCK_WRITE, lock_dbt)) != 0)
		goto err;

	/* Phase 1.  Get a list of the LSNs in this transaction, and sort it. */
	if ((ret = __rep_collect_txn(dbenv, &prev_lsn, &lc)) != 0)
		goto err;
	qsort(lc.array, lc.nlsns, sizeof(DB_LSN), __rep_lsn_cmp);

	/*
	 * The set of records for a transaction may include dbreg_register
	 * records.  Create a txnlist so that they can keep track of file
	 * state between records.
	 */
	if ((ret = __db_txnlist_init(dbenv, 0, 0, NULL, &txninfo)) != 0)
		goto err;

	/* Phase 2: Apply updates. */
	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;
	for (lsnp = &lc.array[0], i = 0; i < lc.nlsns; i++, lsnp++) {
		if ((ret = __log_c_get(logc, lsnp, &data_dbt, DB_SET)) != 0) {
			__db_err(dbenv, "failed to read the log at [%lu][%lu]",
			    (u_long)lsnp->file, (u_long)lsnp->offset);
			goto err;
		}
		if ((ret = __db_dispatch(dbenv, dbenv->recover_dtab,
		    dbenv->recover_dtab_size, &data_dbt, lsnp,
		    DB_TXN_APPLY, txninfo)) != 0) {
			__db_err(dbenv, "transaction failed at [%lu][%lu]",
			    (u_long)lsnp->file, (u_long)lsnp->offset);
			goto err;
		}
	}

err:	memset(&req, 0, sizeof(req));
	req.op = DB_LOCK_PUT_ALL;
	if ((t_ret =
	     __lock_vec(dbenv, lockid, 0, &req, 1, &lvp)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = __lock_id_free(dbenv, lockid)) != 0 && ret == 0)
		ret = t_ret;

err1:	if (txn_args != NULL)
		__os_free(dbenv, txn_args);
	if (prep_args != NULL)
		__os_free(dbenv, prep_args);
	if (lc.array != NULL)
		__os_free(dbenv, lc.array);

	if (logc != NULL && (t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	if (txninfo != NULL)
		__db_txnlist_end(dbenv, txninfo);

	if (F_ISSET(&data_dbt, DB_DBT_REALLOC) && data_dbt.data != NULL)
		__os_ufree(dbenv, data_dbt.data);

	if (ret == 0)
		/*
		 * We don't hold the rep mutex, and could miscount if we race.
		 */
		rep->stat.st_txns_applied++;

	return (ret);
}

/*
 * __rep_collect_txn
 *	Recursive function that will let us visit every entry in a transaction
 *	chain including all child transactions so that we can then apply
 *	the entire transaction family at once.
 */
static int
__rep_collect_txn(dbenv, lsnp, lc)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	LSN_COLLECTION *lc;
{
	__txn_child_args *argp;
	DB_LOGC *logc;
	DB_LSN c_lsn;
	DBT data;
	u_int32_t rectype;
	u_int nalloc;
	int ret, t_ret;

	memset(&data, 0, sizeof(data));
	F_SET(&data, DB_DBT_REALLOC);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	while (!IS_ZERO_LSN(*lsnp) &&
	    (ret = __log_c_get(logc, lsnp, &data, DB_SET)) == 0) {
		memcpy(&rectype, data.data, sizeof(rectype));
		if (rectype == DB___txn_child) {
			if ((ret = __txn_child_read(dbenv,
			    data.data, &argp)) != 0)
				goto err;
			c_lsn = argp->c_lsn;
			*lsnp = argp->prev_lsn;
			__os_free(dbenv, argp);
			ret = __rep_collect_txn(dbenv, &c_lsn, lc);
		} else {
			if (lc->nalloc < lc->nlsns + 1) {
				nalloc = lc->nalloc == 0 ? 20 : lc->nalloc * 2;
				if ((ret = __os_realloc(dbenv,
				    nalloc * sizeof(DB_LSN), &lc->array)) != 0)
					goto err;
				lc->nalloc = nalloc;
			}
			lc->array[lc->nlsns++] = *lsnp;

			/*
			 * Explicitly copy the previous lsn.  The record
			 * starts with a u_int32_t record type, a u_int32_t
			 * txn id, and then the DB_LSN (prev_lsn) that we
			 * want.  We copy explicitly because we have no idea
			 * what kind of record this is.
			 */
			memcpy(lsnp, (u_int8_t *)data.data +
			    sizeof(u_int32_t) + sizeof(u_int32_t),
			    sizeof(DB_LSN));
		}

		if (ret != 0)
			goto err;
	}
	if (ret != 0)
		__db_err(dbenv, "collect failed at: [%lu][%lu]",
		    (u_long)lsnp->file, (u_long)lsnp->offset);

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	if (data.data != NULL)
		__os_ufree(dbenv, data.data);
	return (ret);
}

/*
 * __rep_lsn_cmp --
 *	qsort-type-compatible wrapper for log_compare.
 */
static int
__rep_lsn_cmp(lsn1, lsn2)
	const void *lsn1, *lsn2;
{

	return (log_compare((DB_LSN *)lsn1, (DB_LSN *)lsn2));
}

/*
 * __rep_newfile --
 *	NEWFILE messages have the LSN of the last record in the previous
 * log file.  When applying a NEWFILE message, make sure we haven't already
 * swapped files.
 */
static int
__rep_newfile(dbenv, rc, lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rc;
	DB_LSN *lsnp;
{
	DB_LOG *dblp;
	LOG *lp;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	if (rc->lsn.file + 1 > lp->lsn.file)
		return (__log_newfile(dblp, lsnp, 0));
	else {
		/* We've already applied this NEWFILE.  Just ignore it. */
		*lsnp = lp->lsn;
		return (0);
	}
}

/*
 * __rep_do_ckp --
 * Perform the memp_sync necessary for this checkpoint without holding the
 * REP->mtx_clientdb.  Callers of this function must hold REP->mtx_clientdb
 * and must not be holding the region mutex.
 */
static int
__rep_do_ckp(dbenv, rec, rp)
	DB_ENV *dbenv;
	DBT *rec;
	REP_CONTROL *rp;
{
	DB_LSN ckp_lsn;
	DB_REP *db_rep;
	int ret;

	db_rep = dbenv->rep_handle;

	MUTEX_UNLOCK(dbenv, db_rep->region->mtx_clientdb);

	DB_TEST_WAIT(dbenv, dbenv->test_check);

	/* Sync the memory pool. */
	memcpy(&ckp_lsn, (u_int8_t *)rec->data +
	    SSZ(__txn_ckp_args, ckp_lsn), sizeof(DB_LSN));
	ret = __memp_sync(dbenv, &ckp_lsn);

	/* Update the last_ckp in the txn region. */
	if (ret == 0)
		ret = __txn_updateckp(dbenv, &rp->lsn);
	else {
		__db_err(dbenv, "Error syncing ckp [%lu][%lu]",
		    (u_long)ckp_lsn.file, (u_long)ckp_lsn.offset);
		ret = __db_panic(dbenv, ret);
	}
	MUTEX_LOCK(dbenv, db_rep->region->mtx_clientdb);

	return (ret);
}

/*
 * __rep_remfirst --
 * Remove the first entry from the __db.rep.db
 */
static int
__rep_remfirst(dbenv, cntrl, rec)
	DB_ENV *dbenv;
	DBT *cntrl;
	DBT *rec;
{
	DB *dbp;
	DBC *dbc;
	DB_REP *db_rep;
	int ret, t_ret;

	db_rep = dbenv->rep_handle;
	dbp = db_rep->rep_db;

	if ((ret = __db_cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	/* The DBTs need to persist through another call. */
	F_SET(cntrl, DB_DBT_REALLOC);
	F_SET(rec, DB_DBT_REALLOC);
	if ((ret = __db_c_get(dbc, cntrl, rec, DB_RMW | DB_FIRST)) == 0)
		ret = __db_c_del(dbc, 0);
	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __rep_getnext --
 * Get the next record out of the __db.rep.db table.
 */
static int
__rep_getnext(dbenv)
	DB_ENV *dbenv;
{
	DB *dbp;
	DB_REP *db_rep;
	DB_LOG *dblp;
	DBC *dbc;
	DBT lsn_dbt, nextrec_dbt;
	LOG *lp;
	REP_CONTROL *rp;
	int ret, t_ret;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	db_rep = dbenv->rep_handle;
	dbp = db_rep->rep_db;

	if ((ret = __db_cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	/*
	 * Update waiting_lsn.  We need to move it
	 * forward to the LSN of the next record
	 * in the queue.
	 *
	 * If the next item in the database is a log
	 * record--the common case--we're not
	 * interested in its contents, just in its LSN.
	 * Optimize by doing a partial get of the data item.
	 */
	memset(&nextrec_dbt, 0, sizeof(nextrec_dbt));
	F_SET(&nextrec_dbt, DB_DBT_PARTIAL);
	nextrec_dbt.ulen = nextrec_dbt.dlen = 0;

	memset(&lsn_dbt, 0, sizeof(lsn_dbt));
	ret = __db_c_get(dbc, &lsn_dbt, &nextrec_dbt, DB_FIRST);
	if (ret != DB_NOTFOUND && ret != 0)
		goto err;

	if (ret == DB_NOTFOUND) {
		ZERO_LSN(lp->waiting_lsn);
		/*
		 * Whether or not the current record is
		 * simple, there's no next one, and
		 * therefore we haven't got anything
		 * else to do right now.  Break out.
		 */
		goto err;
	}
	rp = (REP_CONTROL *)lsn_dbt.data;
	lp->waiting_lsn = rp->lsn;

err:	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_process_rec --
 *
 * Given a record in 'rp', process it.  In the case of a NEWFILE, that means
 * potentially switching files.  In the case of a checkpoint, it means doing
 * the checkpoint, and in other cases, it means simply writing the record into
 * the log.
 */
static int
__rep_process_rec(dbenv, rp, rec, typep, ret_lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
	u_int32_t *typep;
	DB_LSN *ret_lsnp;
{
	DB *dbp;
	DB_LOG *dblp;
	DB_REP *db_rep;
	DBT control_dbt, key_dbt, rec_dbt;
	LOG *lp;
	REP *rep;
	u_int32_t txnid;
	int ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dbp = db_rep->rep_db;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	ret = 0;

	if (rp->rectype == REP_NEWFILE) {
		ret = __rep_newfile(dbenv, rp, &lp->ready_lsn);

		/* Make this evaluate to a simple rectype. */
		*typep = 0;
		return (0);
	}

	memcpy(typep, rec->data, sizeof(*typep));
	memset(&control_dbt, 0, sizeof(control_dbt));
	memset(&rec_dbt, 0, sizeof(rec_dbt));

	/*
	 * We write all records except for checkpoint records here.
	 * All non-checkpoint records need to appear in the log before
	 * we take action upon them (i.e., we enforce write-ahead logging).
	 * However, we can't write the checkpoint record here until the
	 * data buffers are actually written to disk, else we are creating
	 * an invalid log -- one that says all data before a certain point
	 * has been written to disk.
	 *
	 * If two threads are both processing the same checkpoint record
	 * (because, for example, it was resent and the original finally
	 * arrived), we handle that below by checking for the existence of
	 * the log record when we add it to the replication database.
	 *
	 * Any log records that arrive while we are processing the checkpoint
	 * are added to the bookkeeping database because ready_lsn is not yet
	 * updated to point after the checkpoint record.
	 */
	if (*typep != DB___txn_ckp || F_ISSET(rep, REP_F_RECOVER_LOG)) {
		if ((ret = __log_rep_put(dbenv, &rp->lsn, rec)) != 0)
			return (ret);
		rep->stat.st_log_records++;
		if (F_ISSET(rep, REP_F_RECOVER_LOG)) {
			*ret_lsnp = rp->lsn;
			goto out;
		}
	}

	switch (*typep) {
	case DB___dbreg_register:
		/*
		 * DB opens occur in the context of a transaction, so we can
		 * simply handle them when we process the transaction.  Closes,
		 * however, are not transaction-protected, so we have to
		 * handle them here.
		 *
		 * Note that it should be unsafe for the master to do a close
		 * of a file that was opened in an active transaction, so we
		 * should be guaranteed to get the ordering right.
		 */
		memcpy(&txnid, (u_int8_t *)rec->data +
		    SSZ(__dbreg_register_args, txnid), sizeof(u_int32_t));
		if (txnid == TXN_INVALID)
			ret = __db_dispatch(dbenv, dbenv->recover_dtab,
			    dbenv->recover_dtab_size, rec, &rp->lsn,
			    DB_TXN_APPLY, NULL);
		break;
	case DB___txn_regop:
		/*
		 * If an application is doing app-specific recovery
		 * and acquires locks while applying a transaction,
		 * it can deadlock.  Any other locks held by this
		 * thread should have been discarded in the
		 * __rep_process_txn error path, so if we simply
		 * retry, we should eventually succeed.
		 */
		do {
			ret = 0;
			if (!F_ISSET(db_rep, DBREP_OPENFILES)) {
				ret = __txn_openfiles(dbenv, NULL, 1);
				F_SET(db_rep, DBREP_OPENFILES);
			}
			if (ret == 0)
				ret = __rep_process_txn(dbenv, rec);
		} while (ret == DB_LOCK_DEADLOCK);

		/* Now flush the log unless we're running TXN_NOSYNC. */
		if (ret == 0 && !F_ISSET(dbenv, DB_ENV_TXN_NOSYNC))
			ret = __log_flush(dbenv, NULL);
		if (ret != 0) {
			__db_err(dbenv, "Error processing txn [%lu][%lu]",
			    (u_long)rp->lsn.file, (u_long)rp->lsn.offset);
			ret = __db_panic(dbenv, ret);
		}
		break;
	case DB___txn_xa_regop:
		ret = __log_flush(dbenv, NULL);
		break;
	case DB___txn_ckp:
		/*
		 * We do not want to hold the REP->mtx_clientdb mutex while
		 * syncing the mpool, so if we get a checkpoint record we are
		 * supposed to process, add it to the __db.rep.db, do the
		 * memp_sync and then go back and process it later, when the
		 * sync has finished.  If this record is already in the table,
		 * then some other thread will process it, so simply return
		 * REP_NOTPERM.
		 */
		memset(&key_dbt, 0, sizeof(key_dbt));
		key_dbt.data = rp;
		key_dbt.size = sizeof(*rp);

		/*
		 * We want to put this record into the tmp DB only if
		 * it doesn't exist, so use DB_NOOVERWRITE.
		 */
		ret = __db_put(dbp, NULL, &key_dbt, rec, DB_NOOVERWRITE);
		if (ret == DB_KEYEXIST) {
			if (ret_lsnp != NULL)
				*ret_lsnp = rp->lsn;
			ret = DB_REP_NOTPERM;
		}
		if (ret != 0)
			break;

		/*
		 * Now, do the checkpoint.  Regardless of
		 * whether the checkpoint succeeds or not,
		 * we need to remove the record we just put
		 * in the temporary database.  If the
		 * checkpoint failed, return an error.  We
		 * will act like we never received the
		 * checkpoint.
		 */
		if ((ret = __rep_do_ckp(dbenv, rec, rp)) == 0)
			ret = __log_rep_put(dbenv, &rp->lsn, rec);
		if ((t_ret = __rep_remfirst(dbenv,
		    &control_dbt, &rec_dbt)) != 0 && ret == 0)
			ret = t_ret;
		break;
	default:
		break;
	}

out:
	if (ret == 0 && F_ISSET(rp, DB_LOG_PERM))
		*ret_lsnp = rp->lsn;
	if (control_dbt.data != NULL)
		__os_ufree(dbenv, control_dbt.data);
	if (rec_dbt.data != NULL)
		__os_ufree(dbenv, rec_dbt.data);

	return (ret);
}

/*
 * __rep_resend_req --
 *	We might have dropped a message, we need to resend our request.
 *	The request we send is dependent on what recovery state we're in.
 *	The caller holds no locks.
 */
static int
__rep_resend_req(dbenv, rereq)
	DB_ENV *dbenv;
	int rereq;
{

	DB_LOG *dblp;
	DB_LSN lsn;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	int ret;
	u_int32_t gapflags, repflags;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	ret = 0;

	repflags = rep->flags;
	/*
	 * If we are delayed we do not rerequest anything.
	 */
	if (FLD_ISSET(repflags, REP_F_DELAY))
		return (ret);
	gapflags = rereq ? REP_GAP_REREQUEST : 0;

	if (FLD_ISSET(repflags, REP_F_RECOVER_VERIFY)) {
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		lsn = lp->verify_lsn;
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		if (!IS_ZERO_LSN(lsn))
			(void)__rep_send_message(dbenv, rep->master_id,
			    REP_VERIFY_REQ, &lsn, NULL, 0, DB_REP_REREQUEST);
	} else if (FLD_ISSET(repflags, REP_F_RECOVER_UPDATE)) {
		/*
		 * UPDATE_REQ only goes to the master.
		 */
		(void)__rep_send_message(dbenv, rep->master_id,
		    REP_UPDATE_REQ, NULL, NULL, 0, 0);
	} else if (FLD_ISSET(repflags, REP_F_RECOVER_PAGE)) {
		REP_SYSTEM_LOCK(dbenv);
		ret = __rep_pggap_req(dbenv, rep, NULL, gapflags);
		REP_SYSTEM_UNLOCK(dbenv);
	} else {
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		ret = __rep_loggap_req(dbenv, rep, NULL, gapflags);
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	}

	return (ret);
}

/*
 * __rep_check_doreq --
 * PUBLIC: int __rep_check_doreq __P((DB_ENV *, REP *));
 *
 * Check if we need to send another request.  If so, compare with
 * the request limits the user might have set.  This assumes the
 * caller holds the REP->mtx_clientdb mutex.  Returns 1 if a request
 * needs to be made, and 0 if it does not.
 */
int
__rep_check_doreq(dbenv, rep)
	DB_ENV *dbenv;
	REP *rep;
{

	DB_LOG *dblp;
	LOG *lp;
	int req;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	req = ++lp->rcvd_recs >= lp->wait_recs;
	if (req) {
		lp->wait_recs *= 2;
		if (lp->wait_recs > rep->max_gap)
			lp->wait_recs = rep->max_gap;
		lp->rcvd_recs = 0;
	}
	return (req);
}

/*
 * __rep_skip_msg -
 *
 *	If we're in recovery we want to skip/ignore the message, but
 *	we also need to see if we need to re-request any retransmissions.
 */
static int
__rep_skip_msg(dbenv, rep, eid, rectype)
	DB_ENV *dbenv;
	REP *rep;
	int eid;
	u_int32_t rectype;
{
	int do_req, ret;

	ret = 0;
	/*
	 * If we have a request message from a client then immediately
	 * send a REP_REREQUEST back to that client since we're skipping it.
	 */
	if (rep->master_id != DB_EID_INVALID && eid != rep->master_id)
		do_req = 1;
	else {
		/* Check for need to retransmit. */
		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		do_req = __rep_check_doreq(dbenv, rep);
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	}
	/*
	 * Don't respond to a MASTER_REQ with
	 * a MASTER_REQ or REREQUEST.
	 */
	if (do_req && rectype != REP_MASTER_REQ) {
		/*
		 * There are three cases:
		 * 1.  If we don't know who the master is, then send MASTER_REQ.
		 * 2.  If the message we're skipping came from the master,
		 * then we need to rerequest.
		 * 3.  If the message didn't come from a master (i.e. client
		 * to client), then send a rerequest back to the sender so
		 * the sender can rerequest it elsewhere.
		 */
		if (rep->master_id == DB_EID_INVALID)	/* Case 1. */
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0, 0);
		else if (eid == rep->master_id)		/* Case 2. */
			ret = __rep_resend_req(dbenv, 0);
		else					/* Case 3. */
			(void)__rep_send_message(dbenv,
			    eid, REP_REREQUEST, NULL, NULL, 0, 0);
	}
	return (ret);
}
