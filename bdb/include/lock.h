/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: lock.h,v 11.20 2000/12/12 17:43:56 bostic Exp $
 */

#define	DB_LOCK_DEFAULT_N	1000	/* Default # of locks in region. */

/*
 * Out of band value for a lock.  Locks contain an offset into a lock region,
 * so we use an invalid region offset to indicate an invalid or unset lock.
 */
#define	LOCK_INVALID	INVALID_ROFF

/*
 * The locker id space is divided between the transaction manager and the lock
 * manager.  Lock IDs start at 0 and go to DB_LOCK_MAXID.  Txn IDs start at
 * DB_LOCK_MAXID + 1 and go up to TXN_INVALID.
 */
#define	DB_LOCK_MAXID		0x7fffffff

/*
 * DB_LOCKREGION --
 *	The lock shared region.
 */
typedef struct __db_lockregion {
	u_int32_t	id;		/* unique id generator */
	u_int32_t	need_dd;	/* flag for deadlock detector */
	u_int32_t	detect;		/* run dd on every conflict */
					/* free lock header */
	SH_TAILQ_HEAD(__flock) free_locks;
					/* free obj header */
	SH_TAILQ_HEAD(__fobj) free_objs;
					/* free locker header */
	SH_TAILQ_HEAD(__flocker) free_lockers;
	SH_TAILQ_HEAD(__dobj) dd_objs;	/* objects with waiters */
	u_int32_t	maxlocks;	/* maximum number of locks in table */
	u_int32_t	maxlockers;	/* maximum number of lockers in table */
	u_int32_t	maxobjects;	/* maximum number of objects in table */
	u_int32_t	locker_t_size;	/* size of locker hash table */
	u_int32_t	object_t_size;	/* size of object hash table */
	u_int32_t	nmodes;		/* number of lock modes */
	u_int32_t	nlocks;		/* current number of locks */
	u_int32_t	maxnlocks;	/* maximum number of locks so far*/
	u_int32_t	nlockers;	/* current number of lockers */
	u_int32_t	maxnlockers;	/* maximum number of lockers so far */
	u_int32_t	nobjects;	/* current number of objects */
	u_int32_t	maxnobjects;	/* maximum number of objects so far */
	roff_t		conf_off;	/* offset of conflicts array */
	roff_t		obj_off;	/* offset of object hash table */
	roff_t		osynch_off;	/* offset of the object mutex table */
	roff_t		locker_off;	/* offset of locker hash table */
	roff_t		lsynch_off;	/* offset of the locker mutex table */
	u_int32_t	nconflicts;	/* number of lock conflicts */
	u_int32_t	nrequests;	/* number of lock gets */
	u_int32_t	nreleases;	/* number of lock puts */
	u_int32_t	nnowaits;	/* number of lock requests that would
					   have waited without nowait */
	u_int32_t	ndeadlocks;	/* number of deadlocks */
#ifdef MUTEX_SYSTEM_RESOURCES
	roff_t		maint_off;	/* offset of region maintenance info */
#endif
} DB_LOCKREGION;

/*
 * Since we will store DBTs in shared memory, we need the equivalent of a
 * DBT that will work in shared memory.
 */
typedef struct __sh_dbt {
	u_int32_t size;			/* Byte length. */
	ssize_t   off;			/* Region offset. */
} SH_DBT;

#define	SH_DBT_PTR(p)	((void *)(((u_int8_t *)(p)) + (p)->off))

/*
 * Object structures;  these live in the object hash table.
 */
typedef struct __db_lockobj {
	SH_DBT	lockobj;		/* Identifies object locked. */
	SH_TAILQ_ENTRY links;		/* Links for free list or hash list. */
	SH_TAILQ_ENTRY dd_links;	/* Links for dd list. */
	SH_TAILQ_HEAD(__wait) waiters;	/* List of waiting locks. */
	SH_TAILQ_HEAD(__hold) holders;	/* List of held locks. */
					/* Declare room in the object to hold
					 * typical DB lock structures so that
					 * we do not have to allocate them from
					 * shalloc at run-time. */
	u_int8_t objdata[sizeof(struct __db_ilock)];
} DB_LOCKOBJ;

/*
 * Locker structures; these live in the locker hash table.
 */
