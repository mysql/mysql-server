/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_iface.c,v 11.121 2004/10/07 17:33:32 sue Exp $
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
#include "dbinc/hash.h"			/* For __db_no_hash_am(). */
#include "dbinc/qam.h"			/* For __db_no_queue_am(). */
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

static int __db_associate_arg __P((DB *, DB *,
	       int (*)(DB *, const DBT *, const DBT *, DBT *), u_int32_t));
static int __db_c_get_arg __P((DBC *, DBT *, DBT *, u_int32_t));
static int __db_c_pget_arg __P((DBC *, DBT *, u_int32_t));
static int __db_c_put_arg __P((DBC *, DBT *, DBT *, u_int32_t));
static int __db_curinval __P((const DB_ENV *));
static int __db_cursor_arg __P((DB *, u_int32_t));
static int __db_del_arg __P((DB *, u_int32_t));
static int __db_get_arg __P((const DB *, const DBT *, DBT *, u_int32_t));
static int __db_join_arg __P((DB *, DBC **, u_int32_t));
static int __db_open_arg __P((DB *,
	       DB_TXN *, const char *, const char *, DBTYPE, u_int32_t));
static int __db_pget_arg __P((DB *, DBT *, u_int32_t));
static int __db_put_arg __P((DB *, DBT *, DBT *, u_int32_t));
static int __db_rdonly __P((const DB_ENV *, const char *));
static int __dbt_ferr __P((const DB *, const char *, const DBT *, int));

/*
 * A database should be required to be readonly if it's been explicitly
 * specified as such or if we're a client in a replicated environment and
 * we don't have the special "client-writer" designation.
 */
#define	IS_READONLY(dbp)						\
    (F_ISSET(dbp, DB_AM_RDONLY) ||					\
    (IS_REP_CLIENT((dbp)->dbenv) &&					\
    !F_ISSET((dbp), DB_AM_CL_WRITER)))

/*
 * These functions implement the Berkeley DB API.  They are organized in a
 * layered fashion.  The interface functions (XXX_pp) perform all generic
 * error checks (for example, PANIC'd region, replication state change
 * in progress, inconsistent transaction usage), call function-specific
 * check routines (_arg) to check for proper flag usage, etc., do pre-amble
 * processing (incrementing handle counts, handling auto-commit), call the
 * function and then do post-amble processing (DB_AUTO_COMMIT, dec handle
 * counts).
 *
 * So, the basic structure is:
 *	Check for generic errors
 *	Call function-specific check routine
 *	Increment handle count
 *	Create internal transaction if necessary
 *	Call underlying worker function
 *	Commit/abort internal transaction if necessary
 *	Decrement handle count
 */

/*
 * __db_associate_pp --
 *	DB->associate pre/post processing.
 *
 * PUBLIC: int __db_associate_pp __P((DB *, DB_TXN *, DB *,
 * PUBLIC:     int (*)(DB *, const DBT *, const DBT *, DBT *), u_int32_t));
 */
int
__db_associate_pp(dbp, txn, sdbp, callback, flags)
	DB *dbp, *sdbp;
	DB_TXN *txn;
	int (*callback) __P((DB *, const DBT *, const DBT *, DBT *));
	u_int32_t flags;
{
	DBC *sdbc;
	DB_ENV *dbenv;
	int handle_check, ret, txn_local;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __db_associate_arg(dbp, sdbp, callback, flags)) != 0)
		return (ret);

	/*
	 * Secondary cursors may have the primary's lock file ID, so we need
	 * to make sure that no older cursors are lying around when we make
	 * the transition.
	 */
	if (TAILQ_FIRST(&sdbp->active_queue) != NULL ||
	    TAILQ_FIRST(&sdbp->join_queue) != NULL) {
		__db_err(dbenv,
    "Databases may not become secondary indices while cursors are open");
		return (EINVAL);
	}

	/*
	 * Create a local transaction as necessary, check for consistent
	 * transaction usage, and, if we have no transaction but do have
	 * locking on, acquire a locker id for the handle lock acquisition.
	 */
	txn_local = 0;
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
			return (ret);
		txn_local = 1;
		LF_CLR(DB_AUTO_COMMIT);
	} else if (txn != NULL && !TXN_ON(dbenv))
		return (__db_not_txn_env(dbenv));

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		goto err;

	while ((sdbc = TAILQ_FIRST(&sdbp->free_queue)) != NULL)
		if ((ret = __db_c_destroy(sdbc)) != 0)
			break;

	if (ret == 0)
		ret = __db_associate(dbp, txn, sdbp, callback, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

err:	return (txn_local ? __db_txn_auto_resolve(dbenv, txn, 0, ret) : ret);
}

/*
 * __db_associate_arg --
 *	Check DB->associate arguments.
 */
static int
__db_associate_arg(dbp, sdbp, callback, flags)
	DB *dbp, *sdbp;
	int (*callback) __P((DB *, const DBT *, const DBT *, DBT *));
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	if (F_ISSET(sdbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "Secondary index handles may not be re-associated");
		return (EINVAL);
	}
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "Secondary indices may not be used as primary databases");
		return (EINVAL);
	}
	if (F_ISSET(dbp, DB_AM_DUP)) {
		__db_err(dbenv,
		    "Primary databases may not be configured with duplicates");
		return (EINVAL);
	}
	if (F_ISSET(dbp, DB_AM_RENUMBER)) {
		__db_err(dbenv,
	    "Renumbering recno databases may not be used as primary databases");
		return (EINVAL);
	}
	if (dbp->dbenv != sdbp->dbenv &&
	    (!F_ISSET(dbp->dbenv, DB_ENV_DBLOCAL) ||
	     !F_ISSET(sdbp->dbenv, DB_ENV_DBLOCAL))) {
		__db_err(dbenv,
	    "The primary and secondary must be opened in the same environment");
		return (EINVAL);
	}
	if ((DB_IS_THREADED(dbp) && !DB_IS_THREADED(sdbp)) ||
	    (!DB_IS_THREADED(dbp) && DB_IS_THREADED(sdbp))) {
		__db_err(dbenv,
	    "The DB_THREAD setting must be the same for primary and secondary");
		return (EINVAL);
	}
	if (callback == NULL &&
	    (!F_ISSET(dbp, DB_AM_RDONLY) || !F_ISSET(sdbp, DB_AM_RDONLY))) {
		__db_err(dbenv,
    "Callback function may be NULL only when database handles are read-only");
		return (EINVAL);
	}

	if ((ret = __db_fchk(dbenv,
	    "DB->associate", flags, DB_CREATE | DB_AUTO_COMMIT)) != 0)
		return (ret);

	return (0);
}

/*
 * __db_close_pp --
 *	DB->close pre/post processing.
 *
 * PUBLIC: int __db_close_pp __P((DB *, u_int32_t));
 */
