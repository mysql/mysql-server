/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: txn_region.c,v 11.73 2002/08/06 04:42:37 bostic Exp $";
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

#include "db_int.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __txn_findlastckp __P((DB_ENV *, DB_LSN *));
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
	DB_TXNMGR *tmgrp;
	int ret;

	/* Create/initialize the transaction manager structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(DB_TXNMGR), &tmgrp)) != 0)
		return (ret);
	TAILQ_INIT(&tmgrp->txn_chain);
	tmgrp->dbenv = dbenv;

	/* Join/create the txn region. */
	tmgrp->reginfo.type = REGION_TYPE_TXN;
	tmgrp->reginfo.id = INVALID_REGION_ID;
	tmgrp->reginfo.mode = dbenv->db_mode;
	tmgrp->reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&tmgrp->reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(dbenv,
	    &tmgrp->reginfo, __txn_region_size(dbenv))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&tmgrp->reginfo, REGION_CREATE))
		if ((ret = __txn_init(dbenv, tmgrp)) != 0)
			goto err;

	/* Set the local addresses. */
	tmgrp->reginfo.primary =
	    R_ADDR(&tmgrp->reginfo, tmgrp->reginfo.rp->primary);

	/* Acquire a mutex to protect the active TXN list. */
	if (F_ISSET(dbenv, DB_ENV_THREAD) &&
	    (ret = __db_mutex_setup(dbenv, &tmgrp->reginfo, &tmgrp->mutexp,
	    MUTEX_ALLOC | MUTEX_NO_RLOCK | MUTEX_THREAD)) != 0)
		goto err;

	R_UNLOCK(dbenv, &tmgrp->reginfo);

	dbenv->tx_handle = tmgrp;
	return (0);

err:	if (tmgrp->reginfo.addr != NULL) {
		if (F_ISSET(&tmgrp->reginfo, REGION_CREATE))
			ret = __db_panic(dbenv, ret);
		R_UNLOCK(dbenv, &tmgrp->reginfo);

		(void)__db_r_detach(dbenv, &tmgrp->reginfo, 0);
	}
	if (tmgrp->mutexp != NULL)
		__db_mutex_free(dbenv, &tmgrp->reginfo, tmgrp->mutexp);
	__os_free(dbenv, tmgrp);
	return (ret);
}

/*
 * __txn_init --
 *	Initialize a transaction region in shared memory.
 */
static int
__txn_init(dbenv, tmgrp)
	DB_ENV *dbenv;
	DB_TXNMGR *tmgrp;
{
	DB_LSN last_ckp;
	DB_TXNREGION *region;
	int ret;
#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
	u_int8_t *addr;
#endif

	/*
	 * Find the last checkpoint in the log.
	 */
	ZERO_LSN(last_ckp);
	if (LOGGING_ON(dbenv)) {
		/*
		 * The log system has already walked through the last
		 * file.  Get the LSN of a checkpoint it may have found.
		 */
		__log_get_cached_ckp_lsn(dbenv, &last_ckp);

		/*
		 * If that didn't work, look backwards from the beginning of
		 * the last log file until we find the last checkpoint.
		 */
		if (IS_ZERO_LSN(last_ckp) &&
		    (ret = __txn_findlastckp(dbenv, &last_ckp)) != 0)
			return (ret);
	}

	if ((ret = __db_shalloc(tmgrp->reginfo.addr,
	    sizeof(DB_TXNREGION), 0, &tmgrp->reginfo.primary)) != 0) {
		__db_err(dbenv,
		    "Unable to allocate memory for the transaction region");
		return (ret);
	}
	tmgrp->reginfo.rp->primary =
	    R_OFFSET(&tmgrp->reginfo, tmgrp->reginfo.primary);
	region = tmgrp->reginfo.primary;
	memset(region, 0, sizeof(*region));

	region->maxtxns = dbenv->tx_max;
	region->last_txnid = TXN_MINIMUM;
	region->cur_maxid = TXN_MAXIMUM;
	region->last_ckp = last_ckp;
	region->time_ckp = time(NULL);

	/*
	 * XXX
	 * If we ever do more types of locking and logging, this changes.
	 */
	region->logtype = 0;
	region->locktype = 0;

	memset(&region->stat, 0, sizeof(region->stat));
	region->stat.st_maxtxns = region->maxtxns;

	SH_TAILQ_INIT(&region->active_txn);
#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
	/* Allocate room for the txn maintenance info and initialize it. */
	if ((ret = __db_shalloc(tmgrp->reginfo.addr,
	    sizeof(REGMAINT) + TXN_MAINT_SIZE, 0, &addr)) != 0) {
		__db_err(dbenv,
		    "Unable to allocate memory for mutex maintenance");
		return (ret);
	}
	__db_maintinit(&tmgrp->reginfo, addr, TXN_MAINT_SIZE);
	region->maint_off = R_OFFSET(&tmgrp->reginfo, addr);
#endif
	return (0);
}

