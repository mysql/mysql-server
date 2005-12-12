/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_method.c,v 12.18 2005/11/08 03:25:13 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/btree.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int  __rep_abort_prepared __P((DB_ENV *));
static int  __rep_bt_cmp __P((DB *, const DBT *, const DBT *));
static void __rep_config_map __P((DB_ENV *, u_int32_t *, u_int32_t *));
static int  __rep_restore_prepared __P((DB_ENV *));

/*
 * __rep_open --
 *	Replication-specific initialization of the DB_ENV structure.
 *
 * PUBLIC: int __rep_open __P((DB_ENV *));
 */
int
__rep_open(dbenv)
	DB_ENV *dbenv;
{
	DB_REP *db_rep;
	int ret;

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_REP), &db_rep)) != 0)
		return (ret);
	dbenv->rep_handle = db_rep;
	ret = __rep_region_init(dbenv);
	return (ret);
}

/*
 * __rep_get_config --
 *	Configure the replication subsystem.
 *
 * PUBLIC: int __rep_get_config __P((DB_ENV *, u_int32_t, int *));
 */
int
__rep_get_config(dbenv, which, onp)
	DB_ENV *dbenv;
	u_int32_t which;
	int *onp;
{
	DB_REP *db_rep;
	REP *rep;
	u_int32_t mapped;

#undef	OK_FLAGS
#define	OK_FLAGS							\
(DB_REP_CONF_BULK | DB_REP_CONF_DELAYCLIENT | DB_REP_CONF_NOAUTOINIT	\
    | DB_REP_CONF_NOWAIT)

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle,
	    "rep_get_config", DB_INIT_REP);
	if (FLD_ISSET(which, ~OK_FLAGS))
		return (__db_ferr(dbenv, "DB_ENV->rep_get_config", 0));

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	mapped = 0;
	__rep_config_map(dbenv, &which, &mapped);
	if (FLD_ISSET(rep->config, mapped))
		*onp = 1;
	else
		*onp = 0;
	return (0);
}

/*
 * __rep_set_config --
 *	Configure the replication subsystem.
 *
 * PUBLIC: int __rep_set_config __P((DB_ENV *, u_int32_t, int));
 */
int
__rep_set_config(dbenv, which, on)
	DB_ENV *dbenv;
	u_int32_t which;
	int on;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	REP_BULK bulk;
	int ret;
	u_int32_t mapped, orig;

#undef	OK_FLAGS
#define	OK_FLAGS							\
(DB_REP_CONF_BULK | DB_REP_CONF_DELAYCLIENT | DB_REP_CONF_NOAUTOINIT	\
    | DB_REP_CONF_NOWAIT)

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle,
	    "rep_config", DB_INIT_REP);
	if (FLD_ISSET(which, ~OK_FLAGS))
		return (__db_ferr(dbenv, "DB_ENV->rep_set_config", 0));

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	mapped = ret = 0;
	__rep_config_map(dbenv, &which, &mapped);
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	REP_SYSTEM_LOCK(dbenv);
	orig = rep->config;
	if (on)
		FLD_SET(rep->config, mapped);
	else
		FLD_CLR(rep->config, mapped);

	/*
	 * Bulk transfer requires special processing if it is getting
	 * toggled.
	 */
	if (FLD_ISSET(rep->config, REP_C_BULK) &&
	    !FLD_ISSET(orig, REP_C_BULK))
		db_rep->bulk = R_ADDR(&dblp->reginfo, lp->bulk_buf);
	REP_SYSTEM_UNLOCK(dbenv);
	/*
	 * If turning bulk off and it was on, send out whatever is in the
	 * buffer already.
	 */
	if (FLD_ISSET(orig, REP_C_BULK) &&
	    !FLD_ISSET(rep->config, REP_C_BULK) && lp->bulk_off != 0) {
		memset(&bulk, 0, sizeof(bulk));
		if (db_rep->bulk == NULL)
			bulk.addr = R_ADDR(&dblp->reginfo, lp->bulk_buf);
		else
			bulk.addr = db_rep->bulk;
		bulk.offp = &lp->bulk_off;
		bulk.len = lp->bulk_len;
		bulk.type = REP_BULK_LOG;
		bulk.eid = DB_EID_BROADCAST;
		bulk.flagsp = &lp->bulk_flags;
		ret = __rep_send_bulk(dbenv, &bulk, 0);
	}
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	return (ret);
}

