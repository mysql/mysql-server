/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
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
 *
 * $Id: txn.c,v 11.249 2004/10/15 16:59:44 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <stdlib.h>

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

#include "db_int.h"
#include "dbinc/crypto.h"
#include "dbinc/hmac.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/hash.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

#define	SET_LOG_FLAGS(dbenv, txnp, lflags)				\
	do {								\
		lflags = DB_LOG_COMMIT | DB_LOG_PERM;			\
		if (F_ISSET(txnp, TXN_SYNC))				\
			lflags |= DB_FLUSH;				\
		else if (!F_ISSET(txnp, TXN_NOSYNC) &&			\
		    !F_ISSET(dbenv, DB_ENV_TXN_NOSYNC)) {		\
			if (F_ISSET(dbenv, DB_ENV_TXN_WRITE_NOSYNC))	\
				lflags |= DB_LOG_WRNOSYNC;		\
			else						\
				lflags |= DB_FLUSH;			\
		}							\
	} while (0)

/*
 * __txn_isvalid enumerated types.  We cannot simply use the transaction
 * statuses, because different statuses need to be handled differently
 * depending on the caller.
 */
typedef enum {
	TXN_OP_ABORT,
	TXN_OP_COMMIT,
	TXN_OP_DISCARD,
	TXN_OP_PREPARE
} txnop_t;

static int  __txn_abort_pp __P((DB_TXN *));
static int  __txn_begin_int __P((DB_TXN *, int));
static int  __txn_commit_pp __P((DB_TXN *, u_int32_t));
static int  __txn_discard_pp __P((DB_TXN *, u_int32_t));
static int  __txn_end __P((DB_TXN *, int));
static int  __txn_isvalid __P((const DB_TXN *, TXN_DETAIL **, txnop_t));
static int  __txn_undo __P((DB_TXN *));
static int  __txn_dispatch_undo __P((DB_ENV *,
		DB_TXN *, DBT *, DB_LSN *, void *));
static void __txn_set_begin_lsnp __P((DB_TXN *txn, DB_LSN **));

/*
 * __txn_begin_pp --
 *	DB_ENV->txn_begin pre/post processing.
 *
 * PUBLIC: int __txn_begin_pp __P((DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t));
 */
int
__txn_begin_pp(dbenv, parent, txnpp, flags)
	DB_ENV *dbenv;
	DB_TXN *parent, **txnpp;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, "txn_begin", DB_INIT_TXN);

	if ((ret = __db_fchk(dbenv,
	    "txn_begin", flags,
	    DB_DEGREE_2 | DB_DIRTY_READ | DB_TXN_NOWAIT |
	    DB_TXN_NOSYNC | DB_TXN_SYNC)) != 0)
		return (ret);
	if ((ret = __db_fcchk(dbenv,
	    "txn_begin", flags, DB_TXN_NOSYNC, DB_TXN_SYNC)) != 0)
		return (ret);

	if (parent == NULL) {
		rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
		if (rep_check)
			__op_rep_enter(dbenv);
	} else
		rep_check = 0;
	ret = __txn_begin(dbenv, parent, txnpp, flags);
	/*
	 * We only decrement the count if the operation fails.
	 * Otherwise the count will be decremented when the
	 * txn is resolved by txn_commit, txn_abort, etc.
	 */
	if (ret != 0 && rep_check)
		__op_rep_exit(dbenv);

	return (ret);
}

/*
 * __txn_begin --
 *	DB_ENV->txn_begin.
 *
 * This is a wrapper to the actual begin process.  Normal transaction begin
 * allocates a DB_TXN structure for the caller, while XA transaction begin
 * does not.  Other than that, both call into common __txn_begin_int code.
 *
 * Internally, we use TXN_DETAIL structures, but the DB_TXN structure
 * provides access to the transaction ID and the offset in the transaction
 * region of the TXN_DETAIL structure.
 *
 * PUBLIC: int __txn_begin __P((DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t));
 */
int
__txn_begin(dbenv, parent, txnpp, flags)
	DB_ENV *dbenv;
	DB_TXN *parent, **txnpp;
	u_int32_t flags;
{
	DB_LOCKREGION *region;
	DB_TXN *txn;
	int ret;

	*txnpp = NULL;
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXN), &txn)) != 0)
		return (ret);

	txn->mgrp = dbenv->tx_handle;
	txn->parent = parent;
	TAILQ_INIT(&txn->kids);
	TAILQ_INIT(&txn->events);
	STAILQ_INIT(&txn->logs);
	txn->flags = TXN_MALLOC;
	if (LF_ISSET(DB_DEGREE_2))
		F_SET(txn, TXN_DEGREE_2);
	if (LF_ISSET(DB_DIRTY_READ))
		F_SET(txn, TXN_DIRTY_READ);
	if (LF_ISSET(DB_TXN_NOSYNC))
		F_SET(txn, TXN_NOSYNC);
	if (LF_ISSET(DB_TXN_SYNC))
		F_SET(txn, TXN_SYNC);
	if (LF_ISSET(DB_TXN_NOWAIT))
		F_SET(txn, TXN_NOWAIT);

	if ((ret = __txn_begin_int(txn, 0)) != 0)
		goto err;

	if (parent != NULL)
		TAILQ_INSERT_HEAD(&parent->kids, txn, klinks);

	if (LOCKING_ON(dbenv)) {
		region = ((DB_LOCKTAB *)dbenv->lk_handle)->reginfo.primary;
		if (parent != NULL) {
			ret = __lock_inherit_timeout(dbenv,
			    parent->txnid, txn->txnid);
			/* No parent locker set yet. */
			if (ret == EINVAL) {
				parent = NULL;
				ret = 0;
			}
			if (ret != 0)
				goto err;
		}

		/*
		 * Parent is NULL if we have no parent
		 * or it has no timeouts set.
		 */
		if (parent == NULL && region->tx_timeout != 0)
			if ((ret = __lock_set_timeout(dbenv, txn->txnid,
			    region->tx_timeout, DB_SET_TXN_TIMEOUT)) != 0)
				goto err;
	}

	*txnpp = txn;
	return (0);

err:
	__os_free(dbenv, txn);
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

	/*
	 * We need to initialize the transaction structure, but must be careful
	 * not to smash the links.  We manually initialize the structure.
	 */
	txn->mgrp = dbenv->tx_handle;
	TAILQ_INIT(&txn->kids);
	TAILQ_INIT(&txn->events);
	STAILQ_INIT(&txn->logs);
	txn->parent = NULL;
	ZERO_LSN(txn->last_lsn);
	txn->txnid = TXN_INVALID;
	txn->tid = 0;
	txn->cursors = 0;
	memset(&txn->lock_timeout, 0, sizeof(db_timeout_t));
	memset(&txn->expire, 0, sizeof(db_timeout_t));

	return (__txn_begin_int(txn, 0));
}

