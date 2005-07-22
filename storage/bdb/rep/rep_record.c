/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_record.c,v 1.255 2004/11/04 18:35:29 sue Exp $
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

static int __rep_apply __P((DB_ENV *, REP_CONTROL *, DBT *, DB_LSN *, int *));
static int __rep_collect_txn __P((DB_ENV *, DB_LSN *, LSN_COLLECTION *));
static int __rep_do_ckp __P((DB_ENV *, DBT *, REP_CONTROL *));
static int __rep_dorecovery __P((DB_ENV *, DB_LSN *, DB_LSN *));
static int __rep_getnext __P((DB_ENV *));
static int __rep_lsn_cmp __P((const void *, const void *));
static int __rep_newfile __P((DB_ENV *, REP_CONTROL *, DB_LSN *));
static int __rep_process_rec __P((DB_ENV *,
    REP_CONTROL *, DBT *, u_int32_t *, DB_LSN *));
static int __rep_remfirst __P((DB_ENV *, DBT *, DBT *));
static int __rep_resend_req __P((DB_ENV *, int));
static int __rep_verify_match __P((DB_ENV *, DB_LSN *, time_t));

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
		    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0);	\
		ret = DB_REP_DUPMASTER;					\
		goto errlock;						\
	}								\
} while (0)

#define	MASTER_CHECK(dbenv, eid, rep) do {				\
	if (rep->master_id == DB_EID_INVALID) {				\
		RPRINT(dbenv, rep, (dbenv, &mb,				\
		    "Received record from %d, master is INVALID", eid));\
		ret = 0;						\
		(void)__rep_send_message(dbenv,				\
		    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0);	\
		goto errlock;						\
	}								\
	if (eid != rep->master_id) {					\
		__db_err(dbenv,						\
		   "Received master record from %d, master is %d",	\
		   eid, rep->master_id);				\
		ret = EINVAL;						\
		goto errlock;						\
	}								\
} while (0)

