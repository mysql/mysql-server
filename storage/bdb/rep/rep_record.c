/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: rep_record.c,v 1.111 2002/09/11 19:39:11 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/rep.h"
#include "dbinc/txn.h"

static int __rep_apply __P((DB_ENV *, REP_CONTROL *, DBT *));
static int __rep_collect_txn __P((DB_ENV *, DB_LSN *, LSN_COLLECTION *));
static int __rep_lsn_cmp __P((const void *, const void *));
static int __rep_newfile __P((DB_ENV *, REP_CONTROL *, DBT *, DB_LSN *));

#define	IS_SIMPLE(R)	((R) != DB___txn_regop && \
    (R) != DB___txn_ckp && (R) != DB___dbreg_register)

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
 *
 * PUBLIC: int __rep_process_message __P((DB_ENV *, DBT *, DBT *, int *));
 */
int
__rep_process_message(dbenv, control, rec, eidp)
	DB_ENV *dbenv;
	DBT *control, *rec;
	int *eidp;
{
	DB_LOG *dblp;
	DB_LOGC *logc;
	DB_LSN init_lsn, lsn, newfilelsn, oldfilelsn;
	DB_REP *db_rep;
	DBT *d, data_dbt, lsndbt, mylog;
	LOG *lp;
	REP *rep;
	REP_CONTROL *rp;
	REP_VOTE_INFO *vi;
	u_int32_t bytes, gen, gbytes, type, unused;
	int check_limit, cmp, done, do_req, i;
	int master, old, recovering, ret, t_ret, *tally;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, "rep_stat", DB_INIT_TXN);

	/* Control argument must be non-Null. */
	if (control == NULL || control->size == 0) {
		__db_err(dbenv,
	"DB_ENV->rep_process_message: control argument must be specified");
		return (EINVAL);
	}

	ret = 0;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	MUTEX_LOCK(dbenv, db_rep->mutexp);
	gen = rep->gen;
	recovering = F_ISSET(rep, REP_F_RECOVER);

	rep->stat.st_msgs_processed++;
	MUTEX_UNLOCK(dbenv, db_rep->mutexp);

	rp = (REP_CONTROL *)control->data;

#if 0
	__rep_print_message(dbenv, *eidp, rp, "rep_process_message");