/*
 * __txn_compensate_begin
 *	Begin an compensation transaction.  This is a special interface
 * that is used only for transactions that must be started to compensate
 * for actions during an abort.  Currently only used for allocations.
 *
 * PUBLIC: int __txn_compensate_begin __P((DB_ENV *, DB_TXN **txnp));
 */
int
__txn_compensate_begin(dbenv, txnpp)
	DB_ENV *dbenv;
	DB_TXN **txnpp;
{
	DB_TXN *txn;
	int ret;

	PANIC_CHECK(dbenv);

	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXN), &txn)) != 0)
		return (ret);

	txn->mgrp = dbenv->tx_handle;
	TAILQ_INIT(&txn->kids);
	TAILQ_INIT(&txn->events);
	STAILQ_INIT(&txn->logs);
	txn->flags = TXN_COMPENSATE | TXN_MALLOC;

	*txnpp = txn;
	return (__txn_begin_int(txn, 1));
}

/*
 * __txn_begin_int --
 *	Normal DB version of txn_begin.
 */
static int
__txn_begin_int(txn, internal)
	DB_TXN *txn;
	int internal;
{
	DB_ENV *dbenv;
	DB_LSN null_lsn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *td;
	size_t off;
	u_int32_t id, *ids;
	int nids, ret;

	mgr = txn->mgrp;
	dbenv = mgr->dbenv;
	region = mgr->reginfo.primary;

	R_LOCK(dbenv, &mgr->reginfo);
	if (!F_ISSET(txn, TXN_COMPENSATE) && F_ISSET(region, TXN_IN_RECOVERY)) {
		__db_err(dbenv, "operation not permitted during recovery");
		ret = EINVAL;
		goto err;
	}

	/* Make sure that we aren't still recovering prepared transactions. */
	if (!internal && region->stat.st_nrestores != 0) {
		__db_err(dbenv,
    "recovery of prepared but not yet committed transactions is incomplete");
		ret = EINVAL;
		goto err;
	}

	/*
	 * Allocate a new transaction id. Our current valid range can span
	 * the maximum valid value, so check for it and wrap manually.
	 */
	if (region->last_txnid == TXN_MAXIMUM &&
	    region->cur_maxid != TXN_MAXIMUM)
		region->last_txnid = TXN_MINIMUM - 1;

	if (region->last_txnid == region->cur_maxid) {
		if ((ret = __os_malloc(dbenv,
		    sizeof(u_int32_t) * region->maxtxns, &ids)) != 0)
			goto err;
		nids = 0;
		for (td = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
		    td != NULL;
		    td = SH_TAILQ_NEXT(td, links, __txn_detail))
			ids[nids++] = td->txnid;
		region->last_txnid = TXN_MINIMUM - 1;
		region->cur_maxid = TXN_MAXIMUM;
		if (nids != 0)
			__db_idspace(ids, nids,
			    &region->last_txnid, &region->cur_maxid);
		__os_free(dbenv, ids);
		if (DBENV_LOGGING(dbenv) &&
		    (ret = __txn_recycle_log(dbenv, NULL, &null_lsn,
			0, region->last_txnid + 1, region->cur_maxid)) != 0)
			goto err;
	}

	/* Allocate a new transaction detail structure. */
	if ((ret =
	    __db_shalloc(&mgr->reginfo, sizeof(TXN_DETAIL), 0, &td)) != 0) {
		__db_err(dbenv,
		    "Unable to allocate memory for transaction detail");
		goto err;
	}

	/* Place transaction on active transaction list. */
	SH_TAILQ_INSERT_HEAD(&region->active_txn, td, links, __txn_detail);

	id = ++region->last_txnid;
	++region->stat.st_nbegins;
	if (++region->stat.st_nactive > region->stat.st_maxnactive)
		region->stat.st_maxnactive = region->stat.st_nactive;

	td->txnid = id;
	ZERO_LSN(td->last_lsn);
	ZERO_LSN(td->begin_lsn);
	if (txn->parent != NULL)
		td->parent = txn->parent->off;
	else
		td->parent = INVALID_ROFF;
	td->status = TXN_RUNNING;
	td->flags = 0;
	td->xa_status = 0;

	off = R_OFFSET(&mgr->reginfo, td);
	R_UNLOCK(dbenv, &mgr->reginfo);

	ZERO_LSN(txn->last_lsn);
	txn->txnid = id;
	txn->off = (u_int32_t)off;

	txn->abort = __txn_abort_pp;
	txn->commit = __txn_commit_pp;
	txn->discard = __txn_discard_pp;
	txn->id = __txn_id;
	txn->prepare = __txn_prepare;
	txn->set_timeout = __txn_set_timeout;
	txn->set_begin_lsnp = __txn_set_begin_lsnp;

	/*
	 * If this is a transaction family, we must link the child to the
	 * maximal grandparent in the lock table for deadlock detection.
	 */
	if (txn->parent != NULL && LOCKING_ON(dbenv))
		if ((ret = __lock_addfamilylocker(dbenv,
		    txn->parent->txnid, txn->txnid)) != 0)
			return (ret);

	if (F_ISSET(txn, TXN_MALLOC)) {
		MUTEX_THREAD_LOCK(dbenv, mgr->mutexp);
		TAILQ_INSERT_TAIL(&mgr->txn_chain, txn, links);
		MUTEX_THREAD_UNLOCK(dbenv, mgr->mutexp);
	}

	return (0);

err:	R_UNLOCK(dbenv, &mgr->reginfo);
	return (ret);
}

/*
 * __txn_commit_pp --
 *	Interface routine to TXN->commit.
 */
static int
__txn_commit_pp(txnp, flags)
	DB_TXN *txnp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int not_child, ret;

	dbenv = txnp->mgrp->dbenv;
	not_child = txnp->parent == NULL;
	ret = __txn_commit(txnp, flags);
	if (not_child && IS_ENV_REPLICATED(dbenv))
		__op_rep_exit(dbenv);
	return (ret);
}

/*
 * __txn_commit --
 *	Commit a transaction.
 *
 * PUBLIC: int __txn_commit __P((DB_TXN *, u_int32_t));
 */
