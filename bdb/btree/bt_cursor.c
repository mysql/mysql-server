/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: bt_cursor.c,v 11.88 2001/01/11 18:19:49 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "btree.h"
#include "lock.h"
#include "qam.h"
#include "common_ext.h"

static int  __bam_c_close __P((DBC *, db_pgno_t, int *));
static int  __bam_c_del __P((DBC *));
static int  __bam_c_destroy __P((DBC *));
static int  __bam_c_first __P((DBC *));
static int  __bam_c_get __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
static int  __bam_c_getstack __P((DBC *));
static int  __bam_c_last __P((DBC *));
static int  __bam_c_next __P((DBC *, int));
static int  __bam_c_physdel __P((DBC *));
static int  __bam_c_prev __P((DBC *));
static int  __bam_c_put __P((DBC *, DBT *, DBT *, u_int32_t, db_pgno_t *));
static void __bam_c_reset __P((BTREE_CURSOR *));
static int  __bam_c_search __P((DBC *, const DBT *, u_int32_t, int *));
static int  __bam_c_writelock __P((DBC *));
static int  __bam_getboth_finddatum __P((DBC *, DBT *));
static int  __bam_getbothc __P((DBC *, DBT *));
static int  __bam_isopd __P((DBC *, db_pgno_t *));

/*
 * Acquire a new page/lock.  If we hold a page/lock, discard the page, and
 * lock-couple the lock.
 *
 * !!!
 * We have to handle both where we have a lock to lock-couple and where we
 * don't -- we don't duplicate locks when we duplicate cursors if we are
 * running in a transaction environment as there's no point if locks are
 * never discarded.  This means that the cursor may or may not hold a lock.
 */
#undef	ACQUIRE
#define	ACQUIRE(dbc, mode, lpgno, lock, fpgno, pagep, ret) {\
	if ((pagep) != NULL) {						\
		ret = memp_fput((dbc)->dbp->mpf, pagep, 0);		\
		pagep = NULL;						\
	} else								\
		ret = 0;						\
	if ((ret) == 0 && STD_LOCKING(dbc))				\
		ret = __db_lget(dbc,					\
		    (lock).off == LOCK_INVALID ? 0 : LCK_COUPLE,	\
		    lpgno, mode, 0, &lock);				\
	else								\
		(lock).off = LOCK_INVALID;				\
	if ((ret) == 0)							\
		ret = memp_fget((dbc)->dbp->mpf, &(fpgno), 0, &(pagep));\
}

/* Acquire a new page/lock for a cursor. */
#undef	ACQUIRE_CUR
#define	ACQUIRE_CUR(dbc, mode, ret) {					\
	BTREE_CURSOR *__cp = (BTREE_CURSOR *)(dbc)->internal;		\
	ACQUIRE(dbc, mode,						\
	    __cp->pgno, __cp->lock, __cp->pgno, __cp->page, ret);	\
	if ((ret) == 0)							\
		__cp->lock_mode = (mode);				\
}

/*
 * Acquire a new page/lock for a cursor, and move the cursor on success.
 * The reason that this is a separate macro is because we don't want to
 * set the pgno/indx fields in the cursor until we actually have the lock,
 * otherwise the cursor adjust routines will adjust the cursor even though
 * we're not really on the page.
 */
#undef	ACQUIRE_CUR_SET
#define	ACQUIRE_CUR_SET(dbc, mode, p, ret) {				\
	BTREE_CURSOR *__cp = (BTREE_CURSOR *)(dbc)->internal;		\
	ACQUIRE(dbc, mode, p, __cp->lock, p, __cp->page, ret);		\
	if ((ret) == 0) {						\
		__cp->pgno = p;					\
		__cp->indx = 0;					\
		__cp->lock_mode = (mode);				\
	}								\
}

/*
 * Acquire a write lock if we don't already have one.
 *
 * !!!
 * See ACQUIRE macro on why we handle cursors that don't have locks.
 */
#undef	ACQUIRE_WRITE_LOCK
#define	ACQUIRE_WRITE_LOCK(dbc, ret) {					\
	BTREE_CURSOR *__cp = (BTREE_CURSOR *)(dbc)->internal;		\
	ret = 0;							\
	if (STD_LOCKING(dbc) &&						\
	    __cp->lock_mode != DB_LOCK_WRITE &&				\
	    ((ret) = __db_lget(dbc,					\
	    __cp->lock.off == LOCK_INVALID ? 0 : LCK_COUPLE,		\
	    __cp->pgno, DB_LOCK_WRITE, 0, &__cp->lock)) == 0)		\
		__cp->lock_mode = DB_LOCK_WRITE;			\
}

/* Discard the current page/lock. */
#undef	DISCARD
#define	DISCARD(dbc, ldiscard, lock, pagep, ret) {			\
	int __t_ret;							\
	if ((pagep) != NULL) {						\
		ret = memp_fput((dbc)->dbp->mpf, pagep, 0);		\
		pagep = NULL;						\
	} else								\
		ret = 0;						\
	if ((lock).off != LOCK_INVALID) {				\
		__t_ret = ldiscard ?					\
		    __LPUT((dbc), lock): __TLPUT((dbc), lock);		\
		if (__t_ret != 0 && (ret) == 0)				\
			ret = __t_ret;					\
		(lock).off = LOCK_INVALID;				\
	}								\
}

/* Discard the current page/lock for a cursor. */
#undef	DISCARD_CUR
#define	DISCARD_CUR(dbc, ret) {						\
	BTREE_CURSOR *__cp = (BTREE_CURSOR *)(dbc)->internal;		\
	DISCARD(dbc, 0, __cp->lock, __cp->page, ret);			\
	if ((ret) == 0)							\
		__cp->lock_mode = DB_LOCK_NG;				\
}

/* If on-page item is a deleted record. */
#undef	IS_DELETED
#define	IS_DELETED(page, indx)						\
	B_DISSET(GET_BKEYDATA(page,					\
	    (indx) + (TYPE(page) == P_LBTREE ? O_INDX : 0))->type)
#undef	IS_CUR_DELETED
#define	IS_CUR_DELETED(dbc)						\
	IS_DELETED((dbc)->internal->page, (dbc)->internal->indx)

/*
 * Test to see if two cursors could point to duplicates of the same key.
 * In the case of off-page duplicates they are they same, as the cursors
 * will be in the same off-page duplicate tree.  In the case of on-page
 * duplicates, the key index offsets must be the same.  For the last test,
 * as the original cursor may not have a valid page pointer, we use the
 * current cursor's.
 */
#undef	IS_DUPLICATE
#define	IS_DUPLICATE(dbc, i1, i2)					\
	    (((PAGE *)(dbc)->internal->page)->inp[i1] ==		\
	     ((PAGE *)(dbc)->internal->page)->inp[i2])
#undef	IS_CUR_DUPLICATE
#define	IS_CUR_DUPLICATE(dbc, orig_pgno, orig_indx)			\
	(F_ISSET(dbc, DBC_OPD) ||					\
	    (orig_pgno == (dbc)->internal->pgno &&			\
	    IS_DUPLICATE(dbc, (dbc)->internal->indx, orig_indx)))

/*
 * __bam_c_reset --
 *	Initialize internal cursor structure.
 */
static void
__bam_c_reset(cp)
	BTREE_CURSOR *cp;
{
	cp->csp = cp->sp;
	cp->lock.off = LOCK_INVALID;
	cp->lock_mode = DB_LOCK_NG;
	cp->recno = RECNO_OOB;
	cp->order = INVALID_ORDER;
	cp->flags = 0;
}

/*
 * __bam_c_init --
 *	Initialize the access private portion of a cursor
 *
 * PUBLIC: int __bam_c_init __P((DBC *, DBTYPE));
 */
