/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: lock_deadlock.c,v 11.23 2000/12/08 20:15:31 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#ifdef	HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"
#include "txn.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
#endif

#define	ISSET_MAP(M, N)	((M)[(N) / 32] & (1 << (N) % 32))

#define	CLEAR_MAP(M, N) {						\
	u_int32_t __i;							\
	for (__i = 0; __i < (N); __i++)					\
		(M)[__i] = 0;						\
}

#define	SET_MAP(M, B)	((M)[(B) / 32] |= (1 << ((B) % 32)))
#define	CLR_MAP(M, B)	((M)[(B) / 32] &= ~(1 << ((B) % 32)))

#define	OR_MAP(D, S, N)	{						\
	u_int32_t __i;							\
	for (__i = 0; __i < (N); __i++)					\
		D[__i] |= S[__i];					\
}
#define	BAD_KILLID	0xffffffff

typedef struct {
	int		valid;
	u_int32_t	id;
	u_int32_t	last_lock;
	u_int32_t	last_locker_id;
	db_pgno_t	pgno;
} locker_info;

static int  __dd_abort __P((DB_ENV *, locker_info *));
static int  __dd_build
	__P((DB_ENV *, u_int32_t **, u_int32_t *, locker_info **));
static int  __dd_find
	__P((DB_ENV *,u_int32_t *, locker_info *, u_int32_t, u_int32_t ***));

#ifdef DIAGNOSTIC
static void __dd_debug __P((DB_ENV *, locker_info *, u_int32_t *, u_int32_t));
#endif

int
lock_detect(dbenv, flags, atype, abortp)
	DB_ENV *dbenv;
	u_int32_t flags, atype;
	int *abortp;
{
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	locker_info *idmap;
	u_int32_t *bitmap, **deadp, **free_me, i, killid, nentries, nlockers;
	int do_pass, ret;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT))
		return (__dbcl_lock_detect(dbenv, flags, atype, abortp));
#endif

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv, dbenv->lk_handle, DB_INIT_LOCK);

	lt = dbenv->lk_handle;
	if (abortp != NULL)
		*abortp = 0;

	/* Validate arguments. */
	if ((ret =
	    __db_fchk(dbenv, "lock_detect", flags, DB_LOCK_CONFLICT)) != 0)
		return (ret);

	/* Check if a detector run is necessary. */
	LOCKREGION(dbenv, lt);
	if (LF_ISSET(DB_LOCK_CONFLICT)) {
		/* Make a pass every time a lock waits. */
		region = lt->reginfo.primary;
		do_pass = region->need_dd != 0;

		if (!do_pass) {
			UNLOCKREGION(dbenv, lt);
			return (0);
		}
	}

	/* Build the waits-for bitmap. */
	ret = __dd_build(dbenv, &bitmap, &nlockers, &idmap);
	UNLOCKREGION(dbenv, lt);
	if (ret != 0)
		return (ret);

	if (nlockers == 0)
		return (0);
#ifdef DIAGNOSTIC
	if (FLD_ISSET(dbenv->verbose, DB_VERB_WAITSFOR))
		__dd_debug(dbenv, idmap, bitmap, nlockers);
