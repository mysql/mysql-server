/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1996
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
 *
 * $Id: txn_rec.c,v 11.64 2004/09/22 17:41:10 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/txn.h"
#include "dbinc/db_am.h"

/*
 * PUBLIC: int __txn_regop_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 *
 * These records are only ever written for commits.  Normally, we redo any
 * committed transaction, however if we are doing recovery to a timestamp, then
 * we may treat transactions that committed after the timestamp as aborted.
 */
int
__txn_regop_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	DB_TXNHEAD *headp;
	__txn_regop_args *argp;
	int ret;
	u_int32_t status;

#ifdef DEBUG_RECOVER
	(void)__txn_regop_print(dbenv, dbtp, lsnp, op, info);
#endif

	if ((ret = __txn_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	headp = info;
	/*
	 * We are only ever called during FORWARD_ROLL or BACKWARD_ROLL.
	 * We check for the former explicitly and the last two clauses
	 * apply to the BACKWARD_ROLL case.
	 */

	if (op == DB_TXN_FORWARD_ROLL) {
		/*
		 * If this was a 2-phase-commit transaction, then it
		 * might already have been removed from the list, and
		 * that's OK.  Ignore the return code from remove.
		 */
		if ((ret = __db_txnlist_remove(dbenv,
		    info, argp->txnid->txnid)) != DB_NOTFOUND && ret != 0)
			goto err;
	} else if ((dbenv->tx_timestamp != 0 &&
	    argp->timestamp > (int32_t)dbenv->tx_timestamp) ||
	    (!IS_ZERO_LSN(headp->trunc_lsn) &&
	    log_compare(&headp->trunc_lsn, lsnp) < 0)) {
		/*
		 * We failed either the timestamp check or the trunc_lsn check,
		 * so we treat this as an abort even if it was a commit record.
		 */
		if ((ret = __db_txnlist_update(dbenv, info,
		    argp->txnid->txnid, TXN_ABORT, NULL, &status, 1)) != 0)
			goto err;
		else if (status != TXN_IGNORE && status != TXN_OK)
			goto err;
	} else {
		/* This is a normal commit; mark it appropriately. */
		if ((ret = __db_txnlist_update(dbenv,
		    info, argp->txnid->txnid, argp->opcode, lsnp,
		    &status, 0)) == DB_NOTFOUND) {
			if ((ret = __db_txnlist_add(dbenv,
			    info, argp->txnid->txnid,
			    argp->opcode == TXN_ABORT ?
			    TXN_IGNORE : argp->opcode, lsnp)) != 0)
				goto err;
		} else if (ret != 0 ||
		    (status != TXN_IGNORE && status != TXN_OK))
			goto err;
	}

	if (ret == 0)
		*lsnp = argp->prev_lsn;

	if (0) {
err:		__db_err(dbenv,
		    "txnid %lx commit record found, already on commit list",
		    (u_long)argp->txnid->txnid);
		ret = EINVAL;
	}
	__os_free(dbenv, argp);

	return (ret);
}

/*
 * PUBLIC: int __txn_xa_regop_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 *
 * These records are only ever written for prepares.
 */
int
__txn_xa_regop_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__txn_xa_regop_args *argp;
	int ret;
	u_int32_t status;

#ifdef DEBUG_RECOVER
	(void)__txn_xa_regop_print(dbenv, dbtp, lsnp, op, info);
#endif

	if ((ret = __txn_xa_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	if (argp->opcode != TXN_PREPARE && argp->opcode != TXN_ABORT) {
		ret = EINVAL;
		goto err;
	}

	/*
	 * The return value here is either a DB_NOTFOUND or it is
	 * the transaction status from the list.  It is not a normal
	 * error return, so we must make sure that in each of the
	 * cases below, we overwrite the ret value so we return
	 * appropriately.
	 */
	ret = __db_txnlist_find(dbenv, info, argp->txnid->txnid, &status);

	/*
	 * If we are rolling forward, then an aborted prepare
	 * indicates that this may the last record we'll see for
	 * this transaction ID, so we should remove it from the
	 * list.
	 */

	if (op == DB_TXN_FORWARD_ROLL) {
		if ((ret = __db_txnlist_remove(dbenv,
		    info, argp->txnid->txnid)) != 0)
			goto txn_err;
	} else if (op == DB_TXN_BACKWARD_ROLL && status == TXN_PREPARE) {
		/*
		 * On the backward pass, we have four possibilities:
		 * 1. The transaction is already committed, no-op.
		 * 2. The transaction is already aborted, no-op.
		 * 3. The prepare failed and was aborted, mark as abort.
		 * 4. The transaction is neither committed nor aborted.
		 *	 Treat this like a commit and roll forward so that
		 *	 the transaction can be resurrected in the region.
		 * We handle cases 3 and 4 here; cases 1 and 2
		 * are the final clause below.
		 */
		if (argp->opcode == TXN_ABORT) {
			if ((ret = __db_txnlist_update(dbenv,
			     info, argp->txnid->txnid,
			     TXN_ABORT, NULL, &status, 0)) != 0 &&
			     status != TXN_PREPARE)
				goto txn_err;
			ret = 0;
		}
		/*
		 * This is prepared, but not yet committed transaction.  We
		 * need to add it to the transaction list, so that it gets
		 * rolled forward. We also have to add it to the region's
		 * internal state so it can be properly aborted or committed
		 * after recovery (see txn_recover).
		 */
		else if ((ret = __db_txnlist_remove(dbenv,
		    info, argp->txnid->txnid)) != 0) {
txn_err:		__db_err(dbenv,
			    "Transaction not in list %x", argp->txnid->txnid);
			ret = DB_NOTFOUND;
		} else if ((ret = __db_txnlist_add(dbenv,
		   info, argp->txnid->txnid, TXN_COMMIT, lsnp)) == 0)
			ret = __txn_restore_txn(dbenv, lsnp, argp);
	} else
		ret = 0;

	if (ret == 0)
		*lsnp = argp->prev_lsn;

err:	__os_free(dbenv, argp);

	return (ret);
}

/*
 * PUBLIC: int __txn_ckp_recover
 * PUBLIC: __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__txn_ckp_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	DB_REP *db_rep;
	REP *rep;
	__txn_ckp_args *argp;
	int ret;

#ifdef DEBUG_RECOVER
	__txn_ckp_print(dbenv, dbtp, lsnp, op, info);
#endif
	if ((ret = __txn_ckp_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	if (op == DB_TXN_BACKWARD_ROLL)
		__db_txnlist_ckp(dbenv, info, lsnp);

	if (op == DB_TXN_FORWARD_ROLL) {
		/* Record the max generation number that we've seen. */
		if (REP_ON(dbenv)) {
			db_rep = dbenv->rep_handle;
			rep = db_rep->region;
			if (argp->rep_gen > rep->recover_gen)
				rep->recover_gen = argp->rep_gen;
		}
	}

	*lsnp = argp->last_ckp;
	__os_free(dbenv, argp);
	return (DB_TXN_CKP);
}