int
__bam_c_init(dbc, dbtype)
	DBC *dbc;
	DBTYPE dbtype;
{
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	int ret;
	u_int32_t minkey;

	dbp = dbc->dbp;

	/* Allocate/initialize the internal structure. */
	if (dbc->internal == NULL) {
		if ((ret = __os_malloc(dbp->dbenv,
		    sizeof(BTREE_CURSOR), NULL, &cp)) != 0)
			return (ret);
		dbc->internal = (DBC_INTERNAL *)cp;

		cp->sp = cp->csp = cp->stack;
		cp->esp = cp->stack + sizeof(cp->stack) / sizeof(cp->stack[0]);
	} else
		cp = (BTREE_CURSOR *)dbc->internal;
	__bam_c_reset(cp);

	/* Initialize methods. */
	dbc->c_close = __db_c_close;
	dbc->c_count = __db_c_count;
	dbc->c_del = __db_c_del;
	dbc->c_dup = __db_c_dup;
	dbc->c_get = __db_c_get;
	dbc->c_put = __db_c_put;
	if (dbtype == DB_BTREE) {
		dbc->c_am_close = __bam_c_close;
		dbc->c_am_del = __bam_c_del;
		dbc->c_am_destroy = __bam_c_destroy;
		dbc->c_am_get = __bam_c_get;
		dbc->c_am_put = __bam_c_put;
		dbc->c_am_writelock = __bam_c_writelock;
	} else {
		dbc->c_am_close = __bam_c_close;
		dbc->c_am_del = __ram_c_del;
		dbc->c_am_destroy = __bam_c_destroy;
		dbc->c_am_get = __ram_c_get;
		dbc->c_am_put = __ram_c_put;
		dbc->c_am_writelock = __bam_c_writelock;
	}

	/*
	 * The btree leaf page data structures require that two key/data pairs
	 * (or four items) fit on a page, but other than that there's no fixed
	 * requirement.  The btree off-page duplicates only require two items,
	 * to be exact, but requiring four for them as well seems reasonable.
	 *
	 * Recno uses the btree bt_ovflsize value -- it's close enough.
	 */
	t = dbp->bt_internal;
	minkey = F_ISSET(dbc, DBC_OPD) ? 2 : t->bt_minkey;
	cp->ovflsize = B_MINKEY_TO_OVFLSIZE(minkey, dbp->pgsize);

	return (0);
}

/*
 * __bam_c_refresh
 *	Set things up properly for cursor re-use.
 *
 * PUBLIC: int __bam_c_refresh __P((DBC *));
 */
int
__bam_c_refresh(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	__bam_c_reset(cp);

	/*
	 * If our caller set the root page number, it's because the root was
	 * known.  This is always the case for off page dup cursors.  Else,
	 * pull it out of our internal information.
	 */
	if (cp->root == PGNO_INVALID)
		cp->root = ((BTREE *)dbp->bt_internal)->bt_root;

	/* Initialize for record numbers. */
	if (F_ISSET(dbc, DBC_OPD) ||
	    dbc->dbtype == DB_RECNO || F_ISSET(dbp, DB_BT_RECNUM)) {
		F_SET(cp, C_RECNUM);

		/*
		 * All btrees that support record numbers, optionally standard
		 * recno trees, and all off-page duplicate recno trees have
		 * mutable record numbers.
		 */
		if ((F_ISSET(dbc, DBC_OPD) && dbc->dbtype == DB_RECNO) ||
		    F_ISSET(dbp, DB_BT_RECNUM | DB_RE_RENUMBER))
			F_SET(cp, C_RENUMBER);
	}

	return (0);
}

/*
 * __bam_c_close --
 *	Close down the cursor.
 */
static int
__bam_c_close(dbc, root_pgno, rmroot)
	DBC *dbc;
	db_pgno_t root_pgno;
	int *rmroot;
{
	BTREE_CURSOR *cp, *cp_opd, *cp_c;
	DB *dbp;
	DBC *dbc_opd, *dbc_c;
	PAGE *h;
	u_int32_t num;
	int cdb_lock, ret, t_ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	cp_opd = (dbc_opd = cp->opd) == NULL ?
	    NULL : (BTREE_CURSOR *)dbc_opd->internal;
	cdb_lock = ret = 0;

	/*
	 * There are 3 ways this function is called:
	 *
	 * 1. Closing a primary cursor: we get called with a pointer to a
	 *    primary cursor that has a NULL opd field.  This happens when
	 *    closing a btree/recno database cursor without an associated
	 *    off-page duplicate tree.
	 *
	 * 2. Closing a primary and an off-page duplicate cursor stack: we
	 *    get called with a pointer to the primary cursor which has a
	 *    non-NULL opd field.  This happens when closing a btree cursor
	 *    into database with an associated off-page btree/recno duplicate
	 *    tree. (It can't be a primary recno database, recno databases
	 *    don't support duplicates.)
	 *
	 * 3. Closing an off-page duplicate cursor stack: we get called with
	 *    a pointer to the off-page duplicate cursor.  This happens when
	 *    closing a non-btree database that has an associated off-page
	 *    btree/recno duplicate tree or for a btree database when the
	 *    opd tree is not empty (root_pgno == PGNO_INVALID).
	 *
	 * If either the primary or off-page duplicate cursor deleted a btree
	 * key/data pair, check to see if the item is still referenced by a
	 * different cursor.  If it is, confirm that cursor's delete flag is
	 * set and leave it to that cursor to do the delete.
	 *
	 * NB: The test for == 0 below is correct.  Our caller already removed
	 * our cursor argument from the active queue, we won't find it when we
	 * search the queue in __bam_ca_delete().
	 * NB: It can't be true that both the primary and off-page duplicate
	 * cursors have deleted a btree key/data pair.  Either the primary
	 * cursor may have deleted an item and there's no off-page duplicate
	 * cursor, or there's an off-page duplicate cursor and it may have
	 * deleted an item.
	 *
	 * Primary recno databases aren't an issue here.  Recno keys are either
	 * deleted immediately or never deleted, and do not have to be handled
	 * here.
	 *
	 * Off-page duplicate recno databases are an issue here, cases #2 and
	 * #3 above can both be off-page recno databases.  The problem is the
	 * same as the final problem for off-page duplicate btree databases.
	 * If we no longer need the off-page duplicate tree, we want to remove
	 * it.  For off-page duplicate btrees, we are done with the tree when
	 * we delete the last item it contains, i.e., there can be no further
	 * references to it when it's empty.  For off-page duplicate recnos,
	 * we remove items from the tree as the application calls the remove
	 * function, so we are done with the tree when we close the last cursor
	 * that references it.
	 *
	 * We optionally take the root page number from our caller.  If the
	 * primary database is a btree, we can get it ourselves because dbc
	 * is the primary cursor.  If the primary database is not a btree,
	 * the problem is that we may be dealing with a stack of pages.  The
	 * cursor we're using to do the delete points at the bottom of that
	 * stack and we need the top of the stack.
	 */
	if (F_ISSET(cp, C_DELETED)) {
		dbc_c = dbc;
		switch (dbc->dbtype) {
		case DB_BTREE:				/* Case #1, #3. */
			if (__bam_ca_delete(dbp, cp->pgno, cp->indx, 1) == 0)
				goto lock;
			goto done;
		case DB_RECNO:
			if (!F_ISSET(dbc, DBC_OPD))	/* Case #1. */
				goto done;
							/* Case #3. */
			if (__ram_ca_delete(dbp, cp->root) == 0)
				goto lock;
			goto done;
		default:
			return (__db_unknown_type(dbp->dbenv,
				"__bam_c_close", dbc->dbtype));
		}
	}

	if (dbc_opd == NULL)
		goto done;

	if (F_ISSET(cp_opd, C_DELETED)) {		/* Case #2. */
		/*
		 * We will not have been provided a root page number.  Acquire
		 * one from the primary database.
		 */
		if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &h)) != 0)
			goto err;
		root_pgno = GET_BOVERFLOW(h, cp->indx + O_INDX)->pgno;
		if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
			goto err;

		dbc_c = dbc_opd;
		switch (dbc_opd->dbtype) {
		case DB_BTREE:
			if (__bam_ca_delete(
			    dbp, cp_opd->pgno, cp_opd->indx, 1) == 0)
				goto lock;
			goto done;
		case DB_RECNO:
			if (__ram_ca_delete(dbp, cp_opd->root) == 0)
				goto lock;
			goto done;
		default:
			return (__db_unknown_type(dbp->dbenv,
				"__bam_c_close", dbc->dbtype));
		}
	}
	goto done;