static void
__rep_config_map(dbenv, inflagsp, outflagsp)
	DB_ENV *dbenv;
	u_int32_t *inflagsp, *outflagsp;
{
	COMPQUIET(dbenv, NULL);

	if (FLD_ISSET(*inflagsp, DB_REP_CONF_BULK)) {
		FLD_SET(*outflagsp, REP_C_BULK);
		FLD_CLR(*inflagsp, DB_REP_CONF_BULK);
	}
	if (FLD_ISSET(*inflagsp, DB_REP_CONF_DELAYCLIENT)) {
		FLD_SET(*outflagsp, REP_C_DELAYCLIENT);
		FLD_CLR(*inflagsp, DB_REP_CONF_DELAYCLIENT);
	}
	if (FLD_ISSET(*inflagsp, DB_REP_CONF_NOAUTOINIT)) {
		FLD_SET(*outflagsp, REP_C_NOAUTOINIT);
		FLD_CLR(*inflagsp, DB_REP_CONF_NOAUTOINIT);
	}
	if (FLD_ISSET(*inflagsp, DB_REP_CONF_NOWAIT)) {
		FLD_SET(*outflagsp, REP_C_NOWAIT);
		FLD_CLR(*inflagsp, DB_REP_CONF_NOWAIT);
	}
}

/*
 * __rep_start --
 *	Become a master or client, and start sending messages to participate
 * in the replication environment.  Must be called after the environment
 * is open.
 *
 * We must protect rep_start, which may change the world, with the rest
 * of the DB library.  Each API interface will count itself as it enters
 * the library.  Rep_start checks the following:
 *
 * rep->msg_th - this is the count of threads currently in rep_process_message
 * rep->start_th - this is set if a thread is in rep_start.
 * rep->handle_cnt - number of threads actively using a dbp in library.
 * rep->txn_cnt - number of active txns.
 * REP_F_READY - Replication flag that indicates that we wish to run
 * recovery, and want to prohibit new transactions from entering and cause
 * existing ones to return immediately (with a DB_LOCK_DEADLOCK error).
 *
 * There is also the renv->rep_timestamp which is updated whenever significant
 * events (i.e., new masters, log rollback, etc).  Upon creation, a handle
 * is associated with the current timestamp.  Each time a handle enters the
 * library it must check if the handle timestamp is the same as the one
 * stored in the replication region.  This prevents the use of handles on
 * clients that reference non-existent files whose creation was backed out
 * during a synchronizing recovery.
 *
 * PUBLIC: int __rep_start __P((DB_ENV *, DBT *, u_int32_t));
 */
int
__rep_start(dbenv, dbt, flags)
	DB_ENV *dbenv;
	DBT *dbt;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_REP *db_rep;
	REP *rep;
	u_int32_t repflags;
	int announce, init_db, redo_prepared, ret, role_chg;
	int sleep_cnt, t_ret;
#ifdef DIAGNOSTIC
	DB_MSGBUF mb;