#define	MASTER_UPDATE(dbenv, renv) do {					\
	MUTEX_LOCK((dbenv), &(renv)->mutex);				\
	F_SET((renv), DB_REGENV_REPLOCKED);				\
	(void)time(&(renv)->op_timestamp);				\
	MUTEX_UNLOCK((dbenv), &(renv)->mutex);				\
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
	DB_LOGC *logc;
	DB_LSN endlsn, lsn, oldfilelsn;
	DB_REP *db_rep;
	DBT *d, data_dbt, mylog;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;
	REP_CONTROL *rp;
	REP_VOTE_INFO *vi;
	u_int32_t bytes, egen, flags, gen, gbytes, rectype, type;
	int check_limit, cmp, done, do_req, is_dup;
	int master, match, old, recovering, ret, t_ret;
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
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (rep->start_th != 0) {
		/*
		 * If we're racing with a thread in rep_start, then
		 * just ignore the message and return.
		 */
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Racing rep_start, ignore message."));
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		goto out;
	}
	rep->msg_th++;
	gen = rep->gen;
	recovering = rep->in_recovery || F_ISSET(rep, REP_F_RECOVER_MASK);
	savetime = renv->rep_timestamp;

	rep->stat.st_msgs_processed++;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

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
				    NULL, NULL, 0);
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
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Updating gen from %lu to %lu",
			    (u_long)gen, (u_long)rp->gen));
			rep->master_id = DB_EID_INVALID;
			gen = rep->gen = rp->gen;
			/*
			 * Updating of egen will happen when we process the
			 * message below for each message type.
			 */
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			if (rp->rectype == REP_ALIVE)
				(void)__rep_send_message(dbenv,
				    DB_EID_BROADCAST, REP_MASTER_REQ, NULL,
				    NULL, 0);
		} else if (rp->rectype != REP_NEWMASTER) {
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0);
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
	if (recovering) {
		switch (rp->rectype) {
		case REP_VERIFY:
			MUTEX_LOCK(dbenv, db_rep->db_mutexp);
			cmp = log_compare(&lp->verify_lsn, &rp->lsn);
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			if (cmp != 0)
				goto skip;
			break;
		case REP_NEWFILE:
		case REP_LOG:
		case REP_LOG_MORE:
			if (!F_ISSET(rep, REP_F_RECOVER_LOG))
				goto skip;
			/*
			 * If we're recovering the log we only want
			 * log records that are in the range we need
			 * to recover.  Otherwise we can end up storing
			 * a huge number of "new" records, only to
			 * truncate the temp database later after we
			 * run recovery.
			 */
			if (log_compare(&rp->lsn, &rep->last_lsn) > 0)
				goto skip;
			break;
		case REP_ALIVE:
		case REP_ALIVE_REQ:
		case REP_DUPMASTER:
		case REP_FILE_FAIL:
		case REP_NEWCLIENT:
		case REP_NEWMASTER:
		case REP_NEWSITE:
		case REP_PAGE:
		case REP_PAGE_FAIL:
		case REP_PAGE_MORE:
		case REP_PAGE_REQ:
		case REP_UPDATE:
		case REP_UPDATE_REQ:
		case REP_VERIFY_FAIL:
		case REP_VOTE1:
		case REP_VOTE2:
			break;
		default:
skip:
			/* Check for need to retransmit. */
			/* Not holding rep_mutex, may miscount */
			rep->stat.st_msgs_recover++;
			MUTEX_LOCK(dbenv, db_rep->db_mutexp);
			do_req = __rep_check_doreq(dbenv, rep);
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			if (do_req) {
				/*
				 * Don't respond to a MASTER_REQ with
				 * a MASTER_REQ.
				 */
				if (rep->master_id == DB_EID_INVALID &&
				    rp->rectype != REP_MASTER_REQ)
					(void)__rep_send_message(dbenv,
					    DB_EID_BROADCAST,
					    REP_MASTER_REQ,
					    NULL, NULL, 0);
				else if (*eidp == rep->master_id)
					ret = __rep_resend_req(dbenv, *eidp);
			}
			goto errlock;
		}
	}

	switch (rp->rectype) {
	case REP_ALIVE:
		ANYSITE(rep);
		egen = *(u_int32_t *)rec->data;
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
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
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		break;
	case REP_ALIVE_REQ:
		ANYSITE(rep);
		dblp = dbenv->lg_handle;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		egen = rep->egen;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		data_dbt.data = &egen;
		data_dbt.size = sizeof(egen);
		(void)__rep_send_message(dbenv,
		    *eidp, REP_ALIVE, &lsn, &data_dbt, 0);
		goto errlock;
	case REP_DUPMASTER:
		if (F_ISSET(rep, REP_F_MASTER))
			ret = DB_REP_DUPMASTER;
		goto errlock;
	case REP_ALL_REQ:
		MASTER_ONLY(rep, rp);
		gbytes  = bytes = 0;
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		gbytes = rep->gbytes;
		bytes = rep->bytes;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		check_limit = gbytes != 0 || bytes != 0;
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		memset(&data_dbt, 0, sizeof(data_dbt));
		oldfilelsn = lsn = rp->lsn;
		type = REP_LOG;
		flags = IS_ZERO_LSN(rp->lsn) ||
		    IS_INIT_LSN(rp->lsn) ?  DB_FIRST : DB_SET;
		for (ret = __log_c_get(logc, &lsn, &data_dbt, flags);
		    ret == 0 && type == REP_LOG;
		    ret = __log_c_get(logc, &lsn, &data_dbt, DB_NEXT)) {
			/*
			 * When a log file changes, we'll have a real log
			 * record with some lsn [n][m], and we'll also want
			 * to send a NEWFILE message with lsn [n-1][MAX].
			 */
			if (lsn.file != oldfilelsn.file)
				(void)__rep_send_message(dbenv,
				    *eidp, REP_NEWFILE, &oldfilelsn, NULL, 0);
			if (check_limit) {
				/*
				 * data_dbt.size is only the size of the log
				 * record;  it doesn't count the size of the
				 * control structure. Factor that in as well
				 * so we're not off by a lot if our log records
				 * are small.
				 */
				while (bytes <
				    data_dbt.size + sizeof(REP_CONTROL)) {
					if (gbytes > 0) {
						bytes += GIGABYTE;
						--gbytes;
						continue;
					}
					/*
					 * We don't hold the rep mutex,
					 * and may miscount.
					 */
					rep->stat.st_nthrottles++;
					type = REP_LOG_MORE;
					goto send;
				}
				bytes -= (data_dbt.size + sizeof(REP_CONTROL));
			}

send:			if (__rep_send_message(dbenv, *eidp, type,
			    &lsn, &data_dbt, DB_LOG_RESEND) != 0)
				break;

			/*
			 * If we are about to change files, then we'll need the
			 * last LSN in the previous file.  Save it here.
			 */
			oldfilelsn = lsn;
			oldfilelsn.offset += logc->c_len;
		}

		if (ret == DB_NOTFOUND)
			ret = 0;
		if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		goto errlock;
#ifdef NOTYET
	case REP_FILE: /* TODO */
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		break;
	case REP_FILE_REQ:
		MASTER_ONLY(rep, rp);
		ret = __rep_send_file(dbenv, rec, *eidp);
		goto errlock;
#endif
	case REP_FILE_FAIL:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		/*
		 * XXX
		 */
		break;
	case REP_LOG:
	case REP_LOG_MORE:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		is_dup = 0;
		ret = __rep_apply(dbenv, rp, rec, ret_lsnp, &is_dup);
		switch (ret) {
		/*
		 * We're in an internal backup and we've gotten 
		 * all the log we need to run recovery.  Do so now.
		 */
		case DB_REP_LOGREADY:
			if ((ret = __log_flush(dbenv, NULL)) != 0)
				goto errlock;
			if ((ret = __rep_verify_match(dbenv, &rep->last_lsn,
			    savetime)) == 0) {
				MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
				ZERO_LSN(rep->first_lsn);
				ZERO_LSN(rep->last_lsn);
				F_CLR(rep, REP_F_RECOVER_LOG);
				MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			}
			break;
		/*
		 * If we get any of the "normal" returns, we only process
		 * LOG_MORE if this is not a duplicate record.  If the 
		 * record is a duplicate we don't want to handle LOG_MORE
		 * and request a multiple data stream (or trigger internal
		 * initialization) since this could be a very old record
		 * that no longer exists on the master.
		 */
		case DB_REP_ISPERM:
		case DB_REP_NOTPERM:
		case 0:
			if (is_dup)
				goto errlock;
			else
				break;
		/*
		 * Any other return (errors), we're done.
		 */
		default:
			goto errlock;
		}
		if (rp->rectype == REP_LOG_MORE) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			master = rep->master_id;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			/*
			 * If the master_id is invalid, this means that since
			 * the last record was sent, somebody declared an
			 * election and we may not have a master to request
			 * things of.
			 *
			 * This is not an error;  when we find a new master,
			 * we'll re-negotiate where the end of the log is and
			 * try to bring ourselves up to date again anyway.
			 */
			MUTEX_LOCK(dbenv, db_rep->db_mutexp);
			if (master == DB_EID_INVALID)
				ret = 0;
			/*
			 * If we've asked for a bunch of records, it could
			 * either be from a LOG_REQ or ALL_REQ.  If we're
			 * waiting for a gap to be filled, call loggap_req,
			 * otherwise use ALL_REQ again.
			 */
			else if (IS_ZERO_LSN(lp->waiting_lsn)) {
				MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
				if (__rep_send_message(dbenv,
				    master, REP_ALL_REQ, &lsn, NULL, 0) != 0)
					break;
			} else {
				__rep_loggap_req(dbenv, rep, &lsn, 1);
				MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			}
		}
		goto errlock;
	case REP_LOG_REQ:
		MASTER_ONLY(rep, rp);
		if (rec != NULL && rec->size != 0) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "[%lu][%lu]: LOG_REQ max lsn: [%lu][%lu]",
			    (u_long) rp->lsn.file, (u_long)rp->lsn.offset,
			    (u_long)((DB_LSN *)rec->data)->file,
			    (u_long)((DB_LSN *)rec->data)->offset));
		}
		/*
		 * There are three different cases here.
		 * 1. We asked for a particular LSN and got it.
		 * 2. We asked for an LSN and it's not found because it is
		 *	beyond the end of a log file and we need a NEWFILE msg.
		 *	and then the record that was requested.
		 * 3. We asked for an LSN and it simply doesn't exist, but
		 *    doesn't meet any of those other criteria, in which case
		 *    it's an error (that should never happen).
		 * If we have a valid LSN and the request has a data_dbt with
		 * it, then we need to send all records up to the LSN in the
		 * data dbt.
		 */
		oldfilelsn = lsn = rp->lsn;
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		memset(&data_dbt, 0, sizeof(data_dbt));
		ret = __log_c_get(logc, &lsn, &data_dbt, DB_SET);

		if (ret == 0) /* Case 1 */
			(void)__rep_send_message(dbenv,
			   *eidp, REP_LOG, &lsn, &data_dbt, DB_LOG_RESEND);
		else if (ret == DB_NOTFOUND) {
			R_LOCK(dbenv, &dblp->reginfo);
			endlsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			if (endlsn.file > lsn.file) {
				/*
				 * Case 2:
				 * Need to find the LSN of the last record in
				 * file lsn.file so that we can send it with
				 * the NEWFILE call.  In order to do that, we
				 * need to try to get {lsn.file + 1, 0} and
				 * then backup.
				 */
				endlsn.file = lsn.file + 1;
				endlsn.offset = 0;
				if ((ret = __log_c_get(logc,
				    &endlsn, &data_dbt, DB_SET)) != 0 ||
				    (ret = __log_c_get(logc,
					&endlsn, &data_dbt, DB_PREV)) != 0) {
					RPRINT(dbenv, rep, (dbenv, &mb,
					    "Unable to get prev of [%lu][%lu]",
					    (u_long)lsn.file,
					    (u_long)lsn.offset));
					/*
					 * We want to push the error back
					 * to the client so that the client
					 * does an internal backup.  The
					 * client asked for a log record
					 * we no longer have and it is
					 * outdated.
					 * XXX - This could be optimized by
					 * having the master perform and
					 * send a REP_UPDATE message.  We
					 * currently want the client to set
					 * up its 'update' state prior to
					 * requesting REP_UPDATE_REQ.
					 */
					ret = 0;
					(void)__rep_send_message(dbenv, *eidp,
					    REP_VERIFY_FAIL, &rp->lsn, NULL, 0);
				} else {
					endlsn.offset += logc->c_len;
					(void)__rep_send_message(dbenv, *eidp,
					    REP_NEWFILE, &endlsn, NULL, 0);
				}
			} else {
				/* Case 3 */
				__db_err(dbenv,
				    "Request for LSN [%lu][%lu] fails",
				    (u_long)lsn.file, (u_long)lsn.offset);
				DB_ASSERT(0);
				ret = EINVAL;
			}
		}

		/*
		 * If the user requested a gap, send the whole thing,
		 * while observing the limits from set_rep_limit.
		 */
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		gbytes = rep->gbytes;
		bytes = rep->bytes;
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		check_limit = gbytes != 0 || bytes != 0;
		type = REP_LOG;
		while (ret == 0 && rec != NULL && rec->size != 0 &&
		    type == REP_LOG) {
			if ((ret =
			    __log_c_get(logc, &lsn, &data_dbt, DB_NEXT)) != 0) {
				if (ret == DB_NOTFOUND)
					ret = 0;
				break;
			}
			if (log_compare(&lsn, (DB_LSN *)rec->data) >= 0)
				break;
			/*
			 * When a log file changes, we'll have a real log
			 * record with some lsn [n][m], and we'll also want
			 * to send a NEWFILE message with lsn [n-1][MAX].
			 */
			if (lsn.file != oldfilelsn.file)
				(void)__rep_send_message(dbenv,
				    *eidp, REP_NEWFILE, &oldfilelsn, NULL, 0);
			if (check_limit) {
				/*
				 * data_dbt.size is only the size of the log
				 * record;  it doesn't count the size of the
				 * control structure. Factor that in as well
				 * so we're not off by a lot if our log records
				 * are small.
				 */
				while (bytes <
				    data_dbt.size + sizeof(REP_CONTROL)) {
					if (gbytes > 0) {
						bytes += GIGABYTE;
						--gbytes;
						continue;
					}
					/*
					 * We don't hold the rep mutex,
					 * and may miscount.
					 */
					rep->stat.st_nthrottles++;
					type = REP_LOG_MORE;
					goto send1;
				}
				bytes -= (data_dbt.size + sizeof(REP_CONTROL));
			}

send1:			 if (__rep_send_message(dbenv, *eidp, type,
			    &lsn, &data_dbt, DB_LOG_RESEND) != 0)
				break;
			/*
			 * If we are about to change files, then we'll need the
			 * last LSN in the previous file.  Save it here.
			 */
			oldfilelsn = lsn;
			oldfilelsn.offset += logc->c_len;
		}

		if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		goto errlock;
	case REP_NEWSITE:
		/* We don't hold the rep mutex, and may miscount. */
		rep->stat.st_newsites++;

		/* This is a rebroadcast; simply tell the application. */
		if (F_ISSET(rep, REP_F_MASTER)) {
			dblp = dbenv->lg_handle;
			lp = dblp->reginfo.primary;
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
		}
		ret = DB_REP_NEWSITE;
		goto errlock;
	case REP_NEWCLIENT:
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
		    DB_EID_BROADCAST, REP_NEWSITE, &rp->lsn, rec, 0);

		ret = DB_REP_NEWSITE;

		if (F_ISSET(rep, REP_F_CLIENT)) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			egen = rep->egen;
			if (*eidp == rep->master_id)
				rep->master_id = DB_EID_INVALID;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			data_dbt.data = &egen;
			data_dbt.size = sizeof(egen);
			(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
			    REP_ALIVE, &rp->lsn, &data_dbt, 0);
			goto errlock;
		}
		/* FALLTHROUGH */
	case REP_MASTER_REQ:
		if (F_ISSET(rep, REP_F_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_NEWMASTER, &lsn, NULL, 0);
		}
		/*
		 * If there is no master, then we could get into a state
		 * where an old client lost the initial ALIVE message and
		 * is calling an election under an old gen and can
		 * never get to the current gen.
		 */
		if (F_ISSET(rep, REP_F_CLIENT) && rp->gen < gen) {
			MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
			egen = rep->egen;
			if (*eidp == rep->master_id)
				rep->master_id = DB_EID_INVALID;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			data_dbt.data = &egen;
			data_dbt.size = sizeof(egen);
			(void)__rep_send_message(dbenv, *eidp,
			    REP_ALIVE, &rp->lsn, &data_dbt, 0);
			goto errlock;
		}
		goto errlock;
	case REP_NEWFILE:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		ret = __rep_apply(dbenv, rp, rec, ret_lsnp, NULL);
		goto errlock;
	case REP_NEWMASTER:
		ANYSITE(rep);
		if (F_ISSET(rep, REP_F_MASTER) &&
		    *eidp != dbenv->rep_eid) {
			/* We don't hold the rep mutex, and may miscount. */
			rep->stat.st_dupmasters++;
			ret = DB_REP_DUPMASTER;
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_DUPMASTER, NULL, NULL, 0);
			goto errlock;
		}
		ret = __rep_new_master(dbenv, rp, *eidp);
		goto errlock;
	case REP_PAGE:
	case REP_PAGE_MORE:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		ret = __rep_page(dbenv, *eidp, rp, rec);
		break;
	case REP_PAGE_FAIL:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		ret = __rep_page_fail(dbenv, *eidp, rec);
		break;
	case REP_PAGE_REQ:
		MASTER_ONLY(rep, rp);
		MASTER_UPDATE(dbenv, renv);
		ret = __rep_page_req(dbenv, *eidp, rec);
		break;
	case REP_UPDATE:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);

		ret = __rep_update_setup(dbenv, *eidp, rp, rec);
		break;
	case REP_UPDATE_REQ:
		MASTER_ONLY(rep, rp);
		infop = dbenv->reginfo;
		renv = infop->primary;
		MASTER_UPDATE(dbenv, renv);
		ret = __rep_update_req(dbenv, *eidp);
		break;
	case REP_VERIFY:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		if (IS_ZERO_LSN(lp->verify_lsn))
			goto errlock;

		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
		memset(&mylog, 0, sizeof(mylog));
		if ((ret = __log_c_get(logc, &rp->lsn, &mylog, DB_SET)) != 0)
			goto rep_verify_err;
		match = 0;
		memcpy(&rectype, mylog.data, sizeof(rectype));
		if (mylog.size == rec->size &&
		    memcmp(mylog.data, rec->data, rec->size) == 0)
			match = 1;
		DB_ASSERT(rectype == DB___txn_ckp);
		/*
		 * If we don't have a match, backup to the previous
		 * checkpoint and try again.
		 */
		if (match == 0) {
			ZERO_LSN(lsn);
			if ((ret = __log_backup(dbenv, logc, &rp->lsn, &lsn,
			    LASTCKP_CMP)) == 0) {
				MUTEX_LOCK(dbenv, db_rep->db_mutexp);
				lp->verify_lsn = lsn;
				lp->rcvd_recs = 0;
				lp->wait_recs = rep->request_gap;
				MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
				(void)__rep_send_message(dbenv,
				    *eidp, REP_VERIFY_REQ, &lsn, NULL, 0);
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
					ret = EINVAL;
				} else {
					rep->stat.st_outdated++;

					R_LOCK(dbenv, &dblp->reginfo);
					lsn = lp->lsn;
					R_UNLOCK(dbenv, &dblp->reginfo);
					MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
					F_CLR(rep, REP_F_RECOVER_VERIFY);
					F_SET(rep, REP_F_RECOVER_UPDATE);
					ZERO_LSN(rep->first_lsn);
					MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
					(void)__rep_send_message(dbenv,
					    *eidp, REP_UPDATE_REQ, NULL,
					    NULL, 0);
				}
			}
		} else
			ret = __rep_verify_match(dbenv, &rp->lsn, savetime);