lock:	cp_c = (BTREE_CURSOR *)dbc_c->internal;

	/*
	 * If this is CDB, upgrade the lock if necessary.  While we acquired
	 * the write lock to logically delete the record, we released it when
	 * we returned from that call, and so may not be holding a write lock
	 * at the moment.  NB: to get here in CDB we must either be holding a
	 * write lock or be the only cursor that is permitted to acquire write
	 * locks.  The reason is that there can never be more than a single CDB
	 * write cursor (that cursor cannot be dup'd), and so that cursor must
	 * be closed and the item therefore deleted before any other cursor
	 * could acquire a reference to this item.
	 *
	 * Note that dbc may be an off-page dup cursor;  this is the sole
	 * instance in which an OPD cursor does any locking, but it's necessary
	 * because we may be closed by ourselves without a parent cursor
	 * handy, and we have to do a lock upgrade on behalf of somebody.
	 * If this is the case, the OPD has been given the parent's locking
	 * info in __db_c_get--the OPD is also a WRITEDUP.
	 */
	if (CDB_LOCKING(dbp->dbenv)) {
		DB_ASSERT(!F_ISSET(dbc, DBC_OPD) || F_ISSET(dbc, DBC_WRITEDUP));
		if (!F_ISSET(dbc, DBC_WRITER)) {
			if ((ret =
			    lock_get(dbp->dbenv, dbc->locker, DB_LOCK_UPGRADE,
			    &dbc->lock_dbt, DB_LOCK_WRITE, &dbc->mylock)) != 0)
				goto err;
			cdb_lock = 1;
		}

		cp_c->lock.off = LOCK_INVALID;
		if ((ret =
		    memp_fget(dbp->mpf, &cp_c->pgno, 0, &cp_c->page)) != 0)
			goto err;

		goto delete;
	}

	/*
	 * The variable dbc_c has been initialized to reference the cursor in
	 * which we're going to do the delete.  Initialize the cursor's page
	 * and lock structures as necessary.
	 *
	 * First, we may not need to acquire any locks.  If we're in case #3,
	 * that is, the primary database isn't a btree database, our caller
	 * is responsible for acquiring any necessary locks before calling us.
	 */
	if (F_ISSET(dbc, DBC_OPD)) {
		cp_c->lock.off = LOCK_INVALID;
		if ((ret =
		    memp_fget(dbp->mpf, &cp_c->pgno, 0, &cp_c->page)) != 0)
			goto err;
		goto delete;
	}

	/*
	 * Otherwise, acquire a write lock.  If the cursor that did the initial
	 * logical deletion (and which had a write lock) is not the same as the
	 * cursor doing the physical deletion (which may have only ever had a
	 * read lock on the item), we need to upgrade.  The confusion comes as
	 * follows:
	 *
	 *	C1	created, acquires item read lock
	 *	C2	dup C1, create C2, also has item read lock.
	 *	C1	acquire write lock, delete item
	 *	C1	close
	 *	C2	close, needs a write lock to physically delete item.
	 *
	 * If we're in a TXN, we know that C2 will be able to acquire the write
	 * lock, because no locker other than the one shared by C1 and C2 can
	 * acquire a write lock -- the original write lock C1 acquire was never
	 * discarded.
	 *
	 * If we're not in a TXN, it's nastier.  Other cursors might acquire
	 * read locks on the item after C1 closed, discarding its write lock,
	 * and such locks would prevent C2 from acquiring a read lock.  That's
	 * OK, though, we'll simply wait until we can acquire a read lock, or
	 * we'll deadlock.  (Which better not happen, since we're not in a TXN.)
	 *
	 * Lock the primary database page, regardless of whether we're deleting
	 * an item on a primary database page or an off-page duplicates page.
	 */
	ACQUIRE(dbc, DB_LOCK_WRITE,
	    cp->pgno, cp_c->lock, cp_c->pgno, cp_c->page, ret);
	if (ret != 0)
		goto err;

delete:	/*
	 * If the delete occurred in a btree, delete the on-page physical item
	 * referenced by the cursor.
	 */
	if (dbc_c->dbtype == DB_BTREE && (ret = __bam_c_physdel(dbc_c)) != 0)
		goto err;

	/*
	 * If we're not working in an off-page duplicate tree, then we're
	 * done.
	 */
	if (!F_ISSET(dbc_c, DBC_OPD) || root_pgno == PGNO_INVALID)
		goto done;

	/*
	 * We may have just deleted the last element in the off-page duplicate
	 * tree, and closed the last cursor in the tree.  For an off-page btree
	 * there are no other cursors in the tree by definition, if the tree is
	 * empty.  For an off-page recno we know we have closed the last cursor
	 * in the tree because the __ram_ca_delete call above returned 0 only
	 * in that case.  So, if the off-page duplicate tree is empty at this
	 * point, we want to remove it.
	 */
	if ((ret = memp_fget(dbp->mpf, &root_pgno, 0, &h)) != 0)
		goto err;
	if ((num = NUM_ENT(h)) == 0) {
		if ((ret = __db_free(dbc, h)) != 0)
			goto err;
	} else {
		if ((ret = memp_fput(dbp->mpf, h, 0)) != 0)
			goto err;
		goto done;
	}

	/*
	 * When removing the tree, we have to do one of two things.  If this is
	 * case #2, that is, the primary tree is a btree, delete the key that's
	 * associated with the tree from the btree leaf page.  We know we are
	 * the only reference to it and we already have the correct lock.  We
	 * detect this case because the cursor that was passed to us references
	 * an off-page duplicate cursor.
	 *
	 * If this is case #3, that is, the primary tree isn't a btree, pass
	 * the information back to our caller, it's their job to do cleanup on
	 * the primary page.
	 */
	if (dbc_opd != NULL) {
		cp->lock.off = LOCK_INVALID;
		if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &cp->page)) != 0)
			goto err;
		if ((ret = __bam_c_physdel(dbc)) != 0)
			goto err;
	} else
		*rmroot = 1;
err:
done:	/*
	 * Discard the page references and locks, and confirm that the stack
	 * has been emptied.
	 */
	if (dbc_opd != NULL) {
		DISCARD_CUR(dbc_opd, t_ret);
		if (t_ret != 0 && ret == 0)
			ret = t_ret;
	}
	DISCARD_CUR(dbc, t_ret);
	if (t_ret != 0 && ret == 0)
		ret = t_ret;

	/* Downgrade any CDB lock we acquired. */
	if (cdb_lock)
		(void)__lock_downgrade(
		    dbp->dbenv, &dbc->mylock, DB_LOCK_IWRITE, 0);

	return (ret);
}

/*
 * __bam_c_destroy --
 *	Close a single cursor -- internal version.
 */
static int
__bam_c_destroy(dbc)
	DBC *dbc;
{
	/* Discard the structures. */
	__os_free(dbc->internal, sizeof(BTREE_CURSOR));

	return (0);
}

/*
 * __bam_c_count --
 *	Return a count of on and off-page duplicates.
 *
 * PUBLIC: int __bam_c_count __P((DBC *, db_recno_t *));
 */
int
__bam_c_count(dbc, recnop)
	DBC *dbc;
	db_recno_t *recnop;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_indx_t indx, top;
	db_recno_t recno;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Called with the top-level cursor that may reference an off-page
	 * duplicates page.  If it's a set of on-page duplicates, get the
	 * page and count.  Otherwise, get the root page of the off-page
	 * duplicate tree, and use the count.  We don't have to acquire any
	 * new locks, we have to have a read lock to even get here.
	 */
	if (cp->opd == NULL) {
		if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &cp->page)) != 0)
			return (ret);

		/*
		 * Move back to the beginning of the set of duplicates and
		 * then count forward.
		 */
		for (indx = cp->indx;; indx -= P_INDX)
			if (indx == 0 ||
			    !IS_DUPLICATE(dbc, indx, indx - P_INDX))
				break;
		for (recno = 1, top = NUM_ENT(cp->page) - P_INDX;
		    indx < top; ++recno, indx += P_INDX)
			if (!IS_DUPLICATE(dbc, indx, indx + P_INDX))
				break;
		*recnop = recno;
	} else {
		if ((ret = memp_fget(dbp->mpf,
		    &cp->opd->internal->root, 0, &cp->page)) != 0)
			return (ret);

		*recnop = RE_NREC(cp->page);
	}

	ret = memp_fput(dbp->mpf, cp->page, 0);
	cp->page = NULL;

	return (ret);
}

/*
 * __bam_c_del --
 *	Delete using a cursor.
 */
