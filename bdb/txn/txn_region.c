/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: txn_region.c,v 11.36 2001/01/11 18:19:55 bostic Exp $";
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
#include "db_page.h"
#include "log.h"	/* for __log_lastckp */
#include "txn.h"
#include "db_am.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int __txn_init __P((DB_ENV *, DB_TXNMGR *));
static int __txn_set_tx_max __P((DB_ENV *, u_int32_t));
static int __txn_set_tx_recover __P((DB_ENV *,
	       int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops)));
static int __txn_set_tx_timestamp __P((DB_ENV *, time_t *));

/*
 * __txn_dbenv_create --
 *	Transaction specific initialization of the DB_ENV structure.
 *
 * PUBLIC: void __txn_dbenv_create __P((DB_ENV *));
 */
void
__txn_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	dbenv->tx_max = DEF_MAX_TXNS;

	dbenv->set_tx_max = __txn_set_tx_max;
	dbenv->set_tx_recover = __txn_set_tx_recover;
	dbenv->set_tx_timestamp = __txn_set_tx_timestamp;

#ifdef HAVE_RPC
	/*
	 * If we have a client, overwrite what we just setup to point to
	 * client functions.
	 */
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->set_tx_max = __dbcl_set_tx_max;
		dbenv->set_tx_recover = __dbcl_set_tx_recover;
		dbenv->set_tx_timestamp = __dbcl_set_tx_timestamp;
	}
#endif
}

/*
 * __txn_set_tx_max --
 *	Set the size of the transaction table.
 */
static int
__txn_set_tx_max(dbenv, tx_max)
	DB_ENV *dbenv;
	u_int32_t tx_max;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_tx_max");

	dbenv->tx_max = tx_max;
	return (0);
}

/*
 * __txn_set_tx_recover --
 *	Set the transaction abort recover function.
 */
static int
__txn_set_tx_recover(dbenv, tx_recover)
	DB_ENV *dbenv;
	int (*tx_recover) __P((DB_ENV *, DBT *, DB_LSN *, db_recops));
{
	dbenv->tx_recover = tx_recover;
	return (0);
}

/*
 * __txn_set_tx_timestamp --
 *	Set the transaction recovery timestamp.
 */
static int
__txn_set_tx_timestamp(dbenv, timestamp)
	DB_ENV *dbenv;
	time_t *timestamp;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_tx_timestamp");

	dbenv->tx_timestamp = *timestamp;
	return (0);
}

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
	    &tmgrp->reginfo, TXN_REGION_SIZE(dbenv->tx_max))) != 0)
		goto err;

	/* If we created the region, initialize it. */
	if (F_ISSET(&tmgrp->reginfo, REGION_CREATE))
		if ((ret = __txn_init(dbenv, tmgrp)) != 0)
			goto err;

	/* Set the local addresses. */
	tmgrp->reginfo.primary =
	    R_ADDR(&tmgrp->reginfo, tmgrp->reginfo.rp->primary);

	/* Acquire a mutex to protect the active TXN list. */
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, &tmgrp->reginfo, &tmgrp->mutexp)) != 0)
			goto err;
		if ((ret = __db_mutex_init(
		    dbenv, tmgrp->mutexp, 0, MUTEX_THREAD)) != 0)
			goto err;
	}

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
	__os_free(tmgrp, sizeof(*tmgrp));
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

	ZERO_LSN(last_ckp);
	/*
	 * If possible, fetch the last checkpoint LSN from the log system
	 * so that the backwards chain of checkpoints is unbroken when
	 * the environment is removed and recreated. [#2865]
	 */
	if (LOGGING_ON(dbenv) && (ret = __log_lastckp(dbenv, &last_ckp)) != 0)
		return (ret);

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
	ZERO_LSN(region->pending_ckp);
	region->last_ckp = last_ckp;
	region->time_ckp = time(NULL);

	/*
	 * XXX
	 * If we ever do more types of locking and logging, this changes.
	 */
	region->logtype = 0;
	region->locktype = 0;
	region->naborts = 0;
	region->ncommits = 0;
	region->nbegins = 0;
	region->nactive = 0;
	region->maxnactive = 0;

	SH_TAILQ_INIT(&region->active_txn);

	return (0);
}

/*
 * __txn_close --
 *	Close a transaction region.
 *
 * PUBLIC: int __txn_close __P((DB_ENV *));
 */