typedef struct __db_locker {
	u_int32_t id;			/* Locker id. */
	u_int32_t dd_id;		/* Deadlock detector id. */
	size_t master_locker;		/* Locker of master transaction. */
	size_t parent_locker;		/* Parent of this child. */
	SH_LIST_HEAD(_child) child_locker;	/* List of descendant txns;
						   only used in a "master"
						   txn. */
	SH_LIST_ENTRY child_link;	/* Links transactions in the family;
					   elements of the child_locker
					   list. */
	SH_TAILQ_ENTRY links;		/* Links for free list. */
	SH_LIST_HEAD(_held) heldby;	/* Locks held by this locker. */

#define	DB_LOCKER_DELETED	0x0001
	u_int32_t flags;
} DB_LOCKER;

/*
 * Lockers can be freed if they are not part of a transaction family.
 * Members of a family either point at the master transaction or are
 * the master transaction and have children lockers.
 */
#define	LOCKER_FREEABLE(lp)						\
    ((lp)->master_locker == TXN_INVALID_ID &&				\
    SH_LIST_FIRST(&(lp)->child_locker, __db_locker) == NULL)

/*
 * DB_LOCKTAB --
 *	The primary library lock data structure (i.e., the one referenced
 * by the environment, as opposed to the internal one laid out in the region.)
 */
typedef struct __db_locktab {
	DB_ENV		*dbenv;		/* Environment. */
	REGINFO		 reginfo;	/* Region information. */
	u_int8_t	*conflicts;	/* Pointer to conflict matrix. */
	DB_HASHTAB	*obj_tab;	/* Beginning of object hash table. */
	DB_HASHTAB	*locker_tab;	/* Beginning of locker hash table. */
} DB_LOCKTAB;

/* Test for conflicts. */
#define	CONFLICTS(T, R, HELD, WANTED) \
	(T)->conflicts[(HELD) * (R)->nmodes + (WANTED)]

#define	OBJ_LINKS_VALID(L) ((L)->links.stqe_prev != -1)

struct __db_lock {
	/*
	 * Wait on mutex to wait on lock.  You reference your own mutex with
	 * ID 0 and others reference your mutex with ID 1.
	 */
	MUTEX		mutex;

	u_int32_t	holder;		/* Who holds this lock. */
	u_int32_t	gen;		/* Generation count. */
	SH_TAILQ_ENTRY	links;		/* Free or holder/waiter list. */
	SH_LIST_ENTRY	locker_links;	/* List of locks held by a locker. */
	u_int32_t	refcount;	/* Reference count the lock. */
	db_lockmode_t	mode;		/* What sort of lock. */
	ssize_t		obj;		/* Relative offset of object struct. */
	db_status_t	status;		/* Status of this lock. */
};

/*
 * Flag values for __lock_put_internal:
 * DB_LOCK_DOALL:     Unlock all references in this lock (instead of only 1).
 * DB_LOCK_FREE:      Free the lock (used in checklocker).
 * DB_LOCK_IGNOREDEL: Remove from the locker hash table even if already
		      deleted (used in checklocker).
 * DB_LOCK_NOPROMOTE: Don't bother running promotion when releasing locks
 *		      (used by __lock_put_internal).
 * DB_LOCK_UNLINK:    Remove from the locker links (used in checklocker).
 */
#define	DB_LOCK_DOALL		0x001
#define	DB_LOCK_FREE		0x002
#define	DB_LOCK_IGNOREDEL	0x004
#define	DB_LOCK_NOPROMOTE	0x008
#define	DB_LOCK_UNLINK		0x010
#define	DB_LOCK_NOWAITERS	0x020

/*
 * Macros to get/release different types of mutexes.
 */
#define	OBJECT_LOCK(lt, reg, obj, ndx)					\
	ndx = __lock_ohash(obj) % (reg)->object_t_size
#define	SHOBJECT_LOCK(lt, reg, shobj, ndx)				\
	ndx = __lock_lhash(shobj) % (reg)->object_t_size
#define	LOCKER_LOCK(lt, reg, locker, ndx)				\
	ndx = __lock_locker_hash(locker) % (reg)->locker_t_size;

#define	LOCKREGION(dbenv, lt)  R_LOCK((dbenv), &(lt)->reginfo)
#define	UNLOCKREGION(dbenv, lt)  R_UNLOCK((dbenv), &(lt)->reginfo)
#include "lock_ext.h"
