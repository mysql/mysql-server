/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock.c,v 11.40 2000/12/19 23:18:58 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef	HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_shash.h"
#include "lock.h"
#include "log.h"
#include "db_am.h"
#include "txn.h"

#ifdef	HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

static int  __lock_checklocker __P((DB_LOCKTAB *,
    struct __db_lock *, u_int32_t, u_int32_t, int *));
static int  __lock_get_internal __P((DB_LOCKTAB *, u_int32_t,
    u_int32_t, const DBT *, db_lockmode_t, DB_LOCK *));
static int __lock_is_parent __P((DB_LOCKTAB *, u_int32_t, DB_LOCKER *));
static int __lock_put_internal __P((DB_LOCKTAB *,
    struct __db_lock *, u_int32_t,  u_int32_t));
static int __lock_put_nolock __P((DB_ENV *, DB_LOCK *, int *, int));
static void __lock_remove_waiter __P((DB_ENV *,
    DB_LOCKOBJ *, struct __db_lock *, db_status_t));

static const char __db_lock_err[] = "Lock table is out of available %s";
static const char __db_lock_invalid[] = "%s: Lock is no longer valid";
static const char __db_locker_invalid[] = "Locker is not valid";

/*
 * lock_id --
 *	Generate a unique locker id.
 */
int
lock_id(dbenv, idp)
	DB_ENV *dbenv;
	u_int32_t *idp;
{
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_lock_id(dbenv, idp));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lk_handle, DB_INIT_LOCK);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	/*
	 * Note that we are letting locker IDs wrap.
	 *
	 * This is potentially dangerous in that it's conceivable that you
	 * could be allocating a new locker id and still have someone using
	 * it.  However, the alternatives are that we keep a bitmap of
	 * locker ids or we forbid wrapping.  Both are probably bad.  The
	 * bitmap of locker ids will take up 64 MB of space.  Forbidding
	 * wrapping means that we'll run out of locker IDs after 2 billion.
	 * In order for the wrap bug to fire, we'd need to have something
	 * that stayed open while 2 billion locker ids were used up.  Since
	 * we cache cursors it means that something would have to stay open
	 * sufficiently long that we open and close a lot of files and a
	 * lot of cursors within them.  Betting that this won't happen seems
	 * to the lesser of the evils.
	 */
	LOCKREGION(dbenv, lt);
	if (region->id >= DB_LOCK_MAXID)
		region->id = 0;
	*idp = ++region->id;
	UNLOCKREGION(dbenv, lt);

	return (0);
}

/*
 * Vector lock routine.  This function takes a set of operations
 * and performs them all at once.  In addition, lock_vec provides
 * functionality for lock inheritance, releasing all locks for a
 * given locker (used during transaction commit/abort), releasing
 * all locks on a given object, and generating debugging information.
 */
int
lock_vec(dbenv, locker, flags, list, nlist, elistp)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	int nlist;
	DB_LOCKREQ *list, **elistp;
{
	struct __db_lock *lp, *next_lock;
	DB_LOCKER *sh_locker, *sh_parent;
	DB_LOCKOBJ *obj, *sh_obj;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t lndx, ndx;
	int did_abort, i, ret, run_dd;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_lock_vec(dbenv, locker,
		    flags, list, nlist, elistp));