int
__txn_close(dbenv)
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
	"Error: closing the transaction region with active transactions\n");
		ret = EINVAL;
		while ((txnp = TAILQ_FIRST(&tmgrp->txn_chain)) != NULL) {
			txnid = txnp->txnid;
			if ((t_ret = txn_abort(txnp)) != 0) {
				__db_err(dbenv,
				    "Unable to abort transaction 0x%x: %s\n",
				    txnid, db_strerror(t_ret));
				ret = __db_panic(dbenv, t_ret);
			}
		}
	}

	/* Flush the log. */
	if (LOGGING_ON(dbenv) &&
	    (t_ret = log_flush(dbenv, NULL)) != 0 && ret == 0)
		ret = t_ret;

	/* Discard the per-thread lock. */
	if (tmgrp->mutexp != NULL)
		__db_mutex_free(dbenv, &tmgrp->reginfo, tmgrp->mutexp);

	/* Detach from the region. */
	if ((t_ret = __db_r_detach(dbenv, &tmgrp->reginfo, 0)) != 0 && ret == 0)
		ret = t_ret;

	__os_free(tmgrp, sizeof(*tmgrp));

	dbenv->tx_handle = NULL;
	return (ret);
}

int
txn_stat(dbenv, statp, db_malloc)
	DB_ENV *dbenv;
	DB_TXN_STAT **statp;
	void *(*db_malloc) __P((size_t));
{
	DB_TXNMGR *mgr;
	DB_TXNREGION *region;
	DB_TXN_STAT *stats;
	TXN_DETAIL *txnp;
	size_t nbytes;
	u_int32_t nactive, ndx;
	int ret, slop;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_txn_stat(dbenv, statp, db_malloc));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->tx_handle, DB_INIT_TXN);

	*statp = NULL;

	slop = 200;
	mgr = dbenv->tx_handle;
	region = mgr->reginfo.primary;

retry:	R_LOCK(dbenv, &mgr->reginfo);
	nactive = region->nactive;
	R_UNLOCK(dbenv, &mgr->reginfo);

	/*
	 * Allocate extra active structures to handle any transactions that
	 * are created while we have the region unlocked.
	 */
	nbytes = sizeof(DB_TXN_STAT) + sizeof(DB_TXN_ACTIVE) * (nactive + slop);
	if ((ret = __os_malloc(dbenv, nbytes, db_malloc, &stats)) != 0)
		return (ret);

	R_LOCK(dbenv, &mgr->reginfo);
	stats->st_last_txnid = region->last_txnid;
	stats->st_last_ckp = region->last_ckp;
	stats->st_maxtxns = region->maxtxns;
	stats->st_naborts = region->naborts;
	stats->st_nbegins = region->nbegins;
	stats->st_ncommits = region->ncommits;
	stats->st_pending_ckp = region->pending_ckp;
	stats->st_time_ckp = region->time_ckp;
	stats->st_nactive = region->nactive;
	if (stats->st_nactive > nactive + 200) {
		R_UNLOCK(dbenv, &mgr->reginfo);
		slop *= 2;
		goto retry;
	}
	stats->st_maxnactive = region->maxnactive;
	stats->st_txnarray = (DB_TXN_ACTIVE *)&stats[1];

	ndx = 0;
	for (txnp = SH_TAILQ_FIRST(&region->active_txn, __txn_detail);
	    txnp != NULL;
	    txnp = SH_TAILQ_NEXT(txnp, links, __txn_detail)) {
		stats->st_txnarray[ndx].txnid = txnp->txnid;
		if (txnp->parent == INVALID_ROFF)
			stats->st_txnarray[ndx].parentid = TXN_INVALID_ID;
		else
			stats->st_txnarray[ndx].parentid =
			    ((TXN_DETAIL *)R_ADDR(&mgr->reginfo,
			    txnp->parent))->txnid;
		stats->st_txnarray[ndx].lsn = txnp->begin_lsn;
		ndx++;

		if (ndx >= stats->st_nactive)
			break;
	}

	stats->st_region_wait = mgr->reginfo.rp->mutex.mutex_set_wait;
	stats->st_region_nowait = mgr->reginfo.rp->mutex.mutex_set_nowait;
	stats->st_regsize = mgr->reginfo.rp->size;

	R_UNLOCK(dbenv, &mgr->reginfo);

	*statp = stats;
	return (0);
}