static int
__bam_c_del(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	int ret, t_ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	ret = 0;

	/* If the item was already deleted, return failure. */
	if (F_ISSET(cp, C_DELETED))
		return (DB_KEYEMPTY);

	/*
	 * This code is always called with a page lock but no page.
	 */
	DB_ASSERT(cp->page == NULL);

	/*
	 * We don't physically delete the record until the cursor moves, so
	 * we have to have a long-lived write lock on the page instead of a
	 * a long-lived read lock.  Note, we have to have a read lock to even
	 * get here.
	 *
	 * If we're maintaining record numbers, we lock the entire tree, else
	 * we lock the single page.
	 */
	if (F_ISSET(cp, C_RECNUM)) {
		if ((ret = __bam_c_getstack(dbc)) != 0)
			goto err;
		cp->page = cp->csp->page;
	} else {
		ACQUIRE_CUR(dbc, DB_LOCK_WRITE, ret);
		if (ret != 0)
			goto err;
	}

	/* Log the change. */
	if (DB_LOGGING(dbc) &&
	    (ret = __bam_cdel_log(dbp->dbenv, dbc->txn, &LSN(cp->page), 0,
	    dbp->log_fileid, PGNO(cp->page), &LSN(cp->page), cp->indx)) != 0)
		goto err;

	/* Set the intent-to-delete flag on the page. */
	if (TYPE(cp->page) == P_LBTREE)
		B_DSET(GET_BKEYDATA(cp->page, cp->indx + O_INDX)->type);
	else
		B_DSET(GET_BKEYDATA(cp->page, cp->indx)->type);

	/* Mark the page dirty. */
	ret = memp_fset(dbp->mpf, cp->page, DB_MPOOL_DIRTY);

err:	/*
	 * If we've been successful so far and the tree has record numbers,
	 * adjust the record counts.  Either way, release acquired page(s).
	 */
	if (F_ISSET(cp, C_RECNUM)) {
		if (ret == 0)
			ret = __bam_adjust(dbc, -1);
		(void)__bam_stkrel(dbc, 0);
	} else
		if (cp->page != NULL &&
		    (t_ret = memp_fput(dbp->mpf, cp->page, 0)) != 0 && ret == 0)
			ret = t_ret;

	cp->page = NULL;

	/* Update the cursors last, after all chance of failure is past. */
	if (ret == 0)
		(void)__bam_ca_delete(dbp, cp->pgno, cp->indx, 1);

	return (ret);
}

/*
 * __bam_c_dup --
 *	Duplicate a btree cursor, such that the new one holds appropriate
 *	locks for the position of the original.
 *
 * PUBLIC: int __bam_c_dup __P((DBC *, DBC *));
 */
int
__bam_c_dup(orig_dbc, new_dbc)
	DBC *orig_dbc, *new_dbc;
{
	BTREE_CURSOR *orig, *new;
	int ret;

	orig = (BTREE_CURSOR *)orig_dbc->internal;
	new = (BTREE_CURSOR *)new_dbc->internal;

	/*
	 * If we're holding a lock we need to acquire a copy of it, unless
	 * we're in a transaction.  We don't need to copy any lock we're
	 * holding inside a transaction because all the locks are retained
	 * until the transaction commits or aborts.
	 */
	if (orig->lock.off != LOCK_INVALID && orig_dbc->txn == NULL) {
		if ((ret = __db_lget(new_dbc,
		    0, new->pgno, new->lock_mode, 0, &new->lock)) != 0)
			return (ret);
	}
	new->ovflsize = orig->ovflsize;
	new->recno = orig->recno;
	new->flags = orig->flags;

	return (0);
}

/*
 * __bam_c_get --
 *	Get using a cursor (btree).
 */
static int
__bam_c_get(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_pgno_t orig_pgno;
	db_indx_t orig_indx;
	int exact, newopd, ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	orig_pgno = cp->pgno;
	orig_indx = cp->indx;

	newopd = 0;
	switch (flags) {
	case DB_CURRENT:
		/* It's not possible to return a deleted record. */
		if (F_ISSET(cp, C_DELETED)) {
			ret = DB_KEYEMPTY;
			goto err;
		}

		/*
		 * Acquire the current page.  We have at least a read-lock
		 * already.  The caller may have set DB_RMW asking for a
		 * write lock, but upgrading to a write lock has no better
		 * chance of succeeding now instead of later, so don't try.
		 */
		if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &cp->page)) != 0)
			goto err;
		break;
	case DB_FIRST:
		newopd = 1;
		if ((ret = __bam_c_first(dbc)) != 0)
			goto err;
		break;
	case DB_GET_BOTH:
		/*
		 * There are two ways to get here based on DBcursor->c_get
		 * with the DB_GET_BOTH flag set:
		 *
		 * 1. Searching a sorted off-page duplicate tree: do a tree
		 * search.
		 *
		 * 2. Searching btree: do a tree search.  If it returns a
		 * reference to off-page duplicate tree, return immediately
		 * and let our caller deal with it.  If the search doesn't
		 * return a reference to off-page duplicate tree, start an
		 * on-page search.
		 */
		if (F_ISSET(dbc, DBC_OPD)) {
			if ((ret = __bam_c_search(
			    dbc, data, DB_GET_BOTH, &exact)) != 0)
				goto err;
			if (!exact) {
				ret = DB_NOTFOUND;
				goto err;
			}
		} else {
			if ((ret = __bam_c_search(
			    dbc, key, DB_GET_BOTH, &exact)) != 0)
				return (ret);
			if (!exact) {
				ret = DB_NOTFOUND;
				goto err;
			}

			if (pgnop != NULL && __bam_isopd(dbc, pgnop)) {
				newopd = 1;
				break;
			}
			if ((ret = __bam_getboth_finddatum(dbc, data)) != 0)
				goto err;
		}
		break;
	case DB_GET_BOTHC:
		if ((ret = __bam_getbothc(dbc, data)) != 0)
			goto err;
		break;
	case DB_LAST:
		newopd = 1;
		if ((ret = __bam_c_last(dbc)) != 0)
			goto err;
		break;
	case DB_NEXT:
		newopd = 1;
		if (cp->pgno == PGNO_INVALID) {
			if ((ret = __bam_c_first(dbc)) != 0)
				goto err;
		} else
			if ((ret = __bam_c_next(dbc, 1)) != 0)
				goto err;
		break;
	case DB_NEXT_DUP:
		if ((ret = __bam_c_next(dbc, 1)) != 0)
			goto err;
		if (!IS_CUR_DUPLICATE(dbc, orig_pgno, orig_indx)) {
			ret = DB_NOTFOUND;
			goto err;
		}
		break;
	case DB_NEXT_NODUP:
		newopd = 1;
		if (cp->pgno == PGNO_INVALID) {
			if ((ret = __bam_c_first(dbc)) != 0)
				goto err;
		} else
			do {
				if ((ret = __bam_c_next(dbc, 1)) != 0)
					goto err;
			} while (IS_CUR_DUPLICATE(dbc, orig_pgno, orig_indx));
		break;
	case DB_PREV:
		newopd = 1;
		if (cp->pgno == PGNO_INVALID) {
			if ((ret = __bam_c_last(dbc)) != 0)
				goto err;
		} else
			if ((ret = __bam_c_prev(dbc)) != 0)
				goto err;
		break;
	case DB_PREV_NODUP:
		newopd = 1;
		if (cp->pgno == PGNO_INVALID) {
			if ((ret = __bam_c_last(dbc)) != 0)
				goto err;
		} else
			do {
				if ((ret = __bam_c_prev(dbc)) != 0)
					goto err;
			} while (IS_CUR_DUPLICATE(dbc, orig_pgno, orig_indx));
		break;
	case DB_SET:
	case DB_SET_RECNO:
		newopd = 1;
		if ((ret = __bam_c_search(dbc, key, flags, &exact)) != 0)
			goto err;
		break;
	case DB_SET_RANGE:
		newopd = 1;
		if ((ret = __bam_c_search(dbc, key, flags, &exact)) != 0)
			goto err;

		/*
		 * As we didn't require an exact match, the search function
		 * may have returned an entry past the end of the page.  Or,
		 * we may be referencing a deleted record.  If so, move to
		 * the next entry.
		 */
		if (cp->indx == NUM_ENT(cp->page) || IS_CUR_DELETED(dbc))
			if ((ret = __bam_c_next(dbc, 0)) != 0)
				goto err;
		break;
	default:
		ret = __db_unknown_flag(dbp->dbenv, "__bam_c_get", flags);
		goto err;
	}

	/*
	 * We may have moved to an off-page duplicate tree.  Return that
	 * information to our caller.
	 */
	if (newopd && pgnop != NULL)
		(void)__bam_isopd(dbc, pgnop);

	/* Don't return the key, it was passed to us */
	if (flags == DB_SET)
		F_SET(key, DB_DBT_ISSET);

err:	/*
	 * Regardless of whether we were successful or not, if the cursor
	 * moved, clear the delete flag, DBcursor->c_get never references
	 * a deleted key, if it moved at all.
	 */
	if (F_ISSET(cp, C_DELETED)
	    && (cp->pgno != orig_pgno || cp->indx != orig_indx))
		F_CLR(cp, C_DELETED);

	return (ret);
}

/*
 * __bam_getbothc --
 *	Search for a matching data item on a join.
 */
