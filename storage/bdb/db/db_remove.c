/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_remove.c,v 11.219 2004/09/16 17:55:17 margo Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/fop.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"

static int __db_dbtxn_remove __P((DB *, DB_TXN *, const char *));
static int __db_subdb_remove __P((DB *, DB_TXN *, const char *, const char *));

/*
 * __dbenv_dbremove_pp
 *	DB_ENV->dbremove pre/post processing.
 *
 * PUBLIC: int __dbenv_dbremove_pp __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, const char *, u_int32_t));
 */
int
__dbenv_dbremove_pp(dbenv, txn, name, subdb, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name, *subdb;
	u_int32_t flags;
{
	DB *dbp;
	int handle_check, ret, t_ret, txn_local;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->dbremove");

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB->remove", flags, DB_AUTO_COMMIT)) != 0)
		return (ret);

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
			return (ret);
		txn_local = 1;
	} else {
		if (txn != NULL && !TXN_ON(dbenv))
			return (__db_not_txn_env(dbenv));
		txn_local = 0;
	}

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		goto err;

	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 1, txn != NULL)) != 0)
		goto err;

	ret = __db_remove_int(dbp, txn, name, subdb, flags);

	if (txn_local) {
		/*
		 * We created the DBP here and when we commit/abort, we'll
		 * release all the transactional locks, including the handle
		 * lock; mark the handle cleared explicitly.
		 */
		LOCK_INIT(dbp->handle_lock);
		dbp->lid = DB_LOCK_INVALIDID;
	} else if (txn != NULL) {
		/*
		 * We created this handle locally so we need to close it
		 * and clean it up.  Unfortunately, it's holding transactional
		 * locks that need to persist until the end of transaction.
		 * If we invalidate the locker id (dbp->lid), then the close
		 * won't free these locks prematurely.
		 */
		 dbp->lid = DB_LOCK_INVALIDID;
	}

	if (handle_check)
		__env_db_rep_exit(dbenv);

