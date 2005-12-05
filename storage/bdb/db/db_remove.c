/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_remove.c,v 12.16 2005/10/27 01:25:53 mjc Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/fop.h"
#include "dbinc/btree.h"
#include "dbinc/hash.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int __db_dbtxn_remove __P((DB *, DB_TXN *, const char *, const char *));
static int __db_subdb_remove __P((DB *, DB_TXN *, const char *, const char *));

/*
 * __env_dbremove_pp
 *	DB_ENV->dbremove pre/post processing.
 *
 * PUBLIC: int __env_dbremove_pp __P((DB_ENV *,
 * PUBLIC:     DB_TXN *, const char *, const char *, u_int32_t));
 */
int
__env_dbremove_pp(dbenv, txn, name, subdb, flags)
	DB_ENV *dbenv;
	DB_TXN *txn;
	const char *name, *subdb;
	u_int32_t flags;
{
	DB *dbp;
	DB_THREAD_INFO *ip;
	int handle_check, ret, t_ret, txn_local;

	dbp = NULL;
	txn_local = 0;

	PANIC_CHECK(dbenv);
	ENV_ILLEGAL_BEFORE_OPEN(dbenv, "DB_ENV->dbremove");

	/*
	 * The actual argument checking is simple, do it inline, outside of
	 * the replication block.
	 */
	if ((ret = __db_fchk(dbenv, "DB->remove", flags, DB_AUTO_COMMIT)) != 0)
		return (ret);

	ENV_ENTER(dbenv, ip);

	/* Check for replication block. */
	handle_check = IS_ENV_REPLICATED(dbenv);
	if (handle_check && (ret = __env_rep_enter(dbenv, 1)) != 0) {
		handle_check = 0;
		goto err;
	}

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	if (IS_ENV_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
			goto err;
		txn_local = 1;
	} else
		if (txn != NULL && !TXN_ON(dbenv)) {
			ret = __db_not_txn_env(dbenv);
			goto err;
		}
	LF_CLR(DB_AUTO_COMMIT);

	if ((ret = db_create(&dbp, dbenv, 0)) != 0)
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

err:	if (txn_local && (t_ret =
	    __db_txn_auto_resolve(dbenv, txn, 0, ret)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * We never opened this dbp for real, so don't include a transaction
	 * handle, and use NOSYNC to avoid calling into mpool.
	 *
	 * !!!
	 * Note we're reversing the order of operations: we started the txn and
	 * then opened the DB handle; we're resolving the txn and then closing
	 * closing the DB handle -- it's safer.
	 */
	if (dbp != NULL &&
	    (t_ret = __db_close(dbp, NULL, DB_NOSYNC)) != 0 && ret == 0)
		ret = t_ret;

	if (handle_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

	ENV_LEAVE(dbenv, ip);
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
	DB_THREAD_INFO *ip;
	int handle_check, ret, t_ret;

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

	ENV_ENTER(dbenv, ip);

	handle_check = IS_ENV_REPLICATED(dbenv);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 1, 0)) != 0) {
		handle_check = 0;
		goto err;
	}

	/* Remove the file. */
	ret = __db_remove(dbp, NULL, name, subdb, flags);

	if (handle_check && (t_ret = __env_db_rep_exit(dbenv)) != 0 && ret == 0)
		ret = t_ret;

err:	ENV_LEAVE(dbenv, ip);
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

	if (name == NULL && subdb == NULL) {
		__db_err(dbenv, "Remove on temporary files invalid");
		ret = EINVAL;
		goto err;
	}

	if (name == NULL) {
		MAKE_INMEM(dbp);
		real_name = (char *)subdb;
	} else if (subdb != NULL) {
		ret = __db_subdb_remove(dbp, txn, name, subdb);
		goto err;
	}

	/* Handle transactional file removes separately. */
	if (txn != NULL) {
		ret = __db_dbtxn_remove(dbp, txn, name, subdb);
		goto err;
	}

	/*
	 * The remaining case is a non-transactional file remove.
	 *
	 * Find the real name of the file.
	 */
	if (!F_ISSET(dbp, DB_AM_INMEM) && (ret =
	    __db_appname(dbenv, DB_APP_DATA, name, 0, NULL, &real_name)) != 0)
		goto err;

	/*
	 * If this is a file and force is set, remove the temporary file, which
	 * may have been left around.  Ignore errors because the temporary file
	 * might not exist.
	 */
	if (!F_ISSET(dbp, DB_AM_INMEM) && LF_ISSET(DB_FORCE) &&
	    (ret = __db_backup_name(dbenv, real_name, NULL, &tmpname)) == 0)
		(void)__os_unlink(dbenv, tmpname);

	if ((ret = __fop_remove_setup(dbp, NULL, real_name, 0)) != 0)
		goto err;

	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp, NULL, name, subdb)) != 0)
		goto err;

	ret = F_ISSET(dbp, DB_AM_INMEM) ?
	    __db_inmem_remove(dbp, NULL, real_name) :
	    __fop_remove(dbenv, NULL, dbp->fileid, name, DB_APP_DATA,
	    F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0);

