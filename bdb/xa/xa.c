/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: xa.c,v 11.10 2000/12/14 07:39:14 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "log.h"
#include "txn.h"
#include "db_am.h"
#include "db_dispatch.h"

static int  __db_xa_close __P((char *, int, long));
static int  __db_xa_commit __P((XID *, int, long));
static int  __db_xa_complete __P((int *, int *, int, long));
static int  __db_xa_end __P((XID *, int, long));
static int  __db_xa_forget __P((XID *, int, long));
static int  __db_xa_open __P((char *, int, long));
static int  __db_xa_prepare __P((XID *, int, long));
static int  __db_xa_recover __P((XID *, long, int, long));
static int  __db_xa_rollback __P((XID *, int, long));
static int  __db_xa_start __P((XID *, int, long));
static void __xa_txn_end __P((DB_ENV *));
static void __xa_txn_init __P((DB_ENV *, TXN_DETAIL *, size_t));

/*
 * Possible flag values:
 *	Dynamic registration	0 => no dynamic registration
 *				TMREGISTER => dynamic registration
 *	Asynchronous operation	0 => no support for asynchrony
 *				TMUSEASYNC => async support
 *	Migration support	0 => migration of transactions across
 *				     threads is possible
 *				TMNOMIGRATE => no migration across threads
 */
const struct xa_switch_t db_xa_switch = {
	 "Berkeley DB",		/* name[RMNAMESZ] */
	 TMNOMIGRATE,		/* flags */
	 0,			/* version */
	 __db_xa_open,		/* xa_open_entry */
	 __db_xa_close,		/* xa_close_entry */
	 __db_xa_start,		/* xa_start_entry */
	 __db_xa_end,		/* xa_end_entry */
	 __db_xa_rollback,	/* xa_rollback_entry */
	 __db_xa_prepare,	/* xa_prepare_entry */
	 __db_xa_commit,	/* xa_commit_entry */
	 __db_xa_recover,	/* xa_recover_entry */
	 __db_xa_forget,	/* xa_forget_entry */
	 __db_xa_complete	/* xa_complete_entry */
};

/*
 * __db_xa_open --
 *	The open call in the XA protocol.  The rmid field is an id number
 * that the TM assigned us and will pass us on every xa call.  We need to
 * map that rmid number into a dbenv structure that we create during
 * initialization.  Since this id number is thread specific, we do not
 * need to store it in shared memory.  The file xa_map.c implements all
 * such xa->db mappings.
 *	The xa_info field is instance specific information.  We require
 * that the value of DB_HOME be passed in xa_info.  Since xa_info is the
 * only thing that we get to pass to db_env_create, any config information
 * will have to be done via a config file instead of via the db_env_create
 * call.
 */
static int
__db_xa_open(xa_info, rmid, flags)
	char *xa_info;
	int rmid;
	long flags;
{
	DB_ENV *env;

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);
	if (flags != TMNOFLAGS)
		return (XAER_INVAL);

	/* Verify if we already have this environment open. */
	if (__db_rmid_to_env(rmid, &env) == 0)
		return (XA_OK);
	if (__os_calloc(env, 1, sizeof(DB_ENV), &env) != 0)
		return (XAER_RMERR);

	/* Open a new environment. */
#define	XA_FLAGS \
	DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN
	if (db_env_create(&env, 0) != 0)
		return (XAER_RMERR);
	if (env->open(env, xa_info, XA_FLAGS, 0) != 0)
		goto err;

	/* Create the mapping. */
	if (__db_map_rmid(rmid, env) != 0)
		goto err;

	/* Allocate space for the current transaction. */
	if (__os_calloc(env, 1, sizeof(DB_TXN), &env->xa_txn) != 0)
		goto err;
	env->xa_txn->txnid = TXN_INVALID;

	return (XA_OK);

err:	(void)env->close(env, 0);

	return (XAER_RMERR);
}

