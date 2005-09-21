/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: lock.h,v 11.53 2004/09/22 21:14:56 ubell Exp $
 */

#ifndef	_DB_LOCK_H_
#define	_DB_LOCK_H_

#define	DB_LOCK_DEFAULT_N	1000	/* Default # of locks in region. */

/*
 * The locker id space is divided between the transaction manager and the lock
 * manager.  Lock IDs start at 1 and go to DB_LOCK_MAXID.  Txn IDs start at
 * DB_LOCK_MAXID + 1 and go up to TXN_MAXIMUM.
 */
#define	DB_LOCK_INVALIDID	0
#define	DB_LOCK_MAXID		0x7fffffff

/*
 * Out of band value for a lock.  Locks contain an offset into a lock region,
 * so we use an invalid region offset to indicate an invalid or unset lock.
 */
#define	LOCK_INVALID		INVALID_ROFF
#define	LOCK_ISSET(lock)	((lock).off != LOCK_INVALID)
#define	LOCK_INIT(lock)		((lock).off = LOCK_INVALID)

/*
 * Macro to identify a write lock for the purpose of counting locks
 * for the NUMWRITES option to deadlock detection.
 */
#define	IS_WRITELOCK(m) \
	((m) == DB_LOCK_WRITE || (m) == DB_LOCK_WWRITE || \
	    (m) == DB_LOCK_IWRITE || (m) == DB_LOCK_IWR)

/*
 * Lock timers.
 */
typedef struct {
	u_int32_t	tv_sec;		/* Seconds. */
	u_int32_t	tv_usec;	/* Microseconds. */
} db_timeval_t;

#define	LOCK_TIME_ISVALID(time)		((time)->tv_sec != 0)
#define	LOCK_SET_TIME_INVALID(time)	((time)->tv_sec = 0)
#define	LOCK_TIME_ISMAX(time)		((time)->tv_sec == UINT32_MAX)
#define	LOCK_SET_TIME_MAX(time)		((time)->tv_sec = UINT32_MAX)
#define	LOCK_TIME_EQUAL(t1, t2)						\
	((t1)->tv_sec == (t2)->tv_sec && (t1)->tv_usec == (t2)->tv_usec)
#define	LOCK_TIME_GREATER(t1, t2)					\
	((t1)->tv_sec > (t2)->tv_sec ||					\
	((t1)->tv_sec == (t2)->tv_sec && (t1)->tv_usec > (t2)->tv_usec))

/*
 * DB_LOCKREGION --
 *	The lock shared region.
 */
typedef struct __db_lockregion {
	u_int32_t	need_dd;	/* flag for deadlock detector */
	u_int32_t	detect;		/* run dd on every conflict */
	db_timeval_t	next_timeout;	/* next time to expire a lock */
					/* free lock header */
	SH_TAILQ_HEAD(__flock) free_locks;
					/* free obj header */
	SH_TAILQ_HEAD(__fobj) free_objs;
					/* free locker header */
	SH_TAILQ_HEAD(__flocker) free_lockers;
	SH_TAILQ_HEAD(__dobj) dd_objs;	/* objects with waiters */
	SH_TAILQ_HEAD(__lkrs) lockers;	/* list of lockers */

	db_timeout_t	lk_timeout;	/* timeout for locks. */
	db_timeout_t	tx_timeout;	/* timeout for txns. */

	u_int32_t	locker_t_size;	/* size of locker hash table */
	u_int32_t	object_t_size;	/* size of object hash table */

	roff_t		conf_off;	/* offset of conflicts array */
	roff_t		obj_off;	/* offset of object hash table */
	roff_t		osynch_off;	/* offset of the object mutex table */
	roff_t		locker_off;	/* offset of locker hash table */
	roff_t		lsynch_off;	/* offset of the locker mutex table */

	DB_LOCK_STAT	stat;		/* stats about locking. */

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	roff_t		maint_off;	/* offset of region maintenance info */
#endif
} DB_LOCKREGION;

/*
 * Since we will store DBTs in shared memory, we need the equivalent of a
 * DBT that will work in shared memory.
 */
typedef struct __sh_dbt {
	u_int32_t size;			/* Byte length. */
	roff_t    off;			/* Region offset. */
} SH_DBT;

#define	SH_DBT_PTR(p)	((void *)(((u_int8_t *)(p)) + (p)->off))

/*
 * Object structures;  these live in the object hash table.
 */