#endif
	/* Find a deadlock. */
	if ((ret = __dd_find(dbenv, bitmap, idmap, nlockers, &deadp)) != 0)
		return (ret);

	nentries = ALIGN(nlockers, 32) / 32;
	killid = BAD_KILLID;
	free_me = deadp;
	for (; *deadp != NULL; deadp++) {
		if (abortp != NULL)
			++*abortp;
		switch (atype) {			/* Kill someone. */
		case DB_LOCK_OLDEST:
			/*
			 * Find the first bit set in the current
			 * array and then look for a lower tid in
			 * the array.
			 */
			for (i = 0; i < nlockers; i++)
				if (ISSET_MAP(*deadp, i)) {
					killid = i;
					break;

				}
			/*
			 * It's conceivable that under XA, the locker could
			 * have gone away.
			 */
			if (killid == BAD_KILLID)
				break;

			/*
			 * The oldest transaction has the lowest
			 * transaction id.
			 */
			for (i = killid + 1; i < nlockers; i++)
				if (ISSET_MAP(*deadp, i) &&
				    idmap[i].id < idmap[killid].id)
					killid = i;
			break;
		case DB_LOCK_DEFAULT:
		case DB_LOCK_RANDOM:
			/*
			 * We are trying to calculate the id of the
			 * locker whose entry is indicated by deadlock.
			 */
			killid = (*deadp - bitmap) / nentries;
			break;
		case DB_LOCK_YOUNGEST:
			/*
			 * Find the first bit set in the current
			 * array and then look for a lower tid in
			 * the array.
			 */
			for (i = 0; i < nlockers; i++)
				if (ISSET_MAP(*deadp, i)) {
					killid = i;
					break;
				}

			/*
			 * It's conceivable that under XA, the locker could
			 * have gone away.
			 */
			if (killid == BAD_KILLID)
				break;

			/*
			 * The youngest transaction has the highest
			 * transaction id.
			 */
			for (i = killid + 1; i < nlockers; i++)
				if (ISSET_MAP(*deadp, i) &&
				    idmap[i].id > idmap[killid].id)
					killid = i;
			break;
		default:
			killid = BAD_KILLID;
			ret = EINVAL;
		}

		if (killid == BAD_KILLID)
			continue;

		/* Kill the locker with lockid idmap[killid]. */
		if ((ret = __dd_abort(dbenv, &idmap[killid])) != 0) {
			/*
			 * It's possible that the lock was already aborted;
			 * this isn't necessarily a problem, so do not treat
			 * it as an error.
			 */
			if (ret == DB_ALREADY_ABORTED)
				ret = 0;
			else
				__db_err(dbenv,
				    "warning: unable to abort locker %lx",
				    (u_long)idmap[killid].id);
		} else if (FLD_ISSET(dbenv->verbose, DB_VERB_DEADLOCK))
			__db_err(dbenv,
			    "Aborting locker %lx", (u_long)idmap[killid].id);
	}
	__os_free(free_me, 0);
	__os_free(bitmap, 0);
	__os_free(idmap, 0);

	return (ret);
}

/*
 * ========================================================================
 * Utilities
 */

# define DD_INVALID_ID	((u_int32_t) -1)

