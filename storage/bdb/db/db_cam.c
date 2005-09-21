/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_cam.c,v 11.156 2004/09/28 18:07:32 ubell Exp $
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
#include "dbinc/lock.h"
#include "dbinc/mp.h"
#include "dbinc/qam.h"

static int __db_buildpartial __P((DB *, DBT *, DBT *, DBT *));
static int __db_c_cleanup __P((DBC *, DBC *, int));
static int __db_c_del_secondary __P((DBC *));
static int __db_c_pget_recno __P((DBC *, DBT *, DBT *, u_int32_t));
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
		    (ret = __lock_get((dbp)->dbenv,			\
		    (dbc)->locker, DB_LOCK_UPGRADE, &(dbc)->lock_dbt,	\
		    DB_LOCK_WRITE, &(dbc)->mylock)) != 0)		\
			return (ret);					\
	}
#define	CDB_LOCKING_DONE(dbp, dbc)					\
	/* Release the upgraded lock. */				\
	if (F_ISSET(dbc, DBC_WRITECURSOR))				\
		(void)__lock_downgrade(					\
		    (dbp)->dbenv, &(dbc)->mylock, DB_LOCK_IWRITE, 0);
/*
 * __db_c_close --
 *	DBC->c_close.
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
	DB_ENV *dbenv;
	int ret, t_ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;
	cp = dbc->internal;
	opd = cp->opd;
	ret = 0;

	/*
	 * Remove the cursor(s) from the active queue.  We may be closing two
	 * cursors at once here, a top-level one and a lower-level, off-page
	 * duplicate one.  The access-method specific cursor close routine must
	 * close both of them in a single call.
	 *
	 * !!!
	 * Cursors must be removed from the active queue before calling the
	 * access specific cursor close routine, btree depends on having that
	 * order of operations.
	 */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);

	if (opd != NULL) {
		F_CLR(opd, DBC_ACTIVE);
		TAILQ_REMOVE(&dbp->active_queue, opd, links);
	}
	F_CLR(dbc, DBC_ACTIVE);
	TAILQ_REMOVE(&dbp->active_queue, dbc, links);

	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	/* Call the access specific cursor close routine. */
	if ((t_ret =
	    dbc->c_am_close(dbc, PGNO_INVALID, NULL)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * Release the lock after calling the access method specific close
	 * routine, a Btree cursor may have had pending deletes.
	 */
	if (CDB_LOCKING(dbenv)) {
		/*
		 * Also, be sure not to free anything if mylock.off is
		 * INVALID;  in some cases, such as idup'ed read cursors
		 * and secondary update cursors, a cursor in a CDB
		 * environment may not have a lock at all.
		 */
		if ((t_ret = __LPUT(dbc, dbc->mylock)) != 0 && ret == 0)
			ret = t_ret;

		/* For safety's sake, since this is going on the free queue. */
		memset(&dbc->mylock, 0, sizeof(dbc->mylock));
		if (opd != NULL)
			memset(&opd->mylock, 0, sizeof(opd->mylock));
	}

	if (dbc->txn != NULL)
		dbc->txn->cursors--;

	/* Move the cursor(s) to the free queue. */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	if (opd != NULL) {
		if (dbc->txn != NULL)
			dbc->txn->cursors--;
		TAILQ_INSERT_TAIL(&dbp->free_queue, opd, links);
		opd = NULL;
	}
	TAILQ_INSERT_TAIL(&dbp->free_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

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
	DB_ENV *dbenv;
	int ret, t_ret;

	dbp = dbc->dbp;
	dbenv = dbp->dbenv;

	/* Remove the cursor from the free queue. */
	MUTEX_THREAD_LOCK(dbenv, dbp->mutexp);
	TAILQ_REMOVE(&dbp->free_queue, dbc, links);
	MUTEX_THREAD_UNLOCK(dbenv, dbp->mutexp);

	/* Free up allocated memory. */
	if (dbc->my_rskey.data != NULL)
		__os_free(dbenv, dbc->my_rskey.data);
	if (dbc->my_rkey.data != NULL)
		__os_free(dbenv, dbc->my_rkey.data);
	if (dbc->my_rdata.data != NULL)
		__os_free(dbenv, dbc->my_rdata.data);

	/* Call the access specific cursor destroy routine. */
	ret = dbc->c_am_destroy == NULL ? 0 : dbc->c_am_destroy(dbc);

	/*
	 * Release the lock id for this cursor.
	 */
	if (LOCKING_ON(dbenv) &&
	    F_ISSET(dbc, DBC_OWN_LID) &&
	    (t_ret = __lock_id_free(dbenv, dbc->lid)) != 0 && ret == 0)
		ret = t_ret;

	__os_free(dbenv, dbc);

	return (ret);
}

/*
 * __db_c_count --
 *	Return a count of duplicate data items.
 *
 * PUBLIC: int __db_c_count __P((DBC *, db_recno_t *));
 */
int
__db_c_count(dbc, recnop)
	DBC *dbc;
	db_recno_t *recnop;
{
	DB_ENV *dbenv;
	int ret;

	dbenv = dbc->dbp->dbenv;

	/*
	 * Cursor Cleanup Note:
	 * All of the cursors passed to the underlying access methods by this
	 * routine are not duplicated and will not be cleaned up on return.
	 * So, pages/locks that the cursor references must be resolved by the
	 * underlying functions.
	 */
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
	case DB_UNKNOWN:
	default:
		return (__db_unknown_type(dbenv, "__db_c_count", dbc->dbtype));
	}
	return (0);
}

/*
 * __db_c_del --
 *	DBC->c_del.
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
	int ret, t_ret;

	dbp = dbc->dbp;

	/*
	 * Cursor Cleanup Note:
	 * All of the cursors passed to the underlying access methods by this
	 * routine are not duplicated and will not be cleaned up on return.
	 * So, pages/locks that the cursor references must be resolved by the
	 * underlying functions.
	 */

	CDB_LOCKING_INIT(dbp, dbc);

	/*
	 * If we're a secondary index, and DB_UPDATE_SECONDARY isn't set
	 * (which it only is if we're being called from a primary update),
	 * then we need to call through to the primary and delete the item.
	 *
	 * Note that this will delete the current item;  we don't need to
	 * delete it ourselves as well, so we can just goto done.
	 */
	if (flags != DB_UPDATE_SECONDARY && F_ISSET(dbp, DB_AM_SECONDARY)) {
		ret = __db_c_del_secondary(dbc);
		goto done;
	}

	/*
	 * If we are a primary and have secondary indices, go through
	 * and delete any secondary keys that point at the current record.
	 */
	if (LIST_FIRST(&dbp->s_secondaries) != NULL &&
	    (ret = __db_c_del_primary(dbc)) != 0)
		goto done;

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

	/*
	 * If this was an update that is supporting dirty reads
	 * then we may have just swapped our read for a write lock
	 * which is held by the surviving cursor.  We need
	 * to explicitly downgrade this lock.  The closed cursor
	 * may only have had a read lock.
	 */
	if (F_ISSET(dbc->dbp, DB_AM_DIRTY) &&
	    dbc->internal->lock_mode == DB_LOCK_WRITE) {
		if ((t_ret =
		    __TLPUT(dbc, dbc->internal->lock)) != 0 && ret == 0)
			ret = t_ret;
		if (t_ret == 0)
			dbc->internal->lock_mode = DB_LOCK_WWRITE;
	}

done:	CDB_LOCKING_DONE(dbp, dbc);

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
	DBC *dbc_n, *dbc_nopd;
	int ret;

	dbc_n = dbc_nopd = NULL;

	/* Allocate a new cursor and initialize it. */
	if ((ret = __db_c_idup(dbc_orig, &dbc_n, flags)) != 0)
		goto err;
	*dbcp = dbc_n;

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
		(void)__db_c_close(dbc_n);
	if (dbc_nopd != NULL)
		(void)__db_c_close(dbc_nopd);

	return (ret);
}

/*
 * __db_c_idup --
 *	Internal version of __db_c_dup.
 *
 * PUBLIC: int __db_c_idup __P((DBC *, DBC **, u_int32_t));
 */