int
__db_close_pp(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret, t_ret;

	dbenv = dbp->dbenv;
	ret = 0;

	PANIC_CHECK(dbenv);

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 *
	 * Validate arguments and complain if they're wrong, but as a DB
	 * handle destructor, we can't fail.
	 */
	if (flags != 0 && flags != DB_NOSYNC &&
	    (t_ret = __db_ferr(dbenv, "DB->close", 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (t_ret = __db_rep_enter(dbp, 0, 0, 0)) != 0) {
		handle_check = 0;
		if (ret == 0)
			ret = t_ret;
	}

	if ((t_ret = __db_close(dbp, NULL, flags)) != 0 && ret == 0)
		ret = t_ret;

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_cursor_pp --
 *	DB->cursor pre/post processing.
 *
 * PUBLIC: int __db_cursor_pp __P((DB *, DB_TXN *, DBC **, u_int32_t));
 */
int
__db_cursor_pp(dbp, txn, dbcp, flags)
	DB *dbp;
	DB_TXN *txn;
	DBC **dbcp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->cursor");

	if ((ret = __db_cursor_arg(dbp, flags)) != 0)
		return (ret);

	/*
	 * Check for consistent transaction usage.  For now, assume that
	 * this cursor might be used for read operations only (in which
	 * case it may not require a txn).  We'll check more stringently
	 * in c_del and c_put.  (Note that this all means that the
	 * read-op txn tests have to be a subset of the write-op ones.)
	 */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 1)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		return (ret);

	ret = __db_cursor(dbp, txn, dbcp, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_cursor --
 *	DB->cursor.
 *
 * PUBLIC: int __db_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
 */
int
__db_cursor(dbp, txn, dbcp, flags)
	DB *dbp;
	DB_TXN *txn;
	DBC **dbcp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DBC *dbc;
	db_lockmode_t mode;
	u_int32_t op;
	int ret;

	dbenv = dbp->dbenv;

	if ((ret = __db_cursor_int(dbp,
	    txn, dbp->type, PGNO_INVALID, 0, DB_LOCK_INVALIDID, &dbc)) != 0)
		return (ret);

	/*
	 * If this is CDB, do all the locking in the interface, which is
	 * right here.
	 */
	if (CDB_LOCKING(dbenv)) {
		op = LF_ISSET(DB_OPFLAGS_MASK);
		mode = (op == DB_WRITELOCK) ? DB_LOCK_WRITE :
		    ((op == DB_WRITECURSOR) ? DB_LOCK_IWRITE : DB_LOCK_READ);
		if ((ret = __lock_get(dbenv, dbc->locker, 0,
		    &dbc->lock_dbt, mode, &dbc->mylock)) != 0)
			goto err;
		if (op == DB_WRITECURSOR)
			F_SET(dbc, DBC_WRITECURSOR);
		if (op == DB_WRITELOCK)
			F_SET(dbc, DBC_WRITER);
	}

	if (LF_ISSET(DB_DIRTY_READ) ||
	    (txn != NULL && F_ISSET(txn, TXN_DIRTY_READ)))
		F_SET(dbc, DBC_DIRTY_READ);

	if (LF_ISSET(DB_DEGREE_2) ||
	    (txn != NULL && F_ISSET(txn, TXN_DEGREE_2)))
		F_SET(dbc, DBC_DEGREE_2);

	*dbcp = dbc;
	return (0);

err:	(void)__db_c_close(dbc);
	return (ret);
}

/*
 * __db_cursor_arg --
 *	Check DB->cursor arguments.
 */
static int
__db_cursor_arg(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	/*
	 * DB_DIRTY_READ  and DB_DGREE_2 are the only valid bit-flags
	 * and requires locking.
	 */
	if (LF_ISSET(DB_DIRTY_READ | DB_DEGREE_2)) {
		if (!LOCKING_ON(dbenv))
			return (__db_fnl(dbenv, "DB->cursor"));
		LF_CLR(DB_DIRTY_READ | DB_DEGREE_2);
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	case DB_WRITECURSOR:
		if (IS_READONLY(dbp))
			return (__db_rdonly(dbenv, "DB->cursor"));
		if (!CDB_LOCKING(dbenv))
			return (__db_ferr(dbenv, "DB->cursor", 0));
		break;
	case DB_WRITELOCK:
		if (IS_READONLY(dbp))
			return (__db_rdonly(dbenv, "DB->cursor"));
		break;
	default:
		return (__db_ferr(dbenv, "DB->cursor", 0));
	}

	return (0);
}

/*
 * __db_del_pp --
 *	DB->del pre/post processing.
 *
 * PUBLIC: int __db_del_pp __P((DB *, DB_TXN *, DBT *, u_int32_t));
 */
int
__db_del_pp(dbp, txn, key, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret, txn_local;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->del");

	if ((ret = __db_del_arg(dbp, flags)) != 0)
		return (ret);

	/* Create local transaction as necessary. */
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
			return (ret);
		txn_local = 1;
		LF_CLR(DB_AUTO_COMMIT);
	} else
		txn_local = 0;

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		goto err;

	ret = __db_del(dbp, txn, key, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

err:	return (txn_local ? __db_txn_auto_resolve(dbenv, txn, 0, ret) : ret);
}

/*
 * __db_del_arg --
 *	Check DB->delete arguments.
 */
static int
__db_del_arg(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;

	dbenv = dbp->dbenv;

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbenv, "DB->del"));

	/* Check for invalid function flags. */
	LF_CLR(DB_AUTO_COMMIT);
	switch (flags) {
	case 0:
		break;
	default:
		return (__db_ferr(dbenv, "DB->del", 0));
	}

	return (0);
}

/*
 * db_fd_pp --
 *	DB->fd pre/post processing.
 *
 * PUBLIC: int __db_fd_pp __P((DB *, int *));
 */
int
__db_fd_pp(dbp, fdp)
	DB *dbp;
	int *fdp;
{
	DB_ENV *dbenv;
	DB_FH *fhp;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->fd");

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, 0)) != 0)
		return (ret);

	/*
	 * !!!
	 * There's no argument checking to be done.
	 *
	 * !!!
	 * The actual method call is simple, do it inline.
	 *
	 * XXX
	 * Truly spectacular layering violation.
	 */
	if ((ret = __mp_xxx_fh(dbp->mpf, &fhp)) != 0)
		goto err;

	if (fhp == NULL) {
		*fdp = -1;
		__db_err(dbenv,
		    "Database does not have a valid file handle");
		ret = ENOENT;
	} else
		*fdp = fhp->fd;

err:	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_get_pp --
 *	DB->get pre/post processing.
 *
 * PUBLIC: int __db_get_pp __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__db_get_pp(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	u_int32_t mode;
	int handle_check, ret, txn_local;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->get");

	if ((ret = __db_get_arg(dbp, key, data, flags)) != 0)
		return (ret);

	mode = 0;
	txn_local = 0;
	if (LF_ISSET(DB_DIRTY_READ))
		mode = DB_DIRTY_READ;
	else if ((flags & DB_OPFLAGS_MASK) == DB_CONSUME ||
	    (flags & DB_OPFLAGS_MASK) == DB_CONSUME_WAIT) {
		mode = DB_WRITELOCK;
		if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
			if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
				return (ret);
			txn_local = 1;
			LF_CLR(DB_AUTO_COMMIT);
		}
	}

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID,
	    mode == DB_WRITELOCK || LF_ISSET(DB_RMW) ? 0 : 1)) != 0)
		goto err;

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		goto err;

	ret = __db_get(dbp, txn, key, data, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

err:	return (txn_local ? __db_txn_auto_resolve(dbenv, txn, 0, ret) : ret);
}