#endif

	/* Complain if we see an improper version number. */
	if (rp->rep_version != DB_REPVERSION) {
		__db_err(dbenv,
		    "unexpected replication message version %d, expected %d",
		    rp->rep_version, DB_REPVERSION);
		return (EINVAL);
	}
	if (rp->log_version != DB_LOGVERSION) {
		__db_err(dbenv,
		    "unexpected log record version %d, expected %d",
		    rp->log_version, DB_LOGVERSION);
		return (EINVAL);
	}

	/*
	 * Check for generation number matching.  Ignore any old messages
	 * except requests that are indicative of a new client that needs
	 * to get in sync.
	 */
	if (rp->gen < gen && rp->rectype != REP_ALIVE_REQ &&
	    rp->rectype != REP_NEWCLIENT && rp->rectype != REP_MASTER_REQ) {
		/*
		 * We don't hold the rep mutex, and could miscount if we race.
		 */
		rep->stat.st_msgs_badgen++;
		return (0);
	}
	if (rp->gen > gen && rp->rectype != REP_ALIVE &&
	    rp->rectype != REP_NEWMASTER)
		return (__rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_MASTER_REQ, NULL, NULL, 0));

	/*
	 * We need to check if we're in recovery and if we are
	 * then we need to ignore any messages except VERIFY, VOTE,
	 * ELECT (the master might fail while we are recovering), and
	 * ALIVE_REQ.
	 */
	if (recovering)
		switch(rp->rectype) {
			case REP_ALIVE:
			case REP_ALIVE_REQ:
			case REP_ELECT:
			case REP_NEWCLIENT:
			case REP_NEWMASTER:
			case REP_NEWSITE:
			case REP_VERIFY:
				R_LOCK(dbenv, &dblp->reginfo);
				cmp = log_compare(&lp->verify_lsn, &rp->lsn);
				R_UNLOCK(dbenv, &dblp->reginfo);
				if (cmp != 0)
					goto skip;
				/* FALLTHROUGH */
			case REP_VOTE1:
			case REP_VOTE2:
				break;
			default:
skip:				/*
				 * We don't hold the rep mutex, and could
				 * miscount if we race.
				 */
				rep->stat.st_msgs_recover++;

				/* Check for need to retransmit. */
				R_LOCK(dbenv, &dblp->reginfo);
				do_req = *eidp == rep->master_id &&
				    ++lp->rcvd_recs >= lp->wait_recs;
				if (do_req) {
					lp->wait_recs *= 2;
					if (lp->wait_recs + rep->max_gap)
						lp->wait_recs = rep->max_gap;
					lp->rcvd_recs = 0;
					lsn = lp->verify_lsn;
				}
				R_UNLOCK(dbenv, &dblp->reginfo);
				if (do_req)
					ret = __rep_send_message(dbenv, *eidp,
					    REP_VERIFY_REQ, &lsn, NULL, 0);

				return (ret);
		}

	switch(rp->rectype) {
	case REP_ALIVE:
		ANYSITE(dbenv);
		if (rp->gen > gen && rp->flags)
			return (__rep_new_master(dbenv, rp, *eidp));
		break;
	case REP_ALIVE_REQ:
		ANYSITE(dbenv);
		dblp = dbenv->lg_handle;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (__rep_send_message(dbenv,
		    *eidp, REP_ALIVE, &lsn, NULL,
		    F_ISSET(dbenv, DB_ENV_REP_MASTER) ? 1 : 0));
	case REP_ALL_REQ:
		MASTER_ONLY(dbenv);
		gbytes  = bytes = 0;
		MUTEX_LOCK(dbenv, db_rep->mutexp);
		gbytes = rep->gbytes;
		bytes = rep->bytes;
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		check_limit = gbytes != 0 || bytes != 0;
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
		memset(&data_dbt, 0, sizeof(data_dbt));
		oldfilelsn = lsn = rp->lsn;
		type = REP_LOG;
		for (ret = logc->get(logc, &rp->lsn, &data_dbt, DB_SET);
		    ret == 0 && type == REP_LOG;
		    ret = logc->get(logc, &lsn, &data_dbt, DB_NEXT)) {
			/*
			 * lsn.offset will only be 0 if this is the
			 * beginning of the log;  DB_SET, but not DB_NEXT,
			 * can set the log cursor to [n][0].
			 */
			if (lsn.offset == 0)
				ret = __rep_send_message(dbenv, *eidp,
				    REP_NEWFILE, &lsn, NULL, 0);
			else {
				/*
				 * DB_NEXT will never run into offsets
				 * of 0;  thus, when a log file changes,
				 * we'll have a real log record with
				 * some lsn [n][m], and we'll also want to send
				 * a NEWFILE message with lsn [n][0].
				 * So that the client can detect gaps,
				 * send in the rec parameter the
				 * last LSN in the old file.
				 */
				if (lsn.file != oldfilelsn.file) {
					newfilelsn.file = lsn.file;
					newfilelsn.offset = 0;

					memset(&lsndbt, 0, sizeof(DBT));
					lsndbt.size = sizeof(DB_LSN);
					lsndbt.data = &oldfilelsn;

					if ((ret = __rep_send_message(dbenv,
					    *eidp, REP_NEWFILE, &newfilelsn,
					    &lsndbt, 0)) != 0)
						break;
				}
				if (check_limit) {
					/*
					 * data_dbt.size is only the size of
					 * the log record;  it doesn't count
					 * the size of the control structure.
					 * Factor that in as well so we're
					 * not off by a lot if our log
					 * records are small.
					 */
					while (bytes < data_dbt.size +
					    sizeof(REP_CONTROL)) {
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
					bytes -= (data_dbt.size +
					    sizeof(REP_CONTROL));
				}
send:				ret = __rep_send_message(dbenv, *eidp,
				    type, &lsn, &data_dbt, 0);
			}

			/*
			 * In case we're about to change files and need it
			 * for a NEWFILE message, save the current LSN.
			 */
			oldfilelsn = lsn;
		}

		if (ret == DB_NOTFOUND)
			ret = 0;
		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		return (ret);
	case REP_ELECT:
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			MUTEX_LOCK(dbenv, db_rep->mutexp);
			rep->gen++;
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			return (__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
		}
		MUTEX_LOCK(dbenv, db_rep->mutexp);
		ret = IN_ELECTION(rep) ? 0 : DB_REP_HOLDELECTION;
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		return (ret);
#ifdef NOTYET
	case REP_FILE: /* TODO */
		CLIENT_ONLY(dbenv);
		break;
	case REP_FILE_REQ:
		MASTER_ONLY(dbenv);
		return (__rep_send_file(dbenv, rec, *eidp));
		break;
#endif
	case REP_LOG:
	case REP_LOG_MORE:
		CLIENT_ONLY(dbenv);
		if ((ret = __rep_apply(dbenv, rp, rec)) != 0)
			return (ret);
		if (rp->rectype == REP_LOG_MORE) {
			MUTEX_LOCK(dbenv, db_rep->db_mutexp);
			master = rep->master_id;
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			ret = __rep_send_message(dbenv, master,
			    REP_ALL_REQ, &lsn, NULL, 0);
		}
		return (ret);
	case REP_LOG_REQ:
		MASTER_ONLY(dbenv);
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
		memset(&data_dbt, 0, sizeof(data_dbt));
		lsn = rp->lsn;

		/*
		 * There are three different cases here.
		 * 1. We asked for a particular LSN and got it.
		 * 2. We asked for an LSN of X,0 which is invalid and got the
		 * 	first log record in a particular file.
		 * 3. We asked for an LSN and it's not found because it is
		 *	beyond the end of a log file and we need a NEWFILE msg.
		 */
		ret = logc->get(logc, &rp->lsn, &data_dbt, DB_SET);
		cmp = log_compare(&lsn, &rp->lsn);

		if (ret == 0 && cmp == 0) /* Case 1 */
			ret = __rep_send_message(dbenv, *eidp,
				    REP_LOG, &rp->lsn, &data_dbt, 0);
		else if (ret == DB_NOTFOUND ||
		    (ret == 0 && cmp < 0 && rp->lsn.offset == 0))
			/* Cases 2 and 3: Send a NEWFILE message. */
			ret = __rep_send_message(dbenv, *eidp,
			    REP_NEWFILE, &lsn, NULL, 0);

		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		return (ret);
	case REP_NEWSITE:
		/* We don't hold the rep mutex, and may miscount. */
		rep->stat.st_newsites++;

		/* This is a rebroadcast; simply tell the application. */
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
			dblp = dbenv->lg_handle;
			lp = dblp->reginfo.primary;
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			(void)__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0);
		}
		return (DB_REP_NEWSITE);
	case REP_NEWCLIENT:
		/*
		 * This message was received and should have resulted in the
		 * application entering the machine ID in its machine table.
		 * We respond to this with an ALIVE to send relevant information
		 * to the new client.  But first, broadcast the new client's
		 * record to all the clients.
		 */
		if ((ret = __rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_NEWSITE, &rp->lsn, rec, 0)) != 0)
			return (ret);

		if (F_ISSET(dbenv, DB_ENV_REP_CLIENT))
			return (0);

		 /* FALLTHROUGH */
	case REP_MASTER_REQ:
		ANYSITE(dbenv);
		if (F_ISSET(dbenv, DB_ENV_REP_CLIENT))
			return (0);
		dblp = dbenv->lg_handle;
		lp = dblp->reginfo.primary;
		R_LOCK(dbenv, &dblp->reginfo);
		lsn = lp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
		return (__rep_send_message(dbenv,
		    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
	case REP_NEWFILE:
		CLIENT_ONLY(dbenv);
		return (__rep_apply(dbenv, rp, rec));
	case REP_NEWMASTER:
		ANYSITE(dbenv);
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER) &&
		    *eidp != dbenv->rep_eid) {
			/* We don't hold the rep mutex, and may miscount. */
			rep->stat.st_dupmasters++;
			return (DB_REP_DUPMASTER);
		}
		return (__rep_new_master(dbenv, rp, *eidp));
	case REP_PAGE: /* TODO */
		CLIENT_ONLY(dbenv);
		break;
	case REP_PAGE_REQ: /* TODO */
		MASTER_ONLY(dbenv);
		break;
	case REP_PLIST: /* TODO */
		CLIENT_ONLY(dbenv);
		break;
	case REP_PLIST_REQ: /* TODO */
		MASTER_ONLY(dbenv);
		break;
	case REP_VERIFY:
		CLIENT_ONLY(dbenv);
		DB_ASSERT((F_ISSET(rep, REP_F_RECOVER) &&
		    !IS_ZERO_LSN(lp->verify_lsn)) ||
		    (!F_ISSET(rep, REP_F_RECOVER) &&
		    IS_ZERO_LSN(lp->verify_lsn)));
		if (IS_ZERO_LSN(lp->verify_lsn))
			return (0);

		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
		memset(&mylog, 0, sizeof(mylog));
		if ((ret = logc->get(logc, &rp->lsn, &mylog, DB_SET)) != 0)
			goto rep_verify_err;
		if (mylog.size == rec->size &&
		    memcmp(mylog.data, rec->data, rec->size) == 0) {
			/*
			 * If we're a logs-only client, we can simply truncate
			 * the log to the point where it last agreed with the
			 * master's;  otherwise, recover to that point.
			 */
			R_LOCK(dbenv, &dblp->reginfo);
			ZERO_LSN(lp->verify_lsn);
			R_UNLOCK(dbenv, &dblp->reginfo);
			if (F_ISSET(dbenv, DB_ENV_REP_LOGSONLY)) {
				INIT_LSN(init_lsn);
				if ((ret = dbenv->log_flush(dbenv,
				    &rp->lsn)) != 0 ||
				    (ret = __log_vtruncate(dbenv,
				    &rp->lsn, &init_lsn)) != 0)
					goto rep_verify_err;
			} else if ((ret = __db_apprec(dbenv, &rp->lsn, 0)) != 0)
				goto rep_verify_err;

			/*
			 * The log has been truncated (either by __db_apprec or
			 * directly).  We want to make sure we're waiting for
			 * the LSN at the new end-of-log, not some later point.
			 */
			R_LOCK(dbenv, &dblp->reginfo);
			lp->ready_lsn = lp->lsn;
			ZERO_LSN(lp->waiting_lsn);
			R_UNLOCK(dbenv, &dblp->reginfo);

			/*
			 * Discard any log records we have queued;  we're
			 * about to re-request them, and can't trust the
			 * ones in the queue.
			 */
			MUTEX_LOCK(dbenv, db_rep->db_mutexp);
			if ((ret = db_rep->rep_db->truncate(db_rep->rep_db,
			    NULL, &unused, 0)) != 0) {
				MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
				goto rep_verify_err;
			}
			rep->stat.st_log_queued = 0;
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

			MUTEX_LOCK(dbenv, db_rep->mutexp);
			F_CLR(rep, REP_F_RECOVER);

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
			if ((master = rep->master_id) == DB_EID_INVALID) {
				DB_ASSERT(IN_ELECTION(rep));
				MUTEX_UNLOCK(dbenv, db_rep->mutexp);
				ret = 0;
			} else {
				MUTEX_UNLOCK(dbenv, db_rep->mutexp);
				ret = __rep_send_message(dbenv, master,
				    REP_ALL_REQ, &rp->lsn, NULL, 0);
			}
		} else if ((ret =
		    logc->get(logc, &lsn, &mylog, DB_PREV)) == 0) {
			R_LOCK(dbenv, &dblp->reginfo);
			lp->verify_lsn = lsn;
			lp->rcvd_recs = 0;
			lp->wait_recs = rep->request_gap;
			R_UNLOCK(dbenv, &dblp->reginfo);
			ret = __rep_send_message(dbenv,
			    *eidp, REP_VERIFY_REQ, &lsn, NULL, 0);
		}