rep_verify_err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
			ret = t_ret;
		goto errlock;
	case REP_VERIFY_FAIL:
		CLIENT_ONLY(rep, rp);
		MASTER_CHECK(dbenv, *eidp, rep);
		/*
		 * If any recovery flags are set, but not VERIFY,
		 * then we ignore this message.  We are already
		 * in the middle of updating.
		 */
		if (F_ISSET(rep, REP_F_RECOVER_MASK) &&
		    !F_ISSET(rep, REP_F_RECOVER_VERIFY))
			goto errlock;
		rep->stat.st_outdated++;

		MUTEX_LOCK(dbenv, db_rep->db_mutexp);
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		/*
		 * We don't want an old or delayed VERIFY_FAIL
		 * message to throw us into internal initialization
		 * when we shouldn't be.  
		 *
		 * Only go into internal initialization if:
		 * We are in RECOVER_VERIFY and this LSN == verify_lsn.
		 * We are not in any RECOVERY and we are expecting
		 *    an LSN that no longer exists on the master.
		 * Otherwise, ignore this message.
		 */
		if (((F_ISSET(rep, REP_F_RECOVER_VERIFY)) &&
		    log_compare(&rp->lsn, &lp->verify_lsn) == 0) ||
		    (F_ISSET(rep, REP_F_RECOVER_MASK) == 0 &&
		    log_compare(&rp->lsn, &lp->ready_lsn) >= 0)) {
			F_CLR(rep, REP_F_RECOVER_VERIFY);
			F_SET(rep, REP_F_RECOVER_UPDATE);
			ZERO_LSN(rep->first_lsn);
			lp->wait_recs = rep->request_gap;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_UPDATE_REQ, NULL, NULL, 0);
		} else {
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
		}
		goto errlock;
	case REP_VERIFY_REQ:
		MASTER_ONLY(rep, rp);
		type = REP_VERIFY;
		if ((ret = __log_cursor(dbenv, &logc)) != 0)
			goto errlock;
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
		 */
		if (ret == DB_NOTFOUND &&
		    __log_is_outdated(dbenv, rp->lsn.file, &old) == 0 &&
		    old != 0)
			type = REP_VERIFY_FAIL;

		if (ret != 0)
			d = NULL;

		(void)__rep_send_message(dbenv, *eidp, type, &rp->lsn, d, 0);
		ret = __log_c_close(logc);
		goto errlock;
	case REP_VOTE1:
		if (F_ISSET(rep, REP_F_MASTER)) {
			RPRINT(dbenv, rep,
			    (dbenv, &mb, "Master received vote"));
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
			goto errlock;
		}

		vi = (REP_VOTE_INFO *)rec->data;
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);

		/*
		 * If we get a vote from a later election gen, we
		 * clear everything from the current one, and we'll
		 * start over by tallying it.  If we get an old vote,
		 * send an ALIVE to the old participant.
		 */
		RPRINT(dbenv, rep, (dbenv, &mb,
		    "Received vote1 egen %lu, egen %lu",
		    (u_long)vi->egen, (u_long)rep->egen));
		if (vi->egen < rep->egen) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Received old vote %lu, egen %lu, ignoring vote1",
			    (u_long)vi->egen, (u_long)rep->egen));
			egen = rep->egen;
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
			data_dbt.data = &egen;
			data_dbt.size = sizeof(egen);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_ALIVE, &rp->lsn, &data_dbt, 0);
			goto errlock;
		}
		if (vi->egen > rep->egen) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Received VOTE1 from egen %lu, my egen %lu; reset",
			    (u_long)vi->egen, (u_long)rep->egen));
			__rep_elect_done(dbenv, rep);
			rep->egen = vi->egen;
		}
		if (!IN_ELECTION(rep))
			F_SET(rep, REP_F_TALLY);

		/* Check if this site knows about more sites than we do. */
		if (vi->nsites > rep->nsites)
			rep->nsites = vi->nsites;

		/* Check if this site requires more votes than we do. */
		if (vi->nvotes > rep->nvotes)
			rep->nvotes = vi->nvotes;

		/*
		 * We are keeping the vote, let's see if that changes our
		 * count of the number of sites.
		 */
		if (rep->sites + 1 > rep->nsites)
			rep->nsites = rep->sites + 1;
		if (rep->nsites > rep->asites &&
		    (ret = __rep_grow_sites(dbenv, rep->nsites)) != 0) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Grow sites returned error %d", ret));
			goto errunlock;
		}

		/*
		 * Ignore vote1's if we're in phase 2.
		 */
		if (F_ISSET(rep, REP_F_EPHASE2)) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "In phase 2, ignoring vote1"));
			goto errunlock;
		}

		/*
		 * Record this vote.  If we get back non-zero, we
		 * ignore the vote.
		 */
		if ((ret = __rep_tally(dbenv, rep, *eidp, &rep->sites,
		    vi->egen, rep->tally_off)) != 0) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Tally returned %d, sites %d",
			    ret, rep->sites));
			ret = 0;
			goto errunlock;
		}
		RPRINT(dbenv, rep, (dbenv, &mb,
	    "Incoming vote: (eid)%d (pri)%d (gen)%lu (egen)%lu [%lu,%lu]",
		    *eidp, vi->priority,
		    (u_long)rp->gen, (u_long)vi->egen,
		    (u_long)rp->lsn.file, (u_long)rp->lsn.offset));