/*
 * __db_get --
 *	DB->get.
 *
 * PUBLIC: int __db_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__db_get(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DBC *dbc;
	u_int32_t mode;
	int ret, t_ret;

	mode = 0;
	if (LF_ISSET(DB_DIRTY_READ)) {
		mode = DB_DIRTY_READ;
		LF_CLR(DB_DIRTY_READ);
	} else if (LF_ISSET(DB_DEGREE_2)) {
		mode = DB_DEGREE_2;
		LF_CLR(DB_DEGREE_2);
	} else if ((flags & DB_OPFLAGS_MASK) == DB_CONSUME ||
	    (flags & DB_OPFLAGS_MASK) == DB_CONSUME_WAIT)
		mode = DB_WRITELOCK;

	if ((ret = __db_cursor(dbp, txn, &dbc, mode)) != 0)
		return (ret);

	DEBUG_LREAD(dbc, txn, "DB->get", key, NULL, flags);

	/*
	 * The DBC_TRANSIENT flag indicates that we're just doing a
	 * single operation with this cursor, and that in case of
	 * error we don't need to restore it to its old position--we're
	 * going to close it right away.  Thus, we can perform the get
	 * without duplicating the cursor, saving some cycles in this
	 * common case.
	 */
	F_SET(dbc, DBC_TRANSIENT);

	/*
	 * SET_RET_MEM indicates that if key and/or data have no DBT
	 * flags set and DB manages the returned-data memory, that memory
	 * will belong to this handle, not to the underlying cursor.
	 */
	SET_RET_MEM(dbc, dbp);

	if (LF_ISSET(~(DB_RMW | DB_MULTIPLE)) == 0)
		LF_SET(DB_SET);

	ret = __db_c_get(dbc, key, data, flags);

	if (dbc != NULL && (t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_get_arg --
 *	DB->get argument checking, used by both DB->get and DB->pget.
 */
static int
__db_get_arg(dbp, key, data, flags)
	const DB *dbp;
	const DBT *key;
	DBT *data;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int check_thread, dirty, multi, ret;

	dbenv = dbp->dbenv;

	/*
	 * Check for read-modify-write validity.  DB_RMW doesn't make sense
	 * with CDB cursors since if you're going to write the cursor, you
	 * had to create it with DB_WRITECURSOR.  Regardless, we check for
	 * LOCKING_ON and not STD_LOCKING, as we don't want to disallow it.
	 * If this changes, confirm that DB does not itself set the DB_RMW
	 * flag in a path where CDB may have been configured.
	 */
	check_thread = dirty = 0;
	if (LF_ISSET(DB_DIRTY_READ | DB_RMW | DB_DEGREE_2)) {
		if (!LOCKING_ON(dbenv))
			return (__db_fnl(dbenv, "DB->get"));
		dirty = LF_ISSET(DB_DIRTY_READ | DB_DEGREE_2);
		if ((ret = __db_fcchk(dbenv,
		    "DB->get", flags, DB_DIRTY_READ, DB_DEGREE_2)) != 0)
			return (ret);
		LF_CLR(DB_DIRTY_READ | DB_RMW | DB_DEGREE_2);
	}

	multi = 0;
	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		if (LF_ISSET(DB_MULTIPLE_KEY))
			goto multi_err;
		multi = LF_ISSET(DB_MULTIPLE) ? 1 : 0;
		LF_CLR(DB_MULTIPLE);
	}

	if (LF_ISSET(DB_AUTO_COMMIT)) {
		LF_CLR(DB_AUTO_COMMIT);
		if (flags != DB_CONSUME && flags != DB_CONSUME_WAIT)
			goto err;
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
	case DB_GET_BOTH:
		break;
	case DB_SET_RECNO:
		check_thread = 1;
		if (!F_ISSET(dbp, DB_AM_RECNUM))
			goto err;
		break;
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		check_thread = 1;
		if (dirty) {
			__db_err(dbenv,
    "%s is not supported with DB_CONSUME or DB_CONSUME_WAIT",
			     LF_ISSET(DB_DIRTY_READ) ?
			     "DB_DIRTY_READ" : "DB_DEGREE_2");
			return (EINVAL);
		}
		if (multi)
multi_err:		return (__db_ferr(dbenv, "DB->get", 1));
		if (dbp->type == DB_QUEUE)
			break;
		/* FALLTHROUGH */
	default:
err:		return (__db_ferr(dbenv, "DB->get", 0));
	}

	/*
	 * Check for invalid key/data flags.
	 *
	 * XXX: Dave Krinsky
	 * Remember to modify this when we fix the flag-returning problem.
	 */
	if ((ret = __dbt_ferr(dbp, "key", key, check_thread)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 1)) != 0)
		return (ret);

	if (multi) {
		if (!F_ISSET(data, DB_DBT_USERMEM)) {
			__db_err(dbenv,
			    "DB_MULTIPLE requires DB_DBT_USERMEM be set");
			return (EINVAL);
		}
		if (F_ISSET(key, DB_DBT_PARTIAL) ||
		    F_ISSET(data, DB_DBT_PARTIAL)) {
			__db_err(dbenv,
			    "DB_MULTIPLE does not support DB_DBT_PARTIAL");
			return (EINVAL);
		}
		if (data->ulen < 1024 ||
		    data->ulen < dbp->pgsize || data->ulen % 1024 != 0) {
			__db_err(dbenv, "%s%s",
			    "DB_MULTIPLE buffers must be ",
			    "aligned, at least page size and multiples of 1KB");
			return (EINVAL);
		}
	}

	return (0);
}

/*
 * __db_join_pp --
 *	DB->join pre/post processing.
 *
 * PUBLIC: int __db_join_pp __P((DB *, DBC **, DBC **, u_int32_t));
 */
int
__db_join_pp(primary, curslist, dbcp, flags)
	DB *primary;
	DBC **curslist, **dbcp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = primary->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __db_join_arg(primary, curslist, flags)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, primary);
	if (handle_check && (ret =
	    __db_rep_enter(primary, 1, 0, curslist[0]->txn != NULL)) != 0)
		return (ret);

	ret = __db_join(primary, curslist, dbcp, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_join_arg --
 *	Check DB->join arguments.
 */
static int
__db_join_arg(primary, curslist, flags)
	DB *primary;
	DBC **curslist;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB_TXN *txn;
	int i;

	dbenv = primary->dbenv;

	switch (flags) {
	case 0:
	case DB_JOIN_NOSORT:
		break;
	default:
		return (__db_ferr(dbenv, "DB->join", 0));
	}

	if (curslist == NULL || curslist[0] == NULL) {
		__db_err(dbenv,
	    "At least one secondary cursor must be specified to DB->join");
		return (EINVAL);
	}

	txn = curslist[0]->txn;
	for (i = 1; curslist[i] != NULL; i++)
		if (curslist[i]->txn != txn) {
			__db_err(dbenv,
		    "All secondary cursors must share the same transaction");
			return (EINVAL);
		}

	return (0);
}

/*
 * __db_key_range_pp --
 *	DB->key_range pre/post processing.
 *
 * PUBLIC: int __db_key_range_pp
 * PUBLIC:     __P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t));
 */