#endif

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->rep_start");
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_start", DB_INIT_REP);

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	if ((ret = __db_fchk(dbenv, "DB_ENV->rep_start", flags,
	    DB_REP_CLIENT | DB_REP_MASTER)) != 0)
		return (ret);

	/* Exactly one of CLIENT and MASTER must be specified. */
	if ((ret = __db_fcchk(dbenv,
	    "DB_ENV->rep_start", flags, DB_REP_CLIENT, DB_REP_MASTER)) != 0)
		return (ret);
	if (!LF_ISSET(DB_REP_CLIENT | DB_REP_MASTER)) {
		__db_err(dbenv,
	"DB_ENV->rep_start: replication mode must be specified");
		return (EINVAL);
	}

	/* We need a transport function. */
	if (dbenv->rep_send == NULL) {
		__db_err(dbenv,
    "DB_ENV->set_rep_transport must be called before DB_ENV->rep_start");
		return (EINVAL);
	}

	/*
	 * If we are about to become (or stay) a master.  Let's flush the log
	 * to close any potential holes that might happen when upgrading from
	 * client to master status.
	 */
	if (LF_ISSET(DB_REP_MASTER) && (ret = __log_flush(dbenv, NULL)) != 0)
		return (ret);

	REP_SYSTEM_LOCK(dbenv);
	/*
	 * We only need one thread to start-up replication, so if
	 * there is another thread in rep_start, we'll let it finish
	 * its work and have this thread simply return.
	 */
	if (rep->start_th != 0) {
		/*
		 * There is already someone in rep_start.  Return.
		 */
		RPRINT(dbenv, rep, (dbenv, &mb, "Thread already in rep_start"));
		goto err;
	} else
		rep->start_th = 1;

	role_chg = (!F_ISSET(rep, REP_F_MASTER) && LF_ISSET(DB_REP_MASTER)) ||
	    (!F_ISSET(rep, REP_F_CLIENT) && LF_ISSET(DB_REP_CLIENT));

	/*
	 * Wait for any active txns or mpool ops to complete, and
	 * prevent any new ones from occurring, only if we're
	 * changing roles.  If we are not changing roles, then we
	 * only need to coordinate with msg_th.
	 */
	if (role_chg) {
		if ((ret = __rep_lockout(dbenv, rep, 0)) != 0)
			goto errunlock;
	} else {
		for (sleep_cnt = 0; rep->msg_th != 0;) {
			if (++sleep_cnt % 60 == 0)
				__db_err(dbenv,
	"DB_ENV->rep_start waiting %d minutes for replication message thread",
				    sleep_cnt / 60);
			REP_SYSTEM_UNLOCK(dbenv);
			__os_sleep(dbenv, 1, 0);
			REP_SYSTEM_LOCK(dbenv);
		}
	}

	if (rep->eid == DB_EID_INVALID)
		rep->eid = dbenv->rep_eid;

	if (LF_ISSET(DB_REP_MASTER)) {
		if (role_chg) {
			/*
			 * If we're upgrading from having been a client,
			 * preclose, so that we close our temporary database
			 * and any files we opened while doing a rep_apply.
			 * If we don't we can infinitely leak file ids if
			 * the master crashed with files open (the likely
			 * case).  If we don't close them we can run into
			 * problems if we try to remove that file or long
			 * running applications end up with an unbounded
			 * number of used fileids, each getting written
			 * on checkpoint.  Just close them.
			 */
			if ((ret = __rep_preclose(dbenv)) != 0)
				goto errunlock;
		}

		redo_prepared = 0;
		if (!F_ISSET(rep, REP_F_MASTER)) {
			/* Master is not yet set. */
			if (role_chg) {
				if (rep->w_gen > rep->recover_gen)
					rep->gen = ++rep->w_gen;
				else if (rep->gen > rep->recover_gen)
					rep->gen++;
				else
					rep->gen = rep->recover_gen + 1;
				/*
				 * There could have been any number of failed
				 * elections, so jump the gen if we need to now.
				 */
				if (rep->egen > rep->gen)
					rep->gen = rep->egen;
				redo_prepared = 1;
			} else if (rep->gen == 0)
				rep->gen = rep->recover_gen + 1;
			if (F_ISSET(rep, REP_F_MASTERELECT)) {
				__rep_elect_done(dbenv, rep);
				F_CLR(rep, REP_F_MASTERELECT);
			}
			if (rep->egen <= rep->gen)
				rep->egen = rep->gen + 1;
			RPRINT(dbenv, rep, (dbenv, &mb,
			    "New master gen %lu, egen %lu",
			    (u_long)rep->gen, (u_long)rep->egen));
		}
		rep->master_id = rep->eid;
		/*
		 * Note, setting flags below implicitly clears out
		 * REP_F_NOARCHIVE, REP_F_INIT and REP_F_READY.
		 */
		rep->flags = REP_F_MASTER;
		rep->start_th = 0;
		REP_SYSTEM_UNLOCK(dbenv);
		dblp = (DB_LOG *)dbenv->lg_handle;
		LOG_SYSTEM_LOCK(dbenv);
		lsn = ((LOG *)dblp->reginfo.primary)->lsn;
		LOG_SYSTEM_UNLOCK(dbenv);

		/*
		 * Send the NEWMASTER message first so that clients know
		 * subsequent messages are coming from the right master.
		 * We need to perform all actions below no master what
		 * regarding errors.
		 */
		(void)__rep_send_message(dbenv,
		    DB_EID_BROADCAST, REP_NEWMASTER, &lsn, NULL, 0, 0);
		ret = 0;
		if (role_chg) {
			ret = __txn_reset(dbenv);
			REP_SYSTEM_LOCK(dbenv);
			F_CLR(rep, REP_F_READY);
			rep->in_recovery = 0;
			REP_SYSTEM_UNLOCK(dbenv);
		}
		/*
		 * Take a transaction checkpoint so that our new generation
		 * number get written to the log.
		 */
		if ((t_ret = __txn_checkpoint(dbenv, 0, 0, DB_FORCE)) != 0 &&
		    ret == 0)
			ret = t_ret;
		if (redo_prepared &&
		    (t_ret = __rep_restore_prepared(dbenv)) != 0 && ret == 0)
			ret = t_ret;
	} else {
		init_db = 0;
		announce = role_chg || rep->master_id == DB_EID_INVALID;

		/*
		 * If we're changing roles from master to client or if
		 * we never were any role at all, we need to init the db.
		 */
		if (role_chg || !F_ISSET(rep, REP_F_CLIENT)) {
			rep->master_id = DB_EID_INVALID;
			init_db = 1;
		}
		/* Zero out everything except recovery and tally flags. */
		repflags = F_ISSET(rep, REP_F_NOARCHIVE |
		    REP_F_RECOVER_MASK | REP_F_TALLY);
		FLD_SET(repflags, REP_F_CLIENT);

		rep->flags = repflags;
		REP_SYSTEM_UNLOCK(dbenv);

		/*
		 * Abort any prepared transactions that were restored
		 * by recovery.  We won't be able to create any txns of
		 * our own until they're resolved, but we can't resolve
		 * them ourselves;  the master has to.  If any get
		 * resolved as commits, we'll redo them when commit
		 * records come in.  Aborts will simply be ignored.
		 */
		if ((ret = __rep_abort_prepared(dbenv)) != 0)
			goto errlock;

		MUTEX_LOCK(dbenv, rep->mtx_clientdb);
		ret = __rep_client_dbinit(dbenv, init_db, REP_DB);
		MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
		if (ret != 0)
			goto errlock;
		REP_SYSTEM_LOCK(dbenv);
		rep->start_th = 0;
		if (role_chg) {
			F_CLR(rep, REP_F_READY);
			rep->in_recovery = 0;
		}
		REP_SYSTEM_UNLOCK(dbenv);

		/*
		 * If this client created a newly replicated environment,
		 * then announce the existence of this client.  The master
		 * should respond with a message that will tell this client
		 * the current generation number and the current LSN.  This
		 * will allow the client to either perform recovery or
		 * simply join in.
		 */
		if (announce)
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_NEWCLIENT, NULL, dbt, 0, 0);
		else
			(void)__rep_send_message(dbenv,
			    DB_EID_BROADCAST, REP_ALIVE_REQ, NULL, NULL, 0, 0);
	}

	if (0) {
		/*
		 * We have separate labels for errors.  If we're returning an
		 * error before we've set start_th, we use 'err'.  If
		 * we are erroring while holding the region mutex, then we use
		 * 'errunlock' label.  If we're erroring without holding the rep
		 * mutex we must use 'errlock'.
		 */
errlock:	REP_SYSTEM_LOCK(dbenv);
errunlock:	rep->start_th = 0;
		if (role_chg) {
			F_CLR(rep, REP_F_READY);
			rep->in_recovery = 0;
		}
err:		REP_SYSTEM_UNLOCK(dbenv);
	}
	return (ret);
}