int
__txn_commit(txnp, flags)
	DB_TXN *txnp;
	u_int32_t flags;
{
	DBT list_dbt;
	DB_ENV *dbenv;
	DB_LOCKREQ request;
	DB_TXN *kid;
	TXN_DETAIL *td;
	u_int32_t lflags;
	int ret, t_ret;

	dbenv = txnp->mgrp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __txn_isvalid(txnp, &td, TXN_OP_COMMIT)) != 0)
		return (ret);

	/*
	 * We clear flags that are incorrect, ignoring any flag errors, and
	 * default to synchronous operations.  By definition, transaction
	 * handles are dead when we return, and this error should never
	 * happen, but we don't want to fail in the field 'cause the app is
	 * specifying the wrong flag for some reason.
	 */
	if (__db_fchk(dbenv,
	    "DB_TXN->commit", flags, DB_TXN_NOSYNC | DB_TXN_SYNC) != 0)
		flags = DB_TXN_SYNC;
	if (__db_fcchk(dbenv,
	    "DB_TXN->commit", flags, DB_TXN_NOSYNC, DB_TXN_SYNC) != 0)
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
	 * Commit any unresolved children.  If anyone fails to commit,
	 * then try to abort the rest of the kids and then abort the parent.
	 * Abort should never fail; if it does, we bail out immediately.
	 */
	while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
		if ((ret = __txn_commit(kid, flags)) != 0)
			while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
				if ((t_ret = __txn_abort(kid)) != 0)
					return (__db_panic(dbenv, t_ret));

	/*
	 * If there are any log records, write a log record and sync the log,
	 * else do no log writes.  If the commit is for a child transaction,
	 * we do not need to commit the child synchronously since it may still
	 * abort (if its parent aborts), and otherwise its parent or ultimate
	 * ancestor will write synchronously.
	 */
	if (DBENV_LOGGING(dbenv) && (!IS_ZERO_LSN(txnp->last_lsn) ||
	    STAILQ_FIRST(&txnp->logs) != NULL)) {
		if (txnp->parent == NULL) {
			/*
			 * We are about to free all the read locks for this
			 * transaction below.  Some of those locks might be
			 * handle locks which should not be freed, because
			 * they will be freed when the handle is closed. Check
			 * the events and preprocess any trades now so we don't
			 * release the locks below.
			 */
			if ((ret =
			    __txn_doevents(dbenv, txnp, TXN_PREPARE, 1)) != 0)
				goto err;

			memset(&request, 0, sizeof(request));
			if (LOCKING_ON(dbenv)) {
				request.op = DB_LOCK_PUT_READ;
				if (IS_REP_MASTER(dbenv) &&
				    !IS_ZERO_LSN(txnp->last_lsn)) {
					memset(&list_dbt, 0, sizeof(list_dbt));
					request.obj = &list_dbt;
				}
				ret = __lock_vec(dbenv,
				    txnp->txnid, 0, &request, 1, NULL);
			}

			if (ret == 0 && !IS_ZERO_LSN(txnp->last_lsn)) {
				SET_LOG_FLAGS(dbenv, txnp, lflags);
				ret = __txn_regop_log(dbenv, txnp,
				    &txnp->last_lsn, lflags, TXN_COMMIT,
				    (int32_t)time(NULL), request.obj);
			}

			if (request.obj != NULL && request.obj->data != NULL)
				__os_free(dbenv, request.obj->data);
			if (ret != 0)
				goto err;
		} else {
			/* Log the commit in the parent! */
			if (!IS_ZERO_LSN(txnp->last_lsn) &&
			    (ret = __txn_child_log(dbenv,
			    txnp->parent, &txnp->parent->last_lsn,
			    0, txnp->txnid, &txnp->last_lsn)) != 0) {
				goto err;
			}
			if (STAILQ_FIRST(&txnp->logs) != NULL) {
				/*
				 * Put the child first so we back it out first.
				 * All records are undone in reverse order.
				 */
				STAILQ_CONCAT(&txnp->logs, &txnp->parent->logs);
				txnp->parent->logs = txnp->logs;
				STAILQ_INIT(&txnp->logs);
			}

			F_SET(txnp->parent, TXN_CHILDCOMMIT);
		}
	}

	/*
	 * Process any aborted pages from our children.  We delay putting pages
	 * on the free list that are newly allocated and then aborted so we can
	 * undo other allocations, if necessary, without worrying about these
	 * pages which were not on the free list before.
	 */
	if (txnp->txn_list != NULL) {
#ifndef HAVE_FTRUNCATE
		t_ret = __db_do_the_limbo(dbenv,
		      NULL, txnp, txnp->txn_list, LIMBO_NORMAL);
		if (t_ret != 0 && ret == 0)
			ret = t_ret;
#endif
		__db_txnlist_end(dbenv, txnp->txn_list);
		txnp->txn_list = NULL;
	}

	if (ret != 0)
		goto err;

	/* This is OK because __txn_end can only fail with a panic. */
	return (__txn_end(txnp, 1));

err:	/*
	 * If we are prepared, then we "must" be able to commit.  We panic here
	 * because even though the coordinator might be able to retry it is not
	 * clear it would know to do that.  Otherwise  we'll try to abort.  If
	 * that is successful, then we return whatever was in ret (that is, the
	 * reason we failed).  If the abort was unsuccessful, abort probably
	 * returned DB_RUNRECOVERY and we need to propagate that up.
	 */
	if (td->status == TXN_PREPARED)
		return (__db_panic(dbenv, ret));

	if ((t_ret = __txn_abort(txnp)) != 0)
		ret = t_ret;
	return (ret);
}

/*
 * __txn_abort_pp --
 *	Interface routine to TXN->abort.
 */
static int
__txn_abort_pp(txnp)
	DB_TXN *txnp;
{
	DB_ENV *dbenv;
	int not_child, ret;

	dbenv = txnp->mgrp->dbenv;
	not_child = txnp->parent == NULL;
	ret = __txn_abort(txnp);
	if (not_child && IS_ENV_REPLICATED(dbenv))
		__op_rep_exit(dbenv);
	return (ret);
}

/*
 * __txn_abort --
 *	Abort a transaction.
 *
 * PUBLIC: int __txn_abort __P((DB_TXN *));
 */