#endif
	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lk_handle, DB_INIT_LOCK);

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv, "lock_vec", flags, DB_LOCK_NOWAIT)) != 0)
		return (ret);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	run_dd = 0;
	LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	for (i = 0, ret = 0; i < nlist && ret == 0; i++)
		switch (list[i].op) {
		case DB_LOCK_GET:
			ret = __lock_get_internal(dbenv->lk_handle,
			    locker, flags,
			    list[i].obj, list[i].mode, &list[i].lock);
			break;
		case DB_LOCK_INHERIT:

			/*
			 * Get the committing locker and mark it as deleted.
			 * This allows us to traverse the locker links without
			 * worrying that someone else is deleting locks out
			 * from under us.  However, if the locker doesn't
			 * exist, that just means that the child holds no
			 * locks, so inheritance is easy!
			 */
			LOCKER_LOCK(lt, region, locker, ndx);
			if ((ret = __lock_getlocker(lt,
			    locker, ndx, 0, &sh_locker)) != 0 ||
			    sh_locker == NULL ||
			    F_ISSET(sh_locker, DB_LOCKER_DELETED)) {
				if (ret == 0 && sh_locker != NULL)
					ret = EACCES;
				__db_err(dbenv, __db_locker_invalid);
				break;
			}

			/* Make sure we are a child transaction. */
			if (sh_locker->parent_locker == INVALID_ROFF) {
				__db_err(dbenv, "Not a child transaction");
				ret = EINVAL;
				break;
			}
			sh_parent = (DB_LOCKER *)
			    R_ADDR(&lt->reginfo, sh_locker->parent_locker);
			F_SET(sh_locker, DB_LOCKER_DELETED);

			/*
			 * Now, lock the parent locker; move locks from
			 * the committing list to the parent's list.
			 */
			LOCKER_LOCK(lt, region, locker, ndx);
			if (F_ISSET(sh_parent, DB_LOCKER_DELETED)) {
				if (ret == 0) {
					__db_err(dbenv,
					     "Parent locker is not valid");
					ret = EACCES;
				}
				break;
			}

			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL;
			    lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock)) {
				SH_LIST_REMOVE(lp, locker_links, __db_lock);
				SH_LIST_INSERT_HEAD(&sh_parent->heldby, lp,
				    locker_links, __db_lock);
				lp->holder = sh_parent->id;

				/* Get the object associated with this lock. */
				obj = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);

				(void)__lock_promote(lt, obj,
				    LF_ISSET(DB_LOCK_NOWAITERS));
			}

			/* Now free the original locker. */
			ret = __lock_checklocker(lt,
			    NULL, locker, DB_LOCK_IGNOREDEL, NULL);
			break;
		case DB_LOCK_PUT:
			ret =
			    __lock_put_nolock(dbenv, &list[i].lock, &run_dd, 0);
			break;
		case DB_LOCK_PUT_ALL:
			/*
			 * Get the locker and mark it as deleted.  This
			 * allows us to traverse the locker links without
			 * worrying that someone else is deleting locks out
			 * from under us.  Since the locker may hold no
			 * locks (i.e., you could call abort before you've
			 * done any work), it's perfectly reasonable for there
			 * to be no locker; this is not an error.
			 */
			LOCKER_LOCK(lt, region, locker, ndx);
			if ((ret = __lock_getlocker(lt,
			    locker, ndx, 0, &sh_locker)) != 0 ||
			    sh_locker == NULL ||
			    F_ISSET(sh_locker, DB_LOCKER_DELETED))
				/*
				 * If ret is set, then we'll generate an
				 * error.  If it's not set, we have nothing
				 * to do.
				 */
				break;
			F_SET(sh_locker, DB_LOCKER_DELETED);

			/* Now traverse the locks, releasing each one. */
			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL;
			    lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock)) {
				SH_LIST_REMOVE(lp, locker_links, __db_lock);
				sh_obj =
				    (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
				SHOBJECT_LOCK(lt, region, sh_obj, lndx);
				ret = __lock_put_internal(lt,
				    lp, lndx, DB_LOCK_FREE | DB_LOCK_DOALL);
				if (ret != 0)
					break;
			}
			ret = __lock_checklocker(lt,
			    NULL, locker, DB_LOCK_IGNOREDEL, NULL);
			break;
		case DB_LOCK_PUT_OBJ:
			/* Remove all the locks associated with an object. */
			OBJECT_LOCK(lt, region, list[i].obj, ndx);
			if ((ret = __lock_getobj(lt, list[i].obj,
			    ndx, 0, &sh_obj)) != 0 || sh_obj == NULL) {
				if (ret == 0)
					ret = EINVAL;
				break;
			}

			/*
			 * Go through both waiters and holders.  Don't bother
			 * to run promotion, because everyone is getting
			 * released.  The processes waiting will still get
			 * awakened as their waiters are released.
			 */
			for (lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock);
			    ret == 0 && lp != NULL;
			    lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock))
				ret = __lock_put_internal(lt,
				    lp, ndx, DB_LOCK_NOPROMOTE | DB_LOCK_DOALL);

			/*
			 * On the last time around, the object will get
			 * reclaimed by __lock_put_internal, structure the
			 * loop carefully so we do not get bitten.
			 */
			for (lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
			    ret == 0 && lp != NULL;
			    lp = next_lock) {
				next_lock = SH_TAILQ_NEXT(lp, links, __db_lock);
				ret = __lock_put_internal(lt,
				    lp, ndx, DB_LOCK_NOPROMOTE | DB_LOCK_DOALL);
			}
			break;
#ifdef DEBUG
		case DB_LOCK_DUMP:
			/* Find the locker. */
			LOCKER_LOCK(lt, region, locker, ndx);
			if ((ret = __lock_getlocker(lt,
			    locker, ndx, 0, &sh_locker)) != 0
			    || sh_locker == NULL
			    || F_ISSET(sh_locker, DB_LOCKER_DELETED))
				break;

			for (lp = SH_LIST_FIRST(&sh_locker->heldby, __db_lock);
			    lp != NULL;
			    lp = SH_LIST_NEXT(lp, locker_links, __db_lock)) {
				__lock_printlock(lt, lp, 1);
			}
			break;
#endif
		default:
			__db_err(dbenv,
			    "Invalid lock operation: %d", list[i].op);
			ret = EINVAL;
			break;
		}

	if (ret == 0 && region->need_dd && region->detect != DB_LOCK_NORUN) {
		run_dd = 1;
		region->need_dd = 0;
	}
	UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

	if (run_dd)
		(void)lock_detect(dbenv, 0, region->detect, &did_abort);

	if (ret != 0 && elistp != NULL)
		*elistp = &list[i - 1];

	return (ret);
}

/*
 * Lock acquisition routines.  There are two library interfaces:
 *
 * lock_get --
 *	original lock get interface that takes a locker id.
 *
 * All the work for lock_get (and for the GET option of lock_vec) is done
 * inside of lock_get_internal.
 */
int
lock_get(dbenv, locker, flags, obj, lock_mode, lock)
	DB_ENV *dbenv;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	int ret;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_lock_get(dbenv, locker,
		    flags, obj, lock_mode, lock));
#endif
	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lk_handle, DB_INIT_LOCK);

	if (IS_RECOVERING(dbenv)) {
		lock->off = LOCK_INVALID;
		return (0);
	}

	/* Validate arguments. */
	if ((ret = __db_fchk(dbenv,
	    "lock_get", flags,
	    DB_LOCK_NOWAIT | DB_LOCK_UPGRADE | DB_LOCK_SWITCH)) != 0)
		return (ret);

	LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	ret = __lock_get_internal(dbenv->lk_handle,
	    locker, flags, obj, lock_mode, lock);
	UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);
	return (ret);
}

