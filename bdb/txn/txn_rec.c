/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
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
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: txn_rec.c,v 11.15 2001/01/11 18:19:55 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#endif

#include "db_int.h"
#include "db_page.h"
#include "txn.h"
#include "db_am.h"
#include "db_dispatch.h"
#include "log.h"
#include "common_ext.h"

static int __txn_restore_txn __P((DB_ENV *, DB_LSN *, __txn_xa_regop_args *));

#define	IS_XA_TXN(R) (R->xid.size != 0)

/*
 * PUBLIC: int __txn_regop_recover
 * PUBLIC:    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
 *
 * These records are only ever written for commits.  Normally, we redo any
 * committed transaction, however if we are doing recovery to a timestamp, then
 * we may treat transactions that commited after the timestamp as aborted.
 */
int
__txn_regop_recover(dbenv, dbtp, lsnp, op, info)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops op;
	void *info;
{
	__txn_regop_args *argp;
	int ret;

#ifdef DEBUG_RECOVER
	(void)__txn_regop_print(dbenv, dbtp, lsnp, op, info);
#endif

	if ((ret = __txn_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	if (argp->opcode != TXN_COMMIT) {
		ret = EINVAL;
		goto err;
	}

	if (op == DB_TXN_FORWARD_ROLL)
		ret = __db_txnlist_remove(info, argp->txnid->txnid);
	else if (dbenv->tx_timestamp == 0 ||
	    argp->timestamp <= (int32_t)dbenv->tx_timestamp)
		/*
		 * We know this is the backward roll case because we
		 * are never called during ABORT or OPENFILES.
		 */
		ret = __db_txnlist_add(dbenv, info, argp->txnid->txnid, 0);
	else
		/*
		 * This is commit record, but we failed the timestamp check
		 * so we should treat it as an abort and add it to the list
		 * as an aborted record.
		 */
		ret = __db_txnlist_add(dbenv, info, argp->txnid->txnid, 1);

	if (ret == 0)
		*lsnp = argp->prev_lsn;
err:	__os_free(argp, 0);

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

#ifdef DEBUG_RECOVER
	(void)__txn_xa_regop_print(dbenv, dbtp, lsnp, op, info);
#endif

	if ((ret = __txn_xa_regop_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	if (argp->opcode != TXN_PREPARE) {
		ret = EINVAL;
		goto err;
	}

	ret = __db_txnlist_find(info, argp->txnid->txnid);

	/*
	 * If we are rolling forward, then an aborted prepare
	 * indicates that this is the last record we'll see for
	 * this transaction ID and we should remove it from the
	 * list.
	 */

	if (op == DB_TXN_FORWARD_ROLL && ret == 1)
		ret = __db_txnlist_remove(info, argp->txnid->txnid);
	else if (op == DB_TXN_BACKWARD_ROLL && ret != 0) {
		/*
		 * On the backward pass, we have three possibilities:
		 * 1. The transaction is already committed, no-op.
		 * 2. The transaction is not committed and we are XA, treat
		 *	 like commited and roll forward so that can be committed
		 *	 or aborted late.
		 * 3. The transaction is not committed and we are not XA
		 *	mark the transaction as aborted.
		 *
		 * Cases 2 and 3 are handled here.
		 */

		/*
		 * Should never have seen this transaction unless it was
		 * commited.
		 */
		DB_ASSERT(ret == DB_NOTFOUND);

		if (IS_XA_TXN(argp)) {
			/*
			 * This is an XA prepared, but not yet committed
			 * transaction.  We need to add it to the
			 * transaction list, so that it gets rolled
			 * forward. We also have to add it to the region's
			 * internal state so it can be properly aborted
			 * or recovered.
			 */
			if ((ret = __db_txnlist_add(dbenv,
			    info, argp->txnid->txnid, 0)) == 0)
				ret = __txn_restore_txn(dbenv, lsnp, argp);
		} else
			ret = __db_txnlist_add(dbenv,
			    info, argp->txnid->txnid, 1);
	} else
		ret = 0;

	if (ret == 0)
		*lsnp = argp->prev_lsn;

err:	__os_free(argp, 0);

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
	__txn_ckp_args *argp;
	int ret;

#ifdef DEBUG_RECOVER
	__txn_ckp_print(dbenv, dbtp, lsnp, op, info);
#endif
	COMPQUIET(dbenv, NULL);

	if ((ret = __txn_ckp_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	/*
	 * Check for 'restart' checkpoint record.  This occurs when the
	 * checkpoint lsn is equal to the lsn of the checkpoint record
	 * and means that we could set the transaction ID back to 1, so
	 * that we don't exhaust the transaction ID name space.
	 */
	if (argp->ckp_lsn.file == lsnp->file &&
	    argp->ckp_lsn.offset == lsnp->offset)
		__db_txnlist_gen(info, DB_REDO(op) ? -1 : 1);

	*lsnp = argp->last_ckp;
	__os_free(argp, 0);
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
	int ret;

#ifdef DEBUG_RECOVER
	(void)__txn_child_print(dbenv, dbtp, lsnp, op, info);
#endif
	if ((ret = __txn_child_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);

	/*
	 * This is a record in a PARENT's log trail indicating that a
	 * child commited.  If we are aborting, we need to update the
	 * parent's LSN array.  If we are in recovery, then if the
	 * parent is commiting, we set ourselves up to commit, else
	 * we do nothing.
	 */
	if (op == DB_TXN_ABORT) {
		/* Note that __db_txnlist_lsnadd rewrites its LSN
		 * parameter, so you cannot reuse the argp->c_lsn field.
		 */
		ret = __db_txnlist_lsnadd(dbenv,
		    info, &argp->c_lsn, TXNLIST_NEW);
	} else if (op == DB_TXN_BACKWARD_ROLL) {
		if (__db_txnlist_find(info, argp->txnid->txnid) == 0)
			ret = __db_txnlist_add(dbenv, info, argp->child, 0);
		else
			ret = __db_txnlist_add(dbenv, info, argp->child, 1);
	} else
		ret = __db_txnlist_remove(info, argp->child);

	if (ret == 0)
		*lsnp = argp->prev_lsn;

	__os_free(argp, 0);

	return (ret);
}

/*
 * __txn_restore_txn --
 *	Using only during XA recovery.  If we find any transactions that are
 * prepared, but not yet committed, then we need to restore the transaction's
 * state into the shared region, because the TM is going to issue a txn_abort
 * or txn_commit and we need to respond correctly.
 *
 * lsnp is the LSN of the returned LSN
 * argp is the perpare record (in an appropriate structure)
 */
static int
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
	    __db_shalloc(mgr->reginfo.addr, sizeof(TXN_DETAIL), 0, &td)) != 0)
		return (ret);

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

	R_UNLOCK(dbenv, &mgr->reginfo);
	return (0);
}
