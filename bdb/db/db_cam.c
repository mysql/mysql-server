/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: db_cam.c,v 11.52 2001/01/18 15:11:16 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "lock.h"
#include "btree.h"
#include "hash.h"
#include "qam.h"
#include "txn.h"
#include "db_ext.h"

static int __db_c_cleanup __P((DBC *, DBC *, int));
static int __db_c_idup __P((DBC *, DBC **, u_int32_t));
static int __db_wrlock_err __P((DB_ENV *));

#define	CDB_LOCKING_INIT(dbp, dbc)					\
	/*								\
	 * If we are running CDB, this had better be either a write	\
	 * cursor or an immediate writer.  If it's a regular writer,	\
	 * that means we have an IWRITE lock and we need to upgrade	\
	 * it to a write lock.						\
	 */								\
	if (CDB_LOCKING((dbp)->dbenv)) {				\
		if (!F_ISSET(dbc, DBC_WRITECURSOR | DBC_WRITER))	\
			return (__db_wrlock_err(dbp->dbenv));		\
									\
		if (F_ISSET(dbc, DBC_WRITECURSOR) &&			\
		    (ret = lock_get((dbp)->dbenv, (dbc)->locker,	\
		    DB_LOCK_UPGRADE, &(dbc)->lock_dbt, DB_LOCK_WRITE,	\
		    &(dbc)->mylock)) != 0)				\
			return (ret);					\
	}
#define	CDB_LOCKING_DONE(dbp, dbc)					\
	/* Release the upgraded lock. */				\
	if (F_ISSET(dbc, DBC_WRITECURSOR))				\
		(void)__lock_downgrade(					\
		    (dbp)->dbenv, &(dbc)->mylock, DB_LOCK_IWRITE, 0);
/*
 * Copy the lock info from one cursor to another, so that locking
 * in CDB can be done in the context of an internally-duplicated
 * or off-page-duplicate cursor.
 */
#define	CDB_LOCKING_COPY(dbp, dbc_o, dbc_n)				\
	if (CDB_LOCKING((dbp)->dbenv) &&				\
	    F_ISSET((dbc_o), DBC_WRITECURSOR | DBC_WRITEDUP)) { \
		memcpy(&(dbc_n)->mylock, &(dbc_o)->mylock,		\
		    sizeof((dbc_o)->mylock));				\
		(dbc_n)->locker = (dbc_o)->locker;			\
	    /* This lock isn't ours to put--just discard it on close. */ \
	    F_SET((dbc_n), DBC_WRITEDUP);				\
	}

/*
 * __db_c_close --
 *	Close the cursor.
 *
 * PUBLIC: int __db_c_close __P((DBC *));
 */