int
__db_key_range_pp(dbp, txn, key, kr, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key;
	DB_KEY_RANGE *kr;
	u_int32_t flags;
{
	DBC *dbc;
	DB_ENV *dbenv;
	int handle_check, ret, t_ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->key_range");

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 */
	if (flags != 0)
		return (__db_ferr(dbenv, "DB->key_range", 0));

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 1)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		return (ret);

	/*
	 * !!!
	 * The actual method call is simple, do it inline.
	 */
	switch (dbp->type) {
	case DB_BTREE:
		/* Acquire a cursor. */
		if ((ret = __db_cursor(dbp, txn, &dbc, 0)) != 0)
			break;

		DEBUG_LWRITE(dbc, NULL, "bam_key_range", NULL, NULL, 0);

		ret = __bam_key_range(dbc, key, kr, flags);

		if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
			ret = t_ret;
		break;
	case DB_HASH:
	case DB_QUEUE:
	case DB_RECNO:
		ret = __dbh_am_chk(dbp, DB_OK_BTREE);
		break;
	case DB_UNKNOWN:
	default:
		ret = __db_unknown_type(dbenv, "DB->key_range", dbp->type);
		break;
	}

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_open_pp --
 *	DB->open pre/post processing.
 *
 * PUBLIC: int __db_open_pp __P((DB *, DB_TXN *,
 * PUBLIC:     const char *, const char *, DBTYPE, u_int32_t, int));
 */
int
__db_open_pp(dbp, txn, fname, dname, type, flags, mode)
	DB *dbp;
	DB_TXN *txn;
	const char *fname, *dname;
	DBTYPE type;
	u_int32_t flags;
	int mode;
{
	DB_ENV *dbenv;
	int handle_check, nosync, remove_me, ret, txn_local;

	dbenv = dbp->dbenv;
	nosync = 1;
	remove_me = 0;

	PANIC_CHECK(dbenv);

	if ((ret = __db_open_arg(dbp, txn, fname, dname, type, flags)) != 0)
		return (ret);

	/*
	 * Save the file and database names and flags.  We do this here
	 * because we don't pass all of the flags down into the actual
	 * DB->open method call, we strip DB_AUTO_COMMIT at this layer.
	 */
	if ((fname != NULL &&
	    (ret = __os_strdup(dbenv, fname, &dbp->fname)) != 0) ||
	    (dname != NULL &&
	    (ret = __os_strdup(dbenv, dname, &dbp->dname)) != 0))
		return (ret);
	dbp->open_flags = flags;

	/* Save the current DB handle flags for refresh. */
	dbp->orig_flags = dbp->flags;

	/*
	 * Create local transaction as necessary, check for consistent
	 * transaction usage.
	 */
	txn_local = 0;
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
			return (ret);
		txn_local = 1;
		LF_CLR(DB_AUTO_COMMIT);
	} else
		if (txn != NULL && !TXN_ON(dbenv))
			return (__db_not_txn_env(dbenv));

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0) {
		handle_check = 0;
		goto err;
	}

	if ((ret = __db_open(dbp,
	    txn, fname, dname, type, flags, mode, PGNO_BASE_MD)) != 0)
		goto err;

	/*
	 * You can open the database that describes the subdatabases in the
	 * rest of the file read-only.  The content of each key's data is
	 * unspecified and applications should never be adding new records
	 * or updating existing records.  However, during recovery, we need
	 * to open these databases R/W so we can redo/undo changes in them.
	 * Likewise, we need to open master databases read/write during
	 * rename and remove so we can be sure they're fully sync'ed, so
	 * we provide an override flag for the purpose.
	 */
	if (dname == NULL && !IS_RECOVERING(dbenv) && !LF_ISSET(DB_RDONLY) &&
	    !LF_ISSET(DB_RDWRMASTER) && F_ISSET(dbp, DB_AM_SUBDB)) {
		__db_err(dbenv,
    "files containing multiple databases may only be opened read-only");
		ret = EINVAL;
		goto err;
	}

	/*
	 * Success: file creations have to be synchronous, otherwise we don't
	 * care.
	 */
	if (F_ISSET(dbp, DB_AM_CREATED | DB_AM_CREATED_MSTR))
		nosync = 0;

	/* Success: don't discard the file on close. */
	F_CLR(dbp, DB_AM_DISCARD | DB_AM_CREATED | DB_AM_CREATED_MSTR);

	/*
	 * If not transactional, remove the databases/subdatabases.  If we're
	 * transactional, the child transaction abort cleans up.
	 */
err:	if (ret != 0 && txn == NULL) {
		remove_me = F_ISSET(dbp, DB_AM_CREATED);
		if (F_ISSET(dbp, DB_AM_CREATED_MSTR) ||
		    (dname == NULL && remove_me))
			/* Remove file. */
			(void)__db_remove_int(dbp, txn, fname, NULL, DB_FORCE);
		else if (remove_me)
			/* Remove subdatabase. */
			(void)__db_remove_int(dbp, txn, fname, dname, DB_FORCE);
	}

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (txn_local ?
	    __db_txn_auto_resolve(dbenv, txn, nosync, ret) : ret);
}

/*
 * __db_open_arg --
 *	Check DB->open arguments.
 */
static int
__db_open_arg(dbp, txn, fname, dname, type, flags)
	DB *dbp;
	DB_TXN *txn;
	const char *fname, *dname;
	DBTYPE type;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	u_int32_t ok_flags;
	int ret;

	dbenv = dbp->dbenv;

	/* Validate arguments. */
#undef	OKFLAGS
#define	OKFLAGS								\
    (DB_AUTO_COMMIT | DB_CREATE | DB_DIRTY_READ | DB_EXCL |		\
     DB_FCNTL_LOCKING | DB_NO_AUTO_COMMIT | DB_NOMMAP | DB_RDONLY |	\
     DB_RDWRMASTER | DB_THREAD | DB_TRUNCATE | DB_WRITEOPEN)
	if ((ret = __db_fchk(dbenv, "DB->open", flags, OKFLAGS)) != 0)
		return (ret);
	if (LF_ISSET(DB_EXCL) && !LF_ISSET(DB_CREATE))
		return (__db_ferr(dbenv, "DB->open", 1));
	if (LF_ISSET(DB_RDONLY) && LF_ISSET(DB_CREATE))
		return (__db_ferr(dbenv, "DB->open", 1));

#ifdef	HAVE_VXWORKS
	if (LF_ISSET(DB_TRUNCATE)) {
		__db_err(dbenv, "DB_TRUNCATE not supported on VxWorks");
		return (DB_OPNOTSUP);
	}