static int
__dd_build(dbenv, bmp, nlockers, idmap)
	DB_ENV *dbenv;
	u_int32_t **bmp, *nlockers;
	locker_info **idmap;
{
	struct __db_lock *lp;
	DB_LOCKER *lip, *lockerp, *child;
	DB_LOCKOBJ *op, *lo;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	locker_info *id_array;
	u_int32_t *bitmap, count, dd, *entryp, i, id, ndx, nentries, *tmpmap;
	u_int8_t *pptr;
	int is_first, ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	/*
	 * We'll check how many lockers there are, add a few more in for
	 * good measure and then allocate all the structures.  Then we'll
	 * verify that we have enough room when we go back in and get the
	 * mutex the second time.
	 */
retry:	count = region->nlockers;
	region->need_dd = 0;

	if (count == 0) {
		*nlockers = 0;
		return (0);
	}

	if (FLD_ISSET(dbenv->verbose, DB_VERB_DEADLOCK))
		__db_err(dbenv, "%lu lockers", (u_long)count);

	count += 40;
	nentries = ALIGN(count, 32) / 32;

	/*
	 * Allocate enough space for a count by count bitmap matrix.
	 *
	 * XXX
	 * We can probably save the malloc's between iterations just
	 * reallocing if necessary because count grew by too much.
	 */
	if ((ret = __os_calloc(dbenv, (size_t)count,
	    sizeof(u_int32_t) * nentries, &bitmap)) != 0)
		return (ret);

	if ((ret = __os_calloc(dbenv,
	    sizeof(u_int32_t), nentries, &tmpmap)) != 0) {
		__os_free(bitmap, sizeof(u_int32_t) * nentries);
		return (ret);
	}

	if ((ret = __os_calloc(dbenv,
	    (size_t)count, sizeof(locker_info), &id_array)) != 0) {
		__os_free(bitmap, count * sizeof(u_int32_t) * nentries);
		__os_free(tmpmap, sizeof(u_int32_t) * nentries);
		return (ret);
	}

	/*
	 * Now go back in and actually fill in the matrix.
	 */
	if (region->nlockers > count) {
		__os_free(bitmap, count * sizeof(u_int32_t) * nentries);
		__os_free(tmpmap, sizeof(u_int32_t) * nentries);
		__os_free(id_array, count * sizeof(locker_info));
		goto retry;
	}

	/*
	 * First we go through and assign each locker a deadlock detector id.
	 */
	for (id = 0, i = 0; i < region->locker_t_size; i++) {
		for (lip = SH_TAILQ_FIRST(&lt->locker_tab[i], __db_locker);
		    lip != NULL; lip = SH_TAILQ_NEXT(lip, links, __db_locker))
			if (lip->master_locker == INVALID_ROFF) {
				lip->dd_id = id++;
				id_array[lip->dd_id].id = lip->id;
			} else
				lip->dd_id = DD_INVALID_ID;
	}

	/*
	 * We only need consider objects that have waiters, so we use
	 * the list of objects with waiters (dd_objs) instead of traversing
	 * the entire hash table.  For each object, we traverse the waiters
	 * list and add an entry in the waitsfor matrix for each waiter/holder
	 * combination.
	 */
	for (op = SH_TAILQ_FIRST(&region->dd_objs, __db_lockobj);
	    op != NULL; op = SH_TAILQ_NEXT(op, dd_links, __db_lockobj)) {
		CLEAR_MAP(tmpmap, nentries);

		/*
		 * First we go through and create a bit map that
		 * represents all the holders of this object.
		 */
		for (lp = SH_TAILQ_FIRST(&op->holders, __db_lock);
		    lp != NULL;
		    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
			LOCKER_LOCK(lt, region, lp->holder, ndx);
			if ((ret = __lock_getlocker(lt,
			    lp->holder, ndx, 0, &lockerp)) != 0)
				continue;
			if (lockerp->dd_id == DD_INVALID_ID)
				dd = ((DB_LOCKER *)
				     R_ADDR(&lt->reginfo,
				     lockerp->master_locker))->dd_id;
			else
				dd = lockerp->dd_id;
			id_array[dd].valid = 1;

			/*
			 * If the holder has already been aborted, then
			 * we should ignore it for now.
			 */
			if (lp->status == DB_LSTAT_HELD)
				SET_MAP(tmpmap, dd);
		}

		/*
		 * Next, for each waiter, we set its row in the matrix
		 * equal to the map of holders we set up above.
		 */
		for (is_first = 1,
		    lp = SH_TAILQ_FIRST(&op->waiters, __db_lock);
		    lp != NULL;
		    is_first = 0,
		    lp = SH_TAILQ_NEXT(lp, links, __db_lock)) {
			LOCKER_LOCK(lt, region, lp->holder, ndx);
			if ((ret = __lock_getlocker(lt,
			    lp->holder, ndx, 0, &lockerp)) != 0)
				continue;
			if (lockerp->dd_id == DD_INVALID_ID)
				dd = ((DB_LOCKER *)
				     R_ADDR(&lt->reginfo,
				     lockerp->master_locker))->dd_id;
			else
				dd = lockerp->dd_id;
			id_array[dd].valid = 1;

			/*
			 * If the transaction is pending abortion, then
			 * ignore it on this iteration.
			 */
			if (lp->status != DB_LSTAT_WAITING)
				continue;

			entryp = bitmap + (nentries * dd);
			OR_MAP(entryp, tmpmap, nentries);
			/*
			 * If this is the first waiter on the queue,
			 * then we remove the waitsfor relationship
			 * with oneself.  However, if it's anywhere
			 * else on the queue, then we have to keep
			 * it and we have an automatic deadlock.
			 */
			if (is_first)
				CLR_MAP(entryp, dd);
		}
	}

	/* Now for each locker; record its last lock. */
	for (id = 0; id < count; id++) {
		if (!id_array[id].valid)
			continue;
		LOCKER_LOCK(lt, region, id_array[id].id, ndx);
		if ((ret = __lock_getlocker(lt,
		    id_array[id].id, ndx, 0, &lockerp)) != 0) {
			__db_err(dbenv,
			    "No locks for locker %lu", (u_long)id_array[id].id);
			continue;
		}

		/*
		 * If this is a master transaction, try to
		 * find one of its children's locks first,
		 * as they are probably more recent.
		 */
		child = SH_LIST_FIRST(&lockerp->child_locker, __db_locker);
		if (child != NULL) {
			do {
				lp = SH_LIST_FIRST(&child->heldby, __db_lock);
				if (lp != NULL &&
				     lp->status == DB_LSTAT_WAITING) {
					id_array[id].last_locker_id = child->id;
					goto get_lock;
				}
				child = SH_LIST_NEXT(
				    child, child_link, __db_locker);
			} while (child != NULL);
		}
		lp = SH_LIST_FIRST(&lockerp->heldby, __db_lock);
		if (lp != NULL) {
			id_array[id].last_locker_id = lockerp->id;
	get_lock:	id_array[id].last_lock = R_OFFSET(&lt->reginfo, lp);
			lo = (DB_LOCKOBJ *)((u_int8_t *)lp + lp->obj);
			pptr = SH_DBT_PTR(&lo->lockobj);
			if (lo->lockobj.size >= sizeof(db_pgno_t))
				memcpy(&id_array[id].pgno,
				    pptr, sizeof(db_pgno_t));
			else
				id_array[id].pgno = 0;
		}
	}

	/* Pass complete, reset the deadlock detector bit. */
	region->need_dd = 0;

	/*
	 * Now we can release everything except the bitmap matrix that we
	 * created.
	 */
	*nlockers = id;
	*idmap = id_array;
	*bmp = bitmap;
	__os_free(tmpmap, sizeof(u_int32_t) * nentries);
	return (0);
}