/*
 * __txn_child_recover
 *	Recover a commit record for a child transaction.
 *
 * PUBLIC: int __txn_child_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__txn_child_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__txn_child_args *argp;
	int ret, t_ret;
	u_int32_t c_stat, p_stat, tmpstat;

#ifdef DEBUG_RECOVER
	(void)__txn_child_print(dbenv, dbtp, lsnp, op, info);
#endif
	if ((ret = __txn_child_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	/*
	 * This is a record in a PARENT's log trail indicating that a
	 * child committed.  If we are aborting, we need to update the
	 * parent's LSN array.  If we are in recovery, then if the
	 * parent is committing, we set ourselves up to commit, else
	 * we do nothing.
	 */
	if (op == DB_TXN_ABORT) {
		/* Note that __db_txnlist_lsnadd rewrites its LSN
		 * parameter, so you cannot reuse the argp->c_lsn field.
		 */
		ret = __db_txnlist_lsnadd(dbenv,
		    info, &argp->c_lsn, TXNLIST_NEW);
	} else if (op == DB_TXN_BACKWARD_ROLL) {
		/* Child might exist -- look for it. */
		ret = __db_txnlist_find(dbenv, info, argp->child, &c_stat);
		t_ret =
		    __db_txnlist_find(dbenv, info, argp->txnid->txnid, &p_stat);
		if (ret != 0 && ret != DB_NOTFOUND)
			goto out;
		if (t_ret != 0 && t_ret != DB_NOTFOUND) {
			ret = t_ret;
			goto out;
		}
		/*
		 * If the parent is in state COMMIT or IGNORE, then we apply
		 * that to the child, else we need to abort the child.
		 */

		if (ret == DB_NOTFOUND  ||
		    c_stat == TXN_OK || c_stat == TXN_COMMIT) {
			if (t_ret == DB_NOTFOUND ||
			     (p_stat != TXN_COMMIT  && p_stat != TXN_IGNORE))
				c_stat = TXN_ABORT;
			else
				c_stat = p_stat;

			if (ret == DB_NOTFOUND)
				ret = __db_txnlist_add(dbenv,
				     info, argp->child, c_stat, NULL);
			else
				ret = __db_txnlist_update(dbenv, info,
				     argp->child, c_stat, NULL, &tmpstat, 0);
		} else if (c_stat == TXN_EXPECTED) {
			/*
			 * The open after this create succeeded.  If the
			 * parent succeeded, we don't want to redo; if the
			 * parent aborted, we do want to undo.
			 */
			switch (p_stat) {
			case TXN_COMMIT:
			case TXN_IGNORE:
				c_stat = TXN_IGNORE;
				break;
			default:
				c_stat = TXN_ABORT;
			}
			ret = __db_txnlist_update(dbenv,
			    info, argp->child, c_stat, NULL, &tmpstat, 0);
		} else if (c_stat == TXN_UNEXPECTED) {
			/*
			 * The open after this create failed.  If the parent
			 * is rolling forward, we need to roll forward.  If
			 * the parent failed, then we do not want to abort
			 * (because the file may not be the one in which we
			 * are interested).
			 */
			ret = __db_txnlist_update(dbenv, info, argp->child,
			    p_stat == TXN_COMMIT ? TXN_COMMIT : TXN_IGNORE,
			    NULL, &tmpstat, 0);
		}
	} else if (op == DB_TXN_OPENFILES) {
		/*
		 * If we have a partial subtransaction, then the whole
		 * transaction should be ignored.
		 */
		if ((ret = __db_txnlist_find(dbenv,
		    info, argp->child, &c_stat)) == DB_NOTFOUND)
			ret = __db_txnlist_update(dbenv, info,
			     argp->txnid->txnid, TXN_IGNORE,
			     NULL, &p_stat, 1);
	} else if (DB_REDO(op)) {
		/* Forward Roll */
		if ((ret =
		    __db_txnlist_remove(dbenv, info, argp->child)) != 0)
			__db_err(dbenv,
			    "Transaction not in list %x", argp->child);
	}

	if (ret == 0)
		*lsnp = argp->prev_lsn;