/*
 * __rep_client_dbinit --
 *
 * Initialize the LSN database on the client side.  This is called from the
 * client initialization code.  The startup flag value indicates if
 * this is the first thread/process starting up and therefore should create
 * the LSN database.  This routine must be called once by each process acting
 * as a client.
 *
 * Assumes caller holds appropriate mutex.
 *
 * PUBLIC: int __rep_client_dbinit __P((DB_ENV *, int, repdb_t));
 */
int
__rep_client_dbinit(dbenv, startup, which)
	DB_ENV *dbenv;
	int startup;
	repdb_t which;
{
	DB_REP *db_rep;
	DB *dbp, **rdbpp;
	REP *rep;
	int ret, t_ret;
	u_int32_t flags;
	const char *name;

	PANIC_CHECK(dbenv);
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dbp = NULL;

#define	REPDBNAME	"__db.rep.db"
#define	REPPAGENAME     "__db.reppg.db"

	if (which == REP_DB) {
		name = REPDBNAME;
		rdbpp = &db_rep->rep_db;
	} else {
		name = REPPAGENAME;
		rdbpp = &rep->file_dbp;
	}
	/* Check if this has already been called on this environment. */
	if (*rdbpp != NULL)
		return (0);

	if (startup) {
		if ((ret = db_create(&dbp, dbenv, 0)) != 0)
			goto err;
		/*
		 * Ignore errors, because if the file doesn't exist, this
		 * is perfectly OK.
		 */
		(void)__db_remove(dbp, NULL, name, NULL, DB_FORCE);
	}

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;
	if (which == REP_DB &&
	    (ret = __bam_set_bt_compare(dbp, __rep_bt_cmp)) != 0)
		goto err;

	/* Allow writes to this database on a client. */
	F_SET(dbp, DB_AM_CL_WRITER);

	flags = DB_NO_AUTO_COMMIT |
	    (startup ? DB_CREATE : 0) |
	    (F_ISSET(dbenv, DB_ENV_THREAD) ? DB_THREAD : 0);

	if ((ret = __db_open(dbp, NULL, name, NULL,
	    (which == REP_DB ? DB_BTREE : DB_RECNO),
	    flags, 0, PGNO_BASE_MD)) != 0)
		goto err;

	*rdbpp= dbp;

	if (0) {
err:		if (dbp != NULL &&
		    (t_ret = __db_close(dbp, NULL, DB_NOSYNC)) != 0 && ret == 0)
			ret = t_ret;
		*rdbpp = NULL;
	}

	return (ret);
}

