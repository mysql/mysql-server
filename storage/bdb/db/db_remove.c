/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_remove.c,v 11.203 2002/08/19 18:34:18 margo Exp $";
#endif /* not lint */

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

static int __db_subdb_remove __P((DB *, DB_TXN *, const char *, const char *));
static int __db_dbtxn_remove __P((DB *, DB_TXN *, const char *));

/*
 * __dbenv_dbremove
 *	Remove method for DB_ENV.
 *
 * PUBLIC: int __dbenv_dbremove __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, const char *, u_int32_t));
 */
int
__dbenv_dbremove(dbenv, txn, name, subdb, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name, *subdb;
	u_int32_t flags;
{
	DB *dbp;
	int ret, t_ret, txn_local;

	txn_local = 0;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->dbremove");

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB->remove", flags, DB_AUTO_COMMIT)) != 0)
		return (ret);

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
		return (ret);

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto(dbp, &txn)) != 0)
			return (ret);
		txn_local = 1;
	} else
		if (txn != NULL && !TXN_ON(dbenv))
			return (__db_not_txn_env(dbenv));

	ret = __db_remove_i(dbp, txn, name, subdb);

	/* Commit for DB_AUTO_COMMIT. */
	if (txn_local) {
		if (ret == 0)
			ret = txn->commit(txn, 0);
		else
			if ((t_ret = txn->abort(txn)) != 0)
				ret = __db_panic(dbenv, t_ret);
		/*
		 * We created the DBP here and when we committed/aborted,
		 * we release all the tranasctional locks, which includes
		 * the handle lock; mark the handle cleared explicitly.
		 */
		LOCK_INIT(dbp->handle_lock);
		dbp->lid = DB_LOCK_INVALIDID;
	}

	/*
	 * We never opened this dbp for real, so don't call the transactional
	 * version of DB->close, and use NOSYNC to avoid calling into mpool.
	 */
	if ((t_ret = dbp->close(dbp, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_remove
 *	Remove method for DB.
 *
 * PUBLIC: int __db_remove __P((DB *, const char *, const char *, u_int32_t));
 */
int
__db_remove(dbp, name, subdb, flags)
	DB *dbp;
	const char *name, *subdb;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret, t_ret;

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
		goto err;
	}

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "DB->remove", flags, 0)) != 0)
		goto err;

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, NULL, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;

	/* Remove the file. */
	ret = __db_remove_i(dbp, NULL, name, subdb);

	/*
	 * We never opened this dbp for real, use NOSYNC to avoid calling into
	 * mpool.
	 */
err:	if ((t_ret = dbp->close(dbp, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_remove_i
 *	Internal remove method for DB.
 *
 * PUBLIC: int __db_remove_i __P((DB *, DB_TXN *, const char *, const char *));
 */
int
__db_remove_i(dbp, txn, name, subdb)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
{
	DB_ENV *dbenv;
	DB_LSN newlsn;
	int ret;
	char *real_name;

	dbenv = dbp->dbenv;
	real_name = NULL;

	/* Handle subdatabase removes separately. */
	if (subdb != NULL)
		return (__db_subdb_remove(dbp, txn, name, subdb));

	/* Handle transactional file removes separately. */
	if (txn != NULL)
		return (__db_dbtxn_remove(dbp, txn, name));

	/*
	 * The remaining case is a non-transactional file remove.
	 *
	 * Find the real name of the file.
	 */
	if ((ret = __db_appname(dbenv,
	    DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		return (ret);

	if ((ret = __fop_remove_setup(dbp, NULL, real_name, 0)) != 0)
		goto err;

	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp, NULL, name, subdb, &newlsn)) != 0)
		goto err;

	ret = __fop_remove(dbenv, NULL, dbp->fileid, name, DB_APP_DATA);

err:
	if (real_name != NULL)
		__os_free(dbenv, real_name);

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
	    txn, name, subdb, DB_UNKNOWN, DB_WRITEOPEN, 0)) != 0)
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
	if ((t_ret = __db_close_i(sdbp, txn, 0)) != 0 && ret == 0)
		ret = t_ret;

	if (mdbp != NULL &&
	    (t_ret = __db_close_i(mdbp, txn, 0)) != 0 && ret == 0)
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
	DB_LSN newlsn;
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

	if ((ret = __db_rename_i(dbp, txn, name, NULL, tmpname)) != 0)
		goto err;

	/* The internal removes will also translate into delayed removes. */
	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp, txn, tmpname, NULL, &newlsn)) != 0)
		goto err;

	ret = __fop_remove(dbenv, txn, dbp->fileid, tmpname, DB_APP_DATA);

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTDESTROY, ret, name);

err:
DB_TEST_RECOVERY_LABEL
	if (tmpname != NULL)
		__os_free(dbenv, tmpname);

	return (ret);
}