/*
 * __txn_findlastckp --
 *	Find the last checkpoint in the log, walking backwards from the
 *	beginning of the last log file.  (The log system looked through
 *	the last log file when it started up.)
 */
static int
__txn_findlastckp(dbenv, lsnp)
	DB_ENV *dbenv;
	DB_LSN *lsnp;
{
	DB_LOGC *logc;
	DB_LSN lsn;
	DBT dbt;
	int ret, t_ret;
	u_int32_t rectype;

	if ((ret = dbenv->log_cursor(dbenv, &logc, 0)) != 0)
		return (ret);

	/* Get the last LSN. */
	memset(&dbt, 0, sizeof(dbt));
	if ((ret = logc->get(logc, &lsn, &dbt, DB_LAST)) != 0)
		goto err;

	/*
	 * Twiddle the last LSN so it points to the beginning of the last
	 * file;  we know there's no checkpoint after that, since the log
	 * system already looked there.
	 */
	lsn.offset = 0;

	/* Read backwards, looking for checkpoints. */
	while ((ret = logc->get(logc, &lsn, &dbt, DB_PREV)) == 0) {
		if (dbt.size < sizeof(u_int32_t))
			continue;
		memcpy(&rectype, dbt.data, sizeof(u_int32_t));
		if (rectype == DB___txn_ckp) {
			*lsnp = lsn;
			break;
		}
	}

err:	if ((t_ret = logc->close(logc, 0)) != 0 && ret == 0)
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
 * Called only from __dbenv_refresh.  (Formerly called __txn_close.)
 *
 * PUBLIC: int __txn_dbenv_refresh __P((DB_ENV *));
 */
int
__txn_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	DB_TXN *txnp;
	DB_TXNMGR *tmgrp;
	u_int32_t txnid;
	int ret, t_ret;

	ret = 0;
	tmgrp = dbenv->tx_handle;

	/*
	 * This function can only be called once per process (i.e., not
	 * once per thread), so no synchronization is required.
	 *
	 * The caller is doing something wrong if close is called with
	 * active transactions.  Try and abort any active transactions,
	 * but it's quite likely the aborts will fail because recovery
	 * won't find open files.  If we can't abort any transaction,
	 * panic, we have to run recovery to get back to a known state.
	 */
	if (TAILQ_FIRST(&tmgrp->txn_chain) != NULL) {
		__db_err(dbenv,
	"Error: closing the transaction region with active transactions");
		ret = EINVAL;
		while ((txnp = TAILQ_FIRST(&tmgrp->txn_chain)) != NULL) {
			txnid = txnp->txnid;
			if ((t_ret = txnp->abort(txnp)) != 0) {
				__db_err(dbenv,
				    "Unable to abort transaction 0x%x: %s",
				    txnid, db_strerror(t_ret));
				ret = __db_panic(dbenv, t_ret);
				break;
			}
		}
	}

	/* Flush the log. */
	if (LOGGING_ON(dbenv) &&
	    (t_ret = dbenv->log_flush(dbenv, NULL)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the per-thread lock. */
	if (tmgrp->mutexp != NULL)
		__db_mutex_free(dbenv, &tmgrp->reginfo, tmgrp->mutexp);

	/* Detach from the region. */
	if ((t_ret = __db_r_detach(dbenv, &tmgrp->reginfo, 0)) != 0 && ret == 0)
		ret = t_ret;

	__os_free(dbenv, tmgrp);

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
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	if (F_ISSET(dbenv, DB_ENV_THREAD))
		s += sizeof(REGMAINT) + TXN_MAINT_SIZE;
#endif
	return (s);
}

/*
 * __txn_region_destroy
 *	Destroy any region maintenance info.
 *
 * PUBLIC: void __txn_region_destroy __P((DB_ENV *, REGINFO *));
 */
void
__txn_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	__db_shlocks_destroy(infop, (REGMAINT *)R_ADDR(infop,
	    ((DB_TXNREGION *)R_ADDR(infop, infop->rp->primary))->maint_off));

	COMPQUIET(dbenv, NULL);
	COMPQUIET(infop, NULL);
}

#ifdef CONFIG_TEST
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
		    cur_txnid);
		ret = EINVAL;
	}
	if (max_txnid < TXN_MINIMUM) {
		__db_err(dbenv, "Maximum ID value %lu below minimum",
		    max_txnid);
		ret = EINVAL;
	}
	return (ret);
}
#endif