#endif
	switch (type) {
	case DB_UNKNOWN:
		if (LF_ISSET(DB_CREATE|DB_TRUNCATE)) {
			__db_err(dbenv,
	    "%s: DB_UNKNOWN type specified with DB_CREATE or DB_TRUNCATE",
			    fname);
			return (EINVAL);
		}
		ok_flags = 0;
		break;
	case DB_BTREE:
		ok_flags = DB_OK_BTREE;
		break;
	case DB_HASH:
#ifndef HAVE_HASH
		return (__db_no_hash_am(dbenv));
#endif
		ok_flags = DB_OK_HASH;
		break;
	case DB_QUEUE:
#ifndef HAVE_QUEUE
		return (__db_no_queue_am(dbenv));
#endif
		ok_flags = DB_OK_QUEUE;
		break;
	case DB_RECNO:
		ok_flags = DB_OK_RECNO;
		break;
	default:
		__db_err(dbenv, "unknown type: %lu", (u_long)type);
		return (EINVAL);
	}
	if (ok_flags)
		DB_ILLEGAL_METHOD(dbp, ok_flags);

	/* The environment may have been created, but never opened. */
	if (!F_ISSET(dbenv, DB_ENV_DBLOCAL | DB_ENV_OPEN_CALLED)) {
		__db_err(dbenv, "environment not yet opened");
		return (EINVAL);
	}

	/*
	 * Historically, you could pass in an environment that didn't have a
	 * mpool, and DB would create a private one behind the scenes.  This
	 * no longer works.
	 */
	if (!F_ISSET(dbenv, DB_ENV_DBLOCAL) && !MPOOL_ON(dbenv)) {
		__db_err(dbenv, "environment did not include a memory pool");
		return (EINVAL);
	}

	/*
	 * You can't specify threads during DB->open if subsystems in the
	 * environment weren't configured with them.
	 */
	if (LF_ISSET(DB_THREAD) &&
	    !F_ISSET(dbenv, DB_ENV_DBLOCAL | DB_ENV_THREAD)) {
		__db_err(dbenv, "environment not created using DB_THREAD");
		return (EINVAL);
	}

	/* DB_TRUNCATE is neither transaction recoverable nor lockable. */
	if (LF_ISSET(DB_TRUNCATE) && (LOCKING_ON(dbenv) || txn != NULL)) {
		__db_err(dbenv,
		    "DB_TRUNCATE illegal with %s specified",
		    LOCKING_ON(dbenv) ? "locking" : "transactions");
		return (EINVAL);
	}

	/* Subdatabase checks. */
	if (dname != NULL) {
		/* Subdatabases must be created in named files. */
		if (fname == NULL) {
			__db_err(dbenv,
		    "multiple databases cannot be created in temporary files");
			return (EINVAL);
		}

		/* QAM can't be done as a subdatabase. */
		if (type == DB_QUEUE) {
			__db_err(dbenv, "Queue databases must be one-per-file");
			return (EINVAL);
		}
	}

	return (0);
}

/*
 * __db_pget_pp --
 *	DB->pget pre/post processing.
 *
 * PUBLIC: int __db_pget_pp
 * PUBLIC:     __P((DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t));
 */
int
__db_pget_pp(dbp, txn, skey, pkey, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *skey, *pkey, *data;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->pget");

	if ((ret = __db_pget_arg(dbp, pkey, flags)) != 0)
		return (ret);

	if ((ret = __db_get_arg(dbp, skey, data, flags)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		return (ret);

	ret = __db_pget(dbp, txn, skey, pkey, data, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_pget --
 *	DB->pget.
 *
 * PUBLIC: int __db_pget
 * PUBLIC:     __P((DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t));
 */
int
__db_pget(dbp, txn, skey, pkey, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *skey, *pkey, *data;
	u_int32_t flags;
{
	DBC *dbc;
	int ret, t_ret;

	if ((ret = __db_cursor(dbp, txn, &dbc, 0)) != 0)
		return (ret);

	SET_RET_MEM(dbc, dbp);

	DEBUG_LREAD(dbc, txn, "__db_pget", skey, NULL, flags);

	/*
	 * !!!
	 * The actual method call is simple, do it inline.
	 *
	 * The underlying cursor pget will fill in a default DBT for null
	 * pkeys, and use the cursor's returned-key memory internally to
	 * store any intermediate primary keys.  However, we've just set
	 * the returned-key memory to the DB handle's key memory, which
	 * is unsafe to use if the DB handle is threaded.  If the pkey
	 * argument is NULL, use the DBC-owned returned-key memory
	 * instead;  it'll go away when we close the cursor before we
	 * return, but in this case that's just fine, as we're not
	 * returning the primary key.
	 */
	if (pkey == NULL)
		dbc->rkey = &dbc->my_rkey;

	/*
	 * The cursor is just a perfectly ordinary secondary database cursor.
	 * Call its c_pget() method to do the dirty work.
	 */
	if (flags == 0 || flags == DB_RMW)
		flags |= DB_SET;

	ret = __db_c_pget(dbc, skey, pkey, data, flags);

	if ((t_ret = __db_c_close(dbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_pget_arg --
 *	Check DB->pget arguments.
 */
static int
__db_pget_arg(dbp, pkey, flags)
	DB *dbp;
	DBT *pkey;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	if (!F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "DB->pget may only be used on secondary indices");
		return (EINVAL);
	}

	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		__db_err(dbenv,
	"DB_MULTIPLE and DB_MULTIPLE_KEY may not be used on secondary indices");
		return (EINVAL);
	}

	/* DB_CONSUME makes no sense on a secondary index. */
	LF_CLR(DB_RMW);
	switch (flags) {
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		return (__db_ferr(dbenv, "DB->pget", 0));
	default:
		/* __db_get_arg will catch the rest. */
		break;
	}

	/*
	 * We allow the pkey field to be NULL, so that we can make the
	 * two-DBT get calls into wrappers for the three-DBT ones.
	 */
	if (pkey != NULL &&
	    (ret = __dbt_ferr(dbp, "primary key", pkey, 1)) != 0)
		return (ret);

	/* But the pkey field can't be NULL if we're doing a DB_GET_BOTH. */
	if (pkey == NULL && flags == DB_GET_BOTH) {
		__db_err(dbenv,
		    "DB_GET_BOTH on a secondary index requires a primary key");
		return (EINVAL);
	}

	return (0);
}

/*
 * __db_put_pp --
 *	DB->put pre/post processing.
 *
 * PUBLIC: int __db_put_pp __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
 */
int
__db_put_pp(dbp, txn, key, data, flags)
	DB *dbp;
	DB_TXN *txn;
	DBT *key, *data;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret, txn_local;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->put");

	if ((ret = __db_put_arg(dbp, key, data, flags)) != 0)
		return (ret);

	/* Create local transaction as necessary. */
	if (IS_AUTO_COMMIT(dbenv, txn, flags)) {
		if ((ret = __db_txn_auto_init(dbenv, &txn)) != 0)
			return (ret);
		txn_local = 1;
		LF_CLR(DB_AUTO_COMMIT);
	} else
		txn_local = 0;

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, txn, DB_LOCK_INVALIDID, 0)) != 0)
		goto err;

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, txn != NULL)) != 0)
		goto err;

	ret = __db_put(dbp, txn, key, data, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

err:	return (txn_local ? __db_txn_auto_resolve(dbenv, txn, 0, ret) : ret);
}

/*
 * __db_put_arg --
 *	Check DB->put arguments.
 */