int
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

	if ((ret = __db_cursor_int(dbp, dbc_orig->txn, dbc_orig->dbtype,
	    dbc_orig->internal->root, F_ISSET(dbc_orig, DBC_OPD),
	    dbc_orig->locker, &dbc_n)) != 0)
		return (ret);

	/* Position the cursor if requested, acquiring the necessary locks. */
	if (flags == DB_POSITION) {
		int_n = dbc_n->internal;
		int_orig = dbc_orig->internal;

		dbc_n->flags |= dbc_orig->flags & ~DBC_OWN_LID;

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
		case DB_UNKNOWN:
		default:
			ret = __db_unknown_type(dbp->dbenv,
			    "__db_c_idup", dbc_orig->dbtype);
			goto err;
		}
	}

	/* Copy the locking flags to the new cursor. */
	F_SET(dbc_n,
	    F_ISSET(dbc_orig, DBC_WRITECURSOR | DBC_DIRTY_READ | DBC_DEGREE_2));

	/*
	 * If we're in CDB and this isn't an offpage dup cursor, then
	 * we need to get a lock for the duplicated cursor.
	 */
	if (CDB_LOCKING(dbp->dbenv) && !F_ISSET(dbc_n, DBC_OPD) &&
	    (ret = __lock_get(dbp->dbenv, dbc_n->locker, 0,
	    &dbc_n->lock_dbt, F_ISSET(dbc_orig, DBC_WRITECURSOR) ?
	    DB_LOCK_IWRITE : DB_LOCK_READ, &dbc_n->mylock)) != 0)
		goto err;

	*dbcp = dbc_n;
	return (0);

err:	(void)__db_c_close(dbc_n);
	return (ret);
}

/*
 * __db_c_newopd --
 *	Create a new off-page duplicate cursor.
 *
 * PUBLIC: int __db_c_newopd __P((DBC *, db_pgno_t, DBC *, DBC **));
 */
int
__db_c_newopd(dbc_parent, root, oldopd, dbcp)
	DBC *dbc_parent;
	db_pgno_t root;
	DBC *oldopd;
	DBC **dbcp;
{
	DB *dbp;
	DBC *opd;
	DBTYPE dbtype;
	int ret;

	dbp = dbc_parent->dbp;
	dbtype = (dbp->dup_compare == NULL) ? DB_RECNO : DB_BTREE;

	/*
	 * On failure, we want to default to returning the old off-page dup
	 * cursor, if any;  our caller can't be left with a dangling pointer
	 * to a freed cursor.  On error the only allowable behavior is to
	 * close the cursor (and the old OPD cursor it in turn points to), so
	 * this should be safe.
	 */
	*dbcp = oldopd;

	if ((ret = __db_cursor_int(dbp,
	    dbc_parent->txn, dbtype, root, 1, dbc_parent->locker, &opd)) != 0)
		return (ret);

	*dbcp = opd;

	/*
	 * Check to see if we already have an off-page dup cursor that we've
	 * passed in.  If we do, close it.  It'd be nice to use it again
	 * if it's a cursor belonging to the right tree, but if we're doing
	 * a cursor-relative operation this might not be safe, so for now
	 * we'll take the easy way out and always close and reopen.
	 *
	 * Note that under no circumstances do we want to close the old
	 * cursor without returning a valid new one;  we don't want to
	 * leave the main cursor in our caller with a non-NULL pointer
	 * to a freed off-page dup cursor.
	 */
	if (oldopd != NULL && (ret = __db_c_close(oldopd)) != 0)
		return (ret);

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
	DB_MPOOLFILE *mpf;
	db_pgno_t pgno;
	u_int32_t multi, tmp_dirty, tmp_flags, tmp_rmw;
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
	mpf = dbp->mpf;
	dbc_n = NULL;
	opd = NULL;

	/* Clear OR'd in additional bits so we can check for flag equality. */
	tmp_rmw = LF_ISSET(DB_RMW);
	LF_CLR(DB_RMW);

	tmp_dirty = LF_ISSET(DB_DIRTY_READ);
	LF_CLR(DB_DIRTY_READ);

	multi = LF_ISSET(DB_MULTIPLE|DB_MULTIPLE_KEY);
	LF_CLR(DB_MULTIPLE|DB_MULTIPLE_KEY);

	/*
	 * Return a cursor's record number.  It has nothing to do with the
	 * cursor get code except that it was put into the interface.
	 */
	if (flags == DB_GET_RECNO) {
		if (tmp_rmw)
			F_SET(dbc_arg, DBC_RMW);
		if (tmp_dirty)
			F_SET(dbc_arg, DBC_DIRTY_READ);
		ret = __bam_c_rget(dbc_arg, data);
		if (tmp_rmw)
			F_CLR(dbc_arg, DBC_RMW);
		if (tmp_dirty)
			F_CLR(dbc_arg, DBC_DIRTY_READ);
		return (ret);
	}

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
		if ((ret = __db_c_idup(cp->opd, &opd, DB_POSITION)) != 0)
			return (ret);

		switch (ret =
		    opd->c_am_get(opd, key, data, flags, NULL)) {
		case 0:
			goto done;
		case DB_NOTFOUND:
			/*
			 * Translate DB_NOTFOUND failures for the DB_NEXT and
			 * DB_PREV operations into a subsequent operation on
			 * the parent cursor.
			 */
			if (flags == DB_NEXT || flags == DB_PREV) {
				if ((ret = __db_c_close(opd)) != 0)
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
		tmp_flags = DB_POSITION;
		break;
	default:
		tmp_flags = 0;
		break;
	}

	if (tmp_dirty)
		F_SET(dbc_arg, DBC_DIRTY_READ);

	/*
	 * If this cursor is going to be closed immediately, we don't
	 * need to take precautions to clean it up on error.
	 */
	if (F_ISSET(dbc_arg, DBC_TRANSIENT))
		dbc_n = dbc_arg;
	else {
		ret = __db_c_idup(dbc_arg, &dbc_n, tmp_flags);
		if (tmp_dirty)
			F_CLR(dbc_arg, DBC_DIRTY_READ);

		if (ret != 0)
			goto err;
		COPY_RET_MEM(dbc_arg, dbc_n);
	}

	if (tmp_rmw)
		F_SET(dbc_n, DBC_RMW);

	switch (multi) {
	case DB_MULTIPLE:
		F_SET(dbc_n, DBC_MULTIPLE);
		break;
	case DB_MULTIPLE_KEY:
		F_SET(dbc_n, DBC_MULTIPLE_KEY);
		break;
	case DB_MULTIPLE | DB_MULTIPLE_KEY:
		F_SET(dbc_n, DBC_MULTIPLE|DBC_MULTIPLE_KEY);
		break;
	case 0:
	default:
		break;
	}

	pgno = PGNO_INVALID;
	ret = dbc_n->c_am_get(dbc_n, key, data, flags, &pgno);
	if (tmp_rmw)
		F_CLR(dbc_n, DBC_RMW);
	if (tmp_dirty)
		F_CLR(dbc_arg, DBC_DIRTY_READ);
	F_CLR(dbc_n, DBC_MULTIPLE|DBC_MULTIPLE_KEY);
	if (ret != 0)
		goto err;

	cp_n = dbc_n->internal;

	/*
	 * We may be referencing a new off-page duplicates tree.  Acquire
	 * a new cursor and call the underlying function.
	 */
	if (pgno != PGNO_INVALID) {
		if ((ret = __db_c_newopd(dbc_arg,
		    pgno, cp_n->opd, &cp_n->opd)) != 0)
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
		case DB_GET_BOTHC:
		case DB_GET_BOTH_RANGE:
			tmp_flags = flags;
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
		    __memp_fget(mpf, &cp_n->pgno, 0, &cp_n->page)) != 0)
			goto err;

		if ((ret = __db_ret(dbp, cp_n->page, cp_n->indx,
		    key, &dbc_arg->rkey->data, &dbc_arg->rkey->ulen)) != 0)
			goto err;
	}
	if (multi != 0) {
		/*
		 * Even if fetching from the OPD cursor we need a duplicate
		 * primary cursor if we are going after multiple keys.
		 */
		if (dbc_n == NULL) {
			/*
			 * Non-"_KEY" DB_MULTIPLE doesn't move the main cursor,
			 * so it's safe to just use dbc_arg, unless dbc_arg
			 * has an open OPD cursor whose state might need to
			 * be preserved.
			 */
			if ((!(multi & DB_MULTIPLE_KEY) &&
			    dbc_arg->internal->opd == NULL) ||
			    F_ISSET(dbc_arg, DBC_TRANSIENT))
				dbc_n = dbc_arg;
			else {
				if ((ret = __db_c_idup(dbc_arg,
				    &dbc_n, DB_POSITION)) != 0)
					goto err;
				if ((ret = dbc_n->c_am_get(dbc_n,
				    key, data, DB_CURRENT, &pgno)) != 0)
					goto err;
			}
			cp_n = dbc_n->internal;
		}

		/*
		 * If opd is set then we dupped the opd that we came in with.
		 * When we return we may have a new opd if we went to another
		 * key.
		 */
		if (opd != NULL) {
			DB_ASSERT(cp_n->opd == NULL);
			cp_n->opd = opd;
			opd = NULL;
		}

		/*
		 * Bulk get doesn't use __db_retcopy, so data.size won't
		 * get set up unless there is an error.  Assume success
		 * here.  This is the only call to c_am_bulk, and it avoids
		 * setting it exactly the same everywhere.  If we have an
		 * DB_BUFFER_SMALL error, it'll get overwritten with the
		 * needed value.
		 */
		data->size = data->ulen;
		ret = dbc_n->c_am_bulk(dbc_n, data, flags | multi);
	} else if (!F_ISSET(data, DB_DBT_ISSET)) {
		dbc = opd != NULL ? opd : cp_n->opd != NULL ? cp_n->opd : dbc_n;
		type = TYPE(dbc->internal->page);
		ret = __db_ret(dbp, dbc->internal->page, dbc->internal->indx +
		    (type == P_LBTREE || type == P_HASH ? O_INDX : 0),
		    data, &dbc_arg->rdata->data, &dbc_arg->rdata->ulen);
	}