static int
__lock_get_internal(lt, locker, flags, obj, lock_mode, lock)
	DB_LOCKTAB *lt;
	u_int32_t locker, flags;
	const DBT *obj;
	db_lockmode_t lock_mode;
	DB_LOCK *lock;
{
	struct __db_lock *newl, *lp;
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	u_int32_t locker_ndx;
	int did_abort, freed, ihold, on_locker_list, no_dd, ret;

	no_dd = ret = 0;
	on_locker_list = 0;
	region = lt->reginfo.primary;
	dbenv = lt->dbenv;

	/*
	 * If we are not going to reuse this lock, initialize
	 * the offset to invalid so that if we fail it
	 * will not look like a valid lock.
	 */
	if (!LF_ISSET(DB_LOCK_UPGRADE | DB_LOCK_SWITCH))
		lock->off = LOCK_INVALID;

	/*
	 * Check that the lock mode is valid.
	 */
	if ((u_int32_t)lock_mode >= region->nmodes) {
		__db_err(dbenv,
		    "lock_get: invalid lock mode %lu\n", (u_long)lock_mode);
		return (EINVAL);
	}

	/* Allocate a new lock.  Optimize for the common case of a grant. */
	region->nrequests++;
	if ((newl = SH_TAILQ_FIRST(&region->free_locks, __db_lock)) != NULL)
		SH_TAILQ_REMOVE(&region->free_locks, newl, links, __db_lock);
	if (newl == NULL) {
		__db_err(dbenv, __db_lock_err, "locks");
		return (ENOMEM);
	}
	if (++region->nlocks > region->maxnlocks)
		region->maxnlocks = region->nlocks;

	/* Allocate a new object. */
	OBJECT_LOCK(lt, region, obj, lock->ndx);
	if ((ret = __lock_getobj(lt, obj, lock->ndx, 1, &sh_obj)) != 0)
		goto err;

	/* Get the locker, we may need it to find our parent. */
	LOCKER_LOCK(lt, region, locker, locker_ndx);
	if ((ret =
	     __lock_getlocker(lt, locker, locker_ndx, 1, &sh_locker)) != 0) {
		/*
		 * XXX: Margo
		 * CLEANUP the object and the lock.
		 */
		return (ret);
	}

	/*
	 * Now we have a lock and an object and we need to see if we should
	 * grant the lock.  We use a FIFO ordering so we can only grant a
	 * new lock if it does not conflict with anyone on the holders list
	 * OR anyone on the waiters list.  The reason that we don't grant if
	 * there's a conflict is that this can lead to starvation (a writer
	 * waiting on a popularly read item will never be granted).  The
	 * downside of this is that a waiting reader can prevent an upgrade
	 * from reader to writer, which is not uncommon.
	 *
	 * There is one exception to the no-conflict rule.  If a lock is held
	 * by the requesting locker AND the new lock does not conflict with
	 * any other holders, then we grant the lock.  The most common place
	 * this happens is when the holder has a WRITE lock and a READ lock
	 * request comes in for the same locker.  If we do not grant the read
	 * lock, then we guarantee deadlock.
	 *
	 * In case of conflict, we put the new lock on the end of the waiters
	 * list, unless we are upgrading in which case the locker goes on the
	 * front of the list.
	 */
	ihold = 0;
	lp = NULL;
	if (LF_ISSET(DB_LOCK_SWITCH))
		goto put_lock;

	for (lp = SH_TAILQ_FIRST(&sh_obj->holders, __db_lock);
	    lp != NULL;
	    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
		if (locker == lp->holder ||
		    __lock_is_parent(lt, lp->holder, sh_locker)) {
			if (lp->mode == lock_mode &&
			    lp->status == DB_LSTAT_HELD) {
				if (LF_ISSET(DB_LOCK_UPGRADE))
					goto upgrade;

				/*
				 * Lock is held, so we can increment the
				 * reference count and return this lock.
				 */
				lp->refcount++;
				lock->off = R_OFFSET(&lt->reginfo, lp);
				lock->gen = lp->gen;

				ret = 0;
				goto done;
			} else
				ihold = 1;
		} else if (CONFLICTS(lt, region, lp->mode, lock_mode))
			break;
	}

	/*
	 * Make the new lock point to the new object, initialize fields.
	 *
	 * This lock is not linked in anywhere, so we can muck with it
	 * without holding any mutexes.
	 */
put_lock:
	newl->holder = locker;
	newl->refcount = 1;
	newl->mode = lock_mode;
	newl->obj = SH_PTR_TO_OFF(newl, sh_obj);
	newl->status = DB_LSTAT_HELD;

	/*
	 * If we are upgrading, then there are two scenarios.  Either
	 * we had no conflicts, so we can do the upgrade.  Or, there
	 * is a conflict and we should wait at the HEAD of the waiters
	 * list.
	 */
	if (LF_ISSET(DB_LOCK_UPGRADE)) {
		if (lp == NULL)
			goto upgrade;

		/*
		 * There was a conflict, wait.  If this is the first waiter,
		 * add the object to the deadlock detector's list.
		 */
		if (SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock) == NULL)
			SH_TAILQ_INSERT_HEAD(&region->dd_objs,
			    sh_obj, dd_links, __db_lockobj);

		SH_TAILQ_INSERT_HEAD(&sh_obj->waiters, newl, links, __db_lock);
		goto llist;
	}

	if (lp == NULL && !ihold)
		for (lp = SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock);
		    lp != NULL;
		    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
			if (CONFLICTS(lt, region, lp->mode, lock_mode) &&
			    locker != lp->holder)
				break;
		}
	if (!LF_ISSET(DB_LOCK_SWITCH) && lp == NULL)
		SH_TAILQ_INSERT_TAIL(&sh_obj->holders, newl, links);
	else if (!LF_ISSET(DB_LOCK_NOWAIT)) {
		/*
		 * If this is the first waiter, add the object to the
		 * deadlock detector's list.
		 */
		if (SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock) == NULL)
			SH_TAILQ_INSERT_HEAD(&region->dd_objs,
			    sh_obj, dd_links, __db_lockobj);
		SH_TAILQ_INSERT_TAIL(&sh_obj->waiters, newl, links);
	} else {
		ret = DB_LOCK_NOTGRANTED;
		if (SH_LIST_FIRST(&sh_locker->heldby, __db_lock) == NULL
		    && LOCKER_FREEABLE(sh_locker))
			__lock_freelocker( lt, region, sh_locker, locker_ndx);
		region->nnowaits++;
		goto err;
	}

