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
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
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
static const char revid[] = "$Id: txn.c,v 11.61 2001/01/10 18:18:52 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

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

#include <string.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "txn.h"
#include "lock.h"
#include "log.h"
#include "db_dispatch.h"
#include "db_page.h"
#include "db_ext.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int  __txn_begin __P((DB_TXN *));
static int  __txn_isvalid __P((const DB_TXN *, TXN_DETAIL **, u_int32_t));
static int  __txn_undo __P((DB_TXN *));

/*
 * txn_begin --
 *	This is a wrapper to the actual begin process.  Normal txn_begin()
 * allocates a DB_TXN structure for the caller, while txn_xa_begin() does
 * not.  Other than that, both call into the common __txn_begin code().
 *
 * Internally, we use TXN_DETAIL structures, but the DB_TXN structure
 * provides access to the transaction ID and the offset in the transaction
 * region of the TXN_DETAIL structure.
 */
int
txn_begin(dbenv, parent, txnpp, flags)
	DB_ENV *dbenv;
	DB_TXN *parent, **txnpp;
	u_int32_t flags;
{
	DB_TXN *txn;
	int ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_begin(dbenv, parent, txnpp, flags));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, DB_INIT_TXN);

	if ((ret = __db_fchk(dbenv,
	    "txn_begin", flags,
	    DB_TXN_NOWAIT | DB_TXN_NOSYNC | DB_TXN_SYNC)) != 0)
		return (ret);
	if ((ret = __db_fcchk(dbenv,
	    "txn_begin", flags, DB_TXN_NOSYNC, DB_TXN_SYNC)) != 0)
		return (ret);

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXN), &txn)) != 0)
		return (ret);

	txn->mgrp = dbenv->tx_handle;
	txn->parent = parent;
	TAILQ_INIT(&txn->kids);
	txn->flags = TXN_MALLOC;
	if (LF_ISSET(DB_TXN_NOSYNC))
		F_SET(txn, TXN_NOSYNC);
	if (LF_ISSET(DB_TXN_SYNC))
		F_SET(txn, TXN_SYNC);
	if (LF_ISSET(DB_TXN_NOWAIT))
		F_SET(txn, TXN_NOWAIT);

	if ((ret = __txn_begin(txn)) != 0) {
		__os_free(txn, sizeof(DB_TXN));
		txn = NULL;
	}

	if (txn != NULL && parent != NULL)
		TAILQ_INSERT_HEAD(&parent->kids, txn, klinks);

	*txnpp = txn;
	return (ret);
}

/*
 * __txn_xa_begin --
 *	XA version of txn_begin.
 *
 * PUBLIC: int __txn_xa_begin __P((DB_ENV *, DB_TXN *));
 */
int
__txn_xa_begin(dbenv, txn)
	DB_ENV *dbenv;
	DB_TXN *txn;
{
	PANIC_CHECK(dbenv);

	memset(txn, 0, sizeof(DB_TXN));

	txn->mgrp = dbenv->tx_handle;

	return (__txn_begin(txn));
}

/*
 * __txn_begin --
 *	Normal DB version of txn_begin.
 */