err:	/* Don't pass DB_DBT_ISSET back to application level, error or no. */
	F_CLR(key, DB_DBT_ISSET);
	F_CLR(data, DB_DBT_ISSET);

	/* Cleanup and cursor resolution. */
	if (opd != NULL) {
		/*
		 * To support dirty reads we must reget the write lock
		 * if we have just stepped off a deleted record.
		 * Since the OPD cursor does not know anything
		 * about the referencing page or cursor we need
		 * to peek at the OPD cursor and get the lock here.
		 */
		if (F_ISSET(dbc_arg->dbp, DB_AM_DIRTY) &&
		     F_ISSET((BTREE_CURSOR *)
		     dbc_arg->internal->opd->internal, C_DELETED))
			if ((t_ret =
			    dbc_arg->c_am_writelock(dbc_arg)) != 0 && ret != 0)
				ret = t_ret;
		if ((t_ret = __db_c_cleanup(
		    dbc_arg->internal->opd, opd, ret)) != 0 && ret == 0)
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
	DB_ENV *dbenv;
	DB *dbp, *sdbp;
	DBC *dbc_n, *oldopd, *opd, *sdbc, *pdbc;
	DBT olddata, oldpkey, oldskey, newdata, pkey, skey, temppkey, tempskey;
	db_pgno_t pgno;
	int cmp, have_oldrec, ispartial, nodel, re_pad, ret, rmw, t_ret;
	u_int32_t re_len, size, tmp_flags;

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
	dbenv = dbp->dbenv;
	sdbp = NULL;
	pdbc = dbc_n = NULL;
	memset(&newdata, 0, sizeof(DBT));
	ret = 0;

	/*
	 * We do multiple cursor operations in some cases and subsequently
	 * access the data DBT information.  Set DB_DBT_MALLOC so we don't risk
	 * modification of the data between our uses of it.
	 */
	memset(&olddata, 0, sizeof(DBT));
	F_SET(&olddata, DB_DBT_MALLOC);

	/*
	 * Putting to secondary indices is forbidden;  when we need
	 * to internally update one, we'll call this with a private
	 * synonym for DB_KEYLAST, DB_UPDATE_SECONDARY, which does
	 * the right thing but won't return an error from cputchk().
	 */
	if (flags == DB_UPDATE_SECONDARY)
		flags = DB_KEYLAST;

	CDB_LOCKING_INIT(dbp, dbc_arg);

	/*
	 * Check to see if we are a primary and have secondary indices.
	 * If we are not, we save ourselves a good bit of trouble and
	 * just skip to the "normal" put.
	 */
	if (LIST_FIRST(&dbp->s_secondaries) == NULL)
		goto skip_s_update;

	/*
	 * We have at least one secondary which we may need to update.
	 *
	 * There is a rather vile locking issue here.  Secondary gets
	 * will always involve acquiring a read lock in the secondary,
	 * then acquiring a read lock in the primary.  Ideally, we
	 * would likewise perform puts by updating all the secondaries
	 * first, then doing the actual put in the primary, to avoid
	 * deadlock (since having multiple threads doing secondary
	 * gets and puts simultaneously is probably a common case).
	 *
	 * However, if this put is a put-overwrite--and we have no way to
	 * tell in advance whether it will be--we may need to delete
	 * an outdated secondary key.  In order to find that old
	 * secondary key, we need to get the record we're overwriting,
	 * before we overwrite it.
	 *
	 * (XXX: It would be nice to avoid this extra get, and have the
	 * underlying put routines somehow pass us the old record
	 * since they need to traverse the tree anyway.  I'm saving
	 * this optimization for later, as it's a lot of work, and it
	 * would be hard to fit into this locking paradigm anyway.)
	 *
	 * The simple thing to do would be to go get the old record before
	 * we do anything else.  Unfortunately, though, doing so would
	 * violate our "secondary, then primary" lock acquisition
	 * ordering--even in the common case where no old primary record
	 * exists, we'll still acquire and keep a lock on the page where
	 * we're about to do the primary insert.
	 *
	 * To get around this, we do the following gyrations, which
	 * hopefully solve this problem in the common case:
	 *
	 * 1) If this is a c_put(DB_CURRENT), go ahead and get the
	 *    old record.  We already hold the lock on this page in
	 *    the primary, so no harm done, and we'll need the primary
	 *    key (which we weren't passed in this case) to do any
	 *    secondary puts anyway.
	 *
	 * 2) If we're doing a partial put, we need to perform the
	 *    get on the primary key right away, since we don't have
	 *    the whole datum that the secondary key is based on.
	 *    We may also need to pad out the record if the primary
	 *    has a fixed record length.
	 *
	 * 3) Loop through the secondary indices, putting into each a
	 *    new secondary key that corresponds to the new record.
	 *
	 * 4) If we haven't done so in (1) or (2), get the old primary
	 *    key/data pair.  If one does not exist--the common case--we're
	 *    done with secondary indices, and can go straight on to the
	 *    primary put.
	 *
	 * 5) If we do have an old primary key/data pair, however, we need
	 *    to loop through all the secondaries a second time and delete
	 *    the old secondary in each.
	 */
	memset(&pkey, 0, sizeof(DBT));
	have_oldrec = nodel = 0;

	/*
	 * Primary indices can't have duplicates, so only DB_CURRENT,
	 * DB_KEYFIRST, and DB_KEYLAST make any sense.  Other flags
	 * should have been caught by the checking routine, but
	 * add a sprinkling of paranoia.
	 */
	DB_ASSERT(flags == DB_CURRENT ||
	    flags == DB_KEYFIRST || flags == DB_KEYLAST);

	/*
	 * We'll want to use DB_RMW in a few places, but it's only legal
	 * when locking is on.
	 */
	rmw = STD_LOCKING(dbc_arg) ? DB_RMW : 0;

	if (flags == DB_CURRENT) {		/* Step 1. */
		/*
		 * This is safe to do on the cursor we already have;
		 * error or no, it won't move.
		 *
		 * We use DB_RMW for all of these gets because we'll be
		 * writing soon enough in the "normal" put code.  In
		 * transactional databases we'll hold those write locks
		 * even if we close the cursor we're reading with.
		 *
		 * The DB_KEYEMPTY return needs special handling -- if the
		 * cursor is on a deleted key, we return DB_NOTFOUND.
		 */
		ret = __db_c_get(dbc_arg, &pkey, &olddata, rmw | DB_CURRENT);
		if (ret == DB_KEYEMPTY)
			ret = DB_NOTFOUND;
		if (ret != 0)
			goto err;

		have_oldrec = 1; /* We've looked for the old record. */
	} else {
		/*
		 * Set pkey so we can use &pkey everywhere instead of key.
		 * If DB_CURRENT is set and there is a key at the current
		 * location, pkey will be overwritten before it's used.
		 */
		pkey.data = key->data;
		pkey.size = key->size;
	}

	/*
	 * Check for partial puts (step 2).
	 */
	if (F_ISSET(data, DB_DBT_PARTIAL)) {
		if (!have_oldrec && !nodel) {
			/*
			 * We're going to have to search the tree for the
			 * specified key.  Dup a cursor (so we have the same
			 * locking info) and do a c_get.
			 */
			if ((ret = __db_c_idup(dbc_arg, &pdbc, 0)) != 0)
				goto err;

			/* We should have gotten DB_CURRENT in step 1. */
			DB_ASSERT(flags != DB_CURRENT);

			ret = __db_c_get(pdbc, &pkey, &olddata, rmw | DB_SET);
			if (ret == DB_KEYEMPTY || ret == DB_NOTFOUND) {
				nodel = 1;
				ret = 0;
			}
			if ((t_ret = __db_c_close(pdbc)) != 0)
				ret = t_ret;
			if (ret != 0)
				goto err;

			have_oldrec = 1;
		}

		/*
		 * Now build the new datum from olddata and the partial
		 * data we were given.
		 */
		if ((ret =
		    __db_buildpartial(dbp, &olddata, data, &newdata)) != 0)
			goto err;
		ispartial = 1;
	} else
		ispartial = 0;

	/*
	 * Handle fixed-length records.  If the primary database has
	 * fixed-length records, we need to pad out the datum before
	 * we pass it into the callback function;  we always index the
	 * "real" record.
	 */
	if ((dbp->type == DB_RECNO && F_ISSET(dbp, DB_AM_FIXEDLEN)) ||
	    (dbp->type == DB_QUEUE)) {
		if (dbp->type == DB_QUEUE) {
			re_len = ((QUEUE *)dbp->q_internal)->re_len;
			re_pad = ((QUEUE *)dbp->q_internal)->re_pad;
		} else {
			re_len = ((BTREE *)dbp->bt_internal)->re_len;
			re_pad = ((BTREE *)dbp->bt_internal)->re_pad;
		}

		size = ispartial ? newdata.size : data->size;
		if (size > re_len) {
			ret = __db_rec_toobig(dbenv, size, re_len);
			goto err;
		} else if (size < re_len) {
			/*
			 * If we're not doing a partial put, copy
			 * data->data into newdata.data, then pad out
			 * newdata.data.
			 *
			 * If we're doing a partial put, the data
			 * we want are already in newdata.data;  we
			 * just need to pad.
			 *
			 * Either way, realloc is safe.
			 */
			if ((ret =
			    __os_realloc(dbenv, re_len, &newdata.data)) != 0)
				goto err;
			if (!ispartial)
				memcpy(newdata.data, data->data, size);
			memset((u_int8_t *)newdata.data + size, re_pad,
			    re_len - size);
			newdata.size = re_len;
			ispartial = 1;
		}
	}

	/*
	 * Loop through the secondaries.  (Step 3.)
	 *
	 * Note that __db_s_first and __db_s_next will take care of
	 * thread-locking and refcounting issues.
	 */
	for (sdbp = __db_s_first(dbp);
	    sdbp != NULL && ret == 0; ret = __db_s_next(&sdbp)) {
		/*
		 * Call the callback for this secondary, to get the
		 * appropriate secondary key.
		 */
		memset(&skey, 0, sizeof(DBT));
		if ((ret = sdbp->s_callback(sdbp,
		    &pkey, ispartial ? &newdata : data, &skey)) != 0) {
			if (ret == DB_DONOTINDEX)
				/*
				 * The callback returned a null value--don't
				 * put this key in the secondary.  Just
				 * move on to the next one--we'll handle
				 * any necessary deletes in step 5.
				 */
				continue;
			else
				goto err;
		}

		/*
		 * Open a cursor in this secondary.
		 *
		 * Use the same locker ID as our primary cursor, so that
		 * we're guaranteed that the locks don't conflict (e.g. in CDB
		 * or if we're subdatabases that share and want to lock a
		 * metadata page).
		 */
		if ((ret = __db_cursor_int(sdbp, dbc_arg->txn, sdbp->type,
		    PGNO_INVALID, 0, dbc_arg->locker, &sdbc)) != 0)
			goto err;

		/*
		 * If we're in CDB, updates will fail since the new cursor
		 * isn't a writer.  However, we hold the WRITE lock in the
		 * primary and will for as long as our new cursor lasts,
		 * and the primary and secondary share a lock file ID,
		 * so it's safe to consider this a WRITER.  The close
		 * routine won't try to put anything because we don't
		 * really have a lock.
		 */
		if (CDB_LOCKING(dbenv)) {
			DB_ASSERT(sdbc->mylock.off == LOCK_INVALID);
			F_SET(sdbc, DBC_WRITER);
		}

		/*
		 * There are three cases here--
		 * 1) The secondary supports sorted duplicates.
		 *	If we attempt to put a secondary/primary pair
		 *	that already exists, that's a duplicate duplicate,
		 *	and c_put will return DB_KEYEXIST (see __db_duperr).
		 *	This will leave us with exactly one copy of the
		 *	secondary/primary pair, and this is just right--we'll
		 *	avoid deleting it later, as the old and new secondaries
		 *	will match (since the old secondary is the dup dup
		 *	that's already there).
		 * 2) The secondary supports duplicates, but they're not
		 *	sorted.  We need to avoid putting a duplicate
		 *	duplicate, because the matching old and new secondaries
		 *	will prevent us from deleting anything and we'll
		 *	wind up with two secondary records that point to the
		 *	same primary key.  Do a c_get(DB_GET_BOTH);  only
		 *	do the put if the secondary doesn't exist.
		 * 3) The secondary doesn't support duplicates at all.
		 *	In this case, secondary keys must be unique;  if
		 *	another primary key already exists for this
		 *	secondary key, we have to either overwrite it or
		 *	not put this one, and in either case we've
		 *	corrupted the secondary index.  Do a c_get(DB_SET).
		 *	If the secondary/primary pair already exists, do
		 *	nothing;  if the secondary exists with a different
		 *	primary, return an error;  and if the secondary
		 *	does not exist, put it.
		 */
		if (!F_ISSET(sdbp, DB_AM_DUP)) {
			/* Case 3. */
			memset(&oldpkey, 0, sizeof(DBT));
			F_SET(&oldpkey, DB_DBT_MALLOC);
			ret = __db_c_get(sdbc,
			    &skey, &oldpkey, rmw | DB_SET);
			if (ret == 0) {
				cmp = __bam_defcmp(sdbp, &oldpkey, &pkey);
				__os_ufree(dbenv, oldpkey.data);
				if (cmp != 0) {
					__db_err(dbenv, "%s%s",
			    "Put results in a non-unique secondary key in an ",
			    "index not configured to support duplicates");
					ret = EINVAL;
					goto skipput;
				}
			} else if (ret != DB_NOTFOUND && ret != DB_KEYEMPTY)
				goto skipput;
		} else if (!F_ISSET(sdbp, DB_AM_DUPSORT)) {
			/* Case 2. */
			memset(&tempskey, 0, sizeof(DBT));
			tempskey.data = skey.data;
			tempskey.size = skey.size;
			memset(&temppkey, 0, sizeof(DBT));
			temppkey.data = pkey.data;
			temppkey.size = pkey.size;
			ret = __db_c_get(sdbc, &tempskey, &temppkey,
			    rmw | DB_GET_BOTH);
			if (ret != DB_NOTFOUND && ret != DB_KEYEMPTY)
				goto skipput;
		}

		ret = __db_c_put(sdbc, &skey, &pkey, DB_UPDATE_SECONDARY);

		/*
		 * We don't know yet whether this was a put-overwrite that
		 * in fact changed nothing.  If it was, we may get DB_KEYEXIST.
		 * This is not an error.
		 */
		if (ret == DB_KEYEXIST)
			ret = 0;

skipput:	FREE_IF_NEEDED(sdbp, &skey)

		if ((t_ret = __db_c_close(sdbc)) != 0 && ret == 0)
			ret = t_ret;

		if (ret != 0)
			goto err;
	}
	if (ret != 0)
		goto err;

	/* If still necessary, go get the old primary key/data.  (Step 4.) */
	if (!have_oldrec) {
		/* See the comments in step 2.  This is real familiar. */
		if ((ret = __db_c_idup(dbc_arg, &pdbc, 0)) != 0)
			goto err;
		DB_ASSERT(flags != DB_CURRENT);
		pkey.data = key->data;
		pkey.size = key->size;
		ret = __db_c_get(pdbc, &pkey, &olddata, rmw | DB_SET);
		if (ret == DB_KEYEMPTY || ret == DB_NOTFOUND) {
			nodel = 1;
			ret = 0;
		}
		if ((t_ret = __db_c_close(pdbc)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err;
		have_oldrec = 1;
	}

	/*
	 * If we don't follow this goto, we do in fact have an old record
	 * we may need to go delete.  (Step 5).
	 */
	if (nodel)
		goto skip_s_update;

	for (sdbp = __db_s_first(dbp);
	    sdbp != NULL && ret == 0; ret = __db_s_next(&sdbp)) {
		/*
		 * Call the callback for this secondary to get the
		 * old secondary key.
		 */
		memset(&oldskey, 0, sizeof(DBT));
		if ((ret = sdbp->s_callback(sdbp,
		    &pkey, &olddata, &oldskey)) != 0) {
			if (ret == DB_DONOTINDEX)
				/*
				 * The callback returned a null value--there's
				 * nothing to delete.  Go on to the next
				 * secondary.
				 */
				continue;
			else
				goto err;
		}
		memset(&skey, 0, sizeof(DBT));
		if ((ret = sdbp->s_callback(sdbp,
		    &pkey, ispartial ? &newdata : data, &skey)) != 0 &&
		    ret != DB_DONOTINDEX)
			goto err;

		/*
		 * If there is no new secondary key, or if the old secondary
		 * key is different from the new secondary key, then
		 * we need to delete the old one.
		 *
		 * Note that bt_compare is (and must be) set no matter
		 * what access method we're in.
		 */
		sdbc = NULL;
		if (ret == DB_DONOTINDEX ||
		    ((BTREE *)sdbp->bt_internal)->bt_compare(sdbp,
		    &oldskey, &skey) != 0) {
			if ((ret = __db_cursor_int(
			    sdbp, dbc_arg->txn, sdbp->type,
			    PGNO_INVALID, 0, dbc_arg->locker, &sdbc)) != 0)
				goto err;
			if (CDB_LOCKING(dbenv)) {
				DB_ASSERT(sdbc->mylock.off == LOCK_INVALID);
				F_SET(sdbc, DBC_WRITER);
			}

			/*
			 * Don't let c_get(DB_GET_BOTH) stomp on
			 * our data.  Use a temp DBT instead.
			 */
			memset(&tempskey, 0, sizeof(DBT));
			tempskey.data = oldskey.data;
			tempskey.size = oldskey.size;
			memset(&temppkey, 0, sizeof(DBT));
			temppkey.data = pkey.data;
			temppkey.size = pkey.size;
			if ((ret = __db_c_get(sdbc,
			    &tempskey, &temppkey, rmw | DB_GET_BOTH)) == 0)
				ret = __db_c_del(sdbc, DB_UPDATE_SECONDARY);
			else if (ret == DB_NOTFOUND)
				ret = __db_secondary_corrupt(dbp);
		}

		FREE_IF_NEEDED(sdbp, &skey);
		FREE_IF_NEEDED(sdbp, &oldskey);
		if (sdbc != NULL && (t_ret = __db_c_close(sdbc)) != 0 &&
		    ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto err;
	}

	/* Secondary index updates are now done.  On to the "real" stuff. */

skip_s_update:
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
		if ((ret = __db_c_dup(dbc_arg, &dbc_n, DB_POSITION)) != 0)
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
	    flags == DB_BEFORE || flags == DB_CURRENT ? DB_POSITION : 0;
	 */
	tmp_flags = DB_POSITION;

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
		oldopd = dbc_n->internal->opd;
		if ((ret = __db_c_newopd(dbc_arg, pgno, oldopd, &opd)) != 0) {
			dbc_n->internal->opd = opd;
			goto err;
		}

		dbc_n->internal->opd = opd;

		if ((ret = opd->c_am_put(
		    opd, key, data, flags, NULL)) != 0)
			goto err;
	}

done:
err:	/* Cleanup and cursor resolution. */
	if ((t_ret = __db_c_cleanup(dbc_arg, dbc_n, ret)) != 0 && ret == 0)
		ret = t_ret;

	/* If newdata or olddata were used, free their buffers. */
	if (newdata.data != NULL)
		__os_free(dbenv, newdata.data);
	if (olddata.data != NULL)
		__os_ufree(dbenv, olddata.data);

	CDB_LOCKING_DONE(dbp, dbc_arg);

	if (sdbp != NULL && (t_ret = __db_s_done(sdbp)) != 0 && ret == 0)
		ret = t_ret;

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

	/*
	 * If we run into this error while updating a secondary index,
	 * don't yell--there's no clean way to pass DB_NODUPDATA in along
	 * with DB_UPDATE_SECONDARY, but we may run into this problem
	 * in a normal, non-error course of events.
	 *
	 * !!!
	 * If and when we ever permit duplicate duplicates in sorted-dup
	 * databases, we need to either change the secondary index code
	 * to check for dup dups, or we need to maintain the implicit
	 * "DB_NODUPDATA" behavior for databases with DB_AM_SECONDARY set.
	 */
	if (flags != DB_NODUPDATA && !F_ISSET(dbp, DB_AM_SECONDARY))
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
	DB_MPOOLFILE *mpf;
	int ret, t_ret;

	dbp = dbc->dbp;
	mpf = dbp->mpf;
	internal = dbc->internal;
	ret = 0;

	/* Discard any pages we're holding. */
	if (internal->page != NULL) {
		if ((t_ret =
		    __memp_fput(mpf, internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		internal->page = NULL;
	}
	opd = internal->opd;
	if (opd != NULL && opd->internal->page != NULL) {
		if ((t_ret =
		    __memp_fput(mpf, opd->internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		opd->internal->page = NULL;
	}

	/*
	 * If dbc_n is NULL, there's no internal cursor swapping to be done
	 * and no dbc_n to close--we probably did the entire operation on an
	 * offpage duplicate cursor.  Just return.
	 *
	 * If dbc and dbc_n are the same, we're either inside a DB->{put/get}
	 * operation, and as an optimization we performed the operation on
	 * the main cursor rather than on a duplicated one, or we're in a
	 * bulk get that can't have moved the cursor (DB_MULTIPLE with the
	 * initial c_get operation on an off-page dup cursor).  Just
	 * return--either we know we didn't move the cursor, or we're going
	 * to close it before we return to application code, so we're sure
	 * not to visibly violate the "cursor stays put on error" rule.
	 */
	if (dbc_n == NULL || dbc == dbc_n)
		return (ret);

	if (dbc_n->internal->page != NULL) {
		if ((t_ret = __memp_fput(
		    mpf, dbc_n->internal->page, 0)) != 0 && ret == 0)
			ret = t_ret;
		dbc_n->internal->page = NULL;
	}
	opd = dbc_n->internal->opd;
	if (opd != NULL && opd->internal->page != NULL) {
		if ((t_ret =
		    __memp_fput(mpf, opd->internal->page, 0)) != 0 && ret == 0)
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
	if ((t_ret = __db_c_close(dbc_n)) != 0 && ret == 0)
		ret = t_ret;

	/*
	 * If this was an update that is supporting dirty reads
	 * then we may have just swapped our read for a write lock
	 * which is held by the surviving cursor.  We need
	 * to explicitly downgrade this lock.  The closed cursor
	 * may only have had a read lock.
	 */
	if (F_ISSET(dbp, DB_AM_DIRTY) &&
	    dbc->internal->lock_mode == DB_LOCK_WRITE) {
		if ((t_ret =
		    __TLPUT(dbc, dbc->internal->lock)) != 0 && ret == 0)
			ret = t_ret;
		if (t_ret == 0)
			dbc->internal->lock_mode = DB_LOCK_WWRITE;
	}

	return (ret);
}

/*
 * __db_c_secondary_get_pp --
 *	This wrapper function for DBC->c_pget() is the DBC->c_get() function
 *	for a secondary index cursor.
 *
 * PUBLIC: int __db_c_secondary_get_pp __P((DBC *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_secondary_get_pp(dbc, skey, data, flags)
	DBC *dbc;
	DBT *skey, *data;
	u_int32_t flags;
{

	DB_ASSERT(F_ISSET(dbc->dbp, DB_AM_SECONDARY));
	return (__db_c_pget_pp(dbc, skey, NULL, data, flags));
}

/*
 * __db_c_pget --
 *	Get a primary key/data pair through a secondary index.
 *
 * PUBLIC: int __db_c_pget __P((DBC *, DBT *, DBT *, DBT *, u_int32_t));
 */
int
__db_c_pget(dbc, skey, pkey, data, flags)
	DBC *dbc;
	DBT *skey, *pkey, *data;
	u_int32_t flags;
{
	DB *pdbp, *sdbp;
	DBC *pdbc;
	DBT *save_rdata, nullpkey;
	u_int32_t save_pkey_flags;
	int pkeymalloc, ret, t_ret;

	sdbp = dbc->dbp;
	pdbp = sdbp->s_primary;
	pkeymalloc = t_ret = 0;

	/*
	 * The challenging part of this function is getting the behavior
	 * right for all the various permutations of DBT flags.  The
	 * next several blocks handle the various cases we need to
	 * deal with specially.
	 */

	/*
	 * We may be called with a NULL pkey argument, if we've been
	 * wrapped by a 2-DBT get call.  If so, we need to use our
	 * own DBT.
	 */
	if (pkey == NULL) {
		memset(&nullpkey, 0, sizeof(DBT));
		pkey = &nullpkey;
	}

	/*
	 * DB_GET_RECNO is a special case, because we're interested not in
	 * the primary key/data pair, but rather in the primary's record
	 * number.
	 */
	if ((flags & DB_OPFLAGS_MASK) == DB_GET_RECNO)
		return (__db_c_pget_recno(dbc, pkey, data, flags));

	/*
	 * If the DBTs we've been passed don't have any of the
	 * user-specified memory management flags set, we want to make sure
	 * we return values using the DBTs dbc->rskey, dbc->rkey, and
	 * dbc->rdata, respectively.
	 *
	 * There are two tricky aspects to this:  first, we need to pass
	 * skey and pkey *in* to the initial c_get on the secondary key,
	 * since either or both may be looked at by it (depending on the
	 * get flag).  Second, we must not use a normal DB->get call
	 * on the secondary, even though that's what we want to accomplish,
	 * because the DB handle may be free-threaded.  Instead,
	 * we open a cursor, then take steps to ensure that we actually use
	 * the rkey/rdata from the *secondary* cursor.
	 *
	 * We accomplish all this by passing in the DBTs we started out
	 * with to the c_get, but having swapped the contents of rskey and
	 * rkey, respectively, into rkey and rdata;  __db_ret will treat
	 * them like the normal key/data pair in a c_get call, and will
	 * realloc them as need be (this is "step 1").  Then, for "step 2",
	 * we swap back rskey/rkey/rdata to normal, and do a get on the primary
	 * with the secondary dbc appointed as the owner of the returned-data
	 * memory.
	 *
	 * Note that in step 2, we copy the flags field in case we need to
	 * pass down a DB_DBT_PARTIAL or other flag that is compatible with
	 * letting DB do the memory management.
	 */
	/* Step 1. */
	save_rdata = dbc->rdata;
	dbc->rdata = dbc->rkey;
	dbc->rkey = dbc->rskey;

	/*
	 * It is correct, though slightly sick, to attempt a partial get
	 * of a primary key.  However, if we do so here, we'll never find the
	 * primary record;  clear the DB_DBT_PARTIAL field of pkey just
	 * for the duration of the next call.
	 */
	save_pkey_flags = pkey->flags;
	F_CLR(pkey, DB_DBT_PARTIAL);

	/*
	 * Now we can go ahead with the meat of this call.  First, get the
	 * primary key from the secondary index.  (What exactly we get depends
	 * on the flags, but the underlying cursor get will take care of the
	 * dirty work.)
	 */
	if ((ret = __db_c_get(dbc, skey, pkey, flags)) != 0) {
		/* Restore rskey/rkey/rdata and return. */
		pkey->flags = save_pkey_flags;
		dbc->rskey = dbc->rkey;
		dbc->rkey = dbc->rdata;
		dbc->rdata = save_rdata;
		goto err;
	}

	/* Restore pkey's flags in case we stomped the PARTIAL flag. */
	pkey->flags = save_pkey_flags;

	/*
	 * Restore the cursor's rskey, rkey, and rdata DBTs.  If DB
	 * is handling the memory management, we now have newly
	 * reallocated buffers and ulens in rkey and rdata which we want
	 * to put in rskey and rkey.  save_rdata contains the old value
	 * of dbc->rdata.
	 */
	dbc->rskey = dbc->rkey;
	dbc->rkey = dbc->rdata;
	dbc->rdata = save_rdata;

	/*
	 * Now we're ready for "step 2".  If either or both of pkey and
	 * data do not have memory management flags set--that is, if DB is
	 * managing their memory--we need to swap around the rkey/rdata
	 * structures so that we don't wind up trying to use memory managed
	 * by the primary database cursor, which we'll close before we return.
	 *
	 * !!!
	 * If you're carefully following the bouncing ball, you'll note
	 * that in the DB-managed case, the buffer hanging off of pkey is
	 * the same as dbc->rkey->data.  This is just fine;  we may well
	 * realloc and stomp on it when we return, if we're going a
	 * DB_GET_BOTH and need to return a different partial or key
	 * (depending on the comparison function), but this is safe.
	 *
	 * !!!
	 * We need to use __db_cursor_int here rather than simply calling
	 * pdbp->cursor, because otherwise, if we're in CDB, we'll
	 * allocate a new locker ID and leave ourselves open to deadlocks.
	 * (Even though we're only acquiring read locks, we'll still block
	 * if there are any waiters.)
	 */
	if ((ret = __db_cursor_int(pdbp,
	    dbc->txn, pdbp->type, PGNO_INVALID, 0, dbc->locker, &pdbc)) != 0)
		goto err;

	/*
	 * We're about to use pkey a second time.  If DB_DBT_MALLOC
	 * is set on it, we'll leak the memory we allocated the first time.
	 * Thus, set DB_DBT_REALLOC instead so that we reuse that memory
	 * instead of leaking it.
	 *
	 * !!!
	 * This assumes that the user must always specify a compatible
	 * realloc function if a malloc function is specified.  I think
	 * this is a reasonable requirement.
	 */
	if (F_ISSET(pkey, DB_DBT_MALLOC)) {
		F_CLR(pkey, DB_DBT_MALLOC);
		F_SET(pkey, DB_DBT_REALLOC);
		pkeymalloc = 1;
	}

	/*
	 * Do the actual get.  Set DBC_TRANSIENT since we don't care
	 * about preserving the position on error, and it's faster.
	 * SET_RET_MEM so that the secondary DBC owns any returned-data
	 * memory.
	 */
	F_SET(pdbc, DBC_TRANSIENT);
	SET_RET_MEM(pdbc, dbc);
	ret = __db_c_get(pdbc, pkey, data, DB_SET);

	/*
	 * If the item wasn't found in the primary, this is a bug;
	 * our secondary has somehow gotten corrupted, and contains
	 * elements that don't correspond to anything in the primary.
	 * Complain.
	 */
	if (ret == DB_NOTFOUND)
		ret = __db_secondary_corrupt(pdbp);

	/* Now close the primary cursor. */
	t_ret = __db_c_close(pdbc);

err:	if (pkeymalloc) {
		/*
		 * If pkey had a MALLOC flag, we need to restore it;
		 * otherwise, if the user frees the buffer but reuses
		 * the DBT without NULL'ing its data field or changing
		 * the flags, we may drop core.
		 */
		F_CLR(pkey, DB_DBT_REALLOC);
		F_SET(pkey, DB_DBT_MALLOC);
	}
	return (t_ret == 0 ? ret : t_ret);
}

/*
 * __db_c_pget_recno --
 *	Perform a DB_GET_RECNO c_pget on a secondary index.  Returns
 * the secondary's record number in the pkey field and the primary's
 * in the data field.
 */
static int
__db_c_pget_recno(sdbc, pkey, data, flags)
	DBC *sdbc;
	DBT *pkey, *data;
	u_int32_t flags;
{
	DB *pdbp, *sdbp;
	DB_ENV *dbenv;
	DBC *pdbc;
	DBT discardme, primary_key;
	db_recno_t oob;
	u_int32_t rmw;
	int ret, t_ret;

	sdbp = sdbc->dbp;
	pdbp = sdbp->s_primary;
	dbenv = sdbp->dbenv;
	pdbc = NULL;
	ret = t_ret = 0;

	rmw = LF_ISSET(DB_RMW);

	memset(&discardme, 0, sizeof(DBT));
	F_SET(&discardme, DB_DBT_USERMEM | DB_DBT_PARTIAL);

	oob = RECNO_OOB;

	/*
	 * If the primary is an rbtree, we want its record number, whether
	 * or not the secondary is one too.  Fetch the recno into "data".
	 *
	 * If it's not an rbtree, return RECNO_OOB in "data".
	 */
	if (F_ISSET(pdbp, DB_AM_RECNUM)) {
		/*
		 * Get the primary key, so we can find the record number
		 * in the primary. (We're uninterested in the secondary key.)
		 */
		memset(&primary_key, 0, sizeof(DBT));
		F_SET(&primary_key, DB_DBT_MALLOC);
		if ((ret = __db_c_get(sdbc,
		    &discardme, &primary_key, rmw | DB_CURRENT)) != 0)
			return (ret);

		/*
		 * Open a cursor on the primary, set it to the right record,
		 * and fetch its recno into "data".
		 *
		 * (See __db_c_pget for comments on the use of __db_cursor_int.)
		 *
		 * SET_RET_MEM so that the secondary DBC owns any returned-data
		 * memory.
		 */
		if ((ret = __db_cursor_int(pdbp, sdbc->txn,
		    pdbp->type, PGNO_INVALID, 0, sdbc->locker, &pdbc)) != 0)
			goto perr;
		SET_RET_MEM(pdbc, sdbc);
		if ((ret = __db_c_get(pdbc,
		    &primary_key, &discardme, rmw | DB_SET)) != 0)
			goto perr;

		ret = __db_c_get(pdbc, &discardme, data, rmw | DB_GET_RECNO);

perr:		__os_ufree(sdbp->dbenv, primary_key.data);
		if (pdbc != NULL &&
		    (t_ret = __db_c_close(pdbc)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			return (ret);
	} else if ((ret = __db_retcopy(dbenv, data, &oob,
		    sizeof(oob), &sdbc->rkey->data, &sdbc->rkey->ulen)) != 0)
			return (ret);

	/*
	 * If the secondary is an rbtree, we want its record number, whether
	 * or not the primary is one too.  Fetch the recno into "pkey".
	 *
	 * If it's not an rbtree, return RECNO_OOB in "pkey".
	 */
	if (F_ISSET(sdbp, DB_AM_RECNUM))
		return (__db_c_get(sdbc, &discardme, pkey, flags));
	else
		return (__db_retcopy(dbenv, pkey, &oob,
		    sizeof(oob), &sdbc->rdata->data, &sdbc->rdata->ulen));
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

/*
 * __db_c_del_secondary --
 *	Perform a delete operation on a secondary index:  call through
 *	to the primary and delete the primary record that this record
 *	points to.
 *
 *	Note that deleting the primary record will call c_del on all
 *	the secondaries, including this one;  thus, it is not necessary
 *	to execute both this function and an actual delete.
 */
static int
__db_c_del_secondary(dbc)
	DBC *dbc;
{
	DB *pdbp;
	DBC *pdbc;
	DBT skey, pkey;
	int ret, t_ret;

	memset(&skey, 0, sizeof(DBT));
	memset(&pkey, 0, sizeof(DBT));

	/*
	 * Get the current item that we're pointing at.
	 * We don't actually care about the secondary key, just
	 * the primary.
	 */
	F_SET(&skey, DB_DBT_PARTIAL | DB_DBT_USERMEM);
	if ((ret = __db_c_get(dbc, &skey, &pkey, DB_CURRENT)) != 0)
		return (ret);

	/*
	 * Create a cursor on the primary with our locker ID,
	 * so that when it calls back, we don't conflict.
	 *
	 * We create a cursor explicitly because there's no
	 * way to specify the same locker ID if we're using
	 * locking but not transactions if we use the DB->del
	 * interface.  This shouldn't be any less efficient
	 * anyway.
	 */
	pdbp = dbc->dbp->s_primary;
	if ((ret = __db_cursor_int(pdbp, dbc->txn,
	    pdbp->type, PGNO_INVALID, 0, dbc->locker, &pdbc)) != 0)
		return (ret);

	/*
	 * See comment in __db_c_put--if we're in CDB,
	 * we already hold the locks we need, and we need to flag
	 * the cursor as a WRITER so we don't run into errors
	 * when we try to delete.
	 */
	if (CDB_LOCKING(pdbp->dbenv)) {
		DB_ASSERT(pdbc->mylock.off == LOCK_INVALID);
		F_SET(pdbc, DBC_WRITER);
	}

	/*
	 * Set the new cursor to the correct primary key.  Then
	 * delete it.  We don't really care about the datum;
	 * just reuse our skey DBT.
	 *
	 * If the primary get returns DB_NOTFOUND, something is amiss--
	 * every record in the secondary should correspond to some record
	 * in the primary.
	 */
	if ((ret = __db_c_get(pdbc, &pkey, &skey,
	    (STD_LOCKING(dbc) ? DB_RMW : 0) | DB_SET)) == 0)
		ret = __db_c_del(pdbc, 0);
	else if (ret == DB_NOTFOUND)
		ret = __db_secondary_corrupt(pdbp);

	if ((t_ret = __db_c_close(pdbc)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __db_c_del_primary --
 *	Perform a delete operation on a primary index.  Loop through
 *	all the secondary indices which correspond to this primary
 *	database, and delete any secondary keys that point at the current
 *	record.
 *
 * PUBLIC: int __db_c_del_primary __P((DBC *));
 */
int
__db_c_del_primary(dbc)
	DBC *dbc;
{
	DB *dbp, *sdbp;
	DBC *sdbc;
	DBT data, pkey, skey, temppkey, tempskey;
	int ret, t_ret;

	dbp = dbc->dbp;

	/*
	 * If we're called at all, we have at least one secondary.
	 * (Unfortunately, we can't assert this without grabbing the mutex.)
	 * Get the current record so that we can construct appropriate
	 * secondary keys as needed.
	 */
	memset(&pkey, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	if ((ret = __db_c_get(dbc, &pkey, &data, DB_CURRENT)) != 0)
		return (ret);

	for (sdbp = __db_s_first(dbp);
	    sdbp != NULL && ret == 0; ret = __db_s_next(&sdbp)) {
		/*
		 * Get the secondary key for this secondary and the current
		 * item.
		 */
		memset(&skey, 0, sizeof(DBT));
		if ((ret = sdbp->s_callback(sdbp, &pkey, &data, &skey)) != 0) {
			/*
			 * If the current item isn't in this index, we
			 * have no work to do.  Proceed.
			 */
			if (ret == DB_DONOTINDEX)
				continue;

			/* We had a substantive error.  Bail. */
			FREE_IF_NEEDED(sdbp, &skey);
			goto done;
		}

		/* Open a secondary cursor. */
		if ((ret = __db_cursor_int(sdbp, dbc->txn, sdbp->type,
		    PGNO_INVALID, 0, dbc->locker, &sdbc)) != 0)
			goto done;
		/* See comment above and in __db_c_put. */
		if (CDB_LOCKING(sdbp->dbenv)) {
			DB_ASSERT(sdbc->mylock.off == LOCK_INVALID);
			F_SET(sdbc, DBC_WRITER);
		}

		/*
		 * Set the secondary cursor to the appropriate item.
		 * Delete it.
		 *
		 * We want to use DB_RMW if locking is on;  it's only
		 * legal then, though.
		 *
		 * !!!
		 * Don't stomp on any callback-allocated buffer in skey
		 * when we do a c_get(DB_GET_BOTH); use a temp DBT instead.
		 * Similarly, don't allow pkey to be invalidated when the
		 * cursor is closed.
		 */
		memset(&tempskey, 0, sizeof(DBT));
		tempskey.data = skey.data;
		tempskey.size = skey.size;
		memset(&temppkey, 0, sizeof(DBT));
		temppkey.data = pkey.data;
		temppkey.size = pkey.size;
		if ((ret = __db_c_get(sdbc, &tempskey, &temppkey,
		    (STD_LOCKING(dbc) ? DB_RMW : 0) | DB_GET_BOTH)) == 0)
			ret = __db_c_del(sdbc, DB_UPDATE_SECONDARY);
		else if (ret == DB_NOTFOUND)
			ret = __db_secondary_corrupt(dbp);

		FREE_IF_NEEDED(sdbp, &skey);

		if ((t_ret = __db_c_close(sdbc)) != 0 && ret == 0)
			ret = t_ret;
		if (ret != 0)
			goto done;
	}

done:	if (sdbp != NULL && (t_ret = __db_s_done(sdbp)) != 0 && ret == 0)
		ret = t_ret;
	return (ret);
}

/*
 * __db_s_first --
 *	Get the first secondary, if any are present, from the primary.
 *
 * PUBLIC: DB *__db_s_first __P((DB *));
 */
DB *
__db_s_first(pdbp)
	DB *pdbp;
{
	DB *sdbp;

	MUTEX_THREAD_LOCK(pdbp->dbenv, pdbp->mutexp);
	sdbp = LIST_FIRST(&pdbp->s_secondaries);

	/* See __db_s_next. */
	if (sdbp != NULL)
		sdbp->s_refcnt++;
	MUTEX_THREAD_UNLOCK(pdbp->dbenv, pdbp->mutexp);

	return (sdbp);
}

/*
 * __db_s_next --
 *	Get the next secondary in the list.
 *
 * PUBLIC: int __db_s_next __P((DB **));
 */
int
__db_s_next(sdbpp)
	DB **sdbpp;
{
	DB *sdbp, *pdbp, *closeme;
	int ret;

	/*
	 * Secondary indices are kept in a linked list, s_secondaries,
	 * off each primary DB handle.  If a primary is free-threaded,
	 * this list may only be traversed or modified while the primary's
	 * thread mutex is held.
	 *
	 * The tricky part is that we don't want to hold the thread mutex
	 * across the full set of secondary puts necessary for each primary
	 * put, or we'll wind up essentially single-threading all the puts
	 * to the handle;  the secondary puts will each take about as
	 * long as the primary does, and may require I/O.  So we instead
	 * hold the thread mutex only long enough to follow one link to the
	 * next secondary, and then we release it before performing the
	 * actual secondary put.
	 *
	 * The only danger here is that we might legitimately close a
	 * secondary index in one thread while another thread is performing
	 * a put and trying to update that same secondary index.  To
	 * prevent this from happening, we refcount the secondary handles.
	 * If close is called on a secondary index handle while we're putting
	 * to it, it won't really be closed--the refcount will simply drop,
	 * and we'll be responsible for closing it here.
	 */
	sdbp = *sdbpp;
	pdbp = sdbp->s_primary;
	closeme = NULL;

	MUTEX_THREAD_LOCK(pdbp->dbenv, pdbp->mutexp);
	DB_ASSERT(sdbp->s_refcnt != 0);
	if (--sdbp->s_refcnt == 0) {
		LIST_REMOVE(sdbp, s_links);
		closeme = sdbp;
	}
	sdbp = LIST_NEXT(sdbp, s_links);
	if (sdbp != NULL)
		sdbp->s_refcnt++;
	MUTEX_THREAD_UNLOCK(pdbp->dbenv, pdbp->mutexp);

	*sdbpp = sdbp;

	/*
	 * closeme->close() is a wrapper;  call __db_close explicitly.
	 */
	ret = closeme != NULL ? __db_close(closeme, NULL, 0) : 0;
	return (ret);
}

/*
 * __db_s_done --
 *	Properly decrement the refcount on a secondary database handle we're
 *	using, without calling __db_s_next.
 *
 * PUBLIC: int __db_s_done __P((DB *));
 */
int
__db_s_done(sdbp)
	DB *sdbp;
{
	DB *pdbp;
	int doclose;

	pdbp = sdbp->s_primary;
	doclose = 0;

	MUTEX_THREAD_LOCK(pdbp->dbenv, pdbp->mutexp);
	DB_ASSERT(sdbp->s_refcnt != 0);
	if (--sdbp->s_refcnt == 0) {
		LIST_REMOVE(sdbp, s_links);
		doclose = 1;
	}
	MUTEX_THREAD_UNLOCK(pdbp->dbenv, pdbp->mutexp);

	return (doclose ? __db_close(sdbp, NULL, 0) : 0);
}

/*
 * __db_buildpartial --
 *	Build the record that will result after a partial put is applied to
 *	an existing record.
 *
 *	This should probably be merged with __bam_build, but that requires
 *	a little trickery if we plan to keep the overflow-record optimization
 *	in that function.
 */
static int
__db_buildpartial(dbp, oldrec, partial, newrec)
	DB *dbp;
	DBT *oldrec, *partial, *newrec;
{
	int ret;
	u_int8_t *buf;
	u_int32_t len, nbytes;

	DB_ASSERT(F_ISSET(partial, DB_DBT_PARTIAL));

	memset(newrec, 0, sizeof(DBT));

	nbytes = __db_partsize(oldrec->size, partial);
	newrec->size = nbytes;

	if ((ret = __os_malloc(dbp->dbenv, nbytes, &buf)) != 0)
		return (ret);
	newrec->data = buf;

	/* Nul or pad out the buffer, for any part that isn't specified. */
	memset(buf,
	    F_ISSET(dbp, DB_AM_FIXEDLEN) ? ((BTREE *)dbp->bt_internal)->re_pad :
	    0, nbytes);

	/* Copy in any leading data from the original record. */
	memcpy(buf, oldrec->data,
	    partial->doff > oldrec->size ? oldrec->size : partial->doff);

	/* Copy the data from partial. */
	memcpy(buf + partial->doff, partial->data, partial->size);

	/* Copy any trailing data from the original record. */
	len = partial->doff + partial->dlen;
	if (oldrec->size > len)
		memcpy(buf + partial->doff + partial->size,
		    (u_int8_t *)oldrec->data + len, oldrec->size - len);

	return (0);
}

/*
 * __db_partsize --
 *	Given the number of bytes in an existing record and a DBT that
 *	is about to be partial-put, calculate the size of the record
 *	after the put.
 *
 *	This code is called from __bam_partsize.
 *
 * PUBLIC: u_int32_t __db_partsize __P((u_int32_t, DBT *));
 */
u_int32_t
__db_partsize(nbytes, data)
	u_int32_t nbytes;
	DBT *data;
{

	/*
	 * There are really two cases here:
	 *
	 * Case 1: We are replacing some bytes that do not exist (i.e., they
	 * are past the end of the record).  In this case the number of bytes
	 * we are replacing is irrelevant and all we care about is how many
	 * bytes we are going to add from offset.  So, the new record length
	 * is going to be the size of the new bytes (size) plus wherever those
	 * new bytes begin (doff).
	 *
	 * Case 2: All the bytes we are replacing exist.  Therefore, the new
	 * size is the oldsize (nbytes) minus the bytes we are replacing (dlen)
	 * plus the bytes we are adding (size).
	 */
	if (nbytes < data->doff + data->dlen)		/* Case 1 */
		return (data->doff + data->size);

	return (nbytes + data->size - data->dlen);	/* Case 2 */
}