llist:
	/*
	 * Now, insert the lock onto its locker's list.  If the locker does
	 * not currently hold any locks, there's no reason to run a deadlock
	 * detector, save that information.
	 */
	on_locker_list = 1;
	no_dd = sh_locker->master_locker == INVALID_ROFF
	    && SH_LIST_FIRST(&sh_locker->child_locker, __db_locker) == NULL
	    && SH_LIST_FIRST(&sh_locker->heldby, __db_lock) == NULL;

	SH_LIST_INSERT_HEAD(&sh_locker->heldby, newl, locker_links, __db_lock);

	if (LF_ISSET(DB_LOCK_SWITCH) || lp != NULL) {
		if (LF_ISSET(DB_LOCK_SWITCH) &&
		    (ret = __lock_put_nolock(dbenv,
		    lock, &ihold, DB_LOCK_NOWAITERS)) != 0)
			goto err;
		/*
		 * This is really a blocker for the thread.  It should be
		 * initialized locked, so that when we try to acquire it, we
		 * block.
		 */
		newl->status = DB_LSTAT_WAITING;
		region->nconflicts++;
		if (region->detect == DB_LOCK_NORUN)
			region->need_dd = 1;
		UNLOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

		/*
		 * We are about to wait; before waiting, see if the deadlock
		 * detector should be run.
		 */
		if (region->detect != DB_LOCK_NORUN && !no_dd)
			(void)lock_detect(dbenv, 0, region->detect, &did_abort);

		MUTEX_LOCK(dbenv, &newl->mutex, dbenv->lockfhp);
		LOCKREGION(dbenv, (DB_LOCKTAB *)dbenv->lk_handle);

		if (newl->status != DB_LSTAT_PENDING) {
			(void)__lock_checklocker(lt,
			    newl, newl->holder, 0, &freed);
			switch (newl->status) {
				case DB_LSTAT_ABORTED:
					on_locker_list = 0;
					ret = DB_LOCK_DEADLOCK;
					break;
				case DB_LSTAT_NOGRANT:
					ret = DB_LOCK_NOTGRANTED;
					break;
				default:
					ret = EINVAL;
					break;
			}
			goto err;
		} else if (LF_ISSET(DB_LOCK_UPGRADE)) {
			/*
			 * The lock that was just granted got put on the
			 * holders list.  Since we're upgrading some other
			 * lock, we've got to remove it here.
			 */
			SH_TAILQ_REMOVE(
			    &sh_obj->holders, newl, links, __db_lock);
			/*
			 * Ensure that the object is not believed to be on
			 * the object's lists, if we're traversing by locker.
			 */
			newl->links.stqe_prev = -1;
			goto upgrade;
		} else
			newl->status = DB_LSTAT_HELD;
	}

	lock->off = R_OFFSET(&lt->reginfo, newl);
	lock->gen = newl->gen;

	return (0);

upgrade:/*
	 * This was an upgrade, so return the new lock to the free list and
	 * upgrade the mode of the original lock.
	 */
	((struct __db_lock *)R_ADDR(&lt->reginfo, lock->off))->mode = lock_mode;

	ret = 0;
	/* FALLTHROUGH */

done:
err:	newl->status = DB_LSTAT_FREE;
	if (on_locker_list) {
		SH_LIST_REMOVE(newl, locker_links, __db_lock);
	}
	SH_TAILQ_INSERT_HEAD(&region->free_locks, newl, links, __db_lock);
	region->nlocks--;
	return (ret);
}

/*
 * Lock release routines.
 *
 * The user callable one is lock_put and the three we use internally are
 * __lock_put_nolock, __lock_put_internal and __lock_downgrade.
 */
int
lock_put(dbenv, lock)
	DB_ENV *dbenv;
	DB_LOCK *lock;
{
	DB_LOCKTAB *lt;
	int ret, run_dd;

#ifdef	HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_lock_put(dbenv, lock));
#endif
	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lk_handle, DB_INIT_LOCK);

	if (IS_RECOVERING(dbenv))
		return (0);

	lt = dbenv->lk_handle;

	LOCKREGION(dbenv, lt);
	ret = __lock_put_nolock(dbenv, lock, &run_dd, 0);
	UNLOCKREGION(dbenv, lt);

	if (ret == 0 && run_dd)
		(void)lock_detect(dbenv, 0,
		    ((DB_LOCKREGION *)lt->reginfo.primary)->detect, NULL);
	return (ret);
}

static int
__lock_put_nolock(dbenv, lock, runp, flags)
	DB_ENV *dbenv;
	DB_LOCK *lock;
	int *runp;
	int flags;
{
	struct __db_lock *lockp;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t locker;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	lockp = (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);
	lock->off = LOCK_INVALID;
	if (lock->gen != lockp->gen) {
		__db_err(dbenv, __db_lock_invalid, "lock_put");
		return (EACCES);
	}

	locker = lockp->holder;
	ret = __lock_put_internal(lt,
	    lockp, lock->ndx, flags | DB_LOCK_UNLINK | DB_LOCK_FREE);

	*runp = 0;
	if (ret == 0 && region->need_dd && region->detect != DB_LOCK_NORUN) {
		*runp = 1;
		region->need_dd = 0;
	}

	return (ret);
}