static int
__txn_begin(txn)
	DB_TXN *txn;
{
	DB_ENV *dbenv;
	DB_LSN begin_lsn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *td;
	size_t off;
	u_int32_t id;
	int ret;

	mgr = txn->mgrp;
	dbenv = mgr->dbenv;
	region = mgr->reginfo.primary;

	/*
	 * We do not have to write begin records (and if we do not, then we
	 * need never write records for read-only transactions).  However,
	 * we do need to find the current LSN so that we can store it in the
	 * transaction structure, so we can know where to take checkpoints.
	 */
	if (LOGGING_ON(dbenv) &&
	    (ret = log_put(dbenv, &begin_lsn, NULL, DB_CURLSN)) != 0)
		goto err2;

	R_LOCK(dbenv, &mgr->reginfo);

	/* Make sure that last_txnid is not going to wrap around. */
	if (region->last_txnid == TXN_INVALID) {
		__db_err(dbenv,
"txn_begin: transaction ID wrapped.  Exit the database environment\nand restart the application as if application failure had occurred");
		ret = EINVAL;
		goto err1;
	}

	/* Allocate a new transaction detail structure. */
	if ((ret =
	    __db_shalloc(mgr->reginfo.addr, sizeof(TXN_DETAIL), 0, &td)) != 0) {
		__db_err(dbenv,
		     "Unable to allocate memory for transaction detail");
		goto err1;
	}

	/* Place transaction on active transaction list. */
	SH_TAILQ_INSERT_HEAD(&region->active_txn, td, links, __txn_detail);

	id = ++region->last_txnid;
	++region->nbegins;
	if (++region->nactive > region->maxnactive)
		region->maxnactive = region->nactive;

	td->txnid = id;
	td->begin_lsn = begin_lsn;
	ZERO_LSN(td->last_lsn);
	td->status = TXN_RUNNING;
	if (txn->parent != NULL)
		td->parent = txn->parent->off;
	else
		td->parent = INVALID_ROFF;

	off = R_OFFSET(&mgr->reginfo, td);
	R_UNLOCK(dbenv, &mgr->reginfo);

	ZERO_LSN(txn->last_lsn);
	txn->txnid = id;
	txn->off = off;

	/*
	 * If this is a transaction family, we must link the child to the
	 * maximal grandparent in the lock table for deadlock detection.
	 */
	if (txn->parent != NULL && LOCKING_ON(dbenv))
		if ((ret = __lock_addfamilylocker(dbenv,
		    txn->parent->txnid, txn->txnid)) != 0)
			goto err2;

	if (F_ISSET(txn, TXN_MALLOC)) {
		MUTEX_THREAD_LOCK(dbenv, mgr->mutexp);
		TAILQ_INSERT_TAIL(&mgr->txn_chain, txn, links);
		MUTEX_THREAD_UNLOCK(dbenv, mgr->mutexp);
	}

	return (0);

err1:	R_UNLOCK(dbenv, &mgr->reginfo);

err2:	return (ret);
}

/*
 * txn_commit --
 *	Commit a transaction.
 */
int
txn_commit(txnp, flags)
	DB_TXN *txnp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_TXN *kid;
	int is_commit, ret, t_ret;

	dbenv = txnp->mgrp->dbenv;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_commit(txnp, flags));