typedef struct __db_lockobj {
	SH_DBT	lockobj;		/* Identifies object locked. */
	SH_TAILQ_ENTRY links;		/* Links for free list or hash list. */
	SH_TAILQ_ENTRY dd_links;	/* Links for dd list. */
	SH_TAILQ_HEAD(__waitl) waiters;	/* List of waiting locks. */
	SH_TAILQ_HEAD(__holdl) holders;	/* List of held locks. */
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
	u_int32_t nlocks;		/* Number of locks held. */
	u_int32_t nwrites;		/* Number of write locks held. */
	roff_t  master_locker;		/* Locker of master transaction. */
	roff_t  parent_locker;		/* Parent of this child. */
	SH_LIST_HEAD(_child) child_locker;	/* List of descendant txns;
						   only used in a "master"
						   txn. */
	SH_LIST_ENTRY child_link;	/* Links transactions in the family;
					   elements of the child_locker
					   list. */
	SH_TAILQ_ENTRY links;		/* Links for free and hash list. */
	SH_TAILQ_ENTRY ulinks;		/* Links in-use list. */
	SH_LIST_HEAD(_held) heldby;	/* Locks held by this locker. */
	db_timeval_t	lk_expire;	/* When current lock expires. */
	db_timeval_t	tx_expire;	/* When this txn expires. */
	db_timeout_t	lk_timeout;	/* How long do we let locks live. */

#define	DB_LOCKER_DELETED	0x0001
#define	DB_LOCKER_DIRTY		0x0002
#define	DB_LOCKER_INABORT	0x0004
#define	DB_LOCKER_TIMEOUT	0x0008
	u_int32_t flags;
} DB_LOCKER;

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

/*
 * Test for conflicts.
 *
 * Cast HELD and WANTED to ints, they are usually db_lockmode_t enums.
 */
#define	CONFLICTS(T, R, HELD, WANTED) \
	(T)->conflicts[((int)HELD) * (R)->stat.st_nmodes + ((int)WANTED)]

#define	OBJ_LINKS_VALID(L) ((L)->links.stqe_prev != -1)

struct __db_lock {
	/*
	 * Wait on mutex to wait on lock.  You reference your own mutex with
	 * ID 0 and others reference your mutex with ID 1.
	 */
	DB_MUTEX	mutex;

	u_int32_t	holder;		/* Who holds this lock. */
	u_int32_t	gen;		/* Generation count. */
	SH_TAILQ_ENTRY	links;		/* Free or holder/waiter list. */
	SH_LIST_ENTRY	locker_links;	/* List of locks held by a locker. */
	u_int32_t	refcount;	/* Reference count the lock. */
	db_lockmode_t	mode;		/* What sort of lock. */
	roff_t		obj;		/* Relative offset of object struct. */
	db_status_t	status;		/* Status of this lock. */
};

/*
 * Flag values for __lock_put_internal:
 * DB_LOCK_DOALL:     Unlock all references in this lock (instead of only 1).
 * DB_LOCK_FREE:      Free the lock (used in checklocker).
 * DB_LOCK_NOPROMOTE: Don't bother running promotion when releasing locks
 *		      (used by __lock_put_internal).
 * DB_LOCK_UNLINK:    Remove from the locker links (used in checklocker).
 * Make sure that these do not conflict with the interface flags because
 * we pass some of those around (i.e., DB_LOCK_REMOVE).
 */
#define	DB_LOCK_DOALL		0x010000
#define	DB_LOCK_DOWNGRADE	0x020000
#define	DB_LOCK_FREE		0x040000
#define	DB_LOCK_NOPROMOTE	0x080000
#define	DB_LOCK_UNLINK		0x100000
#define	DB_LOCK_NOREGION	0x200000
#define	DB_LOCK_NOWAITERS	0x400000

/*
 * Macros to get/release different types of mutexes.
 */
#define	OBJECT_LOCK(lt, reg, obj, ndx)					\
	ndx = __lock_ohash(obj) % (reg)->object_t_size
#define	SHOBJECT_LOCK(lt, reg, shobj, ndx)				\
	ndx = __lock_lhash(shobj) % (reg)->object_t_size
#define	LOCKER_LOCK(lt, reg, locker, ndx)				\
	ndx = __lock_locker_hash(locker) % (reg)->locker_t_size;

#define	LOCKREGION(dbenv, lt)  R_LOCK((dbenv), &((DB_LOCKTAB *)lt)->reginfo)
#define	UNLOCKREGION(dbenv, lt)  R_UNLOCK((dbenv), &((DB_LOCKTAB *)lt)->reginfo)

#include "dbinc_auto/lock_ext.h"
#endif /* !_DB_LOCK_H_ */