#ifdef DIAGNOSTIC
		if (rep->sites > 1)
			RPRINT(dbenv, rep, (dbenv, &mb,
	    "Existing vote: (eid)%d (pri)%d (gen)%lu (sites)%d [%lu,%lu]",
			    rep->winner, rep->w_priority,
			    (u_long)rep->w_gen, rep->sites,
			    (u_long)rep->w_lsn.file,
			    (u_long)rep->w_lsn.offset));
#endif
		__rep_cmp_vote(dbenv, rep, eidp, &rp->lsn, vi->priority,
		    rp->gen, vi->tiebreaker);
		/*
		 * If you get a vote and you're not in an election, we've
		 * already recorded this vote.  But that is all we need
		 * to do.
		 */
		if (!IN_ELECTION(rep)) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Not in election, but received vote1 0x%x",
			    rep->flags));
			ret = DB_REP_HOLDELECTION;
			goto errunlock;
		}

		master = rep->winner;
		lsn = rep->w_lsn;
		/*
		 * We need to check sites == nsites, not more than half
		 * like we do in __rep_elect and the VOTE2 code below.  The
		 * reason is that we want to process all the incoming votes
		 * and not short-circuit once we reach more than half.  The
		 * real winner's vote may be in the last half.
		 */
		done = rep->sites >= rep->nsites && rep->w_priority != 0;
		if (done) {
			RPRINT(dbenv, rep,
			    (dbenv, &mb, "Phase1 election done"));
			RPRINT(dbenv, rep, (dbenv, &mb, "Voting for %d%s",
			    master, master == rep->eid ? "(self)" : ""));
			egen = rep->egen;
			F_SET(rep, REP_F_EPHASE2);
			F_CLR(rep, REP_F_EPHASE1);
			if (master == rep->eid) {
				(void)__rep_tally(dbenv, rep, rep->eid,
				    &rep->votes, egen, rep->v2tally_off);
				goto errunlock;
			}
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

			/* Vote for someone else. */
			__rep_send_vote(dbenv, NULL, 0, 0, 0, 0, egen,
			    master, REP_VOTE2);
		} else
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

		/* Election is still going on. */
		break;
	case REP_VOTE2:
		RPRINT(dbenv, rep, (dbenv, &mb, "We received a vote%s",
		    F_ISSET(rep, REP_F_MASTER) ? " (master)" : ""));
		if (F_ISSET(rep, REP_F_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			rep->stat.st_elections_won++;
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
			goto errlock;
		}

		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);

		/* If we have priority 0, we should never get a vote. */
		DB_ASSERT(rep->priority != 0);

		/*
		 * We might be the last to the party and we haven't had
		 * time to tally all the vote1's, but others have and
		 * decided we're the winner.  So, if we're in the process
		 * of tallying sites, keep the vote so that when our
		 * election thread catches up we'll have the votes we
		 * already received.
		 */
		vi = (REP_VOTE_INFO *)rec->data;
		if (!IN_ELECTION_TALLY(rep) && vi->egen >= rep->egen) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Not in election gen %lu, at %lu, got vote",
			    (u_long)vi->egen, (u_long)rep->egen));
			ret = DB_REP_HOLDELECTION;
			goto errunlock;
		}

		/*
		 * Record this vote.  In a VOTE2, the only valid entry
		 * in the REP_VOTE_INFO is the election generation.
		 *
		 * There are several things which can go wrong that we
		 * need to account for:
		 * 1. If we receive a latent VOTE2 from an earlier election,
		 * we want to ignore it.
		 * 2. If we receive a VOTE2 from a site from which we never
		 * received a VOTE1, we want to ignore it.
		 * 3. If we have received a duplicate VOTE2 from this election
		 * from the same site we want to ignore it.
		 * 4. If this is from the current election and someone is
		 * really voting for us, then we finally get to record it.
		 */
		/*
		 * __rep_cmp_vote2 checks for cases 1 and 2.
		 */
		if ((ret = __rep_cmp_vote2(dbenv, rep, *eidp, vi->egen)) != 0) {
			ret = 0;
			goto errunlock;
		}
		/*
		 * __rep_tally takes care of cases 3 and 4.
		 */
		if ((ret = __rep_tally(dbenv, rep, *eidp, &rep->votes,
		    vi->egen, rep->v2tally_off)) != 0) {
			ret = 0;
			goto errunlock;
		}
		done = rep->votes >= rep->nvotes;
		RPRINT(dbenv, rep, (dbenv, &mb, "Counted vote %d of %d",
		    rep->votes, rep->nvotes));
		if (done) {
			__rep_elect_master(dbenv, rep, eidp);
			ret = DB_REP_NEWMASTER;
			goto errunlock;
		} else
			MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		break;
	default:
		__db_err(dbenv,
	"DB_ENV->rep_process_message: unknown replication message: type %lu",
		   (u_long)rp->rectype);
		ret = EINVAL;
		goto errlock;
	}

	/*
	 * If we already hold rep_mutexp then we goto 'errunlock'
	 * Otherwise we goto 'errlock' to acquire it before we
	 * decrement our message thread count.
	 */