rep_verify_err:	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		return (ret);
	case REP_VERIFY_FAIL:
		rep->stat.st_outdated++;
		return (DB_REP_OUTDATED);
	case REP_VERIFY_REQ:
		MASTER_ONLY(dbenv);
		type = REP_VERIFY;
		if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
			return (ret);
		d = &data_dbt;
		memset(d, 0, sizeof(data_dbt));
		F_SET(logc, DB_LOG_SILENT_ERR);
		ret = logc->get(logc, &rp->lsn, d, DB_SET);
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

		ret = __rep_send_message(dbenv, *eidp, type, &rp->lsn, d, 0);
		if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
			ret = t_ret;
		return (ret);
	case REP_VOTE1:
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Master received vote");
#endif
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			return (__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
		}

		vi = (REP_VOTE_INFO *)rec->data;
		MUTEX_LOCK(dbenv, db_rep->mutexp);

		/*
		 * If you get a vote and you're not in an election, simply
		 * return an indicator to hold an election which will trigger
		 * this site to send its vote again.
		 */
		if (!IN_ELECTION(rep)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
				    "Not in election, but received vote1");
#endif
			ret = DB_REP_HOLDELECTION;
			goto unlock;
		}

		if (F_ISSET(rep, REP_F_EPHASE2))
			goto unlock;

		/* Check if this site knows about more sites than we do. */
		if (vi->nsites > rep->nsites)
			rep->nsites = vi->nsites;

		/* Check if we've heard from this site already. */
		tally = R_ADDR((REGINFO *)dbenv->reginfo, rep->tally_off);
		for (i = 0; i < rep->sites; i++) {
			if (tally[i] == *eidp)
				/* Duplicate vote. */
				goto unlock;
		}

		/*
		 * We are keeping vote, let's see if that changes our count of
		 * the number of sites.
		 */
		if (rep->sites + 1 > rep->nsites)
			rep->nsites = rep->sites + 1;
		if (rep->nsites > rep->asites &&
		    (ret = __rep_grow_sites(dbenv, rep->nsites)) != 0)
				goto unlock;

		tally[rep->sites] = *eidp;
		rep->sites++;

		/*
		 * Change winners if the incoming record has a higher
		 * priority, or an equal priority but a larger LSN, or
		 * an equal priority and LSN but higher "tiebreaker" value.
		 */
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
			__db_err(dbenv,
			    "%s(eid)%d (pri)%d (gen)%d (sites)%d [%d,%d]",
			    "Existing vote: ",
			    rep->winner, rep->w_priority, rep->w_gen,
			    rep->sites, rep->w_lsn.file, rep->w_lsn.offset);
			__db_err(dbenv,
			    "Incoming vote: (eid)%d (pri)%d (gen)%d [%d,%d]",
			    *eidp, vi->priority, rp->gen, rp->lsn.file,
			    rp->lsn.offset);
		}