int
__db_c_close(dbc)
	DBC *dbc;
{
	DB *dbp;
	DBC *opd;
	DBC_INTERNAL *cp;
	int ret, t_ret;

	dbp = dbc->dbp;
	ret = 0;

	PANIC_CHECK(dbp->dbenv);

	/*
	 * If the cursor is already closed we have a serious problem, and we
	 * assume that the cursor isn't on the active queue.  Don't do any of
	 * the remaining cursor close processing.
	 */
	if (!F_ISSET(dbc, DBC_ACTIVE)) {
		if (dbp != NULL)
			__db_err(dbp->dbenv, "Closing closed cursor");

		DB_ASSERT(0);
		return (EINVAL);
	}

	cp = dbc->internal;
	opd = cp->opd;

	/*
	 * Remove the cursor(s) from the active queue.  We may be closing two
	 * cursors at once here, a top-level one and a lower-level, off-page
	 * duplicate one.  The acess-method specific cursor close routine must
	 * close both of them in a single call.
	 *
	 * !!!
	 * Cursors must be removed from the active queue before calling the
	 * access specific cursor close routine, btree depends on having that
	 * order of operations.  It must also happen before any action that
	 * can fail and cause __db_c_close to return an error, or else calls
	 * here from __db_close may loop indefinitely.
	 */
	MUTEX_THREAD_LOCK(dbp->dbenv, dbp->mutexp);

	if (opd != NULL) {
		F_CLR(opd, DBC_ACTIVE);
		TAILQ_REMOVE(&dbp->active_queue, opd, links);
	}
	F_CLR(dbc, DBC_ACTIVE);
	TAILQ_REMOVE(&dbp->active_queue, dbc, links);

	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);

	/* Call the access specific cursor close routine. */
	if ((t_ret =
	    dbc->c_am_close(dbc, PGNO_INVALID, NULL)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Release the lock after calling the access method specific close
	 * routine, a Btree cursor may have had pending deletes.
	 */
	if (CDB_LOCKING(dbc->dbp->dbenv)) {
		/*
		 * If DBC_WRITEDUP is set, the cursor is an internally
		 * duplicated write cursor and the lock isn't ours to put.
		 */
		if (!F_ISSET(dbc, DBC_WRITEDUP) &&
		    dbc->mylock.off != LOCK_INVALID) {
			if ((t_ret = lock_put(dbc->dbp->dbenv,
			    &dbc->mylock)) != 0 && ret == 0)
				ret = t_ret;
			dbc->mylock.off = LOCK_INVALID;
		}

		/* For safety's sake, since this is going on the free queue. */
		memset(&dbc->mylock, 0, sizeof(dbc->mylock));
		F_CLR(dbc, DBC_WRITEDUP);
	}

	if (dbc->txn != NULL)
		dbc->txn->cursors--;

	/* Move the cursor(s) to the free queue. */
	MUTEX_THREAD_LOCK(dbp->dbenv, dbp->mutexp);
	if (opd != NULL) {
		if (dbc->txn != NULL)
			dbc->txn->cursors--;
		TAILQ_INSERT_TAIL(&dbp->free_queue, opd, links);
		opd = NULL;
	}
	TAILQ_INSERT_TAIL(&dbp->free_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);

	return (ret);
}

/*
 * __db_c_destroy --
 *	Destroy the cursor, called after DBC->c_close.
 *
 * PUBLIC: int __db_c_destroy __P((DBC *));
 */
int
__db_c_destroy(dbc)
	DBC *dbc;
{
	DB *dbp;
	DBC_INTERNAL *cp;
	int ret;

	dbp = dbc->dbp;
	cp =  dbc->internal;

	/* Remove the cursor from the free queue. */
	MUTEX_THREAD_LOCK(dbp->dbenv, dbp->mutexp);
	TAILQ_REMOVE(&dbp->free_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbp->dbenv, dbp->mutexp);

	/* Free up allocated memory. */
	if (dbc->rkey.data != NULL)
		__os_free(dbc->rkey.data, dbc->rkey.ulen);
	if (dbc->rdata.data != NULL)
		__os_free(dbc->rdata.data, dbc->rdata.ulen);

	/* Call the access specific cursor destroy routine. */
	ret = dbc->c_am_destroy == NULL ? 0 : dbc->c_am_destroy(dbc);

	__os_free(dbc, sizeof(*dbc));

	return (ret);
}

/*
 * __db_c_count --
 *	Return a count of duplicate data items.
 *
 * PUBLIC: int __db_c_count __P((DBC *, db_recno_t *, u_int32_t));
 */
int
__db_c_count(dbc, recnop, flags)
	DBC *dbc;
	db_recno_t *recnop;
	u_int32_t flags;
{
	DB *dbp;
	int ret;

	/*
	 * Cursor Cleanup Note:
	 * All of the cursors passed to the underlying access methods by this
	 * routine are not duplicated and will not be cleaned up on return.
	 * So, pages/locks that the cursor references must be resolved by the
	 * underlying functions.
	 */
	dbp = dbc->dbp;

	PANIC_CHECK(dbp->dbenv);

	/* Check for invalid flags. */
	if ((ret = __db_ccountchk(dbp, flags, IS_INITIALIZED(dbc))) != 0)
		return (ret);

	switch (dbc->dbtype) {
	case DB_QUEUE:
	case DB_RECNO:
		*recnop = 1;
		break;
	case DB_HASH:
		if (dbc->internal->opd == NULL) {
			if ((ret = __ham_c_count(dbc, recnop)) != 0)
				return (ret);
			break;
		}
		/* FALLTHROUGH */
	case DB_BTREE:
		if ((ret = __bam_c_count(dbc, recnop)) != 0)
			return (ret);
		break;
	default:
		return (__db_unknown_type(dbp->dbenv,
		     "__db_c_count", dbp->type));
	}
	return (0);
}

/*
 * __db_c_del --
 *	Delete using a cursor.
 *
 * PUBLIC: int __db_c_del __P((DBC *, u_int32_t));
 */
int
__db_c_del(dbc, flags)
	DBC *dbc;
	u_int32_t flags;
{
	DB *dbp;
	DBC *opd;
	int ret;

	/*
	 * Cursor Cleanup Note:
	 * All of the cursors passed to the underlying access methods by this
	 * routine are not duplicated and will not be cleaned up on return.
	 * So, pages/locks that the cursor references must be resolved by the
	 * underlying functions.
	 */
	dbp = dbc->dbp;

	PANIC_CHECK(dbp->dbenv);
	DB_CHECK_TXN(dbp, dbc->txn);

	/* Check for invalid flags. */
	if ((ret = __db_cdelchk(dbp, flags,
	    F_ISSET(dbp, DB_AM_RDONLY), IS_INITIALIZED(dbc))) != 0)
		return (ret);

	DEBUG_LWRITE(dbc, dbc->txn, "db_c_del", NULL, NULL, flags);

	CDB_LOCKING_INIT(dbp, dbc);

	/*
	 * Off-page duplicate trees are locked in the primary tree, that is,
	 * we acquire a write lock in the primary tree and no locks in the
	 * off-page dup tree.  If the del operation is done in an off-page
	 * duplicate tree, call the primary cursor's upgrade routine first.
	 */
	opd = dbc->internal->opd;
	if (opd == NULL)
		ret = dbc->c_am_del(dbc);
	else
		if ((ret = dbc->c_am_writelock(dbc)) == 0)
			ret = opd->c_am_del(opd);

	CDB_LOCKING_DONE(dbp, dbc);

	return (ret);
}

/*
 * __db_c_dup --
 *	Duplicate a cursor
 *
 * PUBLIC: int __db_c_dup __P((DBC *, DBC **, u_int32_t));
 */
int
__db_c_dup(dbc_orig, dbcp, flags)
	DBC *dbc_orig;
	DBC **dbcp;
	u_int32_t flags;
{
	DB_ENV *dbenv;
	DB *dbp;
	DBC *dbc_n, *dbc_nopd;
	int ret;

	dbp = dbc_orig->dbp;
	dbenv = dbp->dbenv;
	dbc_n = dbc_nopd = NULL;

	PANIC_CHECK(dbp->dbenv);

	/*
	 * We can never have two write cursors open in CDB, so do not
	 * allow duplication of a write cursor.
	 */
	if (flags != DB_POSITIONI &&
	    F_ISSET(dbc_orig, DBC_WRITER | DBC_WRITECURSOR)) {
		__db_err(dbenv, "Cannot duplicate writeable cursor");
		return (EINVAL);
	}

	/* Allocate a new cursor and initialize it. */
	if ((ret = __db_c_idup(dbc_orig, &dbc_n, flags)) != 0)
		goto err;
	*dbcp = dbc_n;

	/*
	 * If we're in CDB, and this isn't an internal duplication (in which
	 * case we're explicitly overriding CDB locking), the duplicated
	 * cursor needs its own read lock.  (We know it's not a write cursor
	 * because we wouldn't have made it this far;  you can't dup them.)
	 */
	if (CDB_LOCKING(dbenv) && flags != DB_POSITIONI) {
		DB_ASSERT(!F_ISSET(dbc_orig, DBC_WRITER | DBC_WRITECURSOR));

		if ((ret = lock_get(dbenv, dbc_n->locker, 0,
		    &dbc_n->lock_dbt, DB_LOCK_READ, &dbc_n->mylock)) != 0) {
			(void)__db_c_close(dbc_n);
			return (ret);
		}
	}

	/*
	 * If the cursor references an off-page duplicate tree, allocate a
	 * new cursor for that tree and initialize it.
	 */
	if (dbc_orig->internal->opd != NULL) {
		if ((ret =
		   __db_c_idup(dbc_orig->internal->opd, &dbc_nopd, flags)) != 0)
			goto err;
		dbc_n->internal->opd = dbc_nopd;
	}

	return (0);

err:	if (dbc_n != NULL)
		(void)dbc_n->c_close(dbc_n);
	if (dbc_nopd != NULL)
		(void)dbc_nopd->c_close(dbc_nopd);

	return (ret);
}

/*
 * __db_c_idup --
 *	Internal version of __db_c_dup.
 */
static int
__db_c_idup(dbc_orig, dbcp, flags)
	DBC *dbc_orig, **dbcp;
	u_int32_t flags;
{
	DB *dbp;
	DBC *dbc_n;
	DBC_INTERNAL *int_n, *int_orig;
	int ret;

	dbp = dbc_orig->dbp;
	dbc_n = *dbcp;

	if ((ret = __db_icursor(dbp, dbc_orig->txn, dbc_orig->dbtype,
	    dbc_orig->internal->root, F_ISSET(dbc_orig, DBC_OPD), &dbc_n)) != 0)
		return (ret);

	dbc_n->locker = dbc_orig->locker;

	/* If the user wants the cursor positioned, do it here.  */
	if (flags == DB_POSITION || flags == DB_POSITIONI) {
		int_n = dbc_n->internal;
		int_orig = dbc_orig->internal;

		dbc_n->flags = dbc_orig->flags;

		int_n->indx = int_orig->indx;
		int_n->pgno = int_orig->pgno;
		int_n->root = int_orig->root;
		int_n->lock_mode = int_orig->lock_mode;

		switch (dbc_orig->dbtype) {
		case DB_QUEUE:
			if ((ret = __qam_c_dup(dbc_orig, dbc_n)) != 0)
				goto err;
			break;
		case DB_BTREE:
		case DB_RECNO:
			if ((ret = __bam_c_dup(dbc_orig, dbc_n)) != 0)
				goto err;
			break;
		case DB_HASH:
			if ((ret = __ham_c_dup(dbc_orig, dbc_n)) != 0)
				goto err;
			break;
		default:
			ret = __db_unknown_type(dbp->dbenv,
			    "__db_c_idup", dbc_orig->dbtype);
			goto err;
		}
	}

	/* Now take care of duping the CDB information. */
	CDB_LOCKING_COPY(dbp, dbc_orig, dbc_n);

	*dbcp = dbc_n;
	return (0);

err:	(void)dbc_n->c_close(dbc_n);
	return (ret);
}

/*
 * __db_c_newopd --
 *	Create a new off-page duplicate cursor.
 *
 * PUBLIC: int __db_c_newopd __P((DBC *, db_pgno_t, DBC **));
 */
int
__db_c_newopd(dbc_parent, root, dbcp)
	DBC *dbc_parent;
	db_pgno_t root;
	DBC **dbcp;
{
	DB *dbp;
	DBC *opd;
	DBTYPE dbtype;
	int ret;

	dbp = dbc_parent->dbp;
	dbtype = (dbp->dup_compare == NULL) ? DB_RECNO : DB_BTREE;

	if ((ret = __db_icursor(dbp,
	    dbc_parent->txn, dbtype, root, 1, &opd)) != 0)
		return (ret);

	CDB_LOCKING_COPY(dbp, dbc_parent, opd);

	*dbcp = opd;

	return (0);
}

/*
 * __db_c_get --
 *	Get using a cursor.
 *
 * PUBLIC: int __db_c_get __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_get(dbc_arg, key, data, flags)
	DBC *dbc_arg;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DBC *dbc, *dbc_n, *opd;
	DBC_INTERNAL *cp, *cp_n;
	db_pgno_t pgno;
	u_int32_t tmp_flags, tmp_rmw;
	u_int8_t type;
	int ret, t_ret;

	/*
	 * Cursor Cleanup Note:
	 * All of the cursors passed to the underlying access methods by this
	 * routine are duplicated cursors.  On return, any referenced pages
	 * will be discarded, and, if the cursor is not intended to be used
	 * again, the close function will be called.  So, pages/locks that
	 * the cursor references do not need to be resolved by the underlying
	 * functions.
	 */
	dbp = dbc_arg->dbp;
	dbc_n = NULL;
	opd = NULL;

	PANIC_CHECK(dbp->dbenv);

	/* Check for invalid flags. */
	if ((ret =
	    __db_cgetchk(dbp, key, data, flags, IS_INITIALIZED(dbc_arg))) != 0)
		return (ret);

	/* Clear OR'd in additional bits so we can check for flag equality. */
	tmp_rmw = LF_ISSET(DB_RMW);
	LF_CLR(DB_RMW);

	DEBUG_LREAD(dbc_arg, dbc_arg->txn, "db_c_get",
	    flags == DB_SET || flags == DB_SET_RANGE ? key : NULL, NULL, flags);

	/*
	 * Return a cursor's record number.  It has nothing to do with the
	 * cursor get code except that it was put into the interface.
	 */
	if (flags == DB_GET_RECNO)
		return (__bam_c_rget(dbc_arg, data, flags | tmp_rmw));

	if (flags == DB_CONSUME || flags == DB_CONSUME_WAIT)
		CDB_LOCKING_INIT(dbp, dbc_arg);

	/*
	 * If we have an off-page duplicates cursor, and the operation applies
	 * to it, perform the operation.  Duplicate the cursor and call the
	 * underlying function.
	 *
	 * Off-page duplicate trees are locked in the primary tree, that is,
	 * we acquire a write lock in the primary tree and no locks in the
	 * off-page dup tree.  If the DB_RMW flag was specified and the get
	 * operation is done in an off-page duplicate tree, call the primary
	 * cursor's upgrade routine first.
	 */
	cp = dbc_arg->internal;
	if (cp->opd != NULL &&
	    (flags == DB_CURRENT || flags == DB_GET_BOTHC ||
	    flags == DB_NEXT || flags == DB_NEXT_DUP || flags == DB_PREV)) {
		if (tmp_rmw && (ret = dbc_arg->c_am_writelock(dbc_arg)) != 0)
			return (ret);
		if ((ret = __db_c_idup(cp->opd, &opd, DB_POSITIONI)) != 0)
			return (ret);

		switch (ret = opd->c_am_get(
		    opd, key, data, flags, NULL)) {
		case 0:
			goto done;
		case DB_NOTFOUND:
			/*
			 * Translate DB_NOTFOUND failures for the DB_NEXT and
			 * DB_PREV operations into a subsequent operation on
			 * the parent cursor.
			 */
			if (flags == DB_NEXT || flags == DB_PREV) {
				if ((ret = opd->c_close(opd)) != 0)
					goto err;
				opd = NULL;
				break;
			}
			goto err;
		default:
			goto err;
		}
	}

	/*
	 * Perform an operation on the main cursor.  Duplicate the cursor,
	 * upgrade the lock as required, and call the underlying function.
	 */
	switch (flags) {
	case DB_CURRENT:
	case DB_GET_BOTHC:
	case DB_NEXT:
	case DB_NEXT_DUP:
	case DB_NEXT_NODUP:
	case DB_PREV:
	case DB_PREV_NODUP:
		tmp_flags = DB_POSITIONI;
		break;
	default:
		tmp_flags = 0;
		break;
	}

	/*
	 * If this cursor is going to be closed immediately, we don't
	 * need to take precautions to clean it up on error.
	 */
	if (F_ISSET(dbc_arg, DBC_TRANSIENT))
		dbc_n = dbc_arg;
	else if ((ret = __db_c_idup(dbc_arg, &dbc_n, tmp_flags)) != 0)
		goto err;

	if (tmp_rmw)
		F_SET(dbc_n, DBC_RMW);
	pgno = PGNO_INVALID;
	ret = dbc_n->c_am_get(dbc_n, key, data, flags, &pgno);
	if (tmp_rmw)
		F_CLR(dbc_n, DBC_RMW);
	if (ret != 0)
		goto err;

	cp_n = dbc_n->internal;

	/*
	 * We may be referencing a new off-page duplicates tree.  Acquire
	 * a new cursor and call the underlying function.
	 */
	if (pgno != PGNO_INVALID) {
		if ((ret = __db_c_newopd(dbc_arg, pgno, &cp_n->opd)) != 0)
			goto err;

		switch (flags) {
		case DB_FIRST:
		case DB_NEXT:
		case DB_NEXT_NODUP:
		case DB_SET:
		case DB_SET_RECNO:
		case DB_SET_RANGE:
			tmp_flags = DB_FIRST;
			break;
		case DB_LAST:
		case DB_PREV:
		case DB_PREV_NODUP:
			tmp_flags = DB_LAST;
			break;
		case DB_GET_BOTH:
			tmp_flags = DB_GET_BOTH;
			break;
		case DB_GET_BOTHC:
			tmp_flags = DB_GET_BOTHC;
			break;
		default:
			ret =
			    __db_unknown_flag(dbp->dbenv, "__db_c_get", flags);
			goto err;
		}
		if ((ret = cp_n->opd->c_am_get(
		    cp_n->opd, key, data, tmp_flags, NULL)) != 0)
			goto err;
	}

done:	/*
	 * Return a key/data item.  The only exception is that we don't return
	 * a key if the user already gave us one, that is, if the DB_SET flag
	 * was set.  The DB_SET flag is necessary.  In a Btree, the user's key
	 * doesn't have to be the same as the key stored the tree, depending on
	 * the magic performed by the comparison function.  As we may not have
	 * done any key-oriented operation here, the page reference may not be
	 * valid.  Fill it in as necessary.  We don't have to worry about any
	 * locks, the cursor must already be holding appropriate locks.
	 *
	 * XXX
	 * If not a Btree and DB_SET_RANGE is set, we shouldn't return a key
	 * either, should we?
	 */
	cp_n = dbc_n == NULL ? dbc_arg->internal : dbc_n->internal;
	if (!F_ISSET(key, DB_DBT_ISSET)) {
		if (cp_n->page == NULL && (ret =
		    memp_fget(dbp->mpf, &cp_n->pgno, 0, &cp_n->page)) != 0)
			goto err;

		if ((ret = __db_ret(dbp, cp_n->page, cp_n->indx,
		    key, &dbc_arg->rkey.data, &dbc_arg->rkey.ulen)) != 0)
			goto err;
	}
	dbc = opd != NULL ? opd : cp_n->opd != NULL ? cp_n->opd : dbc_n;
	if (!F_ISSET(data, DB_DBT_ISSET)) {
		type = TYPE(dbc->internal->page);
		ret = __db_ret(dbp, dbc->internal->page, dbc->internal->indx +
		    (type == P_LBTREE || type == P_HASH ? O_INDX : 0),
		    data, &dbc_arg->rdata.data, &dbc_arg->rdata.ulen);
	}

err:	/* Don't pass DB_DBT_ISSET back to application level, error or no. */
	F_CLR(key, DB_DBT_ISSET);
	F_CLR(data, DB_DBT_ISSET);

	/* Cleanup and cursor resolution. */
	if (opd != NULL) {
		if ((t_ret =
		     __db_c_cleanup(dbc_arg->internal->opd,
		     opd, ret)) != 0 && ret == 0)
			ret = t_ret;

	}

	if ((t_ret = __db_c_cleanup(dbc_arg, dbc_n, ret)) != 0 && ret == 0)
		ret = t_ret;

	if (flags == DB_CONSUME || flags == DB_CONSUME_WAIT)
		CDB_LOCKING_DONE(dbp, dbc_arg);
	return (ret);
}