/*
 * __db_xa_close --
 *	The close call of the XA protocol.  The only trickiness here
 * is that if there are any active transactions, we must fail.  It is
 * *not* an error to call close on an environment that has already been
 * closed (I am interpreting that to mean it's OK to call close on an
 * environment that has never been opened).
 */
static int
__db_xa_close(xa_info, rmid, flags)
	char *xa_info;
	int rmid;
	long flags;
{
	DB_ENV *env;
	int ret, t_ret;

	COMPQUIET(xa_info, NULL);

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);
	if (flags != TMNOFLAGS)
		return (XAER_INVAL);

	/* If the environment is closed, then we're done. */
	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XA_OK);

	/* Check if there are any pending transactions. */
	if (env->xa_txn != NULL && env->xa_txn->txnid != TXN_INVALID)
		return (XAER_PROTO);

	/* Destroy the mapping. */
	ret = __db_unmap_rmid(rmid);

	/* Discard space held for the current transaction. */
	if (env->xa_txn != NULL)
		__os_free(env->xa_txn, sizeof(DB_TXN));

	/* Close the environment. */
	if ((t_ret = env->close(env, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret == 0 ? XA_OK : XAER_RMERR);
}

/*
 * __db_xa_start --
 *	Begin a transaction for the current resource manager.
 */
static int
__db_xa_start(xid, rmid, flags)
	XID *xid;
	int rmid;
	long flags;
{
	DB_ENV *env;
	TXN_DETAIL *td;
	size_t off;
	int is_known;

#define	OK_FLAGS	(TMJOIN | TMRESUME | TMNOWAIT | TMASYNC | TMNOFLAGS)
	if (LF_ISSET(~OK_FLAGS))
		return (XAER_INVAL);

	if (LF_ISSET(TMJOIN) && LF_ISSET(TMRESUME))
		return (XAER_INVAL);

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);

	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	is_known = __db_xid_to_txn(env, xid, &off) == 0;

	if (is_known && !LF_ISSET(TMRESUME) && !LF_ISSET(TMJOIN))
		return (XAER_DUPID);

	if (!is_known && LF_ISSET(TMRESUME | TMJOIN))
		return (XAER_NOTA);

	/*
	 * This can't block, so we can ignore TMNOWAIT.
	 *
	 * Other error conditions: RMERR, RMFAIL, OUTSIDE, PROTO, RB*
	 */
	if (is_known) {
		td = (TXN_DETAIL *)
		    R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo, off);
		if (td->xa_status == TXN_XA_SUSPENDED &&
		    !LF_ISSET(TMRESUME | TMJOIN))
			return (XAER_PROTO);
		if (td->xa_status == TXN_XA_DEADLOCKED)
			return (XA_RBDEADLOCK);
		if (td->xa_status == TXN_XA_ABORTED)
			return (XA_RBOTHER);

		/* Now, fill in the global transaction structure. */
		__xa_txn_init(env, td, off);
		td->xa_status = TXN_XA_STARTED;
	} else {
		if (__txn_xa_begin(env, env->xa_txn) != 0)
			return (XAER_RMERR);
		(void)__db_map_xid(env, xid, env->xa_txn->off);
		td = (TXN_DETAIL *)
		    R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo,
		    env->xa_txn->off);
		td->xa_status = TXN_XA_STARTED;
	}
	return (XA_OK);
}

/*
 * __db_xa_end --
 *	Disassociate the current transaction from the current process.
 */