static int
__db_put_arg(dbp, key, data, flags)
	DB *dbp;
	DBT *key, *data;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int ret, returnkey;

	dbenv = dbp->dbenv;
	returnkey = 0;

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbenv, "put"));

	/* Check for puts on a secondary. */
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbenv, "DB->put forbidden on secondary indices");
		return (EINVAL);
	}

	/* Check for invalid function flags. */
	LF_CLR(DB_AUTO_COMMIT);
	switch (flags) {
	case 0:
	case DB_NOOVERWRITE:
		break;
	case DB_APPEND:
		if (dbp->type != DB_RECNO && dbp->type != DB_QUEUE)
			goto err;
		returnkey = 1;
		break;
	case DB_NODUPDATA:
		if (F_ISSET(dbp, DB_AM_DUPSORT))
			break;
		/* FALLTHROUGH */
	default:
err:		return (__db_ferr(dbenv, "DB->put", 0));
	}

	/* Check for invalid key/data flags. */
	if ((ret = __dbt_ferr(dbp, "key", key, returnkey)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 0)) != 0)
		return (ret);

	/* Keys shouldn't have partial flags during a put. */
	if (F_ISSET(key, DB_DBT_PARTIAL))
		return (__db_ferr(dbenv, "key DBT", 0));

	/* Check for partial puts in the presence of duplicates. */
	if (F_ISSET(data, DB_DBT_PARTIAL) &&
	    (F_ISSET(dbp, DB_AM_DUP) || F_ISSET(key, DB_DBT_DUPOK))) {
		__db_err(dbenv,
"a partial put in the presence of duplicates requires a cursor operation");
		return (EINVAL);
	}

	return (0);
}

/*
 * __db_sync_pp --
 *	DB->sync pre/post processing.
 *
 * PUBLIC: int __db_sync_pp __P((DB *, u_int32_t));
 */
int
__db_sync_pp(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret;

	dbenv = dbp->dbenv;

	PANIC_CHECK(dbp->dbenv);
	DB_ILLEGAL_BEFORE_OPEN(dbp, "DB->sync");

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 */
	if (flags != 0)
		return (__db_ferr(dbenv, "DB->sync", 0));

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check && (ret = __db_rep_enter(dbp, 1, 0, 0)) != 0)
		return (ret);

	ret = __db_sync(dbp);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_close_pp --
 *	DBC->c_close pre/post processing.
 *
 * PUBLIC: int __db_c_close_pp __P((DBC *));
 */
int
__db_c_close_pp(dbc)
	DBC *dbc;
{
	DB_ENV *dbenv;
	DB *dbp;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	/*
	 * If the cursor is already closed we have a serious problem, and we
	 * assume that the cursor isn't on the active queue.  Don't do any of
	 * the remaining cursor close processing.
	 */
	if (!F_ISSET(dbc, DBC_ACTIVE)) {
		if (dbp != NULL)
			__db_err(dbenv, "Closing already-closed cursor");
		DB_ASSERT(0);
		return (EINVAL);
	}

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 0, 0, dbc->txn != NULL)) != 0)
		return (ret);

	ret = __db_c_close(dbc);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_count_pp --
 *	DBC->c_count pre/post processing.
 *
 * PUBLIC: int __db_c_count_pp __P((DBC *, db_recno_t *, u_int32_t));
 */
int
__db_c_count_pp(dbc, recnop, flags)
	DBC *dbc;
	db_recno_t *recnop;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB *dbp;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 */
	if (flags != 0)
		return (__db_ferr(dbenv, "DBcursor->count", 0));

	/*
	 * The cursor must be initialized, return EINVAL for an invalid cursor,
	 * otherwise 0.
	 */
	if (!IS_INITIALIZED(dbc))
		return (__db_curinval(dbenv));

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	ret = __db_c_count(dbc, recnop);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_del_pp --
 *	DBC->c_del pre/post processing.
 *
 * PUBLIC: int __db_c_del_pp __P((DBC *, u_int32_t));
 */
int
__db_c_del_pp(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __db_c_del_arg(dbc, flags)) != 0)
		return (ret);

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, dbc->txn, dbc->locker, 0)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, dbc->txn, "DBcursor->del", NULL, NULL, flags);

	ret = __db_c_del(dbc, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_del_arg --
 *	Check DBC->c_del arguments.
 *
 * PUBLIC: int __db_c_del_arg __P((DBC *, u_int32_t));
 */
int
__db_c_del_arg(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbenv, "DBcursor->del"));

	/* Check for invalid function flags. */
	switch (flags) {
	case 0:
		break;
	case DB_UPDATE_SECONDARY:
		DB_ASSERT(F_ISSET(dbp, DB_AM_SECONDARY));
		break;
	default:
		return (__db_ferr(dbenv, "DBcursor->del", 0));
	}

	/*
	 * The cursor must be initialized, return EINVAL for an invalid cursor,
	 * otherwise 0.
	 */
	if (!IS_INITIALIZED(dbc))
		return (__db_curinval(dbenv));

	return (0);
}

/*
 * __db_c_dup_pp --
 *	DBC->c_dup pre/post processing.
 *
 * PUBLIC: int __db_c_dup_pp __P((DBC *, DBC **, u_int32_t));
 */