static int
__bam_getbothc(dbc, data)
	DBC *dbc;
	DBT *data;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	int cmp, exact, ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Acquire the current page.  We have at least a read-lock
	 * already.  The caller may have set DB_RMW asking for a
	 * write lock, but upgrading to a write lock has no better
	 * chance of succeeding now instead of later, so don't try.
	 */
	if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &cp->page)) != 0)
		return (ret);

	/*
	 * An off-page duplicate cursor.  Search the remaining duplicates
	 * for one which matches (do a normal btree search, then verify
	 * that the retrieved record is greater than the original one).
	 */
	if (F_ISSET(dbc, DBC_OPD)) {
		/*
		 * Check to make sure the desired item comes strictly after
		 * the current position;  if it doesn't, return DB_NOTFOUND.
		 */
		if ((ret = __bam_cmp(dbp, data, cp->page, cp->indx,
		    dbp->dup_compare == NULL ? __bam_defcmp : dbp->dup_compare,
		    &cmp)) != 0)
			return (ret);

		if (cmp <= 0)
			return (DB_NOTFOUND);

		/* Discard the current page, we're going to do a full search. */
		if ((ret = memp_fput(dbp->mpf, cp->page, 0)) != 0)
			return (ret);
		cp->page = NULL;

		return (__bam_c_search(dbc, data, DB_GET_BOTH, &exact));
	}

	/*
	 * We're doing a DBC->c_get(DB_GET_BOTHC) and we're already searching
	 * a set of on-page duplicates (either sorted or unsorted).  Continue
	 * a linear search from after the current position.
	 *
	 * (Note that we could have just finished a "set" of one duplicate,
	 * i.e. not a duplicate at all, but the following check will always
	 * return DB_NOTFOUND in this case, which is the desired behavior.)
	 */
	if (cp->indx + P_INDX >= NUM_ENT(cp->page) ||
	    !IS_DUPLICATE(dbc, cp->indx, cp->indx + P_INDX))
		return (DB_NOTFOUND);
	cp->indx += P_INDX;

	return (__bam_getboth_finddatum(dbc, data));
}

/*
 * __bam_getboth_finddatum --
 *	Find a matching on-page data item.
 */
static int
__bam_getboth_finddatum(dbc, data)
	DBC *dbc;
	DBT *data;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_indx_t base, lim, top;
	int cmp, ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Called (sometimes indirectly) from DBC->get to search on-page data
	 * item(s) for a matching value.  If the original flag was DB_GET_BOTH,
	 * the cursor argument is set to the first data item for the key.  If
	 * the original flag was DB_GET_BOTHC, the cursor argument is set to
	 * the first data item that we can potentially return.  In both cases,
	 * there may or may not be additional duplicate data items to search.
	 *
	 * If the duplicates are not sorted, do a linear search.
	 *
	 * If the duplicates are sorted, do a binary search.  The reason for
	 * this is that large pages and small key/data pairs result in large
	 * numbers of on-page duplicates before they get pushed off-page.
	 */
	if (dbp->dup_compare == NULL) {
		for (;; cp->indx += P_INDX) {
			if (!IS_CUR_DELETED(dbc) &&
			    (ret = __bam_cmp(dbp, data, cp->page,
			    cp->indx + O_INDX, __bam_defcmp, &cmp)) != 0)
				return (ret);
			if (cmp == 0)
				return (0);

			if (cp->indx + P_INDX >= NUM_ENT(cp->page) ||
			    !IS_DUPLICATE(dbc, cp->indx, cp->indx + P_INDX))
				break;
		}
	} else {
		/*
		 * Find the top and bottom of the duplicate set.  Binary search
		 * requires at least two items, don't loop if there's only one.
		 */
		for (base = top = cp->indx;
		    top < NUM_ENT(cp->page); top += P_INDX)
			if (!IS_DUPLICATE(dbc, cp->indx, top))
				break;
		if (base == (top - P_INDX)) {
			if  ((ret = __bam_cmp(dbp, data,
			    cp->page, cp->indx + O_INDX,
			    dbp->dup_compare, &cmp)) != 0)
			       return (ret);
			return (cmp == 0 ? 0 : DB_NOTFOUND);
		}

		for (lim =
		    (top - base) / (db_indx_t)P_INDX; lim != 0; lim >>= 1) {
			cp->indx = base + ((lim >> 1) * P_INDX);
			if ((ret = __bam_cmp(dbp, data, cp->page,
			    cp->indx + O_INDX, dbp->dup_compare, &cmp)) != 0)
				return (ret);
			if (cmp == 0) {
				if (!IS_CUR_DELETED(dbc))
					return (0);
				break;
			}
			if (cmp > 0) {
				base = cp->indx + P_INDX;
				--lim;
			}
		}
	}
	return (DB_NOTFOUND);
}

/*
 * __bam_c_put --
 *	Put using a cursor.
 */
static int
__bam_c_put(dbc, key, data, flags, pgnop)
	DBC *dbc;
	DBT *key, *data;
	u_int32_t flags;
	db_pgno_t *pgnop;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT dbt;
	u_int32_t iiop;
	int cmp, exact, needkey, ret, stack;
	void *arg;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

split:	needkey = ret = stack = 0;
	switch (flags) {
	case DB_AFTER:
	case DB_BEFORE:
	case DB_CURRENT:
		needkey = 1;
		iiop = flags;

		/*
		 * If the Btree has record numbers (and we're not replacing an
		 * existing record), we need a complete stack so that we can
		 * adjust the record counts.  The check for flags == DB_CURRENT
		 * is superfluous but left in for clarity.  (If C_RECNUM is set
		 * we know that flags must be DB_CURRENT, as DB_AFTER/DB_BEFORE
		 * are illegal in a Btree unless it's configured for duplicates
		 * and you cannot configure a Btree for both record renumbering
		 * and duplicates.)
		 */
		if (flags == DB_CURRENT &&
		    F_ISSET(cp, C_RECNUM) && F_ISSET(cp, C_DELETED)) {
			if ((ret = __bam_c_getstack(dbc)) != 0)
				goto err;
			/*
			 * Initialize the cursor from the stack.  Don't take
			 * the page number or page index, they should already
			 * be set.
			 */
			cp->page = cp->csp->page;
			cp->lock = cp->csp->lock;
			cp->lock_mode = cp->csp->lock_mode;

			stack = 1;
			break;
		}

		/* Acquire the current page with a write lock. */
		ACQUIRE_WRITE_LOCK(dbc, ret);
		if (ret != 0)
			goto err;
		if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &cp->page)) != 0)
			goto err;
		break;
	case DB_KEYFIRST:
	case DB_KEYLAST:
	case DB_NODUPDATA:
		/*
		 * Searching off-page, sorted duplicate tree: do a tree search
		 * for the correct item; __bam_c_search returns the smallest
		 * slot greater than the key, use it.
		 */
		if (F_ISSET(dbc, DBC_OPD)) {
			if ((ret =
			    __bam_c_search(dbc, data, flags, &exact)) != 0)
				goto err;
			stack = 1;

			/* Disallow "sorted" duplicate duplicates. */
			if (exact) {
				ret = __db_duperr(dbp, flags);
				goto err;
			}
			iiop = DB_BEFORE;
			break;
		}

		/* Searching a btree. */
		if ((ret = __bam_c_search(dbc, key,
		    flags == DB_KEYFIRST || dbp->dup_compare != NULL ?
		    DB_KEYFIRST : DB_KEYLAST, &exact)) != 0)
			goto err;
		stack = 1;

		/*
		 * If we don't have an exact match, __bam_c_search returned
		 * the smallest slot greater than the key, use it.
		 */
		if (!exact) {
			iiop = DB_KEYFIRST;
			break;
		}

		/*
		 * If duplicates aren't supported, replace the current item.
		 * (If implementing the DB->put function, our caller already
		 * checked the DB_NOOVERWRITE flag.)
		 */
		if (!F_ISSET(dbp, DB_AM_DUP)) {
			iiop = DB_CURRENT;
			break;
		}

		/*
		 * If we find a matching entry, it may be an off-page duplicate
		 * tree.  Return the page number to our caller, we need a new
		 * cursor.
		 */
		if (pgnop != NULL && __bam_isopd(dbc, pgnop))
			goto done;

		/* If the duplicates aren't sorted, move to the right slot. */
		if (dbp->dup_compare == NULL) {
			if (flags == DB_KEYFIRST)
				iiop = DB_BEFORE;
			else
				for (;; cp->indx += P_INDX)
					if (cp->indx + P_INDX >=
					    NUM_ENT(cp->page) ||
					    !IS_DUPLICATE(dbc, cp->indx,
					    cp->indx + P_INDX)) {
						iiop = DB_AFTER;
						break;
					}
			break;
		}

		/*
		 * We know that we're looking at the first of a set of sorted
		 * on-page duplicates.  Walk the list to find the right slot.
		 */
		for (;; cp->indx += P_INDX) {
			if ((ret = __bam_cmp(dbp, data, cp->page,
			    cp->indx + O_INDX, dbp->dup_compare, &cmp)) !=0)
				return (ret);
			if (cmp < 0) {
				iiop = DB_BEFORE;
				break;
			}

			/* Disallow "sorted" duplicate duplicates. */
			if (cmp == 0) {
				if (IS_DELETED(cp->page, cp->indx)) {
					iiop = DB_CURRENT;
					break;
				}
				ret = __db_duperr(dbp, flags);
				goto err;
			}

			if (cp->indx + P_INDX >= NUM_ENT(cp->page) ||
			    ((PAGE *)cp->page)->inp[cp->indx] !=
			    ((PAGE *)cp->page)->inp[cp->indx + P_INDX]) {
				iiop = DB_AFTER;
				break;
			}
		}
		break;
	default:
		ret = __db_unknown_flag(dbp->dbenv, "__bam_c_put", flags);
		goto err;
	}

	switch (ret = __bam_iitem(dbc, key, data, iiop, 0)) {
	case 0:
		break;
	case DB_NEEDSPLIT:
		/*
		 * To split, we need a key for the page.  Either use the key
		 * argument or get a copy of the key from the page.
		 */
		if (flags == DB_AFTER ||
		    flags == DB_BEFORE || flags == DB_CURRENT) {
			memset(&dbt, 0, sizeof(DBT));
			if ((ret = __db_ret(dbp, cp->page, 0, &dbt,
			    &dbc->rkey.data, &dbc->rkey.ulen)) != 0)
				goto err;
			arg = &dbt;
		} else
			arg = F_ISSET(dbc, DBC_OPD) ? data : key;

		/*
		 * Discard any locks and pinned pages (the locks are discarded
		 * even if we're running with transactions, as they lock pages
		 * that we're sorry we ever acquired).  If stack is set and the
		 * cursor entries are valid, they point to the same entries as
		 * the stack, don't free them twice.
		 */
		if (stack)
			ret = __bam_stkrel(dbc, STK_CLRDBC | STK_NOLOCK);
		else
			DISCARD_CUR(dbc, ret);
		if (ret != 0)
			goto err;

		/* Split the tree. */
		if ((ret = __bam_split(dbc, arg)) != 0)
			return (ret);

		goto split;
	default:
		goto err;
	}