static int
__db_xa_end(xid, rmid, flags)
	XID *xid;
	int rmid;
	long flags;
{
	DB_ENV *env;
	DB_TXN *txn;
	TXN_DETAIL *td;
	size_t off;

	if (flags != TMNOFLAGS && !LF_ISSET(TMSUSPEND | TMSUCCESS | TMFAIL))
		return (XAER_INVAL);

	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	if (__db_xid_to_txn(env, xid, &off) != 0)
		return (XAER_NOTA);

	txn = env->xa_txn;
	if (off != txn->off)
		return (XAER_PROTO);

	td = (TXN_DETAIL *)R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo, off);
	if (td->xa_status == TXN_XA_DEADLOCKED)
		return (XA_RBDEADLOCK);

	if (td->status == TXN_ABORTED)
		return (XA_RBOTHER);

	if (td->xa_status != TXN_XA_STARTED)
		return (XAER_PROTO);

	/* Update the shared memory last_lsn field */
	td->last_lsn = txn->last_lsn;

	/*
	 * If we ever support XA migration, we cannot keep SUSPEND/END
	 * status in the shared region; it would have to be process local.
	 */
	if (LF_ISSET(TMSUSPEND))
		td->xa_status = TXN_XA_SUSPENDED;
	else
		td->xa_status = TXN_XA_ENDED;

	txn->txnid = TXN_INVALID;
	return (XA_OK);
}

/*
 * __db_xa_prepare --
 *	Sync the log to disk so we can guarantee recoverability.
 */
static int
__db_xa_prepare(xid, rmid, flags)
	XID *xid;
	int rmid;
	long flags;
{
	DB_ENV *env;
	TXN_DETAIL *td;
	size_t off;

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);
	if (flags != TMNOFLAGS)
		return (XAER_INVAL);

	/*
	 * We need to know if we've ever called prepare on this.
	 * As part of the prepare, we set the xa_status field to
	 * reflect that fact that prepare has been called, and if
	 * it's ever called again, it's an error.
	 */
	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	if (__db_xid_to_txn(env, xid, &off) != 0)
		return (XAER_NOTA);

	td = (TXN_DETAIL *)R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo, off);
	if (td->xa_status == TXN_XA_DEADLOCKED)
		return (XA_RBDEADLOCK);

	if (td->xa_status != TXN_XA_ENDED && td->xa_status != TXN_XA_SUSPENDED)
		return (XAER_PROTO);

	/* Now, fill in the global transaction structure. */
	__xa_txn_init(env, td, off);

	if (txn_prepare(env->xa_txn) != 0)
		return (XAER_RMERR);

	td->xa_status = TXN_XA_PREPARED;

	/* No fatal value that would require an XAER_RMFAIL. */
	__xa_txn_end(env);
	return (XA_OK);
}

/*
 * __db_xa_commit --
 *	Commit the transaction
 */
static int
__db_xa_commit(xid, rmid, flags)
	XID *xid;
	int rmid;
	long flags;
{
	DB_ENV *env;
	TXN_DETAIL *td;
	size_t off;

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);
#undef	OK_FLAGS
#define	OK_FLAGS	(TMNOFLAGS | TMNOWAIT | TMONEPHASE)
	if (LF_ISSET(~OK_FLAGS))
		return (XAER_INVAL);

	/*
	 * We need to know if we've ever called prepare on this.
	 * We can verify this by examining the xa_status field.
	 */
	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	if (__db_xid_to_txn(env, xid, &off) != 0)
		return (XAER_NOTA);

	td = (TXN_DETAIL *)R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo, off);
	if (td->xa_status == TXN_XA_DEADLOCKED)
		return (XA_RBDEADLOCK);

	if (td->xa_status == TXN_XA_ABORTED)
		return (XA_RBOTHER);

	if (LF_ISSET(TMONEPHASE) &&
	    td->xa_status != TXN_XA_ENDED && td->xa_status != TXN_XA_SUSPENDED)
		return (XAER_PROTO);

	if (!LF_ISSET(TMONEPHASE) && td->xa_status != TXN_XA_PREPARED)
		return (XAER_PROTO);

	/* Now, fill in the global transaction structure. */
	__xa_txn_init(env, td, off);

	if (txn_commit(env->xa_txn, 0) != 0)
		return (XAER_RMERR);

	/* No fatal value that would require an XAER_RMFAIL. */
	__xa_txn_end(env);
	return (XA_OK);
}