errlock:
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
errunlock:
	rep->msg_th--;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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
 */
static int
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
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	lp = dblp->reginfo.primary;
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    log_compare(&lp->ready_lsn, &rep->first_lsn) < 0)
		lp->ready_lsn = rep->first_lsn;
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
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

		while (ret == 0 &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0) {
			/*
			 * We just filled in a gap in the log record stream.
			 * Write subsequent records to the log.
			 */
gap_check:
			lp->rcvd_recs = 0;
			ZERO_LSN(lp->max_wait_lsn);
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

			if ((ret = __rep_getnext(dbenv)) == DB_NOTFOUND) {
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
			 * still be waiting for more records.
			 */
			if (__rep_check_doreq(dbenv, rep))
				__rep_loggap_req(dbenv, rep, &rp->lsn, 0);
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
		if (__rep_check_doreq(dbenv, rep))
			__rep_loggap_req(dbenv, rep, &rp->lsn, 0);

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
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (ret == 0 &&
	    F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    log_compare(&lp->ready_lsn, &rep->last_lsn) >= 0) {
		rep->last_lsn = max_lsn;
		ZERO_LSN(max_lsn);
		ret = DB_REP_LOGREADY;
	}
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

	if (ret == 0 && !F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    !IS_ZERO_LSN(max_lsn)) {
		if (ret_lsnp != NULL)
			*ret_lsnp = max_lsn;
		ret = DB_REP_ISPERM;
		DB_ASSERT(log_compare(&max_lsn, &lp->max_perm_lsn) >= 0);
		lp->max_perm_lsn = max_lsn;
	}
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

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
	LSN_COLLECTION lc;
	REP *rep;
	__txn_regop_args *txn_args;
	__txn_xa_regop_args *prep_args;
	u_int32_t lockid, rectype;
	u_int i;
	int ret, t_ret;
	void *txninfo;

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
	if ((ret = __lock_id(dbenv, &lockid)) != 0)
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
 * __rep_tally --
 * PUBLIC: int __rep_tally __P((DB_ENV *, REP *, int, int *,
 * PUBLIC:    u_int32_t, roff_t));
 *
 * Handle incoming vote1 message on a client.  Called with the db_rep
 * mutex held.  This function will return 0 if we successfully tally
 * the vote and non-zero if the vote is ignored.  This will record
 * both VOTE1 and VOTE2 records, depending on which region offset the
 * caller passed in.
 */
int
__rep_tally(dbenv, rep, eid, countp, egen, vtoff)
	DB_ENV *dbenv;
	REP *rep;
	int eid, *countp;
	u_int32_t egen;
	roff_t vtoff;
{
	REP_VTALLY *tally, *vtp;
	int i;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#else
	COMPQUIET(rep, NULL);
#endif

	tally = R_ADDR((REGINFO *)dbenv->reginfo, vtoff);
	i = 0;
	vtp = &tally[i];
	while (i < *countp) {
		/*
		 * Ignore votes from earlier elections (i.e. we've heard
		 * from this site in this election, but its vote from an
		 * earlier election got delayed and we received it now).
		 * However, if we happened to hear from an earlier vote
		 * and we recorded it and we're now hearing from a later
		 * election we want to keep the updated one.  Note that
		 * updating the entry will not increase the count.
		 * Also ignore votes that are duplicates.
		 */
		if (vtp->eid == eid) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Tally found[%d] (%d, %lu), this vote (%d, %lu)",
				    i, vtp->eid, (u_long)vtp->egen,
				    eid, (u_long)egen));
			if (vtp->egen >= egen)
				return (1);
			else {
				vtp->egen = egen;
				return (0);
			}
		}
		i++;
		vtp = &tally[i];
	}
	/*
	 * If we get here, we have a new voter we haven't
	 * seen before.  Tally this vote.
	 */