err:
done:	/*
	 * Discard any pages pinned in the tree and their locks, except for
	 * the leaf page.  Note, the leaf page participated in any stack we
	 * acquired, and so we have to adjust the stack as necessary.  If
	 * there was only a single page on the stack, we don't have to free
	 * further stack pages.
	 */
	if (stack && BT_STK_POP(cp) != NULL)
		(void)__bam_stkrel(dbc, 0);

	/*
	 * Regardless of whether we were successful or not, clear the delete
	 * flag.  If we're successful, we either moved the cursor or the item
	 * is no longer deleted.  If we're not successful, then we're just a
	 * copy, no need to have the flag set.
	 */
	F_CLR(cp, C_DELETED);

	return (ret);
}

/*
 * __bam_c_rget --
 *	Return the record number for a cursor.
 *
 * PUBLIC: int __bam_c_rget __P((DBC *, DBT *, u_int32_t));
 */
int
__bam_c_rget(dbc, data, flags)
	DBC *dbc;
	DBT *data;
	u_int32_t flags;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT dbt;
	db_recno_t recno;
	int exact, ret;

	COMPQUIET(flags, 0);
	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Get the page with the current item on it.
	 * Get a copy of the key.
	 * Release the page, making sure we don't release it twice.
	 */
	if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &cp->page)) != 0)
		return (ret);
	memset(&dbt, 0, sizeof(DBT));
	if ((ret = __db_ret(dbp, cp->page,
	    cp->indx, &dbt, &dbc->rkey.data, &dbc->rkey.ulen)) != 0)
		goto err;
	ret = memp_fput(dbp->mpf, cp->page, 0);
	cp->page = NULL;
	if (ret != 0)
		return (ret);

	if ((ret = __bam_search(dbc, &dbt,
	    F_ISSET(dbc, DBC_RMW) ? S_FIND_WR : S_FIND,
	    1, &recno, &exact)) != 0)
		goto err;

	ret = __db_retcopy(dbp, data,
	    &recno, sizeof(recno), &dbc->rdata.data, &dbc->rdata.ulen);

	/* Release the stack. */
err:	__bam_stkrel(dbc, 0);

	return (ret);
}

/*
 * __bam_c_writelock --
 *	Upgrade the cursor to a write lock.
 */
static int
__bam_c_writelock(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	int ret;

	cp = (BTREE_CURSOR *)dbc->internal;

	if (cp->lock_mode == DB_LOCK_WRITE)
		return (0);

	/*
	 * When writing to an off-page duplicate tree, we need to have the
	 * appropriate page in the primary tree locked.  The general DBC
	 * code calls us first with the primary cursor so we can acquire the
	 * appropriate lock.
	 */
	ACQUIRE_WRITE_LOCK(dbc, ret);
	return (ret);
}

/*
 * __bam_c_first --
 *	Return the first record.
 */
static int
__bam_c_first(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_pgno_t pgno;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	ret = 0;

	/* Walk down the left-hand side of the tree. */
	for (pgno = cp->root;;) {
		ACQUIRE_CUR_SET(dbc, DB_LOCK_READ, pgno, ret);
		if (ret != 0)
			return (ret);

		/* If we find a leaf page, we're done. */
		if (ISLEAF(cp->page))
			break;

		pgno = GET_BINTERNAL(cp->page, 0)->pgno;
	}

	/* If we want a write lock instead of a read lock, get it now. */
	if (F_ISSET(dbc, DBC_RMW)) {
		ACQUIRE_WRITE_LOCK(dbc, ret);
		if (ret != 0)
			return (ret);
	}

	/* If on an empty page or a deleted record, move to the next one. */
	if (NUM_ENT(cp->page) == 0 || IS_CUR_DELETED(dbc))
		if ((ret = __bam_c_next(dbc, 0)) != 0)
			return (ret);

	return (0);
}

/*
 * __bam_c_last --
 *	Return the last record.
 */
static int
__bam_c_last(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_pgno_t pgno;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	ret = 0;

	/* Walk down the right-hand side of the tree. */
	for (pgno = cp->root;;) {
		ACQUIRE_CUR_SET(dbc, DB_LOCK_READ, pgno, ret);
		if (ret != 0)
			return (ret);

		/* If we find a leaf page, we're done. */
		if (ISLEAF(cp->page))
			break;

		pgno =
		    GET_BINTERNAL(cp->page, NUM_ENT(cp->page) - O_INDX)->pgno;
	}

	/* If we want a write lock instead of a read lock, get it now. */
	if (F_ISSET(dbc, DBC_RMW)) {
		ACQUIRE_WRITE_LOCK(dbc, ret);
		if (ret != 0)
			return (ret);
	}

	cp->indx = NUM_ENT(cp->page) == 0 ? 0 :
	    NUM_ENT(cp->page) -
	    (TYPE(cp->page) == P_LBTREE ? P_INDX : O_INDX);

	/* If on an empty page or a deleted record, move to the previous one. */
	if (NUM_ENT(cp->page) == 0 || IS_CUR_DELETED(dbc))
		if ((ret = __bam_c_prev(dbc)) != 0)
			return (ret);

	return (0);
}

/*
 * __bam_c_next --
 *	Move to the next record.
 */
static int
__bam_c_next(dbc, initial_move)
	DBC *dbc;
	int initial_move;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_indx_t adjust;
	db_lockmode_t lock_mode;
	db_pgno_t pgno;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	ret = 0;

	/*
	 * We're either moving through a page of duplicates or a btree leaf
	 * page.
	 *
	 * !!!
	 * This code handles empty pages and pages with only deleted entries.
	 */
	if (F_ISSET(dbc, DBC_OPD)) {
		adjust = O_INDX;
		lock_mode = DB_LOCK_NG;
	} else {
		adjust = dbc->dbtype == DB_BTREE ? P_INDX : O_INDX;
		lock_mode =
		    F_ISSET(dbc, DBC_RMW) ? DB_LOCK_WRITE : DB_LOCK_READ;
	}
	if (cp->page == NULL) {
		ACQUIRE_CUR(dbc, lock_mode, ret);
		if (ret != 0)
			return (ret);
	}

	if (initial_move)
		cp->indx += adjust;

	for (;;) {
		/*
		 * If at the end of the page, move to a subsequent page.
		 *
		 * !!!
		 * Check for >= NUM_ENT.  If the original search landed us on
		 * NUM_ENT, we may have incremented indx before the test.
		 */
		if (cp->indx >= NUM_ENT(cp->page)) {
			if ((pgno
			    = NEXT_PGNO(cp->page)) == PGNO_INVALID)
				return (DB_NOTFOUND);

			ACQUIRE_CUR_SET(dbc, lock_mode, pgno, ret);
			if (ret != 0)
				return (ret);
			continue;
		}
		if (IS_CUR_DELETED(dbc)) {
			cp->indx += adjust;
			continue;
		}
		break;
	}
	return (0);
}