#endif
		cmp = log_compare(&rp->lsn, &rep->w_lsn);
		if (vi->priority > rep->w_priority ||
		    (vi->priority != 0 && vi->priority == rep->w_priority &&
		    (cmp > 0 ||
		    (cmp == 0 && vi->tiebreaker > rep->w_tiebreaker)))) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Accepting new vote");
#endif
			rep->winner = *eidp;
			rep->w_priority = vi->priority;
			rep->w_lsn = rp->lsn;
			rep->w_gen = rp->gen;
		}
		master = rep->winner;
		lsn = rep->w_lsn;
		done = rep->sites == rep->nsites && rep->w_priority != 0;
		if (done) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION)) {
				__db_err(dbenv, "Phase1 election done");
				__db_err(dbenv, "Voting for %d%s",
				    master, master == rep->eid ? "(self)" : "");
			}
#endif
			F_CLR(rep, REP_F_EPHASE1);
			F_SET(rep, REP_F_EPHASE2);
		}

		if (done && master == rep->eid) {
			rep->votes++;
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			return (0);
		}
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);

		/* Vote for someone else. */
		if (done)
			return (__rep_send_message(dbenv,
			    master, REP_VOTE2, NULL, NULL, 0));

		/* Election is still going on. */
		break;
	case REP_VOTE2:
#ifdef DIAGNOSTIC
		if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
			__db_err(dbenv, "We received a vote%s",
			    F_ISSET(dbenv, DB_ENV_REP_MASTER) ?
			    " (master)" : "");
#endif
		if (F_ISSET(dbenv, DB_ENV_REP_MASTER)) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);
			rep->stat.st_elections_won++;
			return (__rep_send_message(dbenv,
			    *eidp, REP_NEWMASTER, &lsn, NULL, 0));
		}

		MUTEX_LOCK(dbenv, db_rep->mutexp);

		/* If we have priority 0, we should never get a vote. */
		DB_ASSERT(rep->priority != 0);

		if (!IN_ELECTION(rep)) {
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "Not in election, got vote");
#endif
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			return (DB_REP_HOLDELECTION);
		}
		/* avoid counting duplicates. */
		rep->votes++;
		done = rep->votes > rep->nsites / 2;
		if (done) {
			rep->master_id = rep->eid;
			rep->gen = rep->w_gen + 1;
			ELECTION_DONE(rep);
			F_CLR(rep, REP_F_UPGRADE);
			F_SET(rep, REP_F_MASTER);
			*eidp = rep->master_id;
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv,
			"Got enough votes to win; election done; winner is %d",
				    rep->master_id);
#endif
		}
		MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		if (done) {
			R_LOCK(dbenv, &dblp->reginfo);
			lsn = lp->lsn;
			R_UNLOCK(dbenv, &dblp->reginfo);

			/* Declare me the winner. */
#ifdef DIAGNOSTIC
			if (FLD_ISSET(dbenv->verbose, DB_VERB_REPLICATION))
				__db_err(dbenv, "I won, sending NEWMASTER");
#endif
			rep->stat.st_elections_won++;
			if ((ret = __rep_send_message(dbenv, DB_EID_BROADCAST,
			    REP_NEWMASTER, &lsn, NULL, 0)) != 0)
				break;
			return (DB_REP_NEWMASTER);
		}
		break;
	default:
		__db_err(dbenv,
	"DB_ENV->rep_process_message: unknown replication message: type %lu",
		   (u_long)rp->rectype);
		return (EINVAL);
	}

	return (0);

unlock:	MUTEX_UNLOCK(dbenv, db_rep->mutexp);
	return (ret);
}

/*
 * __rep_apply --
 *
 * Handle incoming log records on a client, applying when possible and
 * entering into the bookkeeping table otherwise.  This is the guts of
 * the routine that handles the state machine that describes how we
 * process and manage incoming log records.
 */