#ifdef DIAGNOSTIC
	if (vtoff == rep->tally_off)
		RPRINT(dbenv, rep, (dbenv, &mb, "Tallying VOTE1[%d] (%d, %lu)",
		    i, eid, (u_long)egen));
	else
		RPRINT(dbenv, rep, (dbenv, &mb, "Tallying VOTE2[%d] (%d, %lu)",
		    i, eid, (u_long)egen));
#endif
	vtp->eid = eid;
	vtp->egen = egen;
	(*countp)++;
	return (0);
}

/*
 * __rep_cmp_vote --
 * PUBLIC: void __rep_cmp_vote __P((DB_ENV *, REP *, int *, DB_LSN *,
 * PUBLIC:     int, u_int32_t, u_int32_t));
 *
 * Compare incoming vote1 message on a client.  Called with the db_rep
 * mutex held.
 */
void
__rep_cmp_vote(dbenv, rep, eidp, lsnp, priority, gen, tiebreaker)
	DB_ENV *dbenv;
	REP *rep;
	int *eidp;
	DB_LSN *lsnp;
	int priority;
	u_int32_t gen, tiebreaker;
{
	int cmp;

#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#else
	COMPQUIET(dbenv, NULL);
#endif
	cmp = log_compare(lsnp, &rep->w_lsn);
	/*
	 * If we've seen more than one, compare us to the best so far.
	 * If we're the first, make ourselves the winner to start.
	 */
	if (rep->sites > 1 && priority != 0) {
		/*
		 * LSN is primary determinant. Then priority if LSNs
		 * are equal, then tiebreaker if both are equal.
		 */
		if (cmp > 0 ||
		    (cmp == 0 && (priority > rep->w_priority ||
		    (priority == rep->w_priority &&
		    (tiebreaker > rep->w_tiebreaker))))) {
			RPRINT(dbenv, rep, (dbenv, &mb, "Accepting new vote"));
			rep->winner = *eidp;
			rep->w_priority = priority;
			rep->w_lsn = *lsnp;
			rep->w_gen = gen;
			rep->w_tiebreaker = tiebreaker;
		}
	} else if (rep->sites == 1) {
		if (priority != 0) {
			/* Make ourselves the winner to start. */
			rep->winner = *eidp;
			rep->w_priority = priority;
			rep->w_gen = gen;
			rep->w_lsn = *lsnp;
			rep->w_tiebreaker = tiebreaker;
		} else {
			rep->winner = DB_EID_INVALID;
			rep->w_priority = 0;
			rep->w_gen = 0;
			ZERO_LSN(rep->w_lsn);
			rep->w_tiebreaker = 0;
		}
	}
	return;
}