int
__txn_abort(txnp)
	DB_TXN *txnp;
{
	DB_ENV *dbenv;
	DB_LOCKREQ request;
	DB_TXN *kid;
	TXN_DETAIL *td;
	u_int32_t lflags;
	int ret;

	dbenv = txnp->mgrp->dbenv;

	PANIC_CHECK(dbenv);

	/* Ensure that abort always fails fatally. */
	if ((ret = __txn_isvalid(txnp, &td, TXN_OP_ABORT)) != 0)
		return (__db_panic(dbenv, ret));

	/*
	 * Try to abort any unresolved children.
	 *
	 * Abort either succeeds or panics the region.  As soon as we
	 * see any failure, we just get out of here and return the panic
	 * up.
	 */
	while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
		if ((ret = __txn_abort(kid)) != 0)
			return (ret);

	if (LOCKING_ON(dbenv)) {
		/*
		 * We are about to free all the read locks for this transaction
		 * below.  Some of those locks might be handle locks which
		 * should not be freed, because they will be freed when the
		 * handle is closed.  Check the events and preprocess any
		 * trades now so that we don't release the locks below.
		 */
		if ((ret = __txn_doevents(dbenv, txnp, TXN_ABORT, 1)) != 0)
			return (__db_panic(dbenv, ret));

		/* Turn off timeouts. */
		if ((ret = __lock_set_timeout(dbenv,
		    txnp->txnid, 0, DB_SET_TXN_TIMEOUT)) != 0)
			return (__db_panic(dbenv, ret));

		if ((ret = __lock_set_timeout(dbenv,
		    txnp->txnid, 0, DB_SET_LOCK_TIMEOUT)) != 0)
			return (__db_panic(dbenv, ret));

		request.op = DB_LOCK_UPGRADE_WRITE;
		request.obj = NULL;
		if ((ret = __lock_vec(
		    dbenv, txnp->txnid, DB_LOCK_ABORT, &request, 1, NULL)) != 0)
			return (__db_panic(dbenv, ret));
	}
	if ((ret = __txn_undo(txnp)) != 0)
		return (__db_panic(dbenv, ret));

	/*
	 * Normally, we do not need to log aborts.  However, if we
	 * are a distributed transaction (i.e., we have a prepare),
	 * then we log the abort so we know that this transaction
	 * was actually completed.
	 */
	SET_LOG_FLAGS(dbenv, txnp, lflags);
	if (DBENV_LOGGING(dbenv) && td->status == TXN_PREPARED &&
	    (ret = __txn_regop_log(dbenv, txnp, &txnp->last_lsn,
	    lflags, TXN_ABORT, (int32_t)time(NULL), NULL)) != 0)
		return (__db_panic(dbenv, ret));

	/* __txn_end always panics if it errors, so pass the return along. */
	return (__txn_end(txnp, 0));
}

/*
 * __txn_discard_pp --
 *	Interface routine to TXN->discard.
 */
static int
__txn_discard_pp(txnp, flags)
	DB_TXN *txnp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int not_child, ret;

	dbenv = txnp->mgrp->dbenv;
	not_child = txnp->parent == NULL;
	ret = __txn_discard(txnp, flags);
	if (not_child && IS_ENV_REPLICATED(dbenv))
		__op_rep_exit(dbenv);
	return (ret);
}

/*
 * __txn_discard --
 *	Free the per-process resources associated with this txn handle.
 *
 * PUBLIC: int __txn_discard __P((DB_TXN *, u_int32_t flags));
 */
int
__txn_discard(txnp, flags)
	DB_TXN *txnp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_TXN *freep;
	TXN_DETAIL *td;
	int ret;

	COMPQUIET(flags, 0);

	dbenv = txnp->mgrp->dbenv;
	freep = NULL;

	PANIC_CHECK(dbenv);

	if ((ret = __txn_isvalid(txnp, &td, TXN_OP_DISCARD)) != 0)
		return (ret);

	/* Should be no children. */
	DB_ASSERT(TAILQ_FIRST(&txnp->kids) == NULL);

	/* Free the space. */
	MUTEX_THREAD_LOCK(dbenv, txnp->mgrp->mutexp);
	txnp->mgrp->n_discards++;
	if (F_ISSET(txnp, TXN_MALLOC)) {
		TAILQ_REMOVE(&txnp->mgrp->txn_chain, txnp, links);
		freep = txnp;
	}
	MUTEX_THREAD_UNLOCK(dbenv, txnp->mgrp->mutexp);
	if (freep != NULL)
		__os_free(dbenv, freep);

	return (0);
}

/*
 * __txn_prepare --
 *	Flush the log so a future commit is guaranteed to succeed.
 *
 * PUBLIC: int __txn_prepare __P((DB_TXN *, u_int8_t *));
 */
int
__txn_prepare(txnp, gid)
	DB_TXN *txnp;
	u_int8_t *gid;
{
	DBT list_dbt, xid;
	DB_ENV *dbenv;
	DB_LOCKREQ request;
	DB_TXN *kid;
	TXN_DETAIL *td;
	u_int32_t lflags;
	int ret;

	dbenv = txnp->mgrp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __txn_isvalid(txnp, &td, TXN_OP_PREPARE)) != 0)
		return (ret);

	/* Commit any unresolved children. */
	while ((kid = TAILQ_FIRST(&txnp->kids)) != NULL)
		if ((ret = __txn_commit(kid, DB_TXN_NOSYNC)) != 0)
			return (ret);

#ifndef HAVE_FTRUNCATE
	if (txnp->txn_list != NULL  &&
	    (ret = __db_do_the_limbo(dbenv,
	    NULL, txnp, txnp->txn_list, LIMBO_PREPARE)) != 0)
		return (ret);
#endif
	/*
	 * In XA, the global transaction ID in the txn_detail structure is
	 * already set; in a non-XA environment, we must set it here.  XA
	 * requires that the transaction be either ENDED or SUSPENDED when
	 * prepare is called, so we know that if the xa_status isn't in one
	 * of those states, then we are calling prepare directly and we need
	 * to fill in the td->xid.
	 */
	if ((ret = __txn_doevents(dbenv, txnp, TXN_PREPARE, 1)) != 0)
		return (ret);
	memset(&request, 0, sizeof(request));
	if (LOCKING_ON(dbenv)) {
		request.op = DB_LOCK_PUT_READ;
		if (IS_REP_MASTER(dbenv) &&
		    IS_ZERO_LSN(txnp->last_lsn)) {
			memset(&list_dbt, 0, sizeof(list_dbt));
			request.obj = &list_dbt;
		}
		if ((ret = __lock_vec(dbenv,
		    txnp->txnid, 0, &request, 1, NULL)) != 0)
			return (ret);

	}
	if (DBENV_LOGGING(dbenv)) {
		memset(&xid, 0, sizeof(xid));
		if (td->xa_status != TXN_XA_ENDED &&
		    td->xa_status != TXN_XA_SUSPENDED)
			/* Regular prepare; fill in the gid. */
			memcpy(td->xid, gid, sizeof(td->xid));

		xid.size = sizeof(td->xid);
		xid.data = td->xid;

		lflags = DB_LOG_COMMIT | DB_LOG_PERM | DB_FLUSH;
		if ((ret = __txn_xa_regop_log(dbenv, txnp, &txnp->last_lsn,
		    lflags, TXN_PREPARE, &xid, td->format, td->gtrid, td->bqual,
		    &td->begin_lsn, request.obj)) != 0) {
			__db_err(dbenv, "DB_TXN->prepare: log_write failed %s",
			    db_strerror(ret));
		}
		if (request.obj != NULL && request.obj->data != NULL)
			__os_free(dbenv, request.obj->data);
		if (ret != 0)
			return (ret);

	}

	MUTEX_THREAD_LOCK(dbenv, txnp->mgrp->mutexp);
	td->status = TXN_PREPARED;
	MUTEX_THREAD_UNLOCK(dbenv, txnp->mgrp->mutexp);
	return (0);
}

