/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: txn_failchk.c,v 12.2 2005/10/13 00:51:51 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/txn.h"

/*
 * __txn_failchk --
 *	Check for transactions started by dead threads of control.
 *
 * PUBLIC: int __txn_failchk __P((DB_ENV *));
 */
int
__txn_failchk(dbenv)
	DB_ENV *dbenv;
{
	DB_TXN *ktxn, *txn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *ktd, *td;
	db_threadid_t tid;
	int ret;
	char buf[DB_THREADID_STRLEN];
	pid_t pid;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

retry:	TXN_SYSTEM_LOCK(dbenv);

	SH_TAILQ_FOREACH(td, &region->active_txn, links, __txn_detail) {
		/*
		 * If this is a child transaction, skip it.
		 * The parent will take care of it.
		 */
		if (td->parent != INVALID_ROFF)
			continue;
		/*
		 * If the txn is prepared, then it does not matter
		 * what the state of the thread is.
		 */
		if (td->status == TXN_PREPARED)
			continue;

		/* If the thread is still alive, it's not a problem. */
		if (dbenv->is_alive(dbenv, td->pid, td->tid))
			continue;

		if (F_ISSET(td, TXN_DTL_INMEMORY))
			return (__db_failed(dbenv,
			     "Transaction has in memory logs",
			     td->pid, td->tid));

		/* Abort the transaction. */
		TXN_SYSTEM_UNLOCK(dbenv);
		if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXN), &txn)) != 0)
			return (ret);
		__txn_continue(dbenv, txn, td);
		F_SET(txn, TXN_MALLOC);
		SH_TAILQ_FOREACH(ktd, &td->kids, klinks, __txn_detail) {
			if (F_ISSET(ktd, TXN_DTL_INMEMORY))
				return (__db_failed(dbenv,
				     "Transaction has in memory logs",
				     td->pid, td->tid));
			if ((ret =
			    __os_calloc(dbenv, 1, sizeof(DB_TXN), &ktxn)) != 0)
				return (ret);
			__txn_continue(dbenv, ktxn, ktd);
			F_SET(ktxn, TXN_MALLOC);
			ktxn->parent = txn;
			TAILQ_INSERT_HEAD(&txn->kids, txn, klinks);
		}
		TAILQ_INSERT_TAIL(&mgr->txn_chain, txn, links);
		pid = td->pid;
		tid = td->tid;
		(void)dbenv->thread_id_string(dbenv, pid, tid, buf);
		__db_msg(dbenv,
		    "Aborting txn %#lx: %s", (u_long)txn->txnid, buf);
		if ((ret = __txn_abort(txn)) != 0)
			return (__db_failed(dbenv,
			     "Transaction abort failed", pid, tid));
		goto retry;
	}

	TXN_SYSTEM_UNLOCK(dbenv);

	return (0);
}