/*
 * __lock_downgrade --
 *	Used by the concurrent access product to downgrade write locks
 * back to iwrite locks.
 *
 * PUBLIC: int __lock_downgrade __P((DB_ENV *,
 * PUBLIC:     DB_LOCK *, db_lockmode_t, u_int32_t));
 */
int
__lock_downgrade(dbenv, lock, new_mode, flags)
	DB_ENV *dbenv;
	DB_LOCK *lock;
	db_lockmode_t new_mode;
	u_int32_t flags;
{
	struct __db_lock *lockp;
	DB_LOCKOBJ *obj;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	int ret;

	COMPQUIET(flags, 0);

	PANIC_CHECK(dbenv);

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	LOCKREGION(dbenv, lt);

	lockp = (struct __db_lock *)R_ADDR(&lt->reginfo, lock->off);
	if (lock->gen != lockp->gen) {
		__db_err(dbenv, __db_lock_invalid, "lock_downgrade");
		ret = EACCES;
		goto out;
	}

	lockp->mode = new_mode;

	/* Get the object associated with this lock. */
	obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);
	(void)__lock_promote(lt, obj, LF_ISSET(DB_LOCK_NOWAITERS));

	++region->nreleases;
out:	UNLOCKREGION(dbenv, lt);

	return (0);
}

static int
__lock_put_internal(lt, lockp, obj_ndx, flags)
	DB_LOCKTAB *lt;
	struct __db_lock *lockp;
	u_int32_t obj_ndx;
	u_int32_t flags;
{
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	int no_reclaim, ret, state_changed;

	region = lt->reginfo.primary;
	no_reclaim = ret = state_changed = 0;

	if (!OBJ_LINKS_VALID(lockp)) {
		/*
		 * Someone removed this lock while we were doing a release
		 * by locker id.  We are trying to free this lock, but it's
		 * already been done; all we need to do is return it to the
		 * free list.
		 */
		lockp->status = DB_LSTAT_FREE;
		SH_TAILQ_INSERT_HEAD(
		    &region->free_locks, lockp, links, __db_lock);
		region->nlocks--;
		return (0);
	}

	if (LF_ISSET(DB_LOCK_DOALL))
		region->nreleases += lockp->refcount;
	else
		region->nreleases++;

	if (!LF_ISSET(DB_LOCK_DOALL) && lockp->refcount > 1) {
		lockp->refcount--;
		return (0);
	}

	/* Increment generation number. */
	lockp->gen++;

	/* Get the object associated with this lock. */
	sh_obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);

	/* Remove this lock from its holders/waitlist. */
	if (lockp->status != DB_LSTAT_HELD)
		__lock_remove_waiter(lt->dbenv, sh_obj, lockp, DB_LSTAT_FREE);
	else {
		SH_TAILQ_REMOVE(&sh_obj->holders, lockp, links, __db_lock);
		lockp->links.stqe_prev = -1;
	}

	if (LF_ISSET(DB_LOCK_NOPROMOTE))
		state_changed = 0;
	else
		state_changed =
		    __lock_promote(lt, sh_obj, LF_ISSET(DB_LOCK_NOWAITERS));

	if (LF_ISSET(DB_LOCK_UNLINK))
		ret = __lock_checklocker(lt, lockp, lockp->holder, flags, NULL);

	/* Check if object should be reclaimed. */
	if (SH_TAILQ_FIRST(&sh_obj->holders, __db_lock) == NULL
	    && SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock) == NULL) {
		HASHREMOVE_EL(lt->obj_tab,
		    obj_ndx, __db_lockobj, links, sh_obj);
		if (sh_obj->lockobj.size > sizeof(sh_obj->objdata))
			__db_shalloc_free(lt->reginfo.addr,
			    SH_DBT_PTR(&sh_obj->lockobj));
		SH_TAILQ_INSERT_HEAD(
		    &region->free_objs, sh_obj, links, __db_lockobj);
		region->nobjects--;
		state_changed = 1;
	}

	/* Free lock. */
	if (!LF_ISSET(DB_LOCK_UNLINK) && LF_ISSET(DB_LOCK_FREE)) {
		lockp->status = DB_LSTAT_FREE;
		SH_TAILQ_INSERT_HEAD(
		    &region->free_locks, lockp, links, __db_lock);
		region->nlocks--;
	}

	/*
	 * If we did not promote anyone; we need to run the deadlock
	 * detector again.
	 */
	if (state_changed == 0)
		region->need_dd = 1;

	return (ret);
}

/*
 * Utility functions; listed alphabetically.
 */

/*
 * __lock_checklocker --
 *	If a locker has no more locks, then we can free the object.
 * Return a boolean indicating whether we freed the object or not.
 *
 * Must be called without the locker's lock set.
 */
static int
__lock_checklocker(lt, lockp, locker, flags, freed)
	DB_LOCKTAB *lt;
	struct __db_lock *lockp;
	u_int32_t locker, flags;
	int *freed;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	u_int32_t indx;
	int ret;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;
	ret = 0;

	if (freed != NULL)
		*freed = 0;

	LOCKER_LOCK(lt, region, locker, indx);

	/* If the locker's list is NULL, free up the locker. */
	if ((ret = __lock_getlocker(lt,
	    locker, indx, 0, &sh_locker)) != 0 || sh_locker == NULL) {
		if (ret == 0)
			ret = EACCES;
		__db_err(lt->dbenv, __db_locker_invalid);
		goto freelock;
	}

	if (F_ISSET(sh_locker, DB_LOCKER_DELETED)) {
		LF_CLR(DB_LOCK_FREE);
		if (!LF_ISSET(DB_LOCK_IGNOREDEL))
			goto freelock;
	}

	if (LF_ISSET(DB_LOCK_UNLINK))
		SH_LIST_REMOVE(lockp, locker_links, __db_lock);

	if (SH_LIST_FIRST(&sh_locker->heldby, __db_lock) == NULL
	    && LOCKER_FREEABLE(sh_locker)) {
		__lock_freelocker( lt, region, sh_locker, indx);
		if (freed != NULL)
			*freed = 1;
	}