/*
 * __txn_id --
 *	Return the transaction ID.
 *
 * PUBLIC: u_int32_t __txn_id __P((DB_TXN *));
 */
u_int32_t
__txn_id(txnp)
	DB_TXN *txnp;
{
	return (txnp->txnid);
}

/*
 * __txn_set_timeout --
 *	DB_ENV->set_txn_timeout.
 *
 * PUBLIC: int  __txn_set_timeout __P((DB_TXN *, db_timeout_t, u_int32_t));
 */
int
__txn_set_timeout(txnp, timeout, op)
	DB_TXN *txnp;
	db_timeout_t timeout;
	u_int32_t op;
{
	if (op != DB_SET_TXN_TIMEOUT &&  op != DB_SET_LOCK_TIMEOUT)
		return (__db_ferr(txnp->mgrp->dbenv, "DB_TXN->set_timeout", 0));

	return (__lock_set_timeout(
	    txnp->mgrp->dbenv, txnp->txnid, timeout, op));
}

/*
 * __txn_isvalid --
 *	Return 0 if the txnp is reasonable, otherwise panic.
 */
static int
__txn_isvalid(txnp, tdp, op)
	const DB_TXN *txnp;
	TXN_DETAIL **tdp;
	txnop_t op;
{
	DB_ENV *dbenv;
	DB_TXNMGR *mgrp;
	DB_TXNREGION *region;
	TXN_DETAIL *tp;

	mgrp = txnp->mgrp;
	dbenv = mgrp->dbenv;
	region = mgrp->reginfo.primary;

	/* Check for recovery. */
	if (!F_ISSET(txnp, TXN_COMPENSATE) &&
	    F_ISSET(region, TXN_IN_RECOVERY)) {
		__db_err(dbenv, "operation not permitted during recovery");
		goto err;
	}

	/* Check for live cursors. */
	if (txnp->cursors != 0) {
		__db_err(dbenv, "transaction has active cursors");
		goto err;
	}

	/* Check transaction's state. */
	tp = R_ADDR(&mgrp->reginfo, txnp->off);
	if (tdp != NULL)
		*tdp = tp;

	/* Handle any operation specific checks. */
	switch (op) {
	case TXN_OP_DISCARD:
		/*
		 * Since we're just tossing the per-process space; there are
		 * a lot of problems with the transaction that we can tolerate.
		 */

		/* Transaction is already been reused. */
		if (txnp->txnid != tp->txnid)
			return (0);

		/*
		 * What we've got had better be either a prepared or
		 * restored transaction.
		 */
		if (tp->status != TXN_PREPARED &&
		    !F_ISSET(tp, TXN_DTL_RESTORED)) {
			__db_err(dbenv, "not a restored transaction");
			return (__db_panic(dbenv, EINVAL));
		}

		return (0);
	case TXN_OP_PREPARE:
		if (txnp->parent != NULL) {
			/*
			 * This is not fatal, because you could imagine an
			 * application that simply prepares everybody because
			 * it doesn't distinguish between children and parents.
			 * I'm not arguing this is good, but I could imagine
			 * someone doing it.
			 */
			__db_err(dbenv,
			    "Prepare disallowed on child transactions");
			return (EINVAL);
		}
		break;
	case TXN_OP_ABORT:
	case TXN_OP_COMMIT:
	default:
		break;
	}

	switch (tp->status) {
	case TXN_PREPARED:
		if (op == TXN_OP_PREPARE) {
			__db_err(dbenv, "transaction already prepared");
			/*
			 * Txn_prepare doesn't blow away the user handle, so
			 * in this case, give the user the opportunity to
			 * abort or commit.
			 */
			return (EINVAL);
		}
		break;
	case TXN_RUNNING:
		break;
	case TXN_ABORTED:
	case TXN_COMMITTED:
	default:
		__db_err(dbenv, "transaction already %s",
		    tp->status == TXN_COMMITTED ? "committed" : "aborted");
		goto err;
	}

	return (0);

err:	/*
	 * If there's a serious problem with the transaction, panic.  TXN
	 * handles are dead by definition when we return, and if you use
	 * a cursor you forgot to close, we have no idea what will happen.
	 */
	return (__db_panic(dbenv, EINVAL));
}

/*
 * __txn_end --
 *	Internal transaction end routine.
 */