err:	if (!F_ISSET(dbp, DB_AM_INMEM) && real_name != NULL)
		__os_free(dbenv, real_name);
	if (tmpname != NULL)
		__os_free(dbenv, tmpname);

	return (ret);
}

/*
 * __db_inmem_remove --
 *	Removal of a named in-memory database.
 * PUBLIC: int __db_inmem_remove __P((DB *, DB_TXN *, const char *));
 */
int
__db_inmem_remove(dbp, txn, name)
	DB *dbp;
	DB_TXN *txn;
	const char *name;
{
	DB_ENV *dbenv;
	DB_LSN lsn;
	DBT fid_dbt, name_dbt;
	u_int32_t locker;
	int ret;

	dbenv = dbp->dbenv;
	locker = DB_LOCK_INVALIDID;

	DB_ASSERT(name != NULL);

	/* This had better exist if we are trying to do a remove. */
	(void)__memp_set_flags(dbp->mpf, DB_MPOOL_NOFILE, 1);
	if ((ret = __memp_fopen(dbp->mpf, NULL, name, 0, 0, 0)) != 0)
		return (ret);
	if ((ret = __memp_get_fileid(dbp->mpf, dbp->fileid)) != 0)
		goto err;
	dbp->preserve_fid = 1;

	if (LOCKING_ON(dbenv)) {
		if (dbp->lid == DB_LOCK_INVALIDID &&
		    (ret = __lock_id(dbenv, &dbp->lid, NULL)) != 0)
			goto err;
		locker = txn == NULL ? dbp->lid : txn->txnid;
	}

	/*
	 * In a transactional environment, we'll play the same game
	 * that we play for databases in the file system -- create a
	 * temporary database and put it in with the current name
	 * and then rename this one to another name.  We'll then use
	 * a commit-time event to remove the entry.
	 */

	if ((ret = __fop_lock_handle(dbenv,
	    dbp, locker, DB_LOCK_WRITE, NULL, 0)) != 0)
		goto err;

	if (LOGGING_ON(dbenv)) {
		memset(&fid_dbt, 0, sizeof(fid_dbt));
		fid_dbt.data = dbp->fileid;
		fid_dbt.size = DB_FILE_ID_LEN;
		memset(&name_dbt, 0, sizeof(name_dbt));
		name_dbt.data = (void *)name;
		name_dbt.size = (u_int32_t)strlen(name) + 1;

		if (txn != NULL && (ret =
		    __txn_remevent(dbenv, txn, name, dbp->fileid, 1)) != 0)
			goto err;

		if ((ret = __crdel_inmem_remove_log(dbenv,
		    txn, &lsn, 0, &name_dbt, &fid_dbt)) != 0)
			goto err;
	}

	if (txn == NULL)
		ret = __memp_nameop(dbenv, dbp->fileid, NULL, name, NULL, 1);

err:	return (ret);
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
__db_dbtxn_remove(dbp, txn, name, subdb)
	DB *dbp;
	DB_TXN *txn;
	const char *name, *subdb;
{
	DB_ENV *dbenv;
	int ret;
	char *tmpname;

	dbenv = dbp->dbenv;
	tmpname = NULL;

	/*
	 * This is a transactional remove, so we have to keep the name
	 * of the file locked until the transaction commits.  As a result,
	 * we implement remove by renaming the file to some other name
	 * (which creates a dummy named file as a placeholder for the
	 * file being rename/dremoved) and then deleting that file as
	 * a delayed remove at commit.
	 */
	if ((ret = __db_backup_name(dbenv,
	    F_ISSET(dbp, DB_AM_INMEM) ? subdb : name, txn, &tmpname)) != 0)
		return (ret);

	DB_TEST_RECOVERY(dbp, DB_TEST_PREDESTROY, ret, name);

	if ((ret = __db_rename_int(dbp, txn, name, subdb, tmpname)) != 0)
		goto err;

	/*
	 * The internal removes will also translate into delayed removes.
	 */
	if (dbp->db_am_remove != NULL &&
	    (ret = dbp->db_am_remove(dbp, txn, tmpname, NULL)) != 0)
		goto err;

	ret = F_ISSET(dbp, DB_AM_INMEM) ?
	     __db_inmem_remove(dbp, txn, tmpname) :
	    __fop_remove(dbenv, txn, dbp->fileid, tmpname, DB_APP_DATA,
	    F_ISSET(dbp, DB_AM_NOT_DURABLE) ? DB_LOG_NOT_DURABLE : 0);

	DB_TEST_RECOVERY(dbp, DB_TEST_POSTDESTROY, ret, name);

err:
DB_TEST_RECOVERY_LABEL
	if (tmpname != NULL)
		__os_free(dbenv, tmpname);

	return (ret);
}
