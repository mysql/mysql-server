/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_truncate.c,v 12.10 2005/10/21 19:22:59 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/qam.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/txn.h"

static int __db_cursor_check __P((DB *));

/*
 * __db_truncate_pp
 *	DB->truncate pre/post processing.
 *
 * PUBLIC: int __db_truncate_pp __P((DB *, DB_TXN *, u_int32_t *, u_int32_t));
 */
int
__db_truncate_pp(dbp, txn, countp, flags)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t *countp, flags;
{
	DB_ENV *dbenv;
	DB_THREAD_INFO *ip;
	int handle_check, ret, t_ret, txn_local;

	dbenv = dbp->dbenv;
	txn_local = 0;
	handle_check = 0;

	PANIC_CHECK(dbenv);
	STRIP_AUTO_COMMIT(flags);

	/* Check for invalid flags. */
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "DB->truncate forbidden on secondary indices");
		return (EINVAL);
	}
	if ((ret = __db_fchk(dbenv, "DB->truncate", flags, 0)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);

	/*
	 * Make sure there are no active cursors on this db.  Since we drop
	 * pages we cannot really adjust cursors.
	 */
	if (__db_cursor_check(dbp) != 0) {
		__db_err(dbenv,
		     "DB->truncate not permitted with active cursors");
		goto err;
	}

#if CONFIG_TEST
	if (IS_REP_MASTER(dbenv))
		DB_TEST_WAIT(dbenv, dbenv->test_check);
#endif
	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(dbenv);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0) {
		handle_check = 0;
		goto err;
	}

	/*
	 * Check for changes to a read-only database.
	 * This must be after the replication block so that we
	 * cannot race master/client state changes.
	 */
	if (DB_IS_READONLY(dbp)) {
		ret = __db_rdonly(dbenv, "DB->truncate");
		goto err;
	}

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	if (IS_DB_AUTO_COMMIT(dbp, txn)) {
		if ((ret = __txn_begin(dbenv, NULL, &txn, 0)) != 0)
			goto err;
		txn_local = 1;
	}

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;

	ret = __db_truncate(dbp, txn, countp);

err:	if (txn_local &&
	    (t_ret = __db_txn_auto_resolve(dbenv, txn, 0, ret)) && ret == 0)
		ret = t_ret;

	/* Release replication block. */
	if (handle_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	ENV_LEAVE(dbenv, ip);
	return (ret);
}

/*
 * __db_truncate
 *	DB->truncate.
 *
 * PUBLIC: int __db_truncate __P((DB *, DB_TXN *, u_int32_t *));
 */
int
__db_truncate(dbp, txn, countp)
	DB *dbp;
	DB_TXN *txn;
	u_int32_t *countp;
{
	DB *sdbp;
	DBC *dbc;
	DB_ENV *dbenv;
	u_int32_t scount;
	int ret, t_ret;

	dbenv = dbp->dbenv;
	dbc = NULL;
	ret = 0;

	/*
	 * Run through all secondaries and truncate them first.  The count
	 * returned is the count of the primary only.  QUEUE uses normal
	 * processing to truncate so it will update the secondaries normally.
	 */
	if (dbp->type != DB_QUEUE && LIST_FIRST(&dbp->s_secondaries) != NULL) {
		if ((ret = __db_s_first(dbp, &sdbp)) != 0)
			return (ret);
		for (; sdbp != NULL && ret == 0; ret = __db_s_next(&sdbp))
			if ((ret = __db_truncate(sdbp, txn, &scount)) != 0)
				break;
		if (sdbp != NULL)
			(void)__db_s_done(sdbp);
		if (ret != 0)
			return (ret);
	}

	DB_TEST_RECOVERY(dbp, DB_TEST_PREDESTROY, ret, NULL);

	/* Acquire a cursor. */
	if ((ret = __db_cursor(dbp, txn, &dbc, 0)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, txn, "DB->truncate", NULL, NULL, 0);

	switch (dbp->type) {
	case DB_BTREE:
	case DB_RECNO:
		ret = __bam_truncate(dbc, countp);
		break;
	case DB_HASH:
		ret = __ham_truncate(dbc, countp);
		break;
	case DB_QUEUE:
		ret = __qam_truncate(dbc, countp);
		break;
	case DB_UNKNOWN:
	default:
		ret = __db_unknown_type(dbenv, "DB->truncate", dbp->type);
		break;
	}

	/* Discard the cursor. */
	if (dbc != NULL && (t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTDESTROY, ret, NULL);

DB_TEST_RECOVERY_LABEL

	return (ret);
}

/*
 * __db_cursor_check --
 *	See if there are any active cursors on this db.
 */
static int
__db_cursor_check(dbp)
	DB *dbp;
{
	DB *ldbp;
	DBC *dbc;
	DB_ENV *dbenv;
	int found;

	dbenv = dbp->dbenv;

	MUTEX_LOCK(dbenv, dbenv->mtx_dblist);
	for (found = 0, ldbp = __dblist_get(dbenv, dbp->adj_fileid);
	    ldbp != NULL && ldbp->adj_fileid == dbp->adj_fileid;
	    ldbp = LIST_NEXT(ldbp, dblistlinks)) {
		MUTEX_LOCK(dbenv, dbp->mutex);
		for (dbc = TAILQ_FIRST(&ldbp->active_queue);
		    dbc != NULL; dbc = TAILQ_NEXT(dbc, links)) {
			if (IS_INITIALIZED(dbc)) {
				found = 1;
				break;
			}
		}
		MUTEX_UNLOCK(dbenv, dbp->mutex);
		if (found == 1)
			break;
	}
	MUTEX_UNLOCK(dbenv, dbenv->mtx_dblist);

	return (found);
}