static int
__txn_end(txnp, is_commit)
	DB_TXN *txnp;
	int is_commit;
{
	DB_ENV *dbenv;
	DB_LOCKREQ request;
	DB_TXNLOGREC *lr;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *tp;
	int do_closefiles, ret;

	mgr = txnp->mgrp;
	dbenv = mgr->dbenv;
	region = mgr->reginfo.primary;
	do_closefiles = 0;

	/* Process commit events. */
	if ((ret = __txn_doevents(dbenv,
	    txnp, is_commit ? TXN_COMMIT : TXN_ABORT, 0)) != 0)
		return (__db_panic(dbenv, ret));

	/*
	 * Release the locks.
	 *
	 * __txn_end cannot return an simple error, we MUST return
	 * success/failure from commit or abort, ignoring any internal
	 * errors.  So, we panic if something goes wrong.  We can't
	 * deadlock here because we're not acquiring any new locks,
	 * so DB_LOCK_DEADLOCK is just as fatal as any other error.
	 */
	if (LOCKING_ON(dbenv)) {
		request.op = txnp->parent == NULL ||
		    is_commit == 0 ? DB_LOCK_PUT_ALL : DB_LOCK_INHERIT;
		request.obj = NULL;
		if ((ret = __lock_vec(dbenv,
		    txnp->txnid, 0, &request, 1, NULL)) != 0)
			return (__db_panic(dbenv, ret));
	}

	/* End the transaction. */
	R_LOCK(dbenv, &mgr->reginfo);

	tp = R_ADDR(&mgr->reginfo, txnp->off);
	SH_TAILQ_REMOVE(&region->active_txn, tp, links, __txn_detail);
	if (F_ISSET(tp, TXN_DTL_RESTORED)) {
		region->stat.st_nrestores--;
		do_closefiles = region->stat.st_nrestores == 0;
	}

	__db_shalloc_free(&mgr->reginfo, tp);

	if (is_commit)
		region->stat.st_ncommits++;
	else
		region->stat.st_naborts++;
	--region->stat.st_nactive;

	R_UNLOCK(dbenv, &mgr->reginfo);

	/*
	 * The transaction cannot get more locks, remove its locker info,
	 * if any.
	 */
	if (LOCKING_ON(dbenv) && (ret =
	    __lock_freefamilylocker(dbenv->lk_handle, txnp->txnid)) != 0)
		return (__db_panic(dbenv, ret));
	if (txnp->parent != NULL)
		TAILQ_REMOVE(&txnp->parent->kids, txnp, klinks);

	/* Free the space. */
	while ((lr = STAILQ_FIRST(&txnp->logs)) != NULL) {
		STAILQ_REMOVE(&txnp->logs, lr, __txn_logrec, links);
		__os_free(dbenv, lr);
	}
	if (F_ISSET(txnp, TXN_MALLOC)) {
		MUTEX_THREAD_LOCK(dbenv, mgr->mutexp);
		TAILQ_REMOVE(&mgr->txn_chain, txnp, links);
		MUTEX_THREAD_UNLOCK(dbenv, mgr->mutexp);

		__os_free(dbenv, txnp);
	}

	if (do_closefiles) {
		F_SET((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
		(void)__dbreg_close_files(dbenv);
		F_CLR((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
		mgr->n_discards = 0;
		(void)__txn_checkpoint(dbenv, 0, 0, DB_FORCE);
	}

	return (0);
}

static int
__txn_dispatch_undo(dbenv, txnp, rdbt, key_lsn, txnlist)
	DB_ENV *dbenv;
	DB_TXN *txnp;
	DBT *rdbt;
	DB_LSN *key_lsn;
	void *txnlist;
{
	int ret;

	ret = __db_dispatch(dbenv, dbenv->recover_dtab,
	    dbenv->recover_dtab_size, rdbt, key_lsn, DB_TXN_ABORT, txnlist);
	if (F_ISSET(txnp, TXN_CHILDCOMMIT))
		(void)__db_txnlist_lsnadd(dbenv,
		    txnlist, key_lsn, 0);
	if (ret == DB_SURPRISE_KID) {
		if ((ret = __db_txnlist_lsninit(
		    dbenv, txnlist, key_lsn)) == 0)
			F_SET(txnp, TXN_CHILDCOMMIT);
	}

	return (ret);
}

/*
 * __txn_undo --
 *	Undo the transaction with id txnid.
 */
static int
__txn_undo(txnp)
	DB_TXN *txnp;
{
	DBT rdbt;
	DB_ENV *dbenv;
	DB_LOGC *logc;
	DB_LSN key_lsn;
	DB_TXN *ptxn;
	DB_TXNLOGREC *lr;
	DB_TXNMGR *mgr;
	int ret, t_ret;
	void *txnlist;

	mgr = txnp->mgrp;
	dbenv = mgr->dbenv;
	logc = NULL;
	txnlist = NULL;
	ret = 0;

	if (!DBENV_LOGGING(dbenv))
		return (0);

	/*
	 * This is the simplest way to code this, but if the mallocs during
	 * recovery turn out to be a performance issue, we can do the
	 * allocation here and use DB_DBT_USERMEM.
	 */
	memset(&rdbt, 0, sizeof(rdbt));

	/*
	 * Allocate a txnlist for children and aborted page allocs.
	 * We need to associate the list with the maximal parent
	 * so that aborted pages are recovered when that transaction
	 * is committed or aborted.
	 */
	for (ptxn = txnp->parent; ptxn != NULL && ptxn->parent != NULL;)
		ptxn = ptxn->parent;

	if (ptxn != NULL && ptxn->txn_list != NULL)
		txnlist = ptxn->txn_list;
	else if (txnp->txn_list != NULL)
		txnlist = txnp->txn_list;
	else if ((ret = __db_txnlist_init(dbenv, 0, 0, NULL, &txnlist)) != 0)
		return (ret);
	else if (ptxn != NULL)
		ptxn->txn_list = txnlist;

	if (F_ISSET(txnp, TXN_CHILDCOMMIT) &&
	    (ret = __db_txnlist_lsninit(dbenv, txnlist, &txnp->last_lsn)) != 0)
		return (ret);

	/*
	 * Take log records from the linked list stored in the transaction,
	 * then from the log.
	 */
	for (lr = STAILQ_FIRST(&txnp->logs);
	    lr != NULL; lr = STAILQ_NEXT(lr, links)) {
		rdbt.data = lr->data;
		rdbt.size = 0;
		LSN_NOT_LOGGED(key_lsn);
		ret =
		    __txn_dispatch_undo(dbenv, txnp, &rdbt, &key_lsn, txnlist);
		if (ret != 0) {
			__db_err(dbenv,
			    "DB_TXN->abort: In-memory log undo failed: %s",
			    db_strerror(ret));
			goto err;
		}
	}

	key_lsn = txnp->last_lsn;

	if (!IS_ZERO_LSN(key_lsn) &&
	     (ret = __log_cursor(dbenv, &logc)) != 0)
		goto err;

	while (!IS_ZERO_LSN(key_lsn)) {
		/*
		 * The dispatch routine returns the lsn of the record
		 * before the current one in the key_lsn argument.
		 */
		if ((ret = __log_c_get(logc, &key_lsn, &rdbt, DB_SET)) == 0) {
			ret = __txn_dispatch_undo(dbenv,
			    txnp, &rdbt, &key_lsn, txnlist);
		}

		if (ret != 0) {
			__db_err(dbenv,
		    "DB_TXN->abort: Log undo failed for LSN: %lu %lu: %s",
			    (u_long)key_lsn.file, (u_long)key_lsn.offset,
			    db_strerror(ret));
			goto err;
		}
	}

#ifndef HAVE_FTRUNCATE
	ret = __db_do_the_limbo(dbenv, ptxn, txnp, txnlist, LIMBO_NORMAL);
#endif

err:	if (logc != NULL && (t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	if (ptxn == NULL && txnlist != NULL)
		__db_txnlist_end(dbenv, txnlist);
	return (ret);
}

/*
 * __txn_checkpoint_pp --
 *	DB_ENV->txn_checkpoint pre/post processing.
 *
 * PUBLIC: int __txn_checkpoint_pp
 * PUBLIC:     __P((DB_ENV *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__txn_checkpoint_pp(dbenv, kbytes, minutes, flags)
	DB_ENV *dbenv;
	u_int32_t kbytes, minutes, flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->tx_handle, "txn_checkpoint", DB_INIT_TXN);

	/*
	 * On a replication client, all transactions are read-only; therefore,
	 * a checkpoint is a null-op.
	 *
	 * We permit txn_checkpoint, instead of just rendering it illegal,
	 * so that an application can just let a checkpoint thread continue
	 * to operate as it gets promoted or demoted between being a
	 * master and a client.
	 */
	if (IS_REP_CLIENT(dbenv))
		return (0);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __txn_checkpoint(dbenv, kbytes, minutes, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __txn_checkpoint --
 *	DB_ENV->txn_checkpoint.
 *
 * PUBLIC: int __txn_checkpoint
 * PUBLIC:	__P((DB_ENV *, u_int32_t, u_int32_t, u_int32_t));
 */
int
__txn_checkpoint(dbenv, kbytes, minutes, flags)
	DB_ENV *dbenv;
	u_int32_t kbytes, minutes, flags;
{
	DB_LSN ckp_lsn, last_ckp;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	REGENV *renv;
	REGINFO *infop;
	time_t last_ckp_time, now;
	u_int32_t bytes, gen, id, logflags, mbytes;
	int ret;

	ret = gen = 0;
	/*
	 * A client will only call through here during recovery,
	 * so just sync the Mpool and go home.
	 */
	if (IS_REP_CLIENT(dbenv)) {
		if (MPOOL_ON(dbenv) && (ret = __memp_sync(dbenv, NULL)) != 0) {
			__db_err(dbenv,
		    "txn_checkpoint: failed to flush the buffer cache %s",
			    db_strerror(ret));
			return (ret);
		} else
			return (0);
	}

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;
	infop = dbenv->reginfo;
	renv = infop->primary;
	/*
	 * No mutex is needed as envid is read-only once it is set.
	 */
	id = renv->envid;

	/*
	 * The checkpoint LSN is an LSN such that all transactions begun before
	 * it are complete.  Our first guess (corrected below based on the list
	 * of active transactions) is the last-written LSN.
	 */
	__log_txn_lsn(dbenv, &ckp_lsn, &mbytes, &bytes);

	if (!LF_ISSET(DB_FORCE)) {
		/* Don't checkpoint a quiescent database. */
		if (bytes == 0 && mbytes == 0)
			return (0);

		/*
		 * If either kbytes or minutes is non-zero, then only take the
		 * checkpoint if more than "minutes" minutes have passed or if
		 * more than "kbytes" of log data have been written since the
		 * last checkpoint.
		 */
		if (kbytes != 0 &&
		    mbytes * 1024 + bytes / 1024 >= (u_int32_t)kbytes)
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
	}

do_ckp:
	__txn_getactive(dbenv, &ckp_lsn);

	if (MPOOL_ON(dbenv) && (ret = __memp_sync(dbenv, NULL)) != 0) {
		__db_err(dbenv,
		    "txn_checkpoint: failed to flush the buffer cache %s",
		    db_strerror(ret));
		return (ret);
	}

	/*
	 * Because we can't be a replication client here, and because
	 * recovery (somewhat unusually) calls txn_checkpoint and expects
	 * it to write a log message, LOGGING_ON is the correct macro here.
	 */
	if (LOGGING_ON(dbenv)) {
		R_LOCK(dbenv, &mgr->reginfo);
		last_ckp = region->last_ckp;
		R_UNLOCK(dbenv, &mgr->reginfo);
		if (REP_ON(dbenv))
			__rep_get_gen(dbenv, &gen);

		/*
		 * Put out records for the open files before we log
		 * the checkpoint.  The records are certain to be at
		 * or after ckp_lsn, but before the checkpoint record
		 * itself, so they're sure to be included if we start
		 * recovery from the ckp_lsn contained in this
		 * checkpoint.
		 */
		logflags = DB_LOG_PERM | DB_LOG_CHKPNT;
		if (!IS_RECOVERING(dbenv))
			logflags |= DB_FLUSH;
		if ((ret = __dbreg_log_files(dbenv)) != 0 ||
		    (ret = __txn_ckp_log(dbenv, NULL, &ckp_lsn, logflags,
		    &ckp_lsn, &last_ckp, (int32_t)time(NULL), id, gen)) != 0) {
			__db_err(dbenv,
			    "txn_checkpoint: log failed at LSN [%ld %ld] %s",
			    (long)ckp_lsn.file, (long)ckp_lsn.offset,
			    db_strerror(ret));
			return (ret);
		}

		__txn_updateckp(dbenv, &ckp_lsn);
	}
	return (ret);
}

/*
 * __txn_getactive --
 *	 Find the oldest active transaction and figure out its "begin" LSN.
 *	 This is the lowest LSN we can checkpoint, since any record written
 *	 after it may be involved in a transaction and may therefore need
 *	 to be undone in the case of an abort.
 *
 *	 We check both the file and offset for 0 since the lsn may be in
 *	 transition.  If it is then we don't care about this txn becuase it
 *	 must be starting after we set the initial value of lsnp in the caller.
 *	 All txns must initalize their begin_lsn before writing to the log.
 *
 * PUBLIC: void __txn_getactive __P((DB_ENV *, DB_LSN *));
 */
void
__txn_getactive(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	TXN_DETAIL *txnp;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	R_LOCK(dbenv, &mgr->reginfo);
	for (txnp = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
	    txnp != NULL;
	    txnp = SH_TAILQ_NEXT(txnp, links, __txn_detail))
		if (txnp->begin_lsn.file != 0 &&
		    txnp->begin_lsn.offset != 0 &&
		    log_compare(&txnp->begin_lsn, lsnp) < 0)
			*lsnp = txnp->begin_lsn;
	R_UNLOCK(dbenv, &mgr->reginfo);
}

/*
 * __txn_getckp --
 *	Get the LSN of the last transaction checkpoint.
 *
 * PUBLIC: int __txn_getckp __P((DB_ENV *, DB_LSN *));
 */
int
__txn_getckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_LSN lsn;
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	R_LOCK(dbenv, &mgr->reginfo);
	lsn = region->last_ckp;
	R_UNLOCK(dbenv, &mgr->reginfo);

	if (IS_ZERO_LSN(lsn))
		return (DB_NOTFOUND);

	*lsnp = lsn;
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
	 * committing child at the least.  In that case, skip this check.
	 */
	if (F_ISSET(txnp, TXN_COMPENSATE) || rectype == DB___txn_child)
		return (0);

	if (TAILQ_FIRST(&txnp->kids) != NULL) {
		__db_err(dbenv, "Child transaction is active");
		return (EPERM);
	}
	return (0);
}

/*
 * __txn_force_abort --
 *	Force an abort record into the log if the commit record
 *	failed to get to disk.
 *
 * PUBLIC: int __txn_force_abort __P((DB_ENV *, u_int8_t *));
 */
int
__txn_force_abort(dbenv, buffer)
	DB_ENV *dbenv;
	u_int8_t *buffer;
{
	DB_CIPHER *db_cipher;
	HDR *hdr;
	u_int32_t hdrlen, offset, opcode, sum_len;
	u_int8_t *bp, *key, chksum[DB_MAC_KEY];
	size_t hdrsize, rec_len;
	int ret;

	db_cipher = dbenv->crypto_handle;

	/*
	 * This routine depends on the layout of HDR and the __txn_regop
	 * __txn_xa_regop records in txn.src.  We are passed the beginning
	 * of the commit record in the log buffer and overwrite the
	 * commit with an abort and recalculate the checksum.
	 */
	hdrsize = CRYPTO_ON(dbenv) ? HDR_CRYPTO_SZ : HDR_NORMAL_SZ;

	hdr = (HDR *)buffer;
	memcpy(&hdrlen, buffer + SSZ(HDR, len), sizeof(hdr->len));
	rec_len = hdrlen - hdrsize;

	offset = sizeof(u_int32_t) + sizeof(u_int32_t) + sizeof(DB_LSN);
	if (CRYPTO_ON(dbenv)) {
		key = db_cipher->mac_key;
		sum_len = DB_MAC_KEY;
		if ((ret = db_cipher->decrypt(dbenv, db_cipher->data,
		    &hdr->iv[0], buffer + hdrsize, rec_len)) != 0)
			return (__db_panic(dbenv, ret));
	} else {
		key = NULL;
		sum_len = sizeof(u_int32_t);
	}
	bp = buffer + hdrsize + offset;
	opcode = TXN_ABORT;
	memcpy(bp, &opcode, sizeof(opcode));

	if (CRYPTO_ON(dbenv) &&
	    (ret = db_cipher->encrypt(dbenv,
	    db_cipher->data, &hdr->iv[0], buffer + hdrsize, rec_len)) != 0)
		return (__db_panic(dbenv, ret));

	__db_chksum(buffer + hdrsize, rec_len, key, chksum);
	memcpy(buffer + SSZA(HDR, chksum), chksum, sum_len);

	return (0);
}

/*
 * __txn_preclose
 *	Before we can close an environment, we need to check if we
 * were in the midst of taking care of restored transactions.  If
 * so, then we need to close the files that we opened.
 *
 * PUBLIC: int __txn_preclose __P((DB_ENV *));
 */
int
__txn_preclose(dbenv)
	DB_ENV *dbenv;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	int do_closefiles, ret;

	mgr = (DB_TXNMGR *)dbenv->tx_handle;
	region = mgr->reginfo.primary;
	do_closefiles = 0;

	R_LOCK(dbenv, &mgr->reginfo);
	if (region != NULL &&
	    region->stat.st_nrestores <= mgr->n_discards &&
	    mgr->n_discards != 0)
		do_closefiles = 1;
	R_UNLOCK(dbenv, &mgr->reginfo);

	if (do_closefiles) {
		/*
		 * Set the DBLOG_RECOVER flag while closing these
		 * files so they do not create additional log records
		 * that will confuse future recoveries.
		 */
		F_SET((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
		ret = __dbreg_close_files(dbenv);
		F_CLR((DB_LOG *)dbenv->lg_handle, DBLOG_RECOVER);
	} else
		ret = 0;

	return (ret);
}

/*
 * __txn_reset --
 *	Reset the last txnid to its minimum value, and log the reset.
 *
 * PUBLIC: int __txn_reset __P((DB_ENV *));
 */
int
__txn_reset(dbenv)
	DB_ENV *dbenv;
{
	DB_LSN scrap;
	DB_TXNREGION *region;

	region = ((DB_TXNMGR *)dbenv->tx_handle)->reginfo.primary;
	region->last_txnid = TXN_MINIMUM;

	DB_ASSERT(LOGGING_ON(dbenv));
	return (__txn_recycle_log(dbenv,
	    NULL, &scrap, 0, TXN_MINIMUM, TXN_MAXIMUM));
}

/*
 * __txn_updateckp --
 *	Update the last_ckp field in the transaction region.  This happens
 * at the end of a normal checkpoint and also when a replication client
 * receives a checkpoint record.
 *
 * PUBLIC: void __txn_updateckp __P((DB_ENV *, DB_LSN *));
 */
void
__txn_updateckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

	/*
	 * We want to make sure last_ckp only moves forward;  since we drop
	 * locks above and in log_put, it's possible for two calls to
	 * __txn_ckp_log to finish in a different order from how they were
	 * called.
	 */
	R_LOCK(dbenv, &mgr->reginfo);
	if (log_compare(&region->last_ckp, lsnp) < 0) {
		region->last_ckp = *lsnp;
		(void)time(&region->time_ckp);
	}
	R_UNLOCK(dbenv, &mgr->reginfo);
}

/*
 * txn_set_begin_lsnp --
 *	Set the pointer to the begin_lsn field if that field is zero.
 */
static void
__txn_set_begin_lsnp(txn, rlsnp)
	DB_TXN *txn;
	DB_LSN **rlsnp;
{
	DB_LSN *lsnp;
	TXN_DETAIL *td;

	td = R_ADDR(&txn->mgrp->reginfo, txn->off);
	while (td->parent != INVALID_ROFF)
		td = R_ADDR(&txn->mgrp->reginfo, td->parent);

	lsnp = &td->begin_lsn;
	if (IS_ZERO_LSN(*lsnp))
		*rlsnp = lsnp;
}