/*
 * __rep_bt_cmp --
 *
 * Comparison function for the LSN table.  We use the entire control
 * structure as a key (for simplicity, so we don't have to merge the
 * other fields in the control with the data field), but really only
 * care about the LSNs.
 */
static int
__rep_bt_cmp(dbp, dbt1, dbt2)
	DB *dbp;
	const DBT *dbt1, *dbt2;
{
	DB_LSN lsn1, lsn2;
	REP_CONTROL *rp1, *rp2;

	COMPQUIET(dbp, NULL);

	rp1 = dbt1->data;
	rp2 = dbt2->data;

	(void)__ua_memcpy(&lsn1, &rp1->lsn, sizeof(DB_LSN));
	(void)__ua_memcpy(&lsn2, &rp2->lsn, sizeof(DB_LSN));

	if (lsn1.file > lsn2.file)
		return (1);

	if (lsn1.file < lsn2.file)
		return (-1);

	if (lsn1.offset > lsn2.offset)
		return (1);

	if (lsn1.offset < lsn2.offset)
		return (-1);

	return (0);
}

/*
 * __rep_abort_prepared --
 *	Abort any prepared transactions that recovery restored.
 *
 *	This is used by clients that have just run recovery, since
 * they cannot/should not call txn_recover and handle prepared transactions
 * themselves.
 */
static int
__rep_abort_prepared(dbenv)
	DB_ENV *dbenv;
{
#define	PREPLISTSIZE	50
	DB_PREPLIST prep[PREPLISTSIZE], *p;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	int do_aborts, ret;
	long count, i;
	u_int32_t op;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	do_aborts = 0;
	TXN_SYSTEM_LOCK(dbenv);
	if (region->stat.st_nrestores != 0)
		do_aborts = 1;
	TXN_SYSTEM_UNLOCK(dbenv);

	if (do_aborts) {
		op = DB_FIRST;
		do {
			if ((ret = __txn_recover(dbenv,
			    prep, PREPLISTSIZE, &count, op)) != 0)
				return (ret);
			for (i = 0; i < count; i++) {
				p = &prep[i];
				if ((ret = __txn_abort(p->txn)) != 0)
					return (ret);
			}
			op = DB_NEXT;
		} while (count == PREPLISTSIZE);
	}

	return (0);
}

/*
 * __rep_restore_prepared --
 *	Restore to a prepared state any prepared but not yet committed
 * transactions.
 *
 *	This performs, in effect, a "mini-recovery";  it is called from
 * __rep_start by newly upgraded masters.  There may be transactions that an
 * old master prepared but did not resolve, which we need to restore to an
 * active state.
 */