err:	if (txn_local)
		ret = __db_txn_auto_resolve(dbenv, txn, 0, ret);

	/*
	 * We never opened this dbp for real, so don't include a transaction
	 * handle, and use NOSYNC to avoid calling into mpool.
	 */
	if ((t_ret = __db_close(dbp, NULL, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_remove_pp
 *	DB->remove pre/post processing.
 *
 * PUBLIC: int __db_remove_pp
 * PUBLIC:     __P((DB *, const char *, const char *, u_int32_t));
 */
int
__db_remove_pp(dbp, name, subdb, flags)
	DB *dbp;
	const char *name, *subdb;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	/*
	 * Validate arguments, continuing to destroy the handle on failure.
	 *
	 * Cannot use DB_ILLEGAL_AFTER_OPEN directly because it returns.
	 *
	 * !!!
	 * We have a serious problem if we're here with a handle used to open
	 * a database -- we'll destroy the handle, and the application won't
	 * ever be able to close the database.
	 */
	if (F_ISSET(dbp, DB_AM_OPEN_CALLED)) {
		ret = __db_mi_open(dbenv, "DB->remove", 1);
		return (ret);
	}

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB->remove", flags, 0)) != 0)
		return (ret);

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, NULL, DB_LOCK_INVALIDID, 0)) != 0)
		return (ret);

	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 1, 0)) != 0)
		return (ret);

	/* Remove the file. */
	ret = __db_remove(dbp, NULL, name, subdb, flags);

	if (handle_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __db_remove
 *	DB->remove method.
 *
 * PUBLIC: int __db_remove
 * PUBLIC:     __P((DB *, DB_TXN *, const char *, const char *, u_int32_t));
 */
int
__db_remove(dbp, txn, name, subdb, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	u_int32_t flags;
{
	int ret, t_ret;

	ret = __db_remove_int(dbp, txn, name, subdb, flags);

	if ((t_ret = __db_close(dbp, txn, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_remove_int
 *	Worker function for the DB->remove method.
 *
 * PUBLIC: int __db_remove_int __P((DB *,
 * PUBLIC:    DB_TXN *, const char *, const char *, u_int32_t));
 */
int
__db_remove_int(dbp, txn, name, subdb, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;
	char *real_name, *tmpname;

	dbenv = dbp->dbenv;
	real_name = tmpname = NULL;

	/* Handle subdatabase removes separately. */
	if (subdb != NULL) {
		ret = __db_subdb_remove(dbp, txn, name, subdb);
		goto err;
	}

	/* Handle transactional file removes separately. */
	if (txn != NULL) {
		ret = __db_dbtxn_remove(dbp, txn, name);
		goto err;
	}

	/*
	 * The remaining case is a non-transactional file remove.
	 *
	 * Find the real name of the file.
	 */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		goto err;

	/*
	 * If force is set, remove the temporary file.  Ignore errors because
	 * the backup file might not exist.
	 */
	if (LF_ISSET(DB_FORCE) &&
	    (ret = __db_backup_name(dbenv, real_name, NULL, &tmpname)) == 0)
		(void)__os_unlink(dbenv, tmpname);

	if ((ret = __fop_remove_setup(dbp, NULL, real_name, 0)) != 0)
		goto err;

	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp, NULL, name, subdb)) != 0)
		goto err;

	ret = __fop_remove(dbenv, NULL, dbp->fileid, name, DB_APP_DATA,
	    F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0);

err:	if (real_name != NULL)
		__os_free(dbenv, real_name);
	if (tmpname != NULL)
		__os_free(dbenv, tmpname);

	return (ret);
}

/*
 * __db_subdb_remove --
 *	Remove a subdatabase.
 */
static int
__db_subdb_remove(dbp, txn, name, subdb)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
{
	DB *mdbp, *sdbp;
	int ret, t_ret;

	mdbp = sdbp = NULL;

	/* Open the subdatabase. */
	if ((ret = db_create(&sdbp, dbp->dbenv, 0)) != 0)
		goto err;
	if ((ret = __db_open(sdbp,
	    txn, name, subdb, DB_UNKNOWN, DB_WRITEOPEN, 0, PGNO_BASE_MD)) != 0)
		goto err;

	DB_TEST_RECOVERY(sdbp, DB_TEST_PREDESTROY, ret, name);

	/* Free up the pages in the subdatabase. */
	switch (sdbp->type) {
		case DB_BTREE:
		case DB_RECNO:
			if ((ret = __bam_reclaim(sdbp, txn)) != 0)
				goto err;
			break;
		case DB_HASH:
			if ((ret = __ham_reclaim(sdbp, txn)) != 0)
				goto err;
			break;
		case DB_QUEUE:
		case DB_UNKNOWN:
		default:
			ret = __db_unknown_type(
			    sdbp->dbenv, "__db_subdb_remove", sdbp->type);
			goto err;
	}

	/*
	 * Remove the entry from the main database and free the subdatabase
	 * metadata page.
	 */
	if ((ret = __db_master_open(sdbp, txn, name, 0, 0, &mdbp)) != 0)
		goto err;

	if ((ret = __db_master_update(
	    mdbp, sdbp, txn, subdb, sdbp->type, MU_REMOVE, NULL, 0)) != 0)
		goto err;

	DB_TEST_RECOVERY(sdbp, DB_TEST_POSTDESTROY, ret, name);

DB_TEST_RECOVERY_LABEL
err:
	/* Close the main and subdatabases. */
	if ((t_ret = __db_close(sdbp, txn, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (mdbp != NULL &&
	    (t_ret = __db_close(mdbp, txn, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

static int
__db_dbtxn_remove(dbp, txn, name)
	DB *dbp;
	DB_TXN *txn;
	const char *name;
{
	DB_ENV *dbenv;
	int ret;
	char *tmpname;

	dbenv = dbp->dbenv;
	tmpname = NULL;

	/*
	 * This is a transactional rename, so we have to keep the name
	 * of the file locked until the transaction commits.  As a result,
	 * we implement remove by renaming the file to some other name
	 * (which creates a dummy named file as a placeholder for the
	 * file being rename/dremoved) and then deleting that file as
	 * a delayed remove at commit.
	 */
	if ((ret = __db_backup_name(dbenv, name, txn, &tmpname)) != 0)
		return (ret);

	DB_TEST_RECOVERY(dbp, DB_TEST_PREDESTROY, ret, name);

	if ((ret = __db_rename_int(dbp, txn, name, NULL, tmpname)) != 0)
		goto err;

	/* The internal removes will also translate into delayed removes. */
	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp, txn, tmpname, NULL)) != 0)
		goto err;

	ret = __fop_remove(dbenv, txn, dbp->fileid, tmpname, DB_APP_DATA,
	    F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0);

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTDESTROY, ret, name);

err:
DB_TEST_RECOVERY_LABEL
	if (tmpname != NULL)
		__os_free(dbenv, tmpname);

	return (ret);
}