freelock:
	if (LF_ISSET(DB_LOCK_FREE)) {
		lockp->status = DB_LSTAT_FREE;
		SH_TAILQ_INSERT_HEAD(
		    &region->free_locks, lockp, links, __db_lock);
		region->nlocks--;
	}

	return (ret);
}

/*
 * __lock_addfamilylocker
 *	Put a locker entry in for a child transaction.
 *
 * PUBLIC: int __lock_addfamilylocker __P((DB_ENV *, u_int32_t, u_int32_t));
 */
int
__lock_addfamilylocker(dbenv, pid, id)
	DB_ENV *dbenv;
	u_int32_t pid, id;
{
	DB_LOCKER *lockerp, *mlockerp;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;
	LOCKREGION(dbenv, lt);

	/* get/create the  parent locker info */
	LOCKER_LOCK(lt, region, pid, ndx);
	if ((ret = __lock_getlocker(dbenv->lk_handle,
	    pid, ndx, 1, &mlockerp)) != 0)
		goto err;

	/*
	 * We assume that only one thread can manipulate
	 * a single transaction family.
	 * Therefore the master locker cannot go away while
	 * we manipulate it, nor can another child in the
	 * family be created at the same time.
	 */
	LOCKER_LOCK(lt, region, id, ndx);
	if ((ret = __lock_getlocker(dbenv->lk_handle,
	    id, ndx, 1, &lockerp)) != 0)
		goto err;

	/* Point to our parent. */
	lockerp->parent_locker = R_OFFSET(&lt->reginfo, mlockerp);

	/* See if this locker is the family master. */
	if (mlockerp->master_locker == INVALID_ROFF)
		lockerp->master_locker = R_OFFSET(&lt->reginfo, mlockerp);
	else {
		lockerp->master_locker = mlockerp->master_locker;
		mlockerp = R_ADDR(&lt->reginfo, mlockerp->master_locker);
	}

	/*
	 * Link the child at the head of the master's list.
	 * The guess is when looking for deadlock that
	 * the most recent child is the one thats blocked.
	 */
	SH_LIST_INSERT_HEAD(
	    &mlockerp->child_locker, lockerp, child_link, __db_locker);

err:
	UNLOCKREGION(dbenv, lt);

	return (ret);
}

/*
 * __lock_freefamilylocker
 *	Remove a locker from the hash table and its family.
 *
 * This must be called without the locker bucket locked.
 *
 * PUBLIC: int __lock_freefamilylocker  __P((DB_LOCKTAB *, u_int32_t));
 */
int
__lock_freefamilylocker(lt, locker)
	DB_LOCKTAB *lt;
	u_int32_t locker;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	u_int32_t indx;
	int ret;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	LOCKREGION(dbenv, lt);
	LOCKER_LOCK(lt, region, locker, indx);

	if ((ret = __lock_getlocker(lt,
	    locker, indx, 0, &sh_locker)) != 0 || sh_locker == NULL) {
		if (ret == 0)
			ret = EACCES;
		goto freelock;
	}
	if (SH_LIST_FIRST(&sh_locker->heldby, __db_lock) != NULL) {
		ret = EINVAL;
		__db_err(dbenv, "Freeing locker with locks");
		goto freelock;
	}

	/* If this is part of a family, we must fix up its links. */
	if (sh_locker->master_locker != INVALID_ROFF)
		SH_LIST_REMOVE(sh_locker, child_link, __db_locker);

	__lock_freelocker(lt, region, sh_locker, indx);

freelock:
	UNLOCKREGION(dbenv, lt);
	return (ret);
}

/*
 * __lock_freelocker
 *	common code for deleting a locker.
 *
 * This must be called with the locker bucket locked.
 *
 * PUBLIC: void __lock_freelocker __P((DB_LOCKTAB *,
 * PUBLIC:     DB_LOCKREGION *, DB_LOCKER *, u_int32_t));
 */
void
__lock_freelocker(lt, region, sh_locker, indx)
	DB_LOCKTAB *lt;
	DB_LOCKREGION *region;
	DB_LOCKER *sh_locker;
	u_int32_t indx;

{
	HASHREMOVE_EL(
	    lt->locker_tab, indx, __db_locker, links, sh_locker);
	SH_TAILQ_INSERT_HEAD(
	    &region->free_lockers, sh_locker, links, __db_locker);
	region->nlockers--;
}

/*
 * __lock_getlocker --
 *	Get a locker in the locker hash table.  The create parameter
 * indicates if the locker should be created if it doesn't exist in
 * the table.
 *
 * This must be called with the locker bucket locked.
 *
 * PUBLIC: int __lock_getlocker __P((DB_LOCKTAB *,
 * PUBLIC:     u_int32_t, u_int32_t, int, DB_LOCKER **));
 */
