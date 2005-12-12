/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: txn_region.c,v 12.10 2005/10/14 21:12:18 ubell Exp $
 */

#include "db_config.h"

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

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __txn_init __P((DB_ENV *, DB_TXNMGR *));
static size_t __txn_region_size __P((DB_ENV *));

/*
 * __txn_open --
 *	Open a transaction region.
 *
 * PUBLIC: int __txn_open __P((DB_ENV *));
 */
int
__txn_open(dbenv)
	DB_ENV *dbenv;
{
	DB_TXNMGR *mgr;
	int ret;

	/* Create/initialize the transaction manager structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXNMGR), &mgr)) != 0)
		return (ret);
	TAILQ_INIT(&mgr->txn_chain);
	mgr->dbenv = dbenv;

	/* Join/create the txn region. */
	mgr->reginfo.dbenv = dbenv;
	mgr->reginfo.type = REGION_TYPE_TXN;
	mgr->reginfo.id = INVALID_REGION_ID;
	mgr->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&mgr->reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(dbenv,
	    &mgr->reginfo, __txn_region_size(dbenv))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&mgr->reginfo, REGION_CREATE))
		if ((ret = __txn_init(dbenv, mgr)) != 0)
			goto err;

	/* Set the local addresses. */
	mgr->reginfo.primary =
	    R_ADDR(&mgr->reginfo, mgr->reginfo.rp->primary);

	/* If threaded, acquire a mutex to protect the active TXN list. */
	if ((ret = __mutex_alloc(
	    dbenv, MTX_TXN_ACTIVE, DB_MUTEX_THREAD, &mgr->mutex)) != 0)
		goto err;

	dbenv->tx_handle = mgr;
	return (0);

err:	dbenv->tx_handle = NULL;
	if (mgr->reginfo.addr != NULL)
		(void)__db_r_detach(dbenv, &mgr->reginfo, 0);

	(void)__mutex_free(dbenv, &mgr->mutex);
	__os_free(dbenv, mgr);
	return (ret);
}

/*
 * __txn_init --
 *	Initialize a transaction region in shared memory.
 */
static int
__txn_init(dbenv, mgr)
	DB_ENV *dbenv;
	DB_TXNMGR *mgr;
{
	DB_LSN last_ckp;
	DB_TXNREGION *region;
	int ret;

	/*
	 * Find the last checkpoint in the log.
	 */
	ZERO_LSN(last_ckp);
	if (LOGGING_ON(dbenv)) {
		/*
		 * The log system has already walked through the last
		 * file.  Get the LSN of a checkpoint it may have found.
		 */
		if ((ret = __log_get_cached_ckp_lsn(dbenv, &last_ckp)) != 0)
			return (ret);

		/*
		 * If that didn't work, look backwards from the beginning of
		 * the last log file until we find the last checkpoint.
		 */
		if (IS_ZERO_LSN(last_ckp) &&
		    (ret = __txn_findlastckp(dbenv, &last_ckp, NULL)) != 0)
			return (ret);
	}

	if ((ret = __db_shalloc(&mgr->reginfo,
	    sizeof(DB_TXNREGION), 0, &mgr->reginfo.primary)) != 0) {
		__db_err(dbenv,
		    "Unable to allocate memory for the transaction region");
		return (ret);
	}
	mgr->reginfo.rp->primary =
	    R_OFFSET(&mgr->reginfo, mgr->reginfo.primary);
	region = mgr->reginfo.primary;
	memset(region, 0, sizeof(*region));

	if ((ret = __mutex_alloc(
	    dbenv, MTX_TXN_REGION, 0, &region->mtx_region)) != 0)
		return (ret);

	region->maxtxns = dbenv->tx_max;
	region->last_txnid = TXN_MINIMUM;
	region->cur_maxid = TXN_MAXIMUM;

	if ((ret = __mutex_alloc(
	    dbenv, MTX_TXN_CHKPT, 0, &region->mtx_ckp)) != 0)
		return (ret);
	region->last_ckp = last_ckp;
	region->time_ckp = time(NULL);

	memset(&region->stat, 0, sizeof(region->stat));
	region->stat.st_maxtxns = region->maxtxns;

	SH_TAILQ_INIT(&region->active_txn);
	return (ret);
}

/*
 * __txn_findlastckp --
 *	Find the last checkpoint in the log, walking backwards from the
 *	max_lsn given or the beginning of the last log file.  (The
 *	log system looked through the last log file when it started up.)
 *
 * PUBLIC: int __txn_findlastckp __P((DB_ENV *, DB_LSN *, DB_LSN *));
 */