/*
 * __db_xa_recover --
 *	Returns a list of prepared and heuristically completed transactions.
 *
 * The return value is the number of xids placed into the xid array (less
 * than or equal to the count parameter).  The flags are going to indicate
 * whether we are starting a scan or continuing one.
 */
static int
__db_xa_recover(xids, count, rmid, flags)
	XID *xids;
	long count, flags;
	int rmid;
{
	__txn_xa_regop_args *argp;
	DBT data;
	DB_ENV *env;
	DB_LOG *log;
	XID *xidp;
	int err, ret;
	u_int32_t rectype, txnid;

	ret = 0;
	xidp = xids;

	/* If the environment is closed, then we're done. */
	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	/*
	 * If we are starting a scan, then we need to figure out where
	 * to begin.  If we are not starting a scan, we'll start from
	 * wherever the log cursor is.  Since XA apps cannot be threaded,
	 * we don't have to worry about someone else having moved it.
	 */
	log = env->lg_handle;
	if (LF_ISSET(TMSTARTRSCAN)) {
		if ((err = __log_findckp(env, &log->xa_first)) == DB_NOTFOUND) {
			/*
			 * If there were no log files, then we have no
			 * transactions to return, so we simply return 0.
			 */
			return (0);
		}
		if ((err = __db_txnlist_init(env, &log->xa_info)) != 0)
			return (XAER_RMERR);
	} else {
		/*
		 * If we are not starting a scan, the log cursor had
		 * better be set.
		 */
		if (IS_ZERO_LSN(log->xa_lsn))
			return (XAER_PROTO);
	}

	/*
	 * At this point log->xa_first contains the point in the log
	 * to which we need to roll back.  If we are starting a scan,
	 * we'll start at the last record; if we're continuing a scan,
	 * we'll have to start at log->xa_lsn.
	 */

	memset(&data, 0, sizeof(data));
	for (err = log_get(env, &log->xa_lsn, &data,
	    LF_ISSET(TMSTARTRSCAN) ? DB_LAST : DB_SET);
	    err == 0 && log_compare(&log->xa_lsn, &log->xa_first) > 0;
	    err = log_get(env, &log->xa_lsn, &data, DB_PREV)) {
		memcpy(&rectype, data.data, sizeof(rectype));

		/*
		 * The only record type we care about is an DB_txn_xa_regop.
		 * If it's a commit, we have to add it to a txnlist.  If it's
		 * a prepare, and we don't have a commit, then we return it.
		 * We are redoing some of what's in the xa_regop_recovery
		 * code, but we have to do it here so we can get at the xid
		 * in the record.
		 */
		if (rectype != DB_txn_xa_regop && rectype != DB_txn_regop)
			continue;

		memcpy(&txnid, (u_int8_t *)data.data + sizeof(rectype),
		    sizeof(txnid));
		err = __db_txnlist_find(log->xa_info, txnid);
		switch (rectype) {
		case DB_txn_regop:
			if (err == DB_NOTFOUND)
				__db_txnlist_add(env, log->xa_info, txnid, 0);
			err = 0;
			break;
		case DB_txn_xa_regop:
			/*
			 * This transaction is committed, so we needn't read
			 * the record and do anything.
			 */
			if (err == 0)
				break;
			if ((err =
			    __txn_xa_regop_read(env, data.data, &argp)) != 0) {
				ret = XAER_RMERR;
				goto out;
			}

			xidp->formatID = argp->formatID;
			xidp->gtrid_length = argp->gtrid;
			xidp->bqual_length = argp->bqual;
			memcpy(xidp->data, argp->xid.data, argp->xid.size);
			ret++;
			xidp++;
			__os_free(argp, sizeof(*argp));
			if (ret == count)
				goto done;
			break;
		}
	}

	if (err != 0 && err != DB_NOTFOUND)
		goto out;

done:	if (LF_ISSET(TMENDRSCAN)) {
		ZERO_LSN(log->xa_lsn);
		ZERO_LSN(log->xa_first);

out:		__db_txnlist_end(env, log->xa_info);
		log->xa_info = NULL;
	}
	return (ret);
}