int
__lock_getlocker(lt, locker, indx, create, retp)
	DB_LOCKTAB *lt;
	u_int32_t locker, indx;
	int create;
	DB_LOCKER **retp;
{
	DB_ENV *dbenv;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	HASHLOOKUP(lt->locker_tab,
	    indx, __db_locker, links, locker, sh_locker, __lock_locker_cmp);

	/*
	 * If we found the locker, then we can just return it.  If
	 * we didn't find the locker, then we need to create it.
	 */
	if (sh_locker == NULL && create) {
		/* Create new locker and then insert it into hash table. */
		if ((sh_locker = SH_TAILQ_FIRST(
		    &region->free_lockers, __db_locker)) == NULL) {
			__db_err(lt->dbenv, __db_lock_err, "locker entries");
			return (ENOMEM);
		}
		SH_TAILQ_REMOVE(
		    &region->free_lockers, sh_locker, links, __db_locker);
		if (++region->nlockers > region->maxnlockers)
			region->maxnlockers = region->nlockers;

		sh_locker->id = locker;
		sh_locker->dd_id = 0;
		sh_locker->master_locker = INVALID_ROFF;
		sh_locker->parent_locker = INVALID_ROFF;
		SH_LIST_INIT(&sh_locker->child_locker);
		sh_locker->flags = 0;
		SH_LIST_INIT(&sh_locker->heldby);

		HASHINSERT(lt->locker_tab, indx, __db_locker, links, sh_locker);
	}

	*retp = sh_locker;
	return (0);
}

/*
 * __lock_getobj --
 *	Get an object in the object hash table.  The create parameter
 * indicates if the object should be created if it doesn't exist in
 * the table.
 *
 * This must be called with the object bucket locked.
 *
 * PUBLIC: int __lock_getobj __P((DB_LOCKTAB *,
 * PUBLIC:     const DBT *, u_int32_t, int, DB_LOCKOBJ **));
 */
int
__lock_getobj(lt, obj, ndx, create, retp)
	DB_LOCKTAB *lt;
	const DBT *obj;
	u_int32_t ndx;
	int create;
	DB_LOCKOBJ **retp;
{
	DB_ENV *dbenv;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	int ret;
	void *p;

	dbenv = lt->dbenv;
	region = lt->reginfo.primary;

	/* Look up the object in the hash table. */
	HASHLOOKUP(lt->obj_tab,
	    ndx, __db_lockobj, links, obj, sh_obj, __lock_cmp);

	/*
	 * If we found the object, then we can just return it.  If
	 * we didn't find the object, then we need to create it.
	 */
	if (sh_obj == NULL && create) {
		/* Create new object and then insert it into hash table. */
		if ((sh_obj =
		    SH_TAILQ_FIRST(&region->free_objs, __db_lockobj)) == NULL) {
			__db_err(lt->dbenv, __db_lock_err, "object entries");
			ret = ENOMEM;
			goto err;
		}

		/*
		 * If we can fit this object in the structure, do so instead
		 * of shalloc-ing space for it.
		 */
		if (obj->size <= sizeof(sh_obj->objdata))
			p = sh_obj->objdata;
		else if ((ret = __db_shalloc(
		    lt->reginfo.addr, obj->size, 0, &p)) != 0) {
			__db_err(dbenv, "No space for lock object storage");
			goto err;
		}

		memcpy(p, obj->data, obj->size);

		SH_TAILQ_REMOVE(
		    &region->free_objs, sh_obj, links, __db_lockobj);
		if (++region->nobjects > region->maxnobjects)
			region->maxnobjects = region->nobjects;

		SH_TAILQ_INIT(&sh_obj->waiters);
		SH_TAILQ_INIT(&sh_obj->holders);
		sh_obj->lockobj.size = obj->size;
		sh_obj->lockobj.off = SH_PTR_TO_OFF(&sh_obj->lockobj, p);

		HASHINSERT(lt->obj_tab, ndx, __db_lockobj, links, sh_obj);
	}

	*retp = sh_obj;
	return (0);

err:	return (ret);
}

/*
 * __lock_is_parent --
 *	Given a locker and a transaction, return 1 if the locker is
 * an ancestor of the designcated transaction.  This is used to determine
 * if we should grant locks that appear to conflict, but don't because
 * the lock is already held by an ancestor.
 */
static int
__lock_is_parent(lt, locker, sh_locker)
	DB_LOCKTAB *lt;
	u_int32_t locker;
	DB_LOCKER *sh_locker;
{
	DB_LOCKER *parent;

	parent = sh_locker;
	while (parent->parent_locker != INVALID_ROFF) {
		parent = (DB_LOCKER *)
		     R_ADDR(&lt->reginfo, parent->parent_locker);
		if (parent->id == locker)
			return (1);
	}

	return (0);
}

/*
 * __lock_promote --
 *
 * Look through the waiters and holders lists and decide which (if any)
 * locks can be promoted.   Promote any that are eligible.
 *
 * PUBLIC: int __lock_promote __P((DB_LOCKTAB *, DB_LOCKOBJ *, int));
 */