static int
__dd_find(dbenv, bmp, idmap, nlockers, deadp)
	DB_ENV *dbenv;
	u_int32_t *bmp, nlockers;
	locker_info *idmap;
	u_int32_t ***deadp;
{
	u_int32_t i, j, k, nentries, *mymap, *tmpmap;
	u_int32_t **retp;
	int ndead, ndeadalloc, ret;

#undef	INITIAL_DEAD_ALLOC
#define	INITIAL_DEAD_ALLOC	8

	ndeadalloc = INITIAL_DEAD_ALLOC;
	ndead = 0;
	if ((ret = __os_malloc(dbenv,
	    ndeadalloc * sizeof(u_int32_t *), NULL, &retp)) != 0)
		return (ret);

	/*
	 * For each locker, OR in the bits from the lockers on which that
	 * locker is waiting.
	 */
	nentries = ALIGN(nlockers, 32) / 32;
	for (mymap = bmp, i = 0; i < nlockers; i++, mymap += nentries) {
		if (!idmap[i].valid)
			continue;
		for (j = 0; j < nlockers; j++) {
			if (!ISSET_MAP(mymap, j))
				continue;

			/* Find the map for this bit. */
			tmpmap = bmp + (nentries * j);
			OR_MAP(mymap, tmpmap, nentries);
			if (!ISSET_MAP(mymap, i))
				continue;

			/* Make sure we leave room for NULL. */
			if (ndead + 2 >= ndeadalloc) {
				ndeadalloc <<= 1;
				/*
				 * If the alloc fails, then simply return the
				 * deadlocks that we already have.
				 */
				if (__os_realloc(dbenv,
				    ndeadalloc * sizeof(u_int32_t),
				    NULL, &retp) != 0) {
					retp[ndead] = NULL;
					*deadp = retp;
					return (0);
				}
			}
			retp[ndead++] = mymap;

			/* Mark all participants in this deadlock invalid. */
			for (k = 0; k < nlockers; k++)
				if (ISSET_MAP(mymap, k))
					idmap[k].valid = 0;
			break;
		}
	}
	retp[ndead] = NULL;
	*deadp = retp;
	return (0);
}