static int
__rep_restore_prepared(dbenv)
	DB_ENV *dbenv;
{
	DB_LOGC *logc;
	DB_LSN ckp_lsn, lsn;
	DB_TXNHEAD *txninfo;
	DBT rec;
	__txn_ckp_args *ckp_args;
	__txn_regop_args *regop_args;
	__txn_xa_regop_args *prep_args;
	int ret, t_ret;
	u_int32_t hi_txn, low_txn, rectype, status;

	txninfo = NULL;
	ckp_args = NULL;
	prep_args = NULL;
	regop_args = NULL;
	ZERO_LSN(ckp_lsn);
	ZERO_LSN(lsn);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	/*
	 * We need to consider the set of records between the most recent
	 * checkpoint LSN and the end of the log;  any txn in that
	 * range, and only txns in that range, could still have been
	 * active, and thus prepared but not yet committed (PBNYC),
	 * when the old master died.
	 *
	 * Find the most recent checkpoint LSN, and get the record there.
	 * If there is no checkpoint in the log, start off by getting
	 * the very first record in the log instead.
	 */
	memset(&rec, 0, sizeof(DBT));
	if ((ret = __txn_getckp(dbenv, &lsn)) == 0) {
		if ((ret = __log_c_get(logc, &lsn, &rec, DB_SET)) != 0)  {
			__db_err(dbenv,
			    "Checkpoint record at LSN [%lu][%lu] not found",
			    (u_long)lsn.file, (u_long)lsn.offset);
			goto err;
		}

		if ((ret = __txn_ckp_read(dbenv, rec.data, &ckp_args)) != 0) {
			__db_err(dbenv,
			    "Invalid checkpoint record at [%lu][%lu]",
			    (u_long)lsn.file, (u_long)lsn.offset);
			goto err;
		}

		ckp_lsn = ckp_args->ckp_lsn;
		__os_free(dbenv, ckp_args);

		if ((ret = __log_c_get(logc, &ckp_lsn, &rec, DB_SET)) != 0) {
			__db_err(dbenv,
			    "Checkpoint LSN record [%lu][%lu] not found",
			    (u_long)ckp_lsn.file, (u_long)ckp_lsn.offset);
			goto err;
		}
	} else if ((ret = __log_c_get(logc, &lsn, &rec, DB_FIRST)) != 0) {
		if (ret == DB_NOTFOUND) {
			/* An empty log means no PBNYC txns. */
			ret = 0;
			goto done;
		}
		__db_err(dbenv, "Attempt to get first log record failed");
		goto err;
	}

	/*
	 * We use the same txnlist infrastructure that recovery does;
	 * it demands an estimate of the high and low txnids for
	 * initialization.
	 *
	 * First, the low txnid.
	 */
	do {
		/* txnid is after rectype, which is a u_int32. */
		memcpy(&low_txn,
		    (u_int8_t *)rec.data + sizeof(u_int32_t), sizeof(low_txn));
		if (low_txn != 0)
			break;
	} while ((ret = __log_c_get(logc, &lsn, &rec, DB_NEXT)) == 0);

	/* If there are no txns, there are no PBNYC txns. */
	if (ret == DB_NOTFOUND) {
		ret = 0;
		goto done;
	} else if (ret != 0)
		goto err;

	/* Now, the high txnid. */
	if ((ret = __log_c_get(logc, &lsn, &rec, DB_LAST)) != 0) {
		/*
		 * Note that DB_NOTFOUND is unacceptable here because we
		 * had to have looked at some log record to get this far.
		 */
		__db_err(dbenv, "Final log record not found");
		goto err;
	}
	do {
		/* txnid is after rectype, which is a u_int32. */
		memcpy(&hi_txn,
		    (u_int8_t *)rec.data + sizeof(u_int32_t), sizeof(hi_txn));
		if (hi_txn != 0)
			break;
	} while ((ret = __log_c_get(logc, &lsn, &rec, DB_PREV)) == 0);
	if (ret == DB_NOTFOUND) {
		ret = 0;
		goto done;
	} else if (ret != 0)
		goto err;

	/* We have a high and low txnid.  Initialise the txn list. */
	if ((ret =
	    __db_txnlist_init(dbenv, low_txn, hi_txn, NULL, &txninfo)) != 0)
		goto err;

	/*
	 * Now, walk backward from the end of the log to ckp_lsn.  Any
	 * prepares that we hit without first hitting a commit or
	 * abort belong to PBNYC txns, and we need to apply them and
	 * restore them to a prepared state.
	 *
	 * Note that we wind up applying transactions out of order.
	 * Since all PBNYC txns still held locks on the old master and
	 * were isolated, this should be safe.
	 */
	for (ret = __log_c_get(logc, &lsn, &rec, DB_LAST);
	    ret == 0 && log_compare(&lsn, &ckp_lsn) > 0;
	    ret = __log_c_get(logc, &lsn, &rec, DB_PREV)) {
		memcpy(&rectype, rec.data, sizeof(rectype));
		switch (rectype) {
		case DB___txn_regop:
			/*
			 * It's a commit or abort--but we don't care
			 * which!  Just add it to the list of txns
			 * that are resolved.
			 */
			if ((ret = __txn_regop_read(dbenv, rec.data,
			    &regop_args)) != 0)
				goto err;

			ret = __db_txnlist_find(dbenv,
			    txninfo, regop_args->txnid->txnid, &status);
			if (ret == DB_NOTFOUND)
				ret = __db_txnlist_add(dbenv, txninfo,
				    regop_args->txnid->txnid,
				    regop_args->opcode, &lsn);
			else if (ret != 0)
				goto err;
			__os_free(dbenv, regop_args);
			break;
		case DB___txn_xa_regop:
			/*
			 * It's a prepare.  If its not aborted and
			 * we haven't put the txn on our list yet, it
			 * hasn't been resolved, so apply and restore it.
			 */
			if ((ret = __txn_xa_regop_read(dbenv, rec.data,
			    &prep_args)) != 0)
				goto err;
			ret = __db_txnlist_find(dbenv, txninfo,
			    prep_args->txnid->txnid, &status);
			if (ret == DB_NOTFOUND) {
				if (prep_args->opcode == TXN_ABORT)
					ret = __db_txnlist_add(dbenv, txninfo,
					    prep_args->txnid->txnid,
					    prep_args->opcode, &lsn);
				else if ((ret =
				    __rep_process_txn(dbenv, &rec)) == 0)
					ret = __txn_restore_txn(dbenv,
					    &lsn, prep_args);
			} else if (ret != 0)
				goto err;
			__os_free(dbenv, prep_args);
			break;
		default:
			continue;
		}
	}

	/* It's not an error to have hit the beginning of the log. */
	if (ret == DB_NOTFOUND)
		ret = 0;

done:
err:	t_ret = __log_c_close(logc);

	if (txninfo != NULL)
		__db_txnlist_end(dbenv, txninfo);

	return (ret == 0 ? t_ret : ret);
}

