/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: region.h,v 11.13 2000/11/15 19:25:37 sue Exp $
 */

/*
 * The DB environment consists of some number of "regions", which are described
 * by the following four structures:
 *
 *	REGENV	   -- shared information about the environment
 *	REGENV_REF -- file describing system memory version of REGENV
 *	REGION	   -- shared information about a single region
 *	REGINFO	   -- per-process information about a REGION
 *
 * There are three types of memory that hold regions:
 *	per-process heap (malloc)
 *	file mapped into memory (mmap, MapViewOfFile)
 *	system memory (shmget, CreateFileMapping)
 *
 * If the regions are private to a process, they're in malloc.  If they're
 * public, they're in file mapped memory, or, optionally, in system memory.
 * Regions in the filesystem are named "__db.001", "__db.002" and so on.  If
 * we're not using a private environment allocated using malloc(3), the file
 * "__db.001" will always exist, as we use it to synchronize on the regions,
 * whether they exist in file mapped memory or system memory.
 *
 * The file "__db.001" contains a REGENV structure and a linked list of some
 * number of REGION structures.  Each of the REGION structures describes and
 * locks one of the underlying shared regions used by DB.
 *
 *	__db.001
 *	+---------+
 *	|REGENV  |
 *	+---------+   +----------+
 *	|REGION   |-> | __db.002 |
 *	|	  |   +----------+
 *	+---------+   +----------+
 *	|REGION   |-> | __db.003 |
 *	|	  |   +----------+
 *	+---------+   +----------+
 *	|REGION   |-> | __db.004 |
 *	|	  |   +----------+
 *	+---------+
 *
 * The only tricky part about manipulating the regions is correctly creating
 * or joining the REGENV file, i.e., __db.001.  We have to be absolutely sure
 * that only one process creates it, and that everyone else joins it without
 * seeing inconsistent data.  Once that region is created, we can use normal
 * shared locking procedures to do mutal exclusion for all other regions.
 *
 * One of the REGION structures in the main environment region describes the
 * environment region itself.
 *
 * To lock a region, locate the REGION structure that describes it and acquire
 * the region's mutex.  There is one exception to this rule -- the lock for the
 * environment region itself is in the REGENV structure, and not in the REGION
 * that describes the environment region.  That's so that we can acquire a lock
 * without walking linked lists that could potentially change underneath us.
 * The REGION will not be moved or removed during the life of the region, and
 * so long-lived references to it can be held by the process.
 *
 * All requests to create or join a region return a REGINFO structure, which
 * is held by the caller and used to open and subsequently close the reference
 * to the region.  The REGINFO structure contains the per-process information
 * that we need to access the region.
 *
 * The one remaining complication.  If the regions (including the environment
 * region) live in system memory, and the system memory isn't "named" somehow
 * in the filesystem name space, we need some way of finding it.  Do this by
 * by writing the REGENV_REF structure into the "__db.001" file.  When we find
 * a __db.001 file that is too small to be a real, on-disk environment, we use
 * the information it contains to redirect to the real "__db.001" file/memory.
 * This currently only happens when the REGENV file is in shared system memory.
 *
 * Although DB does not currently grow regions when they run out of memory, it
 * would be possible to do so.  To grow a region, allocate a new region of the
 * appropriate size, then copy the old region over it and insert the additional
 * space into the already existing shalloc arena.  Callers may have to fix up
 * local references, but that should be easy to do.  This failed in historic
 * versions of DB because the region lock lived in the mapped memory, and when
 * it was unmapped and remapped (or copied), threads could lose track of it.
 * Once we moved that lock into a region that is never unmapped, growing should
 * work.  That all said, current versions of DB don't implement region grow
 * because some systems don't support mutex copying, e.g., from OSF1 V4.0:
 *
 *      The address of an msemaphore structure may be significant.  If the
 *	msemaphore structure contains any value copied from an msemaphore
 *	structure at a different address, the result is undefined.
 */