static int
__dd_abort(dbenv, info)
	DB_ENV *dbenv;
	locker_info *info;
{
	struct __db_lock *lockp;
	DB_LOCKER *lockerp;
	DB_LOCKOBJ *sh_obj;
	DB_LOCKREGION *region;
	DB_LOCKTAB *lt;
	u_int32_t ndx;
	int ret;

	lt = dbenv->lk_handle;
	region = lt->reginfo.primary;

	LOCKREGION(dbenv, lt);
	/* Find the locker's last lock. */
	LOCKER_LOCK(lt, region, info->last_locker_id, ndx);
	if ((ret = __lock_getlocker(lt,
	    info->last_locker_id, ndx, 0, &lockerp)) != 0 || lockerp == NULL) {
		if (ret == 0)
			ret = DB_ALREADY_ABORTED;
		goto out;
	}

	lockp = SH_LIST_FIRST(&lockerp->heldby, __db_lock);

	/*
	 * It's possible that this locker was already aborted.  If that's
	 * the case, make sure that we remove its locker from the hash table.
	 */
	if (lockp == NULL) {
		if (LOCKER_FREEABLE(lockerp)) {
			__lock_freelocker(lt, region, lockerp, ndx);
			goto out;
		}
	} else if (R_OFFSET(&lt->reginfo, lockp) != info->last_lock ||
	    lockp->status != DB_LSTAT_WAITING) {
		ret = DB_ALREADY_ABORTED;
		goto out;
	}

	sh_obj = (DB_LOCKOBJ *)((u_int8_t *)lockp + lockp->obj);
	SH_LIST_REMOVE(lockp, locker_links, __db_lock);

	/* Abort lock, take it off list, and wake up this lock. */
	SHOBJECT_LOCK(lt, region, sh_obj, ndx);
	lockp->status = DB_LSTAT_ABORTED;
	SH_TAILQ_REMOVE(&sh_obj->waiters, lockp, links, __db_lock);

	/*
	 * Either the waiters list is now empty, in which case we remove
	 * it from dd_objs, or it is not empty, in which case we need to
	 * do promotion.
	 */
	if (SH_TAILQ_FIRST(&sh_obj->waiters, __db_lock) == NULL)
		SH_TAILQ_REMOVE(&region->dd_objs,
		    sh_obj, dd_links, __db_lockobj);
	else
		ret = __lock_promote(lt, sh_obj, 0);
	MUTEX_UNLOCK(dbenv, &lockp->mutex);

	region->ndeadlocks++;
	UNLOCKREGION(dbenv, lt);

	return (0);

out:	UNLOCKREGION(dbenv, lt);
	return (ret);
}

#ifdef DIAGNOSTIC
static void
__dd_debug(dbenv, idmap, bitmap, nlockers)
	DB_ENV *dbenv;
	locker_info *idmap;
	u_int32_t *bitmap, nlockers;
{
	u_int32_t i, j, *mymap, nentries;
	int ret;
	char *msgbuf;

	__db_err(dbenv, "Waitsfor array\nWaiter:\tWaiting on:");

	/* Allocate space to print 10 bytes per item waited on. */
#undef	MSGBUF_LEN
#define	MSGBUF_LEN ((nlockers + 1) * 10 + 64)
	if ((ret = __os_malloc(dbenv, MSGBUF_LEN, NULL, &msgbuf)) != 0)
		return;

	nentries = ALIGN(nlockers, 32) / 32;
	for (mymap = bitmap, i = 0; i < nlockers; i++, mymap += nentries) {
		if (!idmap[i].valid)
			continue;
		sprintf(msgbuf,					/* Waiter. */
		    "%lx/%lu:\t", (u_long)idmap[i].id, (u_long)idmap[i].pgno);
		for (j = 0; j < nlockers; j++)
			if (ISSET_MAP(mymap, j))
				sprintf(msgbuf, "%s %lx", msgbuf,
				    (u_long)idmap[j].id);
		(void)sprintf(msgbuf,
		    "%s %lu", msgbuf, (u_long)idmap[i].last_lock);
		__db_err(dbenv, msgbuf);
	}

	__os_free(msgbuf, MSGBUF_LEN);
}
#endif