int
__db_c_dup_pp(dbc, dbcp, flags)
	DBC *dbc, **dbcp;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 */
	if (flags != 0 && flags != DB_POSITION)
		return (__db_ferr(dbenv, "DBcursor->dup", 0));

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	ret = __db_c_dup(dbc, dbcp, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_get_pp --
 *	DBC->c_get pre/post processing.
 *
 * PUBLIC: int __db_c_get_pp __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_get_pp(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __db_c_get_arg(dbc, key, data, flags)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	DEBUG_LREAD(dbc, dbc->txn, "DBcursor->get",
	    flags == DB_SET || flags == DB_SET_RANGE ? key : NULL, NULL, flags);

	ret = __db_c_get(dbc, key, data, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_get_arg --
 *	Common DBC->get argument checking, used by both DBC->get and DBC->pget.
 */
static int
__db_c_get_arg(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int dirty, multi, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	/*
	 * Typically in checking routines that modify the flags, we have
	 * to save them and restore them, because the checking routine
	 * calls the work routine.  However, this is a pure-checking
	 * routine which returns to a function that calls the work routine,
	 * so it's OK that we do not save and restore the flags, even though
	 * we modify them.
	 *
	 * Check for read-modify-write validity.  DB_RMW doesn't make sense
	 * with CDB cursors since if you're going to write the cursor, you
	 * had to create it with DB_WRITECURSOR.  Regardless, we check for
	 * LOCKING_ON and not STD_LOCKING, as we don't want to disallow it.
	 * If this changes, confirm that DB does not itself set the DB_RMW
	 * flag in a path where CDB may have been configured.
	 */
	dirty = 0;
	if (LF_ISSET(DB_DIRTY_READ | DB_RMW)) {
		if (!LOCKING_ON(dbenv))
			return (__db_fnl(dbenv, "DBcursor->get"));
		if (LF_ISSET(DB_DIRTY_READ))
			dirty = 1;
		LF_CLR(DB_DIRTY_READ | DB_RMW);
	}

	multi = 0;
	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		multi = 1;
		if (LF_ISSET(DB_MULTIPLE) && LF_ISSET(DB_MULTIPLE_KEY))
			goto multi_err;
		LF_CLR(DB_MULTIPLE | DB_MULTIPLE_KEY);
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		if (dirty) {
			__db_err(dbenv,
    "DB_DIRTY_READ is not supported with DB_CONSUME or DB_CONSUME_WAIT");
			return (EINVAL);
		}
		if (dbp->type != DB_QUEUE)
			goto err;
		break;
	case DB_CURRENT:
	case DB_FIRST:
	case DB_GET_BOTH:
	case DB_GET_BOTH_RANGE:
	case DB_NEXT:
	case DB_NEXT_DUP:
	case DB_NEXT_NODUP:
	case DB_SET:
	case DB_SET_RANGE:
		break;
	case DB_LAST:
	case DB_PREV:
	case DB_PREV_NODUP:
		if (multi)
multi_err:		return (__db_ferr(dbenv, "DBcursor->get", 1));
		break;
	case DB_GET_BOTHC:
		if (dbp->type == DB_QUEUE)
			goto err;
		break;
	case DB_GET_RECNO:
		/*
		 * The one situation in which this might be legal with a
		 * non-RECNUM dbp is if dbp is a secondary and its primary is
		 * DB_AM_RECNUM.
		 */
		if (!F_ISSET(dbp, DB_AM_RECNUM) &&
		    (!F_ISSET(dbp, DB_AM_SECONDARY) ||
		    !F_ISSET(dbp->s_primary, DB_AM_RECNUM)))
			goto err;
		break;
	case DB_SET_RECNO:
		if (!F_ISSET(dbp, DB_AM_RECNUM))
			goto err;
		break;
	default:
err:		return (__db_ferr(dbenv, "DBcursor->get", 0));
	}

	/* Check for invalid key/data flags. */
	if ((ret = __dbt_ferr(dbp, "key", key, 0)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 0)) != 0)
		return (ret);

	if (multi) {
		if (!F_ISSET(data, DB_DBT_USERMEM)) {
			__db_err(dbenv,
	    "DB_MULTIPLE/DB_MULTIPLE_KEY require DB_DBT_USERMEM be set");
			return (EINVAL);
		}
		if (F_ISSET(key, DB_DBT_PARTIAL) ||
		    F_ISSET(data, DB_DBT_PARTIAL)) {
			__db_err(dbenv,
	    "DB_MULTIPLE/DB_MULTIPLE_KEY do not support DB_DBT_PARTIAL");
			return (EINVAL);
		}
		if (data->ulen < 1024 ||
		    data->ulen < dbp->pgsize || data->ulen % 1024 != 0) {
			__db_err(dbenv, "%s%s",
			    "DB_MULTIPLE/DB_MULTIPLE_KEY buffers must be ",
			    "aligned, at least page size and multiples of 1KB");
			return (EINVAL);
		}
	}

	/*
	 * The cursor must be initialized for DB_CURRENT, DB_GET_RECNO and
	 * DB_NEXT_DUP.  Return EINVAL for an invalid cursor, otherwise 0.
	 */
	if (!IS_INITIALIZED(dbc) && (flags == DB_CURRENT ||
	    flags == DB_GET_RECNO || flags == DB_NEXT_DUP))
		return (__db_curinval(dbenv));

	/* Check for consistent transaction usage. */
	if (LF_ISSET(DB_RMW) &&
	    (ret = __db_check_txn(dbp, dbc->txn, dbc->locker, 0)) != 0)
		return (ret);

	return (0);
}

/*
 * __db_secondary_close_pp --
 *	DB->close for secondaries
 *
 * PUBLIC: int __db_secondary_close_pp __P((DB *, u_int32_t));
 */
int
__db_secondary_close_pp(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	int handle_check, ret, t_ret;

	dbenv = dbp->dbenv;
	ret = 0;

	PANIC_CHECK(dbenv);

	/*
	 * !!!
	 * The actual argument checking is simple, do it inline.
	 *
	 * Validate arguments and complain if they're wrong, but as a DB
	 * handle destructor, we can't fail.
	 */
	if (flags != 0 && flags != DB_NOSYNC &&
	    (t_ret = __db_ferr(dbenv, "DB->close", 0)) != 0 && ret == 0)
		ret = t_ret;

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (t_ret = __db_rep_enter(dbp, 0, 0, 0)) != 0) {
		handle_check = 0;
		if (ret == 0)
			ret = t_ret;
	}

	if ((t_ret = __db_secondary_close(dbp, flags)) != 0 && ret == 0)
		ret = t_ret;

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_pget_pp --
 *	DBC->c_pget pre/post processing.
 *
 * PUBLIC: int __db_c_pget_pp __P((DBC *, DBT *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_pget_pp(dbc, skey, pkey, data, flags)
	DBC *dbc;
	DBT *skey, *pkey, *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __db_c_pget_arg(dbc, pkey, flags)) != 0)
		return (ret);

	if ((ret = __db_c_get_arg(dbc, skey, data, flags)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	ret = __db_c_pget(dbc, skey, pkey, data, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_pget_arg --
 *	Check DBC->pget arguments.
 */
static int
__db_c_pget_arg(dbc, pkey, flags)
	DBC *dbc;
	DBT *pkey;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	if (!F_ISSET(dbp, DB_AM_SECONDARY)) {
		__db_err(dbenv,
		    "DBcursor->pget may only be used on secondary indices");
		return (EINVAL);
	}

	if (LF_ISSET(DB_MULTIPLE | DB_MULTIPLE_KEY)) {
		__db_err(dbenv,
	"DB_MULTIPLE and DB_MULTIPLE_KEY may not be used on secondary indices");
		return (EINVAL);
	}

	switch (LF_ISSET(~DB_RMW)) {
	case DB_CONSUME:
	case DB_CONSUME_WAIT:
		/* These flags make no sense on a secondary index. */
		return (__db_ferr(dbenv, "DBcursor->pget", 0));
	case DB_GET_BOTH:
		/* DB_GET_BOTH is "get both the primary and the secondary". */
		if (pkey == NULL) {
			__db_err(dbenv,
		    "DB_GET_BOTH requires both a secondary and a primary key");
			return (EINVAL);
		}
		break;
	default:
		/* __db_c_get_arg will catch the rest. */
		break;
	}

	/*
	 * We allow the pkey field to be NULL, so that we can make the
	 * two-DBT get calls into wrappers for the three-DBT ones.
	 */
	if (pkey != NULL &&
	    (ret = __dbt_ferr(dbp, "primary key", pkey, 0)) != 0)
		return (ret);

	/* But the pkey field can't be NULL if we're doing a DB_GET_BOTH. */
	if (pkey == NULL && (flags & DB_OPFLAGS_MASK) == DB_GET_BOTH) {
		__db_err(dbenv,
		    "DB_GET_BOTH on a secondary index requires a primary key");
		return (EINVAL);
	}
	return (0);
}

/*
 * __db_c_put_pp --
 *	DBC->put pre/post processing.
 *
 * PUBLIC: int __db_c_put_pp __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_put_pp(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int handle_check, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	PANIC_CHECK(dbenv);

	if ((ret = __db_c_put_arg(dbc, key, data, flags)) != 0)
		return (ret);

	/* Check for consistent transaction usage. */
	if ((ret = __db_check_txn(dbp, dbc->txn, dbc->locker, 0)) != 0)
		return (ret);

	/* Check for replication block. */
	handle_check = IS_REPLICATED(dbenv, dbp);
	if (handle_check &&
	    (ret = __db_rep_enter(dbp, 1, 0, dbc->txn != NULL)) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, dbc->txn, "DBcursor->put",
	    flags == DB_KEYFIRST || flags == DB_KEYLAST ||
	    flags == DB_NODUPDATA || flags == DB_UPDATE_SECONDARY ?
	    key : NULL, data, flags);

	ret =__db_c_put(dbc, key, data, flags);

	/* Release replication block. */
	if (handle_check)
		__env_db_rep_exit(dbenv);

	return (ret);
}

/*
 * __db_c_put_arg --
 *	Check DBC->put arguments.
 */
static int
__db_c_put_arg(dbc, key, data, flags)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DB_ENV *dbenv;
	int key_flags, ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	key_flags = 0;

	/* Check for changes to a read-only tree. */
	if (IS_READONLY(dbp))
		return (__db_rdonly(dbenv, "c_put"));

	/* Check for puts on a secondary. */
	if (F_ISSET(dbp, DB_AM_SECONDARY)) {
		if (flags == DB_UPDATE_SECONDARY)
			flags = DB_KEYLAST;
		else {
			__db_err(dbenv,
		    "DBcursor->put forbidden on secondary indices");
			return (EINVAL);
		}
	}

	/* Check for invalid function flags. */
	switch (flags) {
	case DB_AFTER:
	case DB_BEFORE:
		switch (dbp->type) {
		case DB_BTREE:
		case DB_HASH:		/* Only with unsorted duplicates. */
			if (!F_ISSET(dbp, DB_AM_DUP))
				goto err;
			if (dbp->dup_compare != NULL)
				goto err;
			break;
		case DB_QUEUE:		/* Not permitted. */
			goto err;
		case DB_RECNO:		/* Only with mutable record numbers. */
			if (!F_ISSET(dbp, DB_AM_RENUMBER))
				goto err;
			key_flags = 1;
			break;
		case DB_UNKNOWN:
		default:
			goto err;
		}
		break;
	case DB_CURRENT:
		/*
		 * If there is a comparison function, doing a DB_CURRENT
		 * must not change the part of the data item that is used
		 * for the comparison.
		 */
		break;
	case DB_NODUPDATA:
		if (!F_ISSET(dbp, DB_AM_DUPSORT))
			goto err;
		/* FALLTHROUGH */
	case DB_KEYFIRST:
	case DB_KEYLAST:
		key_flags = 1;
		break;
	default:
err:		return (__db_ferr(dbenv, "DBcursor->put", 0));
	}

	/* Check for invalid key/data flags. */
	if (key_flags && (ret = __dbt_ferr(dbp, "key", key, 0)) != 0)
		return (ret);
	if ((ret = __dbt_ferr(dbp, "data", data, 0)) != 0)
		return (ret);

	/* Keys shouldn't have partial flags during a put. */
	if (F_ISSET(key, DB_DBT_PARTIAL))
		return (__db_ferr(dbenv, "key DBT", 0));

	/*
	 * The cursor must be initialized for anything other than DB_KEYFIRST
	 * and DB_KEYLAST, return EINVAL for an invalid cursor, otherwise 0.
	 */
	if (!IS_INITIALIZED(dbc) && flags != DB_KEYFIRST &&
	    flags != DB_KEYLAST && flags != DB_NODUPDATA)
		return (__db_curinval(dbenv));

	return (0);
}

/*
 * __dbt_ferr --
 *	Check a DBT for flag errors.
 */
static int
__dbt_ferr(dbp, name, dbt, check_thread)
	const DB *dbp;
	const char *name;
	const DBT *dbt;
	int check_thread;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbp->dbenv;

	/*
	 * Check for invalid DBT flags.  We allow any of the flags to be
	 * specified to any DB or DBcursor call so that applications can
	 * set DB_DBT_MALLOC when retrieving a data item from a secondary
	 * database and then specify that same DBT as a key to a primary
	 * database, without having to clear flags.
	 */
	if ((ret = __db_fchk(dbenv, name, dbt->flags, DB_DBT_APPMALLOC |
	    DB_DBT_MALLOC | DB_DBT_DUPOK | DB_DBT_REALLOC | DB_DBT_USERMEM |
	    DB_DBT_PARTIAL)) != 0)
		return (ret);
	switch (F_ISSET(dbt, DB_DBT_MALLOC | DB_DBT_REALLOC | DB_DBT_USERMEM)) {
	case 0:
	case DB_DBT_MALLOC:
	case DB_DBT_REALLOC:
	case DB_DBT_USERMEM:
		break;
	default:
		return (__db_ferr(dbenv, name, 1));
	}

	if (check_thread && DB_IS_THREADED(dbp) &&
	    !F_ISSET(dbt, DB_DBT_MALLOC | DB_DBT_REALLOC | DB_DBT_USERMEM)) {
		__db_err(dbenv,
		    "DB_THREAD mandates memory allocation flag on DBT %s",
		    name);
		return (EINVAL);
	}
	return (0);
}