/*
 * __rep_cmp_vote2 --
 * PUBLIC: int __rep_cmp_vote2 __P((DB_ENV *, REP *, int, u_int32_t));
 *
 * Compare incoming vote2 message with vote1's we've recorded.  Called
 * with the db_rep mutex held.  We return 0 if the VOTE2 is from a
 * site we've heard from and it is from this election.  Otherwise we return 1.
 */
int
__rep_cmp_vote2(dbenv, rep, eid, egen)
	DB_ENV *dbenv;
	REP *rep;
	int eid;
	u_int32_t egen;
{
	int i;
	REP_VTALLY *tally, *vtp;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	tally = R_ADDR((REGINFO *)dbenv->reginfo, rep->tally_off);
	i = 0;
	vtp = &tally[i];
	for (i = 0; i < rep->sites; i++) {
		vtp = &tally[i];
		if (vtp->eid == eid && vtp->egen == egen) {
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "Found matching vote1 (%d, %lu), at %d of %d",
			    eid, (u_long)egen, i, rep->sites));
			return (0);
		}
	}
	RPRINT(dbenv, rep,
	    (dbenv, &mb, "Didn't find vote1 for eid %d, egen %lu",
	    eid, (u_long)egen));
	return (1);
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
 */
static int
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
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	done = savetime != renv->rep_timestamp;
	if (done) {
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
		return (0);
	}
	ZERO_LSN(lp->verify_lsn);
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	/*
	 * Make sure the world hasn't changed while we tried to get
	 * the lock.  If it hasn't then it's time for us to kick all
	 * operations out of DB and run recovery.
	 */
	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	if (!F_ISSET(rep, REP_F_RECOVER_LOG) &&
	    (F_ISSET(rep, REP_F_READY) || rep->in_recovery != 0)) {
		rep->stat.st_msgs_recover++;
		goto errunlock;
	}

	__rep_lockout(dbenv, db_rep, rep, 1);

	/* OK, everyone is out, we can now run recovery. */
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);

	if ((ret = __rep_dorecovery(dbenv, reclsnp, &trunclsn)) != 0) {
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		rep->in_recovery = 0;
		F_CLR(rep, REP_F_READY);
		goto errunlock;
	}

	/*
	 * The log has been truncated (either directly by us or by __db_apprec)
	 * We want to make sure we're waiting for the LSN at the new end-of-log,
	 * not some later point.
	 */
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
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
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
	ret = __db_truncate(db_rep->rep_db, NULL, &unused);
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	F_CLR(db_rep->rep_db, DB_AM_RECOVER);
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	rep->stat.st_log_queued = 0;
	rep->in_recovery = 0;
	F_CLR(rep, REP_F_NOARCHIVE | REP_F_RECOVER_MASK);

	if (ret != 0)
		goto errunlock;

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
	MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	if (master == DB_EID_INVALID)
		ret = 0;
	else
		(void)__rep_send_message(dbenv,
		    master, REP_ALL_REQ, reclsnp, NULL, 0);
	if (0) {
errunlock:
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	}
	return (ret);
}