#if defined(__cplusplus)
extern "C" {
#endif

#define	DB_REGION_FMT	"__db.%03d"	/* Region file name format. */
#define	DB_REGION_NAME_NUM	5	/* First digit offset in file names. */
#define	DB_REGION_NAME_LENGTH	8	/* Length of file names. */

#define	DB_REGION_ENV	"__db.001"	/* Primary environment name. */

#define	INVALID_REGION_ID	0	/* Out-of-band region ID. */
#define	REGION_ID_ENV		1	/* Primary environment ID. */

typedef enum {
	INVALID_REGION_TYPE=0,		/* Region type. */
	REGION_TYPE_ENV,
	REGION_TYPE_LOCK,
	REGION_TYPE_LOG,
	REGION_TYPE_MPOOL,
	REGION_TYPE_MUTEX,
	REGION_TYPE_TXN } reg_type;

#define	INVALID_REGION_SEGID	-1	/* Segment IDs are either shmget(2) or
					 * Win16 segment identifiers.  They are
					 * both stored in a "long", and we need
					 * an out-of-band value.
					 */
/*
 * Nothing can live at region offset 0, because, in all cases, that's where
 * we store *something*.  Lots of code needs an out-of-band value for region
 * offsets, so we use 0.
 */
#define	INVALID_ROFF		0

/* Reference describing system memory version of REGENV. */
typedef struct __db_reg_env_ref {
	roff_t	   size;		/* Region size. */
	long	   segid;		/* UNIX shmget(2) ID. */
} REGENV_REF;

/* Per-environment region information. */
typedef struct __db_reg_env {
	/*
	 * !!!
	 * The mutex must be the first entry in the structure to guarantee
	 * correct alignment.
	 */
	MUTEX      mutex;		/* Environment mutex. */

	/*
	 * !!!
	 * Note, the magic and panic fields are NOT protected by any mutex,
	 * and for this reason cannot be anything more complicated than a
	 * zero/non-zero value.
	 *
	 * !!!
	 * The valid region magic number must appear at the same byte offset
	 * in both the environment and each shared region, as Windows/95 uses
	 * it to determine if the memory has been zeroed since it was last used.
	 */
	u_int32_t  magic;		/* Valid region magic number. */

	int	   panic;		/* Environment is dead. */

	int	   majver;		/* Major DB version number. */
	int	   minver;		/* Minor DB version number. */
	int	   patch;		/* Patch DB version number. */

	u_int32_t  init_flags;		/* Flags the env was initialized with.*/

					/* List of regions. */
	SH_LIST_HEAD(__db_regionh) regionq;

	u_int32_t  refcnt;		/* References to the environment. */

	size_t	   pad;			/* Guarantee that following memory is
					 * size_t aligned.  This is necessary
					 * because we're going to store the
					 * allocation region information there.
					 */
} REGENV;

/* Per-region shared region information. */
typedef struct __db_region {
	/*
	 * !!!
	 * The mutex must be the first entry in the structure to guarantee
	 * correct alignment.
	 */
	MUTEX	   mutex;		/* Region mutex. */

	/*
	 * !!!
	 * The valid region magic number must appear at the same byte offset
	 * in both the environment and each shared region, as Windows/95 uses
	 * it to determine if the memory has been zeroed since it was last used.
	 */
	u_int32_t  magic;

	SH_LIST_ENTRY q;		/* Linked list of REGIONs. */

	reg_type   type;		/* Region type. */
	u_int32_t  id;			/* Region id. */

	roff_t	   size;		/* Region size in bytes. */

	roff_t	   primary;		/* Primary data structure offset. */

	long	   segid;		/* UNIX shmget(2), Win16 segment ID. */
} REGION;

/*
 * Per-process/per-attachment information about a single region.
 */
struct __db_reginfo_t {		/* __db_r_attach IN parameters. */
	reg_type    type;		/* Region type. */
	u_int32_t   id;			/* Region id. */
	int	    mode;		/* File creation mode. */

				/* __db_r_attach OUT parameters. */
	REGION	   *rp;			/* Shared region. */

	char	   *name;		/* Region file name. */

	void	   *addr;		/* Region allocation address. */
	void	   *primary;		/* Primary data structure address. */

	void	   *wnt_handle;		/* Win/NT HANDLE. */

#define	REGION_CREATE		0x01	/* Caller created region. */
#define	REGION_CREATE_OK	0x02	/* Caller willing to create region. */
#define	REGION_JOIN_OK		0x04	/* Caller is looking for a match. */
	u_int32_t   flags;
};

/*
 * Mutex maintenance information each subsystem region must keep track
 * of to manage resources adequately.
 */
typedef struct __db_regmaint_stat_t {
	u_int32_t	st_hint_hit;
	u_int32_t	st_hint_miss;
	u_int32_t	st_records;
	u_int32_t	st_clears;
	u_int32_t	st_destroys;
	u_int32_t	st_max_locks;
} REGMAINT_STAT;

typedef struct __db_regmaint_t {
	u_int32_t  reglocks;		/* Maximum # of mutexes we track. */
	u_int32_t  regmutex_hint;	/* Hint for next slot */
	REGMAINT_STAT stat;		/* Stats */
	roff_t	   regmutexes[1];	/* Region mutexes in use. */
} REGMAINT;

/*
 * R_ADDR	Return a per-process address for a shared region offset.
 * R_OFFSET	Return a shared region offset for a per-process address.
 *
 * !!!
 * R_OFFSET should really be returning a ptrdiff_t, but that's not yet
 * portable.  We use u_int32_t, which restricts regions to 4Gb in size.
 */
#define	R_ADDR(base, offset)						\
	((void *)((u_int8_t *)((base)->addr) + offset))
#define	R_OFFSET(base, p)						\
	((u_int32_t)((u_int8_t *)(p) - (u_int8_t *)(base)->addr))

/*
 * R_LOCK	Lock/unlock a region.
 * R_UNLOCK
 */
#define	R_LOCK(dbenv, reginfo)						\
	MUTEX_LOCK(dbenv, &(reginfo)->rp->mutex, (dbenv)->lockfhp)
#define	R_UNLOCK(dbenv, reginfo)					\
	MUTEX_UNLOCK(dbenv, &(reginfo)->rp->mutex)

/* PANIC_CHECK:	Check to see if the DB environment is dead. */
#define	PANIC_CHECK(dbenv)						\
	if (DB_GLOBAL(db_panic) &&					\
	    (dbenv)->reginfo != NULL && ((REGENV *)			\
	    ((REGINFO *)(dbenv)->reginfo)->primary)->panic != 0)	\
		return (DB_RUNRECOVERY);

/*
 * All regions are created on 8K boundaries out of sheer paranoia, so that
 * we don't make some underlying VM unhappy.
 */
#define	OS_ROUNDOFF(i, s) {						\
	(i) += (s) - 1;							\
	(i) -= (i) % (s);						\
}
#define	OS_VMPAGESIZE		(8 * 1024)
#define	OS_VMROUNDOFF(i)	OS_ROUNDOFF(i, OS_VMPAGESIZE)

#if defined(__cplusplus)
}
#endif