/*
 * __db_c_put --
 *	Put using a cursor.
 *
 * PUBLIC: int __db_c_put __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_put(dbc_arg, key, data, flags)
	DBC *dbc_arg;
	DBT *key, *data;
	u_int32_t flags;
{
	DB *dbp;
	DBC *dbc_n, *opd;
	db_pgno_t pgno;
	u_int32_t tmp_flags;
	int ret, t_ret;

	/*
	 * Cursor Cleanup Note:
	 * All of the cursors passed to the underlying access methods by this
	 * routine are duplicated cursors.  On return, any referenced pages
	 * will be discarded, and, if the cursor is not intended to be used
	 * again, the close function will be called.  So, pages/locks that
	 * the cursor references do not need to be resolved by the underlying
	 * functions.
	 */
	dbp = dbc_arg->dbp;
	dbc_n = NULL;

	PANIC_CHECK(dbp->dbenv);
	DB_CHECK_TXN(dbp, dbc_arg->txn);

	/* Check for invalid flags. */
	if ((ret = __db_cputchk(dbp, key, data, flags,
	    F_ISSET(dbp, DB_AM_RDONLY), IS_INITIALIZED(dbc_arg))) != 0)
		return (ret);

	DEBUG_LWRITE(dbc_arg, dbc_arg->txn, "db_c_put",
	    flags == DB_KEYFIRST || flags == DB_KEYLAST ||
	    flags == DB_NODUPDATA ? key : NULL, data, flags);

	CDB_LOCKING_INIT(dbp, dbc_arg);

	/*
	 * If we have an off-page duplicates cursor, and the operation applies
	 * to it, perform the operation.  Duplicate the cursor and call the
	 * underlying function.
	 *
	 * Off-page duplicate trees are locked in the primary tree, that is,
	 * we acquire a write lock in the primary tree and no locks in the
	 * off-page dup tree.  If the put operation is done in an off-page
	 * duplicate tree, call the primary cursor's upgrade routine first.
	 */
	if (dbc_arg->internal->opd != NULL &&
	    (flags == DB_AFTER || flags == DB_BEFORE || flags == DB_CURRENT)) {
		/*
		 * A special case for hash off-page duplicates.  Hash doesn't
		 * support (and is documented not to support) put operations
		 * relative to a cursor which references an already deleted
		 * item.  For consistency, apply the same criteria to off-page
		 * duplicates as well.
		 */
		if (dbc_arg->dbtype == DB_HASH && F_ISSET(
		    ((BTREE_CURSOR *)(dbc_arg->internal->opd->internal)),
		    C_DELETED)) {
			ret = DB_NOTFOUND;
			goto err;
		}

		if ((ret = dbc_arg->c_am_writelock(dbc_arg)) != 0)
			return (ret);
		if ((ret = __db_c_dup(dbc_arg, &dbc_n, DB_POSITIONI)) != 0)
			goto err;
		opd = dbc_n->internal->opd;
		if ((ret = opd->c_am_put(
		    opd, key, data, flags, NULL)) != 0)
			goto err;
		goto done;
	}

	/*
	 * Perform an operation on the main cursor.  Duplicate the cursor,
	 * and call the underlying function.
	 *
	 * XXX: MARGO
	 *
	tmp_flags = flags == DB_AFTER ||
	    flags == DB_BEFORE || flags == DB_CURRENT ? DB_POSITIONI : 0;
	 */
	tmp_flags = DB_POSITIONI;

	/*
	 * If this cursor is going to be closed immediately, we don't
	 * need to take precautions to clean it up on error.
	 */
	if (F_ISSET(dbc_arg, DBC_TRANSIENT))
		dbc_n = dbc_arg;
	else if ((ret = __db_c_idup(dbc_arg, &dbc_n, tmp_flags)) != 0)
		goto err;

	pgno = PGNO_INVALID;
	if ((ret = dbc_n->c_am_put(dbc_n, key, data, flags, &pgno)) != 0)
		goto err;

	/*
	 * We may be referencing a new off-page duplicates tree.  Acquire
	 * a new cursor and call the underlying function.
	 */
	if (pgno != PGNO_INVALID) {
		if ((ret = __db_c_newopd(dbc_arg, pgno, &opd)) != 0)
			goto err;
		dbc_n->internal->opd = opd;

		if ((ret = opd->c_am_put(
		    opd, key, data, flags, NULL)) != 0)
			goto err;
	}