#endif

	PANIC_CHECK(dbenv);

	if ((ret = __txn_isvalid(txnp, NULL, TXN_COMMITTED)) != 0)
		return (ret);

	/*
	 * We clear flags that are incorrect, ignoring any flag errors, and
	 * default to synchronous operations.  By definition, transaction
	 * handles are dead when we return, and this error should never
	 * happen, but we don't want to fail in the field 'cause the app is
	 * specifying the wrong flag for some reason.
	 */
	if (__db_fchk(dbenv,
	    "txn_commit", flags, DB_TXN_NOSYNC | DB_TXN_SYNC) != 0)
		flags = DB_TXN_SYNC;
	if (__db_fcchk(dbenv,
	    "txn_commit", flags, DB_TXN_NOSYNC, DB_TXN_SYNC) != 0)
		flags = DB_TXN_SYNC;
	if (LF_ISSET(DB_TXN_NOSYNC)) {
		F_CLR(txnp, TXN_SYNC);
		F_SET(txnp, TXN_NOSYNC);
	}
	if (LF_ISSET(DB_TXN_SYNC)) {
		F_CLR(txnp, TXN_NOSYNC);
		F_SET(txnp, TXN_SYNC);
	}

	/*
	 * Commit any unresolved children.  If there's an error, abort any
	 * unresolved children and the parent.
	 */
	while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
		if ((ret = txn_commit(kid, flags)) != 0) {
			while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
				(void)txn_abort(kid);
			(void)txn_abort(txnp);
			goto err;
		}

	/*
	 * If there are any log records, write a log record and sync the log,
	 * else do no log writes.  If the commit is for a child transaction,
	 * we do not need to commit the child synchronously since it may still
	 * abort (if its parent aborts), and otherwise its parent or ultimate
	 * ancestor will write synchronously.
	 *
	 * I'd rather return a logging error than a flag-wrong error, so if
	 * the log routines fail, set "ret" without regard to previous value.
	 */
	if (LOGGING_ON(dbenv) && !IS_ZERO_LSN(txnp->last_lsn)) {
		if (txnp->parent == NULL) {
			if ((t_ret = __txn_regop_log(dbenv,
			    txnp, &txnp->last_lsn,
			    (F_ISSET(dbenv, DB_ENV_TXN_NOSYNC) &&
			    !F_ISSET(txnp, TXN_SYNC)) ||
			    F_ISSET(txnp, TXN_NOSYNC) ?  0 : DB_FLUSH,
			    TXN_COMMIT, (int32_t)time(NULL))) != 0) {
				ret = t_ret;
				goto err;
			}
		} else {
			/* Log the commit in the parent! */
			if ((t_ret = __txn_child_log(dbenv,
			    txnp->parent, &txnp->parent->last_lsn,
			    0, txnp->txnid, &txnp->last_lsn)) != 0) {
				ret = t_ret;
				goto err;
			}

			F_SET(txnp->parent, TXN_CHILDCOMMIT);
		}
	}

	is_commit = 1;
	if (0) {
err:		is_commit = 0;
	}
	if ((t_ret = __txn_end(txnp, is_commit)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * txn_abort --
 *	Abort a transaction.
 */
int
txn_abort(txnp)
	DB_TXN *txnp;
{
	DB_ENV *dbenv;
	DB_TXN *kid;
	int ret, t_ret;

	dbenv = txnp->mgrp->dbenv;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_abort(txnp));
#endif

	PANIC_CHECK(dbenv);

	if ((ret = __txn_isvalid(txnp, NULL, TXN_ABORTED)) != 0)
		return (ret);

	/* Abort any unresolved children. */
	while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
		if ((t_ret = txn_abort(kid)) != 0 && ret == 0)
			ret = t_ret;

	if ((t_ret = __txn_undo(txnp)) != 0 && ret == 0)
		ret = t_ret;

	if ((t_ret = __txn_end(txnp, 0)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * txn_prepare --
 *	Flush the log so a future commit is guaranteed to succeed.
 */
int
txn_prepare(txnp)
	DB_TXN *txnp;
{
	DBT xid;
	DB_ENV *dbenv;
	DB_TXN *kid;
	TXN_DETAIL *td;
	int ret;

	dbenv = txnp->mgrp->dbenv;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_prepare(txnp));
#endif

	PANIC_CHECK(dbenv);

	if ((ret = __txn_isvalid(txnp, &td, TXN_PREPARED)) != 0)
		return (ret);

	/* Prepare any unresolved children. */
	while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
		if ((ret = txn_prepare(kid)) != 0)
			return (ret);

	/*
	 * We indicate that a transaction is an XA transaction by putting
	 * a valid size in the xid.size fiels.  XA requires that the transaction
	 * be either ENDED or SUSPENDED when prepare is called, so we know
	 * that if the xa_status isn't in one of those states, but we are
	 * calling prepare that we are not an XA transaction.
	 */

	if (LOGGING_ON(dbenv)) {
		memset(&xid, 0, sizeof(xid));
		xid.data = td->xid;
		xid.size = td->xa_status != TXN_XA_ENDED &&
		    td->xa_status != TXN_XA_SUSPENDED ?  0 : sizeof(td->xid);
		if ((ret = __txn_xa_regop_log(dbenv, txnp, &txnp->last_lsn,
		    (F_ISSET(dbenv, DB_ENV_TXN_NOSYNC) &&
		    !F_ISSET(txnp, TXN_SYNC)) ||
		    F_ISSET(txnp, TXN_NOSYNC) ? 0 : DB_FLUSH, TXN_PREPARE,
		    &xid, td->format, td->gtrid, td->bqual,
		    &td->begin_lsn)) != 0) {
			__db_err(dbenv, "txn_prepare: log_write failed %s",
			    db_strerror(ret));
			return (ret);
		}
		if (txnp->parent != NULL)
			F_SET(txnp->parent, TXN_CHILDCOMMIT);
	}

	MUTEX_THREAD_LOCK(dbenv, txnp->mgrp->mutexp);
	td->status = TXN_PREPARED;
	MUTEX_THREAD_UNLOCK(dbenv, txnp->mgrp->mutexp);
	return (0);
}

/*
 * txn_id --
 *	Return the transaction ID.
 */
u_int32_t
txn_id(txnp)
	DB_TXN *txnp;
{
	return (txnp->txnid);
}

/*
 * __txn_isvalid --
 *	Return 0 if the txnp is reasonable, otherwise panic.
 */
static int
__txn_isvalid(txnp, tdp, op)
	const DB_TXN *txnp;
	TXN_DETAIL **tdp;
	u_int32_t op;
{
	DB_TXNMGR *mgrp;
	TXN_DETAIL *tp;

	mgrp = txnp->mgrp;

	/* Check for live cursors. */
	if (txnp->cursors != 0) {
		__db_err(mgrp->dbenv, "transaction has active cursors");
		goto err;
	}

	/* Check transaction's status. */
	tp = (TXN_DETAIL *)R_ADDR(&mgrp->reginfo, txnp->off);
	if (tdp != NULL)
		*tdp = tp;

	switch (tp->status) {
	case TXN_ABORTED:
	case TXN_COMMITTED:
	default:
		__db_err(mgrp->dbenv, "transaction already %s",
		    tp->status == TXN_COMMITTED ? "committed" : "aborted");
		goto err;
	case TXN_PREPARED:
		if (op == TXN_PREPARED) {
			__db_err(mgrp->dbenv, "transaction already prepared");
			goto err;
		}
	case TXN_RUNNING:
		break;
	}

	return (0);

err:	/*
	 * If there's a serious problem with the transaction, panic.  TXN
	 * handles are dead by definition when we return, and if you use
	 * a cursor you forgot to close, we have no idea what will happen.
	 */
	return (__db_panic(mgrp->dbenv, EINVAL));
}

/*
 * __txn_end --
 *	Internal transaction end routine.
 *
 * PUBLIC: int __txn_end __P((DB_TXN *, int));
 */
int
__txn_end(txnp, is_commit)
	DB_TXN *txnp;
	int is_commit;
{
	DB_ENV *dbenv;
	DB_LOCKREQ request;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *tp;
	int ret;

	mgr = txnp->mgrp;
	dbenv = mgr->dbenv;
	region = mgr->reginfo.primary;

	/* Release the locks. */
	request.op = txnp->parent == NULL ||
	    is_commit == 0 ? DB_LOCK_PUT_ALL : DB_LOCK_INHERIT;

	if (LOCKING_ON(dbenv)) {
		ret = lock_vec(dbenv, txnp->txnid, 0, &request, 1, NULL);
		if (ret != 0 && (ret != DB_LOCK_DEADLOCK || is_commit)) {
			__db_err(dbenv, "%s: release locks failed %s",
			    is_commit ? "txn_commit" : "txn_abort",
			    db_strerror(ret));
			__db_panic(dbenv, ret);
		}
	}

	/* End the transaction. */
	R_LOCK(dbenv, &mgr->reginfo);

	tp = (TXN_DETAIL *)R_ADDR(&mgr->reginfo, txnp->off);
	SH_TAILQ_REMOVE(&region->active_txn, tp, links, __txn_detail);
	__db_shalloc_free(mgr->reginfo.addr, tp);

	if (is_commit)
		region->ncommits++;
	else
		region->naborts++;
	--region->nactive;

	R_UNLOCK(dbenv, &mgr->reginfo);

	/*
	 * The transaction cannot get more locks, remove its locker info.
	 */
	if (LOCKING_ON(dbenv))
		__lock_freefamilylocker(dbenv->lk_handle, txnp->txnid);
	if (txnp->parent != NULL)
		TAILQ_REMOVE(&txnp->parent->kids, txnp, klinks);

	/* Free the space. */
	if (F_ISSET(txnp, TXN_MALLOC)) {
		MUTEX_THREAD_LOCK(dbenv, mgr->mutexp);
		TAILQ_REMOVE(&mgr->txn_chain, txnp, links);
		MUTEX_THREAD_UNLOCK(dbenv, mgr->mutexp);

		__os_free(txnp, sizeof(*txnp));
	}

	return (0);
}

/*
 * __txn_undo --
 *	Undo the transaction with id txnid.  Returns 0 on success and
 *	errno on failure.
 */
static int
__txn_undo(txnp)
	DB_TXN *txnp;
{
	DBT rdbt;
	DB_ENV *dbenv;
	DB_LSN key_lsn;
	DB_TXNMGR *mgr;
	void *txnlist;
	int ret, threaded;

	mgr = txnp->mgrp;
	dbenv = mgr->dbenv;
	txnlist = NULL;

	if (!LOGGING_ON(dbenv))
		return (0);

	/*
	 * This is the simplest way to code this, but if the mallocs during
	 * recovery turn out to be a performance issue, we can do the
	 * allocation here and use DB_DBT_USERMEM.
	 */
	memset(&rdbt, 0, sizeof(rdbt));
	threaded = F_ISSET(dbenv, DB_ENV_THREAD) ? 1 : 0;
	if (threaded)
		F_SET(&rdbt, DB_DBT_MALLOC);

	key_lsn = txnp->last_lsn;

	/* Allocate a transaction list for children or aborted page creates. */
	if ((ret = __db_txnlist_init(dbenv, &txnlist)) != 0)
		return (ret);

	if (F_ISSET(txnp, TXN_CHILDCOMMIT) &&
	    (ret = __db_txnlist_lsninit(dbenv,
	    txnlist, &txnp->last_lsn)) != 0)
		return (ret);

	for (ret = 0; ret == 0 && !IS_ZERO_LSN(key_lsn);) {
		/*
		 * The dispatch routine returns the lsn of the record
		 * before the current one in the key_lsn argument.
		 */
		if ((ret = log_get(dbenv, &key_lsn, &rdbt, DB_SET)) == 0) {
			ret = __db_dispatch(dbenv,
			    &rdbt, &key_lsn, DB_TXN_ABORT, txnlist);
			if (threaded && rdbt.data != NULL) {
				__os_free(rdbt.data, rdbt.size);
				rdbt.data = NULL;
			}
			if (F_ISSET(txnp, TXN_CHILDCOMMIT))
				(void)__db_txnlist_lsnadd(dbenv,
				    txnlist, &key_lsn, 0);
		}
		if (ret != 0) {
			__db_err(txnp->mgrp->dbenv,
			    "txn_abort: Log undo failed for LSN: %lu %lu: %s",
			    (u_long)key_lsn.file, (u_long)key_lsn.offset,
			    db_strerror(ret));
			if (txnlist != NULL)
				__db_txnlist_end(dbenv, txnlist);
			return (ret);
		}
	}

	if (txnlist != NULL) {
		__db_do_the_limbo(dbenv, txnlist);
		__db_txnlist_end(dbenv, txnlist);
	}

	return (ret);
}

/*
 * Transaction checkpoint.
 * If either kbytes or minutes is non-zero, then we only take the checkpoint
 * more than "minutes" minutes have passed since the last checkpoint or if
 * more than "kbytes" of log data have been written since the last checkpoint.
 * When taking a checkpoint, find the oldest active transaction and figure out
 * its first LSN.  This is the lowest LSN we can checkpoint, since any record
 * written after since that point may be involved in a transaction and may
 * therefore need to be undone in the case of an abort.
 */
int
txn_checkpoint(dbenv, kbytes, minutes, flags)
	DB_ENV *dbenv;
	u_int32_t kbytes, minutes, flags;
{
	DB_LOG *dblp;
	DB_LSN ckp_lsn, sync_lsn, last_ckp;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	LOG *lp;
	TXN_DETAIL *txnp;
	time_t last_ckp_time, now;
	u_int32_t bytes, mbytes;
	int interval, ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_checkpoint(dbenv, kbytes, minutes));
#endif
	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, DB_INIT_TXN);

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	/*
	 * Check if we need to checkpoint.
	 */
	ZERO_LSN(ckp_lsn);

	if (LF_ISSET(DB_FORCE))
		goto do_ckp;

	R_LOCK(dbenv, &dblp->reginfo);
	mbytes = lp->stat.st_wc_mbytes;
	/*
	 * We add the current buffer offset so as to count bytes that
	 * have not yet been written, but are sitting in the log buffer.
	 */
	bytes = lp->stat.st_wc_bytes + lp->b_off;
	ckp_lsn = lp->lsn;
	R_UNLOCK(dbenv, &dblp->reginfo);

	/* Don't checkpoint a quiescent database. */
	if (bytes == 0 && mbytes == 0)
		return (0);

	if (kbytes != 0 && mbytes * 1024 + bytes / 1024 >= (u_int32_t)kbytes)
		goto do_ckp;

	if (minutes != 0) {
		(void)time(&now);

		R_LOCK(dbenv, &mgr->reginfo);
		last_ckp_time = region->time_ckp;
		R_UNLOCK(dbenv, &mgr->reginfo);

		if (now - last_ckp_time >= (time_t)(minutes * 60))
			goto do_ckp;
	}

	/*
	 * If we checked time and data and didn't go to checkpoint,
	 * we're done.
	 */
	if (minutes != 0 || kbytes != 0)
		return (0);

do_ckp:
	if (IS_ZERO_LSN(ckp_lsn)) {
		R_LOCK(dbenv, &dblp->reginfo);
		ckp_lsn = lp->lsn;
		R_UNLOCK(dbenv, &dblp->reginfo);
	}

	/*
	 * We have to find an LSN such that all transactions begun
	 * before that LSN are complete.
	 */
	R_LOCK(dbenv, &mgr->reginfo);

	if (IS_ZERO_LSN(region->pending_ckp)) {
		for (txnp =
		    SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
		    txnp != NULL;
		    txnp = SH_TAILQ_NEXT(txnp, links, __txn_detail)) {

			/*
			 * Look through the active transactions for the
			 * lowest begin lsn.
			 */
			if (!IS_ZERO_LSN(txnp->begin_lsn) &&
			    log_compare(&txnp->begin_lsn, &ckp_lsn) < 0)
				ckp_lsn = txnp->begin_lsn;
		}
		region->pending_ckp = ckp_lsn;
	} else
		ckp_lsn = region->pending_ckp;

	R_UNLOCK(dbenv, &mgr->reginfo);

	/*
	 * Try three times to sync the mpool buffers up to the specified LSN,
	 * sleeping 1, 2 and 4 seconds between attempts.
	 */
	if (MPOOL_ON(dbenv))
		for (interval = 1;;) {
			/*
			 * memp_sync may change the lsn you pass it, so don't
			 * pass it the actual ckp_lsn, pass it a local instead.
			 */
			sync_lsn = ckp_lsn;
			if ((ret = memp_sync(dbenv, &sync_lsn)) == 0)
				break;

			/*
			 * ret == DB_INCOMPLETE means there are still buffers
			 * to flush, the checkpoint is not complete.
			 */
			if (ret == DB_INCOMPLETE) {
				if (interval > 4)
					return (ret);

				(void)__os_sleep(dbenv, interval, 0);
				interval *= 2;
			} else {
				__db_err(dbenv,
				    "txn_checkpoint: failure in memp_sync %s",
				    db_strerror(ret));
				return (ret);
			}
		}

	if (LOGGING_ON(dbenv)) {
		R_LOCK(dbenv, &mgr->reginfo);
		last_ckp = region->last_ckp;
		ZERO_LSN(region->pending_ckp);
		R_UNLOCK(dbenv, &mgr->reginfo);

		if ((ret = __txn_ckp_log(dbenv,
		    NULL, &ckp_lsn, DB_CHECKPOINT, &ckp_lsn,
		    &last_ckp, (int32_t)time(NULL))) != 0) {
			__db_err(dbenv,
			    "txn_checkpoint: log failed at LSN [%ld %ld] %s",
			    (long)ckp_lsn.file, (long)ckp_lsn.offset,
			    db_strerror(ret));
			return (ret);
		}

		R_LOCK(dbenv, &mgr->reginfo);
		region->last_ckp = ckp_lsn;
		(void)time(&region->time_ckp);
		R_UNLOCK(dbenv, &mgr->reginfo);
	}
	return (0);
}

/*
 * __txn_activekids --
 *	Return if this transaction has any active children.
 *
 * PUBLIC: int __txn_activekids __P((DB_ENV *, u_int32_t, DB_TXN *));
 */
int
__txn_activekids(dbenv, rectype, txnp)
	DB_ENV *dbenv;
	u_int32_t rectype;
	DB_TXN *txnp;
{
	/*
	 * On a child commit, we know that there are children (i.e., the
	 * commiting child at the least.  In that case, skip this check.
	 */
	if (rectype == DB_txn_child)
		return (0);

	if (TAILQ_FIRST(&txnp->kids) != NULL) {
		__db_err(dbenv, "Child transaction is active");
		return (EPERM);
	}
	return (0);
}