out:	__os_free(dbenv, argp);

	return (ret);
}

/*
 * __txn_restore_txn --
 *	Using only during XA recovery.  If we find any transactions that are
 * prepared, but not yet committed, then we need to restore the transaction's
 * state into the shared region, because the TM is going to issue an abort
 * or commit and we need to respond correctly.
 *
 * lsnp is the LSN of the returned LSN
 * argp is the prepare record (in an appropriate structure)
 *
 * PUBLIC: int __txn_restore_txn __P((DB_ENV *,
 * PUBLIC:     DB_LSN *, __txn_xa_regop_args *));
 */
int
__txn_restore_txn(dbenv, lsnp, argp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	__txn_xa_regop_args *argp;
{
	DB_TXNMGR *mgr;
	TXN_DETAIL *td;
	DB_TXNREGION *region;
	int ret;

	if (argp->xid.size == 0)
		return (0);

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;
	R_LOCK(dbenv, &mgr->reginfo);

	/* Allocate a new transaction detail structure. */
	if ((ret =
	    __db_shalloc(&mgr->reginfo, sizeof(TXN_DETAIL), 0, &td)) != 0) {
		R_UNLOCK(dbenv, &mgr->reginfo);
		return (ret);
	}

	/* Place transaction on active transaction list. */
	SH_TAILQ_INSERT_HEAD(&region->active_txn, td, links, __txn_detail);

	td->txnid = argp->txnid->txnid;
	td->begin_lsn = argp->begin_lsn;
	td->last_lsn = *lsnp;
	td->parent = 0;
	td->status = TXN_PREPARED;
	td->xa_status = TXN_XA_PREPARED;
	memcpy(td->xid, argp->xid.data, argp->xid.size);
	td->bqual = argp->bqual;
	td->gtrid = argp->gtrid;
	td->format = argp->formatID;
	td->flags = 0;
	F_SET(td, TXN_DTL_RESTORED);

	region->stat.st_nrestores++;
	region->stat.st_nactive++;
	if (region->stat.st_nactive > region->stat.st_maxnactive)
		region->stat.st_maxnactive = region->stat.st_nactive;
	R_UNLOCK(dbenv, &mgr->reginfo);
	return (0);
}

/*
 * __txn_recycle_recover --
 *	Recovery function for recycle.
 *
 * PUBLIC: int __txn_recycle_recover
 * PUBLIC:   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 */
int
__txn_recycle_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__txn_recycle_args *argp;
	int ret;

#ifdef DEBUG_RECOVER
	(void)__txn_child_print(dbenv, dbtp, lsnp, op, info);
#endif
	if ((ret = __txn_recycle_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	COMPQUIET(lsnp, NULL);

	if ((ret = __db_txnlist_gen(dbenv, info,
	    DB_UNDO(op) ? -1 : 1, argp->min, argp->max)) != 0)
		return (ret);

	__os_free(dbenv, argp);

	return (0);
}