/*
 * __rep_do_ckp --
 * Perform the memp_sync necessary for this checkpoint without holding
 * the db_rep->db_mutexp.  All callers of this function must hold the
 * db_rep->db_mutexp and must not be holding the db_rep->rep_mutexp.
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

	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	DB_TEST_CHECKPOINT(dbenv, dbenv->test_check);

	/* Sync the memory pool. */
	memcpy(&ckp_lsn, (u_int8_t *)rec->data +
	    SSZ(__txn_ckp_args, ckp_lsn), sizeof(DB_LSN));
	ret = __memp_sync(dbenv, &ckp_lsn);

	/* Update the last_ckp in the txn region. */
	if (ret == 0)
		__txn_updateckp(dbenv, &rp->lsn);
	else {
		__db_err(dbenv, "Error syncing ckp [%lu][%lu]",
		    (u_long)ckp_lsn.file, (u_long)ckp_lsn.offset);
		ret = __db_panic(dbenv, ret);
	}
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);

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
	u_int32_t rectype;

	db_rep = dbenv->rep_handle;
	dbp = db_rep->rep_db;

	if ((ret = __db_cursor(dbp, NULL, &dbc, 0)) != 0)
		return (ret);

	/* The DBTs need to persist through another call. */
	F_SET(cntrl, DB_DBT_REALLOC);
	F_SET(rec, DB_DBT_REALLOC);
	if ((ret = __db_c_get(dbc, cntrl, rec, DB_RMW | DB_FIRST)) == 0) {
		memcpy(&rectype, rec->data, sizeof(rectype));
		ret = __db_c_del(dbc, 0);
	}
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
		 * We do not want to hold the db_rep->db_mutexp
		 * mutex while syncing the mpool, so if we get
		 * a checkpoint record that we are supposed to
		 * process, we add it to the __db.rep.db, do
		 * the memp_sync and then go back and process
		 * it later, when the sync has finished.  If
		 * this record is already in the table, then
		 * some other thread will process it, so simply
		 * return REP_NOTPERM;
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
__rep_resend_req(dbenv, eid)
	DB_ENV *dbenv;
	int eid;
{

	DB_LOG *dblp;
	DB_LSN lsn;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	int ret;
	u_int32_t repflags;

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	repflags = rep->flags;
	if (FLD_ISSET(repflags, REP_F_RECOVER_VERIFY)) {
		MUTEX_LOCK(dbenv, db_rep->db_mutexp);
		lsn = lp->verify_lsn;
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
		if (!IS_ZERO_LSN(lsn))
			(void)__rep_send_message(dbenv, eid,
			    REP_VERIFY_REQ, &lsn, NULL, 0);
		goto out;
	} else if (FLD_ISSET(repflags, REP_F_RECOVER_UPDATE)) {
		(void)__rep_send_message(dbenv, eid,
		    REP_UPDATE_REQ, NULL, NULL, 0);
	} else if (FLD_ISSET(repflags, REP_F_RECOVER_PAGE)) {
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
		ret = __rep_pggap_req(dbenv, rep, NULL, 0);
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
	} else if (FLD_ISSET(repflags, REP_F_RECOVER_LOG)) {
		MUTEX_LOCK(dbenv, db_rep->db_mutexp);
		__rep_loggap_req(dbenv, rep, NULL, 0);
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
	}

out:
	return (ret);
}

/*
 * __rep_check_doreq --
 * PUBLIC: int __rep_check_doreq __P((DB_ENV *, REP *));
 *
 * Check if we need to send another request.  If so, compare with
 * the request limits the user might have set.  This assumes the
 * caller holds the db_rep->db_mutexp mutex.  Returns 1 if a request
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
 * __rep_lockout --
 * PUBLIC: void __rep_lockout __P((DB_ENV *, DB_REP *, REP *, u_int32_t));
 *
 * Coordinate with other threads in the library and active txns so
 * that we can run single-threaded, for recovery or internal backup.
 * Assumes the caller holds rep_mutexp.
 */
void
__rep_lockout(dbenv, db_rep, rep, msg_th)
	DB_ENV *dbenv;
	DB_REP *db_rep;
	REP *rep;
	u_int32_t msg_th;
{
	int wait_cnt;

	/* Phase 1: set REP_F_READY and wait for op_cnt to go to 0. */
	F_SET(rep, REP_F_READY);
	for (wait_cnt = 0; rep->op_cnt != 0;) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		__os_sleep(dbenv, 1, 0);
#ifdef DIAGNOSTIC
		if (++wait_cnt % 60 == 0)
			__db_err(dbenv,
	"Waiting for txn_cnt to run replication recovery/backup for %d minutes",
			wait_cnt / 60);
#endif
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	}

	/*
	 * Phase 2: set in_recovery and wait for handle count to go
	 * to 0 and for the number of threads in __rep_process_message
	 * to go to 1 (us).
	 */
	rep->in_recovery = 1;
	for (wait_cnt = 0; rep->handle_cnt != 0 || rep->msg_th > msg_th;) {
		MUTEX_UNLOCK(dbenv, db_rep->rep_mutexp);
		__os_sleep(dbenv, 1, 0);
#ifdef DIAGNOSTIC
		if (++wait_cnt % 60 == 0)
			__db_err(dbenv,
"Waiting for handle count to run replication recovery/backup for %d minutes",
			wait_cnt / 60);
#endif
		MUTEX_LOCK(dbenv, db_rep->rep_mutexp);
	}
}