/*
 * __db_xa_rollback
 *	Abort an XA transaction.
 */
static int
__db_xa_rollback(xid, rmid, flags)
	XID *xid;
	int rmid;
	long flags;
{
	DB_ENV *env;
	TXN_DETAIL *td;
	size_t off;

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);
	if (flags != TMNOFLAGS)
		return (XAER_INVAL);

	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	if (__db_xid_to_txn(env, xid, &off) != 0)
		return (XAER_NOTA);

	td = (TXN_DETAIL *)R_ADDR(&((DB_TXNMGR *)env->tx_handle)->reginfo, off);
	if (td->xa_status == TXN_XA_DEADLOCKED)
		return (XA_RBDEADLOCK);

	if (td->xa_status == TXN_XA_ABORTED)
		return (XA_RBOTHER);

	if (td->xa_status != TXN_XA_ENDED && td->xa_status != TXN_XA_SUSPENDED
	    && td->xa_status != TXN_XA_PREPARED)
		return (XAER_PROTO);

	/* Now, fill in the global transaction structure. */
	__xa_txn_init(env, td, off);
	if (txn_abort(env->xa_txn) != 0)
		return (XAER_RMERR);

	/* No fatal value that would require an XAER_RMFAIL. */
	__xa_txn_end(env);
	return (XA_OK);
}

/*
 * __db_xa_forget --
 *	Forget about an XID for a transaction that was heuristically
 * completed.  Since we do not heuristically complete anything, I
 * don't think we have to do anything here, but we should make sure
 * that we reclaim the slots in the txnid table.
 */
static int
__db_xa_forget(xid, rmid, flags)
	XID *xid;
	int rmid;
	long flags;
{
	DB_ENV *env;
	size_t off;

	if (LF_ISSET(TMASYNC))
		return (XAER_ASYNC);
	if (flags != TMNOFLAGS)
		return (XAER_INVAL);

	if (__db_rmid_to_env(rmid, &env) != 0)
		return (XAER_PROTO);

	/*
	 * If mapping is gone, then we're done.
	 */
	if (__db_xid_to_txn(env, xid, &off) != 0)
		return (XA_OK);

	__db_unmap_xid(env, xid, off);

	/* No fatal value that would require an XAER_RMFAIL. */
	return (XA_OK);
}

/*
 * __db_xa_complete --
 *	Used to wait for asynchronous operations to complete.  Since we're
 *	not doing asynch, this is an invalid operation.
 */
static int
__db_xa_complete(handle, retval, rmid, flags)
	int *handle, *retval, rmid;
	long flags;
{
	COMPQUIET(handle, NULL);
	COMPQUIET(retval, NULL);
	COMPQUIET(rmid, 0);
	COMPQUIET(flags, 0);

	return (XAER_INVAL);
}

/*
 * __xa_txn_init --
 *	Fill in the fields of the local transaction structure given
 *	the detail transaction structure.
 */
static void
__xa_txn_init(env, td, off)
	DB_ENV *env;
	TXN_DETAIL *td;
	size_t off;
{
	DB_TXN *txn;

	txn = env->xa_txn;
	txn->mgrp = env->tx_handle;
	txn->parent = NULL;
	txn->last_lsn = td->last_lsn;
	txn->txnid = td->txnid;
	txn->off = off;
	txn->flags = 0;
}

/*
 * __xa_txn_end --
 *	Invalidate a transaction structure that was generated by xa_txn_init.
 */
static void
__xa_txn_end(env)
	DB_ENV *env;
{
	DB_TXN *txn;

	txn = env->xa_txn;
	if (txn != NULL)
		txn->txnid = TXN_INVALID;
}