int
__lock_promote(lt, obj, not_waiters)
	DB_LOCKTAB *lt;
	DB_LOCKOBJ *obj;
	int not_waiters;
{
	struct __db_lock *lp_w, *lp_h, *next_waiter;
	DB_LOCKER *sh_locker;
	DB_LOCKREGION *region;
	u_int32_t locker_ndx;
	int had_waiters, state_changed;

	region = lt->reginfo.primary;
	had_waiters = 0;

	/*
	 * We need to do lock promotion.  We also need to determine if we're
	 * going to need to run the deadlock detector again.  If we release
	 * locks, and there are waiters, but no one gets promoted, then we
	 * haven't fundamentally changed the lockmgr state, so we may still
	 * have a deadlock and we have to run again.  However, if there were
	 * no waiters, or we actually promoted someone, then we are OK and we
	 * don't have to run it immediately.
	 *
	 * During promotion, we look for state changes so we can return this
	 * information to the caller.
	 */

	for (lp_w = SH_TAILQ_FIRST(&obj->waiters, __db_lock),
	    state_changed = lp_w == NULL;
	    lp_w != NULL;
	    lp_w = next_waiter) {
		had_waiters = 1;
		next_waiter = SH_TAILQ_NEXT(lp_w, links, __db_lock);
		/* Are we switching locks? */
		if (not_waiters && lp_w->mode == DB_LOCK_WAIT)
			continue;
		for (lp_h = SH_TAILQ_FIRST(&obj->holders, __db_lock);
		    lp_h != NULL;
		    lp_h = SH_TAILQ_NEXT(lp_h, links, __db_lock)) {
			if (lp_h->holder != lp_w->holder &&
			    CONFLICTS(lt, region, lp_h->mode, lp_w->mode)) {

				LOCKER_LOCK(lt, region, lp_w->holder, locker_ndx);
				if ((__lock_getlocker(lt, lp_w->holder,
				    locker_ndx, 0, &sh_locker)) != 0) {
					DB_ASSERT(0);
					break;
				}
				if (!__lock_is_parent(lt,
				    lp_h->holder, sh_locker))
					break;
			}
		}
		if (lp_h != NULL)	/* Found a conflict. */
			break;

		/* No conflict, promote the waiting lock. */
		SH_TAILQ_REMOVE(&obj->waiters, lp_w, links, __db_lock);
		lp_w->status = DB_LSTAT_PENDING;
		SH_TAILQ_INSERT_TAIL(&obj->holders, lp_w, links);

		/* Wake up waiter. */
		MUTEX_UNLOCK(lt->dbenv, &lp_w->mutex);
		state_changed = 1;
	}

	/*
	 * If this object had waiters and doesn't any more, then we need
	 * to remove it from the dd_obj list.
	 */
	if (had_waiters && SH_TAILQ_FIRST(&obj->waiters, __db_lock) == NULL)
		SH_TAILQ_REMOVE(&region->dd_objs, obj, dd_links, __db_lockobj);
	return (state_changed);
}

/*
 * __lock_remove_waiter --
 *	Any lock on the waitlist has a process waiting for it.  Therefore,
 * we can't return the lock to the freelist immediately.  Instead, we can
 * remove the lock from the list of waiters, set the status field of the
 * lock, and then let the process waking up return the lock to the
 * free list.
 *
 * This must be called with the Object bucket locked.
 */
static void
__lock_remove_waiter(dbenv, sh_obj, lockp, status)
	DB_ENV *dbenv;
	DB_LOCKOBJ *sh_obj;
	struct __db_lock *lockp;
	db_status_t status;
{
	int do_wakeup;

	do_wakeup = lockp->status == DB_LSTAT_WAITING;

	SH_TAILQ_REMOVE(&sh_obj->waiters, lockp, links, __db_lock);
	lockp->links.stqe_prev = -1;
	lockp->status = status;

	/*
	 * Wake whoever is waiting on this lock.
	 *
	 * The MUTEX_UNLOCK macro normally resolves to a single argument,
	 * keep the compiler quiet.
	 */
	if (do_wakeup)
		MUTEX_UNLOCK(dbenv, &lockp->mutex);
}

/*
 * __lock_printlock --
 *
 * PUBLIC: void __lock_printlock __P((DB_LOCKTAB *, struct __db_lock *, int));
 */
void
__lock_printlock(lt, lp, ispgno)
	DB_LOCKTAB *lt;
	struct __db_lock *lp;
	int ispgno;
{
	DB_LOCKOBJ *lockobj;
	db_pgno_t pgno;
	u_int32_t *fidp;
	u_int8_t *ptr, type;
	const char *mode, *status;

	switch (lp->mode) {
	case DB_LOCK_IREAD:
		mode = "IREAD";
		break;
	case DB_LOCK_IWR:
		mode = "IWR";
		break;
	case DB_LOCK_IWRITE:
		mode = "IWRITE";
		break;
	case DB_LOCK_NG:
		mode = "NG";
		break;
	case DB_LOCK_READ:
		mode = "READ";
		break;
	case DB_LOCK_WRITE:
		mode = "WRITE";
		break;
	case DB_LOCK_WAIT:
		mode = "WAIT";
		break;
	default:
		mode = "UNKNOWN";
		break;
	}
	switch (lp->status) {
	case DB_LSTAT_ABORTED:
		status = "ABORT";
		break;
	case DB_LSTAT_ERR:
		status = "ERROR";
		break;
	case DB_LSTAT_FREE:
		status = "FREE";
		break;
	case DB_LSTAT_HELD:
		status = "HELD";
		break;
	case DB_LSTAT_NOGRANT:
		status = "NONE";
		break;
	case DB_LSTAT_WAITING:
		status = "WAIT";
		break;
	case DB_LSTAT_PENDING:
		status = "PENDING";
		break;
	default:
		status = "UNKNOWN";
		break;
	}
	printf("\t%lx\t%s\t%lu\t%s\t",
	    (u_long)lp->holder, mode, (u_long)lp->refcount, status);

	lockobj = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
	ptr = SH_DBT_PTR(&lockobj->lockobj);
	if (ispgno && lockobj->lockobj.size == sizeof(struct __db_ilock)) {
		/* Assume this is a DBT lock. */
		memcpy(&pgno, ptr, sizeof(db_pgno_t));
		fidp = (u_int32_t *)(ptr + sizeof(db_pgno_t));
		type = *(u_int8_t *)(ptr + sizeof(db_pgno_t) + DB_FILE_ID_LEN);
		printf("%s  %lu (%lu %lu %lu %lu %lu)\n",
			type == DB_PAGE_LOCK ? "page" : "record",
			(u_long)pgno,
			(u_long)fidp[0], (u_long)fidp[1], (u_long)fidp[2],
			(u_long)fidp[3], (u_long)fidp[4]);
	} else {
		printf("0x%lx ", (u_long)R_OFFSET(&lt->reginfo, lockobj));
		__db_pr(ptr, lockobj->lockobj.size);
		printf("\n");
	}
}