/*
 * __bam_c_prev --
 *	Move to the previous record.
 */
static int
__bam_c_prev(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	db_indx_t adjust;
	db_lockmode_t lock_mode;
	db_pgno_t pgno;
	int ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	ret = 0;

	/*
	 * We're either moving through a page of duplicates or a btree leaf
	 * page.
	 *
	 * !!!
	 * This code handles empty pages and pages with only deleted entries.
	 */
	if (F_ISSET(dbc, DBC_OPD)) {
		adjust = O_INDX;
		lock_mode = DB_LOCK_NG;
	} else {
		adjust = dbc->dbtype == DB_BTREE ? P_INDX : O_INDX;
		lock_mode =
		    F_ISSET(dbc, DBC_RMW) ? DB_LOCK_WRITE : DB_LOCK_READ;
	}
	if (cp->page == NULL) {
		ACQUIRE_CUR(dbc, lock_mode, ret);
		if (ret != 0)
			return (ret);
	}

	for (;;) {
		/* If at the beginning of the page, move to a previous one. */
		if (cp->indx == 0) {
			if ((pgno =
			    PREV_PGNO(cp->page)) == PGNO_INVALID)
				return (DB_NOTFOUND);

			ACQUIRE_CUR_SET(dbc, lock_mode, pgno, ret);
			if (ret != 0)
				return (ret);

			if ((cp->indx = NUM_ENT(cp->page)) == 0)
				continue;
		}

		/* Ignore deleted records. */
		cp->indx -= adjust;
		if (IS_CUR_DELETED(dbc))
			continue;

		break;
	}
	return (0);
}

/*
 * __bam_c_search --
 *	Move to a specified record.
 */
static int
__bam_c_search(dbc, key, flags, exactp)
	DBC *dbc;
	const DBT *key;
	u_int32_t flags;
	int *exactp;
{
	BTREE *t;
	BTREE_CURSOR *cp;
	DB *dbp;
	PAGE *h;
	db_indx_t indx;
	db_pgno_t bt_lpgno;
	db_recno_t recno;
	u_int32_t sflags;
	int cmp, ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	t = dbp->bt_internal;
	ret = 0;

	/*
	 * Find an entry in the database.  Discard any lock we currently hold,
	 * we're going to search the tree.
	 */
	DISCARD_CUR(dbc, ret);
	if (ret != 0)
		return (ret);

	switch (flags) {
	case DB_SET_RECNO:
		if ((ret = __ram_getno(dbc, key, &recno, 0)) != 0)
			return (ret);
		sflags = (F_ISSET(dbc, DBC_RMW) ? S_FIND_WR : S_FIND) | S_EXACT;
		if ((ret = __bam_rsearch(dbc, &recno, sflags, 1, exactp)) != 0)
			return (ret);
		break;
	case DB_SET:
	case DB_GET_BOTH:
		sflags = (F_ISSET(dbc, DBC_RMW) ? S_FIND_WR : S_FIND) | S_EXACT;
		goto search;
	case DB_SET_RANGE:
		sflags =
		    (F_ISSET(dbc, DBC_RMW) ? S_WRITE : S_READ) | S_DUPFIRST;
		goto search;
	case DB_KEYFIRST:
		sflags = S_KEYFIRST;
		goto fast_search;
	case DB_KEYLAST:
	case DB_NODUPDATA:
		sflags = S_KEYLAST;
fast_search:	/*
		 * If the application has a history of inserting into the first
		 * or last pages of the database, we check those pages first to
		 * avoid doing a full search.
		 *
		 * If the tree has record numbers, we need a complete stack so
		 * that we can adjust the record counts, so fast_search isn't
		 * possible.
		 */
		if (F_ISSET(cp, C_RECNUM))
			goto search;

		/*
		 * !!!
		 * We do not mutex protect the t->bt_lpgno field, which means
		 * that it can only be used in an advisory manner.  If we find
		 * page we can use, great.  If we don't, we don't care, we do
		 * it the slow way instead.  Regardless, copy it into a local
		 * variable, otherwise we might acquire a lock for a page and
		 * then read a different page because it changed underfoot.
		 */
		bt_lpgno = t->bt_lpgno;

		/*
		 * If the tree has no history of insertion, do it the slow way.
		 */
		if (bt_lpgno == PGNO_INVALID)
			goto search;

		/* Lock and retrieve the page on which we last inserted. */
		h = NULL;
		ACQUIRE(dbc,
		    DB_LOCK_WRITE, bt_lpgno, cp->lock, bt_lpgno, h, ret);
		if (ret != 0)
			goto fast_miss;

		/*
		 * It's okay if the page type isn't right or it's empty, it
		 * just means that the world changed.
		 */
		if (TYPE(h) != P_LBTREE || NUM_ENT(h) == 0)
			goto fast_miss;

		/*
		 * What we do here is test to see if we're at the beginning or
		 * end of the tree and if the new item sorts before/after the
		 * first/last page entry.  We don't try and catch inserts into
		 * the middle of the tree (although we could, as long as there
		 * were two keys on the page and we saved both the index and
		 * the page number of the last insert).
		 */
		if (h->next_pgno == PGNO_INVALID) {
			indx = NUM_ENT(h) - P_INDX;
			if ((ret = __bam_cmp(dbp,
			    key, h, indx, t->bt_compare, &cmp)) != 0)
				return (ret);

			if (cmp < 0)
				goto try_begin;
			if (cmp > 0) {
				indx += P_INDX;
				goto fast_hit;
			}

			/*
			 * Found a duplicate.  If doing DB_KEYLAST, we're at
			 * the correct position, otherwise, move to the first
			 * of the duplicates.  If we're looking at off-page
			 * duplicates, duplicate duplicates aren't permitted,
			 * so we're done.
			 */
			if (flags == DB_KEYLAST)
				goto fast_hit;
			for (;
			    indx > 0 && h->inp[indx - P_INDX] == h->inp[indx];
			    indx -= P_INDX)
				;
			goto fast_hit;
		}
try_begin:	if (h->prev_pgno == PGNO_INVALID) {
			indx = 0;
			if ((ret = __bam_cmp(dbp,
			    key, h, indx, t->bt_compare, &cmp)) != 0)
				return (ret);

			if (cmp > 0)
				goto fast_miss;
			if (cmp < 0)
				goto fast_hit;

			/*
			 * Found a duplicate.  If doing DB_KEYFIRST, we're at
			 * the correct position, otherwise, move to the last
			 * of the duplicates.  If we're looking at off-page
			 * duplicates, duplicate duplicates aren't permitted,
			 * so we're done.
			 */
			if (flags == DB_KEYFIRST)
				goto fast_hit;
			for (;
			    indx < (db_indx_t)(NUM_ENT(h) - P_INDX) &&
			    h->inp[indx] == h->inp[indx + P_INDX];
			    indx += P_INDX)
				;
			goto fast_hit;
		}
		goto fast_miss;

fast_hit:	/* Set the exact match flag, we may have found a duplicate. */
		*exactp = cmp == 0;

		/*
		 * Insert the entry in the stack.  (Our caller is likely to
		 * call __bam_stkrel() after our return.)
		 */
		BT_STK_CLR(cp);
		BT_STK_ENTER(dbp->dbenv,
		    cp, h, indx, cp->lock, cp->lock_mode, ret);
		if (ret != 0)
			return (ret);
		break;

fast_miss:	/*
		 * This was not the right page, so we do not need to retain
		 * the lock even in the presence of transactions.
		 */
		DISCARD(dbc, 1, cp->lock, h, ret);
		if (ret != 0)
			return (ret);

search:		if ((ret =
		    __bam_search(dbc, key, sflags, 1, NULL, exactp)) != 0)
			return (ret);
		break;
	default:
		return (__db_unknown_flag(dbp->dbenv, "__bam_c_search", flags));
	}

	/* Initialize the cursor from the stack. */
	cp->page = cp->csp->page;
	cp->pgno = cp->csp->page->pgno;
	cp->indx = cp->csp->indx;
	cp->lock = cp->csp->lock;
	cp->lock_mode = cp->csp->lock_mode;

	/*
	 * If we inserted a key into the first or last slot of the tree,
	 * remember where it was so we can do it more quickly next time.
	 */
	if (TYPE(cp->page) == P_LBTREE &&
	    (flags == DB_KEYFIRST || flags == DB_KEYLAST))
		t->bt_lpgno =
		    (NEXT_PGNO(cp->page) == PGNO_INVALID &&
		    cp->indx >= NUM_ENT(cp->page)) ||
		    (PREV_PGNO(cp->page) == PGNO_INVALID &&
		    cp->indx == 0) ? cp->pgno : PGNO_INVALID;
	return (0);
}