static int
__rep_apply(dbenv, rp, rec)
	DB_ENV *dbenv;
	REP_CONTROL *rp;
	DBT *rec;
{
	__dbreg_register_args dbreg_args;
	__txn_ckp_args ckp_args;
	DB_REP *db_rep;
	DBT control_dbt, key_dbt, lsn_dbt, nextrec_dbt, rec_dbt;
	DB *dbp;
	DBC *dbc;
	DB_LOG *dblp;
	DB_LSN ckp_lsn, lsn, newfile_lsn, next_lsn, waiting_lsn;
	LOG *lp;
	REP *rep;
	REP_CONTROL lsn_rc;
	u_int32_t rectype, txnid;
	int cmp, do_req, eid, have_mutex, ret, t_ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dbp = db_rep->rep_db;
	dbc = NULL;
	have_mutex = ret = 0;
	memset(&control_dbt, 0, sizeof(control_dbt));
	memset(&rec_dbt, 0, sizeof(rec_dbt));

	/*
	 * If this is a log record and it's the next one in line, simply
	 * write it to the log.  If it's a "normal" log record, i.e., not
	 * a COMMIT or CHECKPOINT or something that needs immediate processing,
	 * just return.  If it's a COMMIT, CHECKPOINT or LOG_REGISTER (i.e.,
	 * not SIMPLE), handle it now.  If it's a NEWFILE record, then we
	 * have to be prepared to deal with a logfile change.
	 */
	dblp = dbenv->lg_handle;
	R_LOCK(dbenv, &dblp->reginfo);
	lp = dblp->reginfo.primary;
	cmp = log_compare(&rp->lsn, &lp->ready_lsn);

	/*
	 * This is written to assume that you don't end up with a lot of
	 * records after a hole.  That is, it optimizes for the case where
	 * there is only a record or two after a hole.  If you have a lot
	 * of records after a hole, what you'd really want to do is write
	 * all of them and then process all the commits, checkpoints, etc.
	 * together.  That is more complicated processing that we can add
	 * later if necessary.
	 *
	 * That said, I really don't want to do db operations holding the
	 * log mutex, so the synchronization here is tricky.
	 */
	if (cmp == 0) {
		/* We got the log record that we are expecting. */
		if (rp->rectype == REP_NEWFILE) {
newfile:		ret = __rep_newfile(dbenv, rp, rec, &lp->ready_lsn);

			/* Make this evaluate to a simple rectype. */
			rectype = 0;
		} else {
			DB_ASSERT(log_compare(&rp->lsn, &lp->lsn) == 0);
			ret = __log_rep_put(dbenv, &rp->lsn, rec);
			lp->ready_lsn = lp->lsn;
			memcpy(&rectype, rec->data, sizeof(rectype));
			if (ret == 0)
				/*
				 * We may miscount if we race, since we
				 * don't currently hold the rep mutex.
				 */
				rep->stat.st_log_records++;
		}
		while (ret == 0 && IS_SIMPLE(rectype) &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0) {
			/*
			 * We just filled in a gap in the log record stream.
			 * Write subsequent records to the log.
			 */
gap_check:		lp->wait_recs = 0;
			lp->rcvd_recs = 0;
			R_UNLOCK(dbenv, &dblp->reginfo);
			if (have_mutex == 0) {
				MUTEX_LOCK(dbenv, db_rep->db_mutexp);
				have_mutex = 1;
			}
			if (dbc == NULL &&
			    (ret = dbp->cursor(dbp, NULL, &dbc, 0)) != 0)
				goto err;

			/* The DBTs need to persist through another call. */
			F_SET(&control_dbt, DB_DBT_REALLOC);
			F_SET(&rec_dbt, DB_DBT_REALLOC);
			if ((ret = dbc->c_get(dbc,
			    &control_dbt, &rec_dbt, DB_RMW | DB_FIRST)) != 0)
				goto err;

			rp = (REP_CONTROL *)control_dbt.data;
			rec = &rec_dbt;
			memcpy(&rectype, rec->data, sizeof(rectype));
			R_LOCK(dbenv, &dblp->reginfo);
			/*
			 * We need to check again, because it's possible that
			 * some other thread of control changed the waiting_lsn
			 * or removed that record from the database.
			 */
			if (log_compare(&lp->ready_lsn, &rp->lsn) == 0) {
				if (rp->rectype != REP_NEWFILE) {
					DB_ASSERT(log_compare
					    (&rp->lsn, &lp->lsn) == 0);
					ret = __log_rep_put(dbenv,
					    &rp->lsn, rec);
					lp->ready_lsn = lp->lsn;

					/*
					 * We may miscount if we race, since we
					 * don't currently hold the rep mutex.
					 */
					if (ret == 0)
						rep->stat.st_log_records++;
				} else {
					ret = __rep_newfile(dbenv,
					    rp, rec, &lp->ready_lsn);
					rectype = 0;
				}
				waiting_lsn = lp->waiting_lsn;
				R_UNLOCK(dbenv, &dblp->reginfo);
				if ((ret = dbc->c_del(dbc, 0)) != 0)
					goto err;

				/*
				 * We may miscount, as we don't hold the rep
				 * mutex.
				 */
				--rep->stat.st_log_queued;

				/*
				 * Update waiting_lsn.  We need to move it
				 * forward to the LSN of the next record
				 * in the queue.
				 */
				memset(&lsn_dbt, 0, sizeof(lsn_dbt));
				F_SET(&lsn_dbt, DB_DBT_USERMEM);
				lsn_dbt.data = &lsn_rc;
				lsn_dbt.ulen = sizeof(lsn_rc);
				memset(&lsn_rc, 0, sizeof(lsn_rc));

				/*
				 * If the next item in the database is a log
				 * record--the common case--we're not
				 * interested in its contents, just in its LSN.
				 * If it's a newfile message, though, the
				 * data field may be the LSN of the last
				 * record in the old file, and we need to use
				 * that to determine whether or not there's
				 * a gap.
				 *
				 * Optimize both these cases by doing a partial
				 * get of the data item.  If it's a newfile
				 * record, we'll get the whole LSN, and if
				 * it's not, we won't waste time allocating.
				 */
				memset(&nextrec_dbt, 0, sizeof(nextrec_dbt));
				F_SET(&nextrec_dbt,
				    DB_DBT_USERMEM | DB_DBT_PARTIAL);
				nextrec_dbt.ulen =
				    nextrec_dbt.dlen = sizeof(newfile_lsn);
				ZERO_LSN(newfile_lsn);
				nextrec_dbt.data = &newfile_lsn;

				ret = dbc->c_get(dbc,
				    &lsn_dbt, &nextrec_dbt, DB_NEXT);
				if (ret != DB_NOTFOUND && ret != 0)
					goto err;

				R_LOCK(dbenv, &dblp->reginfo);
				if (ret == DB_NOTFOUND) {
					/*
					 * Do a quick double-check to make
					 * sure waiting_lsn hasn't changed.
					 * It's possible that between the
					 * DB_NOTFOUND return and the R_LOCK,
					 * some record was added to the
					 * database, and we don't want to lose
					 * sight of the fact that it's there.
					 */
					if (log_compare(&waiting_lsn,
					    &lp->waiting_lsn) == 0)
						ZERO_LSN(
						    lp->waiting_lsn);

					/*
					 * Whether or not the current record is
					 * simple, there's no next one, and
					 * therefore we haven't got anything
					 * else to do right now.  Break out.
					 */
					break;
				}

				DB_ASSERT(lsn_dbt.size == sizeof(lsn_rc));

				/*
				 * NEWFILE records have somewhat convoluted
				 * semantics, so there are five cases
				 * pertaining to what the newly-gotten record
				 * is and what we want to do about it.
				 *
				 * 1) This isn't a NEWFILE record.  Advance
				 *    waiting_lsn and proceed.
				 *
				 * 2) NEWFILE, no LSN stored as the datum,
				 *    lsn_rc.lsn == ready_lsn.  The NEWFILE
				 *    record is next, so set waiting_lsn =
				 *    ready_lsn.
				 *
				 * 3) NEWFILE, no LSN stored as the datum, but
				 *    lsn_rc.lsn > ready_lsn.  There's still a
				 *    gap; set waiting_lsn = lsn_rc.lsn.
				 *
				 * 4) NEWFILE, newfile_lsn in datum, and it's <
				 *    ready_lsn. (If the datum is non-empty,
				 *    it's the LSN of the last record in a log
				 *    file, not the end of the log, and
				 *    lsn_rc.lsn is the LSN of the start of
				 *    the new file--we didn't have the end of
				 *    the old log handy when we sent the
				 *    record.)  No gap--we're ready to
				 *    proceed.  Set both waiting and ready_lsn
				 *    to lsn_rc.lsn.
				 *
				 * 5) NEWFILE, newfile_lsn in datum, and it's >=
				 *    ready_lsn.  We're still missing at
				 *    least one record;  set waiting_lsn,
				 *    but not ready_lsn, to lsn_rc.lsn.
				 */
				if (lsn_rc.rectype == REP_NEWFILE &&
				    nextrec_dbt.size > 0 && log_compare(
				    &newfile_lsn, &lp->ready_lsn) < 0)
					/* Case 4. */
					lp->ready_lsn =
					    lp->waiting_lsn = lsn_rc.lsn;
				else {
					/* Cases 1, 2, 3, and 5. */
					DB_ASSERT(log_compare(&lsn_rc.lsn,
					    &lp->ready_lsn) >= 0);
					lp->waiting_lsn = lsn_rc.lsn;
				}

				/*
				 * If the current rectype is simple, we're
				 * done with it, and we should check and see
				 * whether the next record queued is the next
				 * one we're ready for.  This is just the loop
				 * condition, so we continue.
				 *
				 * Otherwise, we need to break out of this loop
				 * and process this record first.
				 */
				if (!IS_SIMPLE(rectype))
					break;
			}
		}

		/*
		 * Check if we're at a gap in the table and if so, whether we
		 * need to ask for any records.
		 */
		do_req = 0;
		if (!IS_ZERO_LSN(lp->waiting_lsn) &&
		    log_compare(&lp->ready_lsn, &lp->waiting_lsn) != 0) {
			next_lsn = lp->ready_lsn;
			do_req = ++lp->rcvd_recs >= lp->wait_recs;
			if (do_req) {
				lp->wait_recs = rep->request_gap;
				lp->rcvd_recs = 0;
			}
		}

		R_UNLOCK(dbenv, &dblp->reginfo);
		if (dbc != NULL) {
			if ((ret = dbc->c_close(dbc)) != 0)
				goto err;
			MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);
			have_mutex = 0;
		}
		dbc = NULL;

		if (do_req) {
			MUTEX_LOCK(dbenv, db_rep->mutexp);
			eid = db_rep->region->master_id;
			MUTEX_UNLOCK(dbenv, db_rep->mutexp);
			if (eid != DB_EID_INVALID) {
				rep->stat.st_log_requested++;
				if ((ret = __rep_send_message(dbenv,
				    eid, REP_LOG_REQ, &next_lsn, NULL, 0)) != 0)
					goto err;
			}
		}
	} else if (cmp > 0) {
		/*
		 * The LSN is higher than the one we were waiting for.
		 * If it is a NEWFILE message, this may not mean that
		 * there's a gap;  in some cases, NEWFILE messages contain
		 * the LSN of the beginning of the new file instead
		 * of the end of the old.
		 *
		 * In these cases, the rec DBT will contain the last LSN
		 * of the old file, so we can tell whether there's a gap.
		 */
		if (rp->rectype == REP_NEWFILE &&
		    rp->lsn.file == lp->ready_lsn.file + 1 &&
		    rp->lsn.offset == 0) {
			DB_ASSERT(rec != NULL && rec->data != NULL &&
			    rec->size == sizeof(DB_LSN));
			memcpy(&lsn, rec->data, sizeof(DB_LSN));
			if (log_compare(&lp->ready_lsn, &lsn) > 0)
				/*
				 * The last LSN in the old file is smaller
				 * than the one we're expecting, so there's
				 * no gap--the one we're expecting just
				 * doesn't exist.
				 */
				goto newfile;
		}

		/*
		 * This record isn't in sequence; add it to the table and
		 * update waiting_lsn if necessary.
		 */
		memset(&key_dbt, 0, sizeof(key_dbt));
		key_dbt.data = rp;
		key_dbt.size = sizeof(*rp);
		next_lsn = lp->lsn;
		do_req = 0;
		if (lp->wait_recs == 0) {
			/*
			 * This is a new gap. Initialize the number of
			 * records that we should wait before requesting
			 * that it be resent.  We grab the limits out of
			 * the rep without the mutex.
			 */
			lp->wait_recs = rep->request_gap;
			lp->rcvd_recs = 0;
		}

		if (++lp->rcvd_recs >= lp->wait_recs) {
			/*
			 * If we've waited long enough, request the record
			 * and double the wait interval.
			 */
			do_req = 1;
			lp->wait_recs <<= 1;
			lp->rcvd_recs = 0;
			if (lp->wait_recs > rep->max_gap)
				lp->wait_recs = rep->max_gap;
		}
		R_UNLOCK(dbenv, &dblp->reginfo);

		MUTEX_LOCK(dbenv, db_rep->db_mutexp);
		ret = dbp->put(dbp, NULL, &key_dbt, rec, 0);
		rep->stat.st_log_queued++;
		rep->stat.st_log_queued_total++;
		if (rep->stat.st_log_queued_max < rep->stat.st_log_queued)
			rep->stat.st_log_queued_max = rep->stat.st_log_queued;
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

		if (ret != 0)
			return (ret);

		R_LOCK(dbenv, &dblp->reginfo);
		if (IS_ZERO_LSN(lp->waiting_lsn) ||
		    log_compare(&rp->lsn, &lp->waiting_lsn) < 0)
			lp->waiting_lsn = rp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);

		if (do_req) {
			/* Request the LSN we are still waiting for. */
			MUTEX_LOCK(dbenv, db_rep->mutexp);

			/* May as well do this after we grab the mutex. */
			eid = db_rep->region->master_id;

			/*
			 * If the master_id is invalid, this means that since
			 * the last record was sent, somebody declared an
			 * election and we may not have a master to request
			 * things of.
			 *
			 * This is not an error;  when we find a new master,
			 * we'll re-negotiate where the end of the log is and
			 * try to to bring ourselves up to date again anyway.
			 */
			if (eid != DB_EID_INVALID) {
				rep->stat.st_log_requested++;
				MUTEX_UNLOCK(dbenv, db_rep->mutexp);
				ret = __rep_send_message(dbenv,
				    eid, REP_LOG_REQ, &next_lsn, NULL, 0);
			} else
				MUTEX_UNLOCK(dbenv, db_rep->mutexp);
		}
		return (ret);
	} else {
		R_UNLOCK(dbenv, &dblp->reginfo);

		/*
		 * We may miscount if we race, since we
		 * don't currently hold the rep mutex.
		 */
		rep->stat.st_log_duplicated++;
	}
	if (ret != 0 || cmp < 0 || (cmp == 0 && IS_SIMPLE(rectype)))
		goto done;

	/*
	 * If we got here, then we've got a log record in rp and rec that
	 * we need to process.
	 */
	switch(rectype) {
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
		    ((u_int8_t *)&dbreg_args.txnid - (u_int8_t *)&dbreg_args),
		    sizeof(u_int32_t));
		if (txnid == TXN_INVALID &&
		    !F_ISSET(dbenv, DB_ENV_REP_LOGSONLY))
			ret = __db_dispatch(dbenv, dbenv->recover_dtab,
			    dbenv->recover_dtab_size, rec, &rp->lsn,
			    DB_TXN_APPLY, NULL);
		break;
	case DB___txn_ckp:
		/* Sync the memory pool. */
		memcpy(&ckp_lsn, (u_int8_t *)rec->data +
		    ((u_int8_t *)&ckp_args.ckp_lsn - (u_int8_t *)&ckp_args),
		    sizeof(DB_LSN));
		if (!F_ISSET(dbenv, DB_ENV_REP_LOGSONLY))
			ret = dbenv->memp_sync(dbenv, &ckp_lsn);
		else
			/*
			 * We ought to make sure the logs on a logs-only
			 * replica get flushed now and again.
			 */
			ret = dbenv->log_flush(dbenv, &ckp_lsn);
		/* Update the last_ckp in the txn region. */
		if (ret == 0)
			__txn_updateckp(dbenv, &rp->lsn);
		break;
	case DB___txn_regop:
		if (!F_ISSET(dbenv, DB_ENV_REP_LOGSONLY))
			do {
				/*
				 * If an application is doing app-specific
				 * recovery and acquires locks while applying
				 * a transaction, it can deadlock.  Any other
				 * locks held by this thread should have been
				 * discarded in the __rep_process_txn error
				 * path, so if we simply retry, we should
				 * eventually succeed.
				 */
				ret = __rep_process_txn(dbenv, rec);
			} while (ret == DB_LOCK_DEADLOCK);
		break;
	default:
		goto err;
	}

	/* Check if we need to go back into the table. */
	if (ret == 0) {
		R_LOCK(dbenv, &dblp->reginfo);
		if (log_compare(&lp->ready_lsn, &lp->waiting_lsn) == 0)
			goto gap_check;
		R_UNLOCK(dbenv, &dblp->reginfo);
	}