done:
err:	/* Cleanup and cursor resolution. */
	if ((t_ret = __db_c_cleanup(dbc_arg, dbc_n, ret)) != 0 && ret == 0)
		ret = t_ret;

	CDB_LOCKING_DONE(dbp, dbc_arg);

	return (ret);
}

/*
 * __db_duperr()
 *	Error message: we don't currently support sorted duplicate duplicates.
 * PUBLIC: int __db_duperr __P((DB *, u_int32_t));
 */
int
__db_duperr(dbp, flags)
	DB *dbp;
	u_int32_t flags;
{
	if (flags != DB_NODUPDATA)
		__db_err(dbp->dbenv,
		    "Duplicate data items are not supported with sorted data");
	return (DB_KEYEXIST);
}

/*
 * __db_c_cleanup --
 *	Clean up duplicate cursors.
 */
static int
__db_c_cleanup(dbc, dbc_n, failed)
	DBC *dbc, *dbc_n;
	int failed;
{
	DB *dbp;
	DBC *opd;
	DBC_INTERNAL *internal;
	int ret, t_ret;

	dbp = dbc->dbp;
	internal = dbc->internal;
	ret = 0;

	/* Discard any pages we're holding. */
	if (internal->page != NULL) {
		if ((t_ret =
		    memp_fput(dbp->mpf, internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		internal->page = NULL;
	}
	opd = internal->opd;
	if (opd != NULL && opd->internal->page != NULL) {
		if ((t_ret = memp_fput(dbp->mpf,
		     opd->internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		 opd->internal->page = NULL;
	}

	/*
	 * If dbc_n is NULL, there's no internal cursor swapping to be
	 * done and no dbc_n to close--we probably did the entire
	 * operation on an offpage duplicate cursor.  Just return.
	 */
	if (dbc_n == NULL)
		return (ret);

	/*
	 * If dbc is marked DBC_TRANSIENT, we're inside a DB->{put/get}
	 * operation, and as an optimization we performed the operation on
	 * the main cursor rather than on a duplicated one.  Assert
	 * that dbc_n == dbc (i.e., that we really did skip the
	 * duplication).  Then just do nothing--even if there was
	 * an error, we're about to close the cursor, and the fact that we
	 * moved it isn't a user-visible violation of our "cursor
	 * stays put on error" rule.
	 */
	if (F_ISSET(dbc, DBC_TRANSIENT)) {
		DB_ASSERT(dbc == dbc_n);
		return (ret);
	}

	if (dbc_n->internal->page != NULL) {
		if ((t_ret = memp_fput(dbp->mpf,
		    dbc_n->internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		dbc_n->internal->page = NULL;
	}
	opd = dbc_n->internal->opd;
	if (opd != NULL && opd->internal->page != NULL) {
		if ((t_ret = memp_fput(dbp->mpf,
		     opd->internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		opd->internal->page = NULL;
	}

	/*
	 * If we didn't fail before entering this routine or just now when
	 * freeing pages, swap the interesting contents of the old and new
	 * cursors.
	 */
	if (!failed && ret == 0) {
		dbc->internal = dbc_n->internal;
		dbc_n->internal = internal;
	}

	/*
	 * Close the cursor we don't care about anymore.  The close can fail,
	 * but we only expect DB_LOCK_DEADLOCK failures.  This violates our
	 * "the cursor is unchanged on error" semantics, but since all you can
	 * do with a DB_LOCK_DEADLOCK failure is close the cursor, I believe
	 * that's OK.
	 *
	 * XXX
	 * There's no way to recover from failure to close the old cursor.
	 * All we can do is move to the new position and return an error.
	 *
	 * XXX
	 * We might want to consider adding a flag to the cursor, so that any
	 * subsequent operations other than close just return an error?
	 */
	if ((t_ret = dbc_n->c_close(dbc_n)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_wrlock_err -- do not have a write lock.
 */
static int
__db_wrlock_err(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv, "Write attempted on read-only cursor");
	return (EPERM);
}