/*
 * __bam_c_physdel --
 *	Physically remove an item from the page.
 */
static int
__bam_c_physdel(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT key;
	DB_LOCK lock;
	PAGE *h;
	db_pgno_t pgno;
	int delete_page, empty_page, exact, level, ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;
	delete_page = empty_page = ret = 0;

	/* If the page is going to be emptied, consider deleting it. */
	delete_page = empty_page =
	    NUM_ENT(cp->page) == (TYPE(cp->page) == P_LBTREE ? 2 : 1);

	/*
	 * Check if the application turned off reverse splits.  Applications
	 * can't turn off reverse splits in off-page duplicate trees, that
	 * space will never be reused unless the exact same key is specified.
	 */
	if (delete_page &&
	    !F_ISSET(dbc, DBC_OPD) && F_ISSET(dbp, DB_BT_REVSPLIT))
		delete_page = 0;

	/*
	 * We never delete the last leaf page.  (Not really true -- we delete
	 * the last leaf page of off-page duplicate trees, but that's handled
	 * by our caller, not down here.)
	 */
	if (delete_page && cp->pgno == cp->root)
		delete_page = 0;

	/*
	 * To delete a leaf page other than an empty root page, we need a
	 * copy of a key from the page.  Use the 0th page index since it's
	 * the last key the page held.
	 */
	if (delete_page) {
		memset(&key, 0, sizeof(DBT));
		if ((ret = __db_ret(dbp, cp->page,
		    0, &key, &dbc->rkey.data, &dbc->rkey.ulen)) != 0)
			return (ret);
	}

	/*
	 * Delete the items.  If page isn't empty, we adjust the cursors.
	 *
	 * !!!
	 * The following operations to delete a page may deadlock.  The easy
	 * scenario is if we're deleting an item because we're closing cursors
	 * because we've already deadlocked and want to call txn_abort().  If
	 * we fail due to deadlock, we'll leave a locked, possibly empty page
	 * in the tree, which won't be empty long because we'll undo the delete
	 * when we undo the transaction's modifications.
	 *
	 * !!!
	 * Delete the key item first, otherwise the on-page duplicate checks
	 * in __bam_ditem() won't work!
	 */
	if (TYPE(cp->page) == P_LBTREE) {
		if ((ret = __bam_ditem(dbc, cp->page, cp->indx)) != 0)
			return (ret);
		if (!empty_page)
			if ((ret = __bam_ca_di(dbc,
			    PGNO(cp->page), cp->indx, -1)) != 0)
				return (ret);
	}
	if ((ret = __bam_ditem(dbc, cp->page, cp->indx)) != 0)
		return (ret);
	if (!empty_page)
		if ((ret = __bam_ca_di(dbc, PGNO(cp->page), cp->indx, -1)) != 0)
			return (ret);

	/* If we're not going to try and delete the page, we're done. */
	if (!delete_page)
		return (0);

	/*
	 * Call __bam_search to reacquire the empty leaf page, but this time
	 * get both the leaf page and it's parent, locked.  Jump back up the
	 * tree, until we have the top pair of pages that we want to delete.
	 * Once we have the top page that we want to delete locked, lock the
	 * underlying pages and check to make sure they're still empty.  If
	 * they are, delete them.
	 */
	for (level = LEAFLEVEL;; ++level) {
		/* Acquire a page and its parent, locked. */
		if ((ret = __bam_search(
		    dbc, &key, S_WRPAIR, level, NULL, &exact)) != 0)
			return (ret);

		/*
		 * If we reach the root or the parent page isn't going to be
		 * empty when we delete one record, stop.
		 */
		h = cp->csp[-1].page;
		if (h->pgno == cp->root || NUM_ENT(h) != 1)
			break;

		/* Discard the stack, retaining no locks. */
		(void)__bam_stkrel(dbc, STK_NOLOCK);
	}

	/*
	 * Move the stack pointer one after the last entry, we may be about
	 * to push more items onto the page stack.
	 */
	++cp->csp;

	/*
	 * cp->csp[-2].page is now the parent page, which we may or may not be
	 * going to delete, and cp->csp[-1].page is the first page we know we
	 * are going to delete.  Walk down the chain of pages, acquiring pages
	 * until we've acquired a leaf page.  Generally, this shouldn't happen;
	 * we should only see a single internal page with one item and a single
	 * leaf page with no items.  The scenario where we could see something
	 * else is if reverse splits were turned off for awhile and then turned
	 * back on.  That could result in all sorts of strangeness, e.g., empty
	 * pages in the tree, trees that looked like linked lists, and so on.
	 *
	 * !!!
	 * Sheer paranoia: if we find any pages that aren't going to be emptied
	 * by the delete, someone else added an item while we were walking the
	 * tree, and we discontinue the delete.  Shouldn't be possible, but we
	 * check regardless.
	 */
	for (h = cp->csp[-1].page;;) {
		if (ISLEAF(h)) {
			if (NUM_ENT(h) != 0)
				break;
			break;
		} else
			if (NUM_ENT(h) != 1)
				break;

		/*
		 * Get the next page, write lock it and push it onto the stack.
		 * We know it's index 0, because it can only have one element.
		 */
		switch (TYPE(h)) {
		case P_IBTREE:
			pgno = GET_BINTERNAL(h, 0)->pgno;
			break;
		case P_IRECNO:
			pgno = GET_RINTERNAL(h, 0)->pgno;
			break;
		default:
			return (__db_pgfmt(dbp, PGNO(h)));
		}

		if ((ret =
		    __db_lget(dbc, 0, pgno, DB_LOCK_WRITE, 0, &lock)) != 0)
			break;
		if ((ret = memp_fget(dbp->mpf, &pgno, 0, &h)) != 0)
			break;
		BT_STK_PUSH(dbp->dbenv, cp, h, 0, lock, DB_LOCK_WRITE, ret);
		if (ret != 0)
			break;
	}

	/* Adjust the cursor stack to reference the last page on the stack. */
	BT_STK_POP(cp);

	/*
	 * If everything worked, delete the stack, otherwise, release the
	 * stack and page locks without further damage.
	 */
	if (ret == 0)
		ret = __bam_dpages(dbc, cp->sp);
	else
		(void)__bam_stkrel(dbc, 0);

	return (ret);
}

/*
 * __bam_c_getstack --
 *	Acquire a full stack for a cursor.
 */
static int
__bam_c_getstack(dbc)
	DBC *dbc;
{
	BTREE_CURSOR *cp;
	DB *dbp;
	DBT dbt;
	PAGE *h;
	int exact, ret, t_ret;

	dbp = dbc->dbp;
	cp = (BTREE_CURSOR *)dbc->internal;

	/*
	 * Get the page with the current item on it.  The caller of this
	 * routine has to already hold a read lock on the page, so there
	 * is no additional lock to acquire.
	 */
	if ((ret = memp_fget(dbp->mpf, &cp->pgno, 0, &h)) != 0)
		return (ret);

	/* Get a copy of a key from the page. */
	memset(&dbt, 0, sizeof(DBT));
	if ((ret = __db_ret(dbp,
	    h, 0, &dbt, &dbc->rkey.data, &dbc->rkey.ulen)) != 0)
		goto err;

	/* Get a write-locked stack for the page. */
	exact = 0;
	ret = __bam_search(dbc, &dbt, S_KEYFIRST, 1, NULL, &exact);

err:	/* Discard the key and the page. */
	if ((t_ret = memp_fput(dbp->mpf, h, 0)) != 0 && ret == 0)
		ret = t_ret;

	return (ret);
}

/*
 * __bam_isopd --
 *	Return if the cursor references an off-page duplicate tree via its
 *	page number.
 */
static int
__bam_isopd(dbc, pgnop)
	DBC *dbc;
	db_pgno_t *pgnop;
{
	BOVERFLOW *bo;

	if (TYPE(dbc->internal->page) != P_LBTREE)
		return (0);

	bo = GET_BOVERFLOW(dbc->internal->page, dbc->internal->indx + O_INDX);
	if (B_TYPE(bo->type) == B_DUPLICATE) {
		*pgnop = bo->pgno;
		return (1);
	}
	return (0);
}