/*
 * PUBLIC: int __rep_get_limit __P((DB_ENV *, u_int32_t *, u_int32_t *));
 */
int
__rep_get_limit(dbenv, gbytesp, bytesp)
	DB_ENV *dbenv;
	u_int32_t *gbytesp, *bytesp;
{
	DB_REP *db_rep;
	REP *rep;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_get_limit",
	    DB_INIT_REP);

	if (!REP_ON(dbenv)) {
		__db_err(dbenv,
    "DB_ENV->get_rep_limit: database environment not properly initialized");
		return (__db_panic(dbenv, EINVAL));
	}
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	if (gbytesp != NULL)
		*gbytesp = rep->gbytes;
	if (bytesp != NULL)
		*bytesp = rep->bytes;

	return (0);
}

/*
 * __rep_set_limit --
 *	Set a limit on the amount of data that will be sent during a single
 * invocation of __rep_process_message.
 *
 * PUBLIC: int __rep_set_limit __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__rep_set_limit(dbenv, gbytes, bytes)
	DB_ENV *dbenv;
	u_int32_t gbytes, bytes;
{
	DB_REP *db_rep;
	REP *rep;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->rep_set_limit");
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_set_limit",
	    DB_INIT_REP);

	if (!REP_ON(dbenv)) {
		__db_err(dbenv,
    "DB_ENV->set_rep_limit: database environment not properly initialized");
		return (__db_panic(dbenv, EINVAL));
	}
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	REP_SYSTEM_LOCK(dbenv);
	if (bytes > GIGABYTE) {
		gbytes += bytes / GIGABYTE;
		bytes = bytes % GIGABYTE;
	}
	rep->gbytes = gbytes;
	rep->bytes = bytes;
	REP_SYSTEM_UNLOCK(dbenv);

	return (0);
}

/*
 * __rep_set_request --
 *	Set the minimum and maximum number of log records that we wait
 *	before retransmitting.
 *
 * !!!
 * UNDOCUMENTED.
 *
 * PUBLIC: int __rep_set_request __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__rep_set_request(dbenv, min, max)
	DB_ENV *dbenv;
	u_int32_t min, max;
{
	LOG *lp;
	DB_LOG *dblp;
	DB_REP *db_rep;
	REP *rep;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->rep_set_request");
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_set_request",
	    DB_INIT_REP);

	if (!REP_ON(dbenv)) {
		__db_err(dbenv,
    "DB_ENV->set_rep_request: database environment not properly initialized");
		return (__db_panic(dbenv, EINVAL));
	}
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	/*
	 * We acquire the mtx_region or mtx_clientdb mutexes as needed.
	 */
	REP_SYSTEM_LOCK(dbenv);
	rep->request_gap = min;
	rep->max_gap = max;
	REP_SYSTEM_UNLOCK(dbenv);

	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	dblp = dbenv->lg_handle;
	if (dblp != NULL && (lp = dblp->reginfo.primary) != NULL) {
		lp->wait_recs = 0;
		lp->rcvd_recs = 0;
	}
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);

	return (0);
}