int
__txn_findlastckp(dbenv, lsnp, max_lsn)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
	DB_LSN *max_lsn;
{
	DB_LOGC *logc;
	DB_LSN lsn;
	DBT dbt;
	int ret, t_ret;
	u_int32_t rectype;

	ZERO_LSN(*lsnp);

	if ((ret = __log_cursor(dbenv, &logc)) != 0)
		return (ret);

	/* Get the last LSN. */
	memset(&dbt, 0, sizeof(dbt));
	if (max_lsn != NULL) {
		lsn = *max_lsn;
		if ((ret = __log_c_get(logc, &lsn, &dbt, DB_SET)) != 0)
			goto err;
	} else {
		if ((ret = __log_c_get(logc, &lsn, &dbt, DB_LAST)) != 0)
			goto err;
		/*
		 * Twiddle the last LSN so it points to the beginning of the
		 * last file; we know there's no checkpoint after that, since
		 * the log system already looked there.
		 */
		lsn.offset = 0;
	}

	/* Read backwards, looking for checkpoints. */
	while ((ret = __log_c_get(logc, &lsn, &dbt, DB_PREV)) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&rectype, dbt.data, sizeof(u_int32_t));
		if (rectype == DB___txn_ckp) {
			*lsnp = lsn;
			break;
		}
	}

err:	if ((t_ret = __log_c_close(logc)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Not finding a checkpoint is not an error;  there may not exist
	 * one in the log.
	 */
	return ((ret == 0 || ret == DB_NOTFOUND) ? 0 : ret);
}

/*
 * __txn_dbenv_refresh --
 *	Clean up after the transaction system on a close or failed open.
 *
 * PUBLIC: int __txn_dbenv_refresh __P((DB_ENV *));
 */
int
__txn_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_TXN *txn;
	DB_TXNMGR *mgr;
	REGINFO *reginfo;
	u_int32_t txnid;
	int aborted, ret, t_ret;

	ret = 0;
	mgr = dbenv->tx_handle;
	reginfo = &mgr->reginfo;

	/*
	 * This function can only be called once per process (i.e., not
	 * once per thread), so no synchronization is required.
	 *
	 * The caller is probably doing something wrong if close is called with
	 * active transactions.  Try and abort any active transactions that are
	 * not prepared, but it's quite likely the aborts will fail because
	 * recovery won't find open files.  If we can't abort any of the
	 * unprepared transaction, panic, we have to run recovery to get back
	 * to a known state.
	 */
	aborted = 0;
	if (TAILQ_FIRST(&mgr->txn_chain) != NULL) {
		while ((txn = TAILQ_FIRST(&mgr->txn_chain)) != NULL) {
			/* Prepared transactions are OK. */
			txnid = txn->txnid;
			if (((TXN_DETAIL *)txn->td)->status == TXN_PREPARED) {
				if ((ret = __txn_discard_int(txn, 0)) != 0) {
					__db_err(dbenv,
					    "Unable to discard txn 0x%x: %s",
					    txnid, db_strerror(ret));
					break;
				}
				continue;
			}
			aborted = 1;
			if ((t_ret = __txn_abort(txn)) != 0) {
				__db_err(dbenv,
				    "Unable to abort transaction 0x%x: %s",
				    txnid, db_strerror(t_ret));
				ret = __db_panic(dbenv, t_ret);
				break;
			}
		}
		if (aborted) {
			__db_err(dbenv,
	"Error: closing the transaction region with active transactions");
			if (ret == 0)
				ret = EINVAL;
		}
	}

	/* Flush the log. */
	if (LOGGING_ON(dbenv) &&
	    (t_ret = __log_flush(dbenv, NULL)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the per-thread lock. */
	if ((t_ret = __mutex_free(dbenv, &mgr->mutex)) != 0 && ret == 0)
		ret = t_ret;

	/* Detach from the region. */
	if ((t_ret = __db_r_detach(dbenv, reginfo, 0)) != 0 && ret == 0)
		ret = t_ret;

	__os_free(dbenv, mgr);

	dbenv->tx_handle = NULL;
	return (ret);
}

/*
 * __txn_region_size --
 *	 Return the amount of space needed for the txn region.  Make the
 *	 region large enough to hold txn_max transaction detail structures
 *	 plus some space to hold thread handles and the beginning of the
 *	 shalloc region and anything we need for mutex system resource
 *	 recording.
 */
static size_t
__txn_region_size(dbenv)
	DB_ENV *dbenv;
{
	size_t s;

	s = sizeof(DB_TXNREGION) +
	    dbenv->tx_max * sizeof(TXN_DETAIL) + 10 * 1024;
	return (s);
}

/*
 * __txn_id_set --
 *	Set the current transaction ID and current maximum unused ID (for
 *	testing purposes only).
 *
 * PUBLIC: int __txn_id_set __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__txn_id_set(dbenv, cur_txnid, max_txnid)
	DB_ENV *dbenv;
	u_int32_t cur_txnid, max_txnid;
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	int ret;

	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, "txn_id_set", DB_INIT_TXN);

	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;
	region->last_txnid = cur_txnid;
	region->cur_maxid = max_txnid;

	ret = 0;
	if (cur_txnid < TXN_MINIMUM) {
		__db_err(dbenv, "Current ID value %lu below minimum",
		    (u_long)cur_txnid);
		ret = EINVAL;
	}
	if (max_txnid < TXN_MINIMUM) {
		__db_err(dbenv, "Maximum ID value %lu below minimum",
		    (u_long)max_txnid);
		ret = EINVAL;
	}
	return (ret);
}