/*
 * __db_rdonly --
 *	Common readonly message.
 */
static int
__db_rdonly(dbenv, name)
	const DB_ENV *dbenv;
	const char *name;
{
	__db_err(dbenv, "%s: attempt to modify a read-only tree", name);
	return (EACCES);
}

/*
 * __db_curinval
 *	Report that a cursor is in an invalid state.
 */
static int
__db_curinval(dbenv)
	const DB_ENV *dbenv;
{
	__db_err(dbenv,
	    "Cursor position must be set before performing this operation");
	return (EINVAL);
}

/*
 * __db_txn_auto_init --
 *	Handle DB_AUTO_COMMIT initialization.
 *
 * PUBLIC: int __db_txn_auto_init __P((DB_ENV *, DB_TXN **));
 */
int
__db_txn_auto_init(dbenv, txnidp)
	DB_ENV *dbenv;
	DB_TXN **txnidp;
{
	if (*txnidp != NULL) {
		__db_err(dbenv,
    "DB_AUTO_COMMIT may not be specified along with a transaction handle");
		return (EINVAL);
	}

	if (!TXN_ON(dbenv)) {
		__db_err(dbenv,
    "DB_AUTO_COMMIT may not be specified in non-transactional environment");
		return (EINVAL);
	}

	/*
	 * We're creating a transaction for the user, and we want it to block
	 * if replication recovery is running.  Call the user-level API.
	 */
	return (dbenv->txn_begin(dbenv, NULL, txnidp, 0));
}

/*
 * __db_txn_auto_resolve --
 *	Handle DB_AUTO_COMMIT resolution.
 *
 * PUBLIC: int __db_txn_auto_resolve __P((DB_ENV *, DB_TXN *, int, int));
 */
int
__db_txn_auto_resolve(dbenv, txn, nosync, ret)
	DB_ENV *dbenv;
	DB_TXN *txn;
	int nosync, ret;
{
	int t_ret;

	/*
	 * We're resolving a transaction for the user, and must decrement the
	 * replication handle count.  Call the user-level API.
	 */
	if (ret == 0)
		return (txn->commit(txn, nosync ? DB_TXN_NOSYNC : 0));

	if ((t_ret = txn->abort(txn)) != 0)
		return (__db_panic(dbenv, t_ret));

	return (ret);
}