/*
 * __rep_set_transport --
 *	Set the transport function for replication.
 *
 * PUBLIC: int __rep_set_rep_transport __P((DB_ENV *, int,
 * PUBLIC:     int (*)(DB_ENV *, const DBT *, const DBT *, const DB_LSN *,
 * PUBLIC:     int, u_int32_t)));
 */
int
__rep_set_rep_transport(dbenv, eid, f_send)
	DB_ENV *dbenv;
	int eid;
	int (*f_send) __P((DB_ENV *, const DBT *, const DBT *, const DB_LSN *,
	    int, u_int32_t));
{
	PANIC_CHECK(dbenv);

	if (f_send == NULL) {
		__db_err(dbenv,
	"DB_ENV->set_rep_transport: no send function specified");
		return (EINVAL);
	}
	if (eid < 0) {
		__db_err(dbenv,
	"DB_ENV->set_rep_transport: eid must be greater than or equal to 0");
		return (EINVAL);
	}
	dbenv->rep_send = f_send;
	dbenv->rep_eid = eid;
	return (0);
}

/*
 * __rep_flush --
 *	Re-push the last log record to all clients, in case they've lost
 *	messages and don't know it.
 *
 * PUBLIC: int __rep_flush __P((DB_ENV *));
 */
int
__rep_flush(dbenv)
	DB_ENV *dbenv;
{
	DBT rec;
	DB_LOGC *logc;
	DB_LSN lsn;
	int ret, t_ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle, "rep_flush", DB_INIT_REP);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	memset(&rec, 0, sizeof(rec));
	memset(&lsn, 0, sizeof(lsn));

	if ((ret = __log_c_get(logc, &lsn, &rec, DB_LAST)) != 0)
		goto err;

	(void)__rep_send_message(dbenv,
	    DB_EID_BROADCAST, REP_LOG, &lsn, &rec, 0, 0);

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __rep_sync --
 *	Force a synchronization to occur between this client and the master.
 *	This is the other half of configuring DELAYCLIENT.
 *
 * PUBLIC: int __rep_sync __P((DB_ENV *, u_int32_t));
 */
int
__rep_sync(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_LSN lsn;
	DB_REP *db_rep;
	LOG *lp;
	REP *rep;
	int master;
	u_int32_t type;

	COMPQUIET(flags, 0);
	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->rep_handle,
	    "rep_sync", DB_INIT_REP);

	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;
	db_rep = dbenv->rep_handle;
	rep = db_rep->region;

	/*
	 * Simple cases.  If we're not in the DELAY state we have nothing
	 * to do.  If we don't know who the master is, send a MASTER_REQ.
	 */
	MUTEX_LOCK(dbenv, rep->mtx_clientdb);
	lsn = lp->verify_lsn;
	MUTEX_UNLOCK(dbenv, rep->mtx_clientdb);
	REP_SYSTEM_LOCK(dbenv);
	master = rep->master_id;
	if (master == DB_EID_INVALID) {
		REP_SYSTEM_UNLOCK(dbenv);
		(void)__rep_send_message(dbenv, DB_EID_BROADCAST,
		    REP_MASTER_REQ, NULL, NULL, 0, 0);
		return (0);
	}
	/*
	 * We want to hold the rep mutex to test and then clear the
	 * DELAY flag.  Racing threads in here could otherwise result
	 * in dual data streams.
	 */
	if (!F_ISSET(rep, REP_F_DELAY)) {
		REP_SYSTEM_UNLOCK(dbenv);
		return (0);
	}

	/*
	 * If we get here, we clear the delay flag and kick off a
	 * synchronization.  From this point forward, we will
	 * synchronize until the next time the master changes.
	 */
	F_CLR(rep, REP_F_DELAY);
	REP_SYSTEM_UNLOCK(dbenv);
	/*
	 * When we set REP_F_DELAY, we set verify_lsn to the real verify
	 * lsn if we need to verify, or we zeroed it out if this is a client
	 * that needs to sync up from the beginning.  So, send the type
	 * of message now that __rep_new_master delayed sending.
	 */
	if (IS_ZERO_LSN(lsn))
		type = REP_ALL_REQ;
	else
		type = REP_VERIFY_REQ;
	(void)__rep_send_message(dbenv, master, type, &lsn, NULL, 0,
	    DB_REP_ANYWHERE);
	return (0);
}