done:
err:	if (dbc != NULL && (t_ret = dbc->c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;
	if (have_mutex)
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	if (control_dbt.data != NULL)
		__os_ufree(dbenv, control_dbt.data);
	if (rec_dbt.data != NULL)
		__os_ufree(dbenv, rec_dbt.data);

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
	DBT data_dbt;
	DB_LOCKREQ req, *lvp;
	DB_LOGC *logc;
	DB_LSN prev_lsn, *lsnp;
	DB_REP *db_rep;
	LSN_COLLECTION lc;
	REP *rep;
	__txn_regop_args *txn_args;
	__txn_xa_regop_args *prep_args;
	u_int32_t lockid, op, rectype;
	int i, ret, t_ret;
	int (**dtab)__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t dtabsize;
	void *txninfo;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	logc = NULL;
	txninfo = NULL;
	memset(&data_dbt, 0, sizeof(data_dbt));
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		F_SET(&data_dbt, DB_DBT_REALLOC);

	/*
	 * There are two phases:  First, we have to traverse
	 * backwards through the log records gathering the list
	 * of all LSNs in the transaction.  Once we have this information,
	 * we can loop through, acquire the locks we need for each record,
	 * and then apply it.
	 */
	dtab = NULL;

	/*
	 * We may be passed a prepare (if we're restoring a prepare
	 * on upgrade) instead of a commit (the common case).
	 * Check which and behave appropriately.
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
		op = txn_args->opcode;
		prev_lsn = txn_args->prev_lsn;
		__os_free(dbenv, txn_args);
		if (op != TXN_COMMIT)
			return (0);
	} else {
		/* We're a prepare. */
		DB_ASSERT(rectype == DB___txn_xa_regop);

		if ((ret =
		    __txn_xa_regop_read(dbenv, rec->data, &prep_args)) != 0)
			return (ret);
		prev_lsn = prep_args->prev_lsn;
		__os_free(dbenv, prep_args);
	}

	/* Phase 1.  Get a list of the LSNs in this transaction, and sort it. */
	if ((ret = __rep_collect_txn(dbenv, &prev_lsn, &lc)) != 0)
		return (ret);
	qsort(lc.array, lc.nlsns, sizeof(DB_LSN), __rep_lsn_cmp);

	if ((ret = dbenv->lock_id(dbenv, &lockid)) != 0)
		goto err;

	/* Initialize the getpgno dispatch table. */
	if ((ret = __rep_lockpgno_init(dbenv, &dtab, &dtabsize)) != 0)
		goto err;

	/*
	 * The set of records for a transaction may include dbreg_register
	 * records.  Create a txnlist so that they can keep track of file
	 * state between records.
	 */
	if ((ret = __db_txnlist_init(dbenv, 0, 0, NULL, &txninfo)) != 0)
		goto err;

	/* Phase 2: Apply updates. */
	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		goto err;
	for (lsnp = &lc.array[0], i = 0; i < lc.nlsns; i++, lsnp++) {
		if ((ret = __rep_lockpages(dbenv,
		    dtab, dtabsize, lsnp, NULL, NULL, lockid)) != 0)
			goto err;
		if ((ret = logc->get(logc, lsnp, &data_dbt, DB_SET)) != 0)
			goto err;
		if ((ret = __db_dispatch(dbenv, dbenv->recover_dtab,
		    dbenv->recover_dtab_size, &data_dbt, lsnp,
		    DB_TXN_APPLY, txninfo)) != 0)
			goto err;
	}

err:	memset(&req, 0, sizeof(req));
	req.op = DB_LOCK_PUT_ALL;
	if ((t_ret = dbenv->lock_vec(dbenv, lockid,
	    DB_LOCK_FREE_LOCKER, &req, 1, &lvp)) != 0 && ret == 0)
		ret = t_ret;

	if (lc.nalloc != 0)
		__os_free(dbenv, lc.array);

	if ((t_ret =
	    dbenv->lock_id_free(dbenv, lockid)) != 0 && ret == 0)
		ret = t_ret;

	if (logc != NULL && (t_ret = logc->close(logc, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (txninfo != NULL)
		__db_txnlist_end(dbenv, txninfo);

	if (F_ISSET(&data_dbt, DB_DBT_REALLOC) && data_dbt.data != NULL)
		__os_ufree(dbenv, data_dbt.data);

	if (dtab != NULL)
		__os_free(dbenv, dtab);

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
	int nalloc, ret, t_ret;

	memset(&data, 0, sizeof(data));
	F_SET(&data, DB_DBT_REALLOC);

	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);

	while (!IS_ZERO_LSN(*lsnp) &&
	    (ret = logc->get(logc, lsnp, &data, DB_SET)) == 0) {
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

err:	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
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
 *	NEWFILE messages can contain either the last LSN of the old file
 * or the first LSN of the new one, depending on which we have available
 * when the message is sent.  When applying a NEWFILE message, make sure
 * we haven't already swapped files, as it's possible (given the right sequence
 * of out-of-order messages) to wind up with a NEWFILE message of each
 * variety, and __rep_apply won't detect the two as duplicates of each other.
 */
static int
__rep_newfile(dbenv, rc, msgdbt, lsnp)
	DB_ENV *dbenv;
	REP_CONTROL *rc;
	DBT *msgdbt;
	DB_LSN *lsnp;
{
	DB_LOG *dblp;
	LOG *lp;
	u_int32_t newfile;

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	/*
	 * A NEWFILE message containing the old file's LSN will be
	 * accompanied by a NULL rec DBT;  one containing the new one's LSN
	 * will need to supply the last record in the old file by
	 * sending it in the rec DBT.
	 */
	if (msgdbt == NULL || msgdbt->size == 0)
		newfile = rc->lsn.file + 1;
	else
		newfile = rc->lsn.file;

	if (newfile > lp->lsn.file)
		return (__log_newfile(dblp, lsnp));
	else {
		/* We've already applied this NEWFILE.  Just ignore it. */
		*lsnp = lp->lsn;
		return (0);
	}
}
