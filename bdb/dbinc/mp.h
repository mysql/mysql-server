/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp.h,v 11.44 2002/08/06 06:11:21 bostic Exp $
 */

#ifndef	_DB_MP_H_
#define	_DB_MP_H_

struct __bh;		typedef struct __bh BH;
struct __db_mpool_hash; typedef struct __db_mpool_hash DB_MPOOL_HASH;
struct __db_mpreg;	typedef struct __db_mpreg DB_MPREG;
struct __mpool;		typedef struct __mpool MPOOL;

				/* We require at least 20KB of cache. */
#define	DB_CACHESIZE_MIN	(20 * 1024)

typedef enum {
	DB_SYNC_ALLOC,		/* Flush for allocation. */
	DB_SYNC_CACHE,		/* Checkpoint or flush entire cache. */
	DB_SYNC_FILE,		/* Flush file. */
	DB_SYNC_TRICKLE		/* Trickle sync. */
} db_sync_op;

/*
 * DB_MPOOL --
 *	Per-process memory pool structure.
 */
struct __db_mpool {
	/* These fields need to be protected for multi-threaded support. */
	DB_MUTEX   *mutexp;		/* Structure thread lock. */

					/* List of pgin/pgout routines. */
	LIST_HEAD(__db_mpregh, __db_mpreg) dbregq;

					/* List of DB_MPOOLFILE's. */
	TAILQ_HEAD(__db_mpoolfileh, __db_mpoolfile) dbmfq;

	/*
	 * The dbenv, nreg and reginfo fields are not thread protected,
	 * as they are initialized during mpool creation, and not modified
	 * again.
	 */
	DB_ENV	   *dbenv;		/* Enclosing environment. */

	u_int32_t   nreg;		/* N underlying cache regions. */
	REGINFO	   *reginfo;		/* Underlying cache regions. */
};

/*
 * DB_MPREG --
 *	DB_MPOOL registry of pgin/pgout functions.
 */
struct __db_mpreg {
	LIST_ENTRY(__db_mpreg) q;	/* Linked list. */

	int32_t ftype;			/* File type. */
					/* Pgin, pgout routines. */
	int (*pgin) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*pgout) __P((DB_ENV *, db_pgno_t, void *, DBT *));
};

/*
 * NCACHE --
 *	Select a cache based on the file and the page number.  Assumes accesses
 *	are uniform across pages, which is probably OK.  What we really want to
 *	avoid is anything that puts all pages from any single file in the same
 *	cache, as we expect that file access will be bursty, and to avoid
 *	putting all page number N pages in the same cache as we expect access
 *	to the metapages (page 0) and the root of a btree (page 1) to be much
 *	more frequent than a random data page.
 */
#define	NCACHE(mp, mf_offset, pgno)					\
	(((pgno) ^ ((mf_offset) >> 3)) % ((MPOOL *)mp)->nreg)

/*
 * NBUCKET --
 *	 We make the assumption that early pages of the file are more likely
 *	 to be retrieved than the later pages, which means the top bits will
 *	 be more interesting for hashing as they're less likely to collide.
 *	 That said, as 512 8K pages represents a 4MB file, so only reasonably
 *	 large files will have page numbers with any other than the bottom 9
 *	 bits set.  We XOR in the MPOOL offset of the MPOOLFILE that backs the
 *	 page, since that should also be unique for the page.  We don't want
 *	 to do anything very fancy -- speed is more important to us than using
 *	 good hashing.
 */
#define	NBUCKET(mc, mf_offset, pgno)					\
	(((pgno) ^ ((mf_offset) << 9)) % (mc)->htab_buckets)

/*
 * MPOOL --
 *	Shared memory pool region.
 */
struct __mpool {
	/*
	 * The memory pool can be broken up into individual pieces/files.
	 * Not what we would have liked, but on Solaris you can allocate
	 * only a little more than 2GB of memory in a contiguous chunk,
	 * and I expect to see more systems with similar issues.
	 *
	 * While this structure is duplicated in each piece of the cache,
	 * the first of these pieces/files describes the entire pool, the
	 * second only describe a piece of the cache.
	 */

	/*
	 * The lsn field and list of underlying MPOOLFILEs are thread protected
	 * by the region lock.
	 */
	DB_LSN	  lsn;			/* Maximum checkpoint LSN. */

	SH_TAILQ_HEAD(__mpfq) mpfq;	/* List of MPOOLFILEs. */

	/*
	 * The nreg, regids and maint_off fields are not thread protected,
	 * as they are initialized during mpool creation, and not modified
	 * again.
	 */
	u_int32_t nreg;			/* Number of underlying REGIONS. */
	roff_t	  regids;		/* Array of underlying REGION Ids. */

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	roff_t	    maint_off;		/* Maintenance information offset */
#endif

	/*
	 * The following structure fields only describe the per-cache portion
	 * of the region.
	 *
	 * The htab and htab_buckets fields are not thread protected as they
	 * are initialized during mpool creation, and not modified again.
	 *
	 * The last_checked and lru_count fields are thread protected by
	 * the region lock.
	 */
	int	    htab_buckets;	/* Number of hash table entries. */
	roff_t	    htab;		/* Hash table offset. */
	u_int32_t   last_checked;	/* Last bucket checked for free. */
	u_int32_t   lru_count;		/* Counter for buffer LRU */

	/*
	 * The stat fields are generally not thread protected, and cannot be
	 * trusted.  Note that st_pages is an exception, and is always updated
	 * inside a region lock (although it is sometimes read outside of the
	 * region lock).
	 */
	DB_MPOOL_STAT stat;		/* Per-cache mpool statistics. */
};

struct __db_mpool_hash {
	DB_MUTEX	hash_mutex;	/* Per-bucket mutex. */

	DB_HASHTAB	hash_bucket;	/* Head of bucket. */

	u_int32_t	hash_page_dirty;/* Count of dirty pages. */
	u_int32_t	hash_priority;	/* Minimum priority of bucket buffer. */
};

/*
 * The base mpool priority is 1/4th of the name space, or just under 2^30.
 * When the LRU counter wraps, we shift everybody down to a base-relative
 * value.
 */
#define	MPOOL_BASE_DECREMENT	(UINT32_T_MAX - (UINT32_T_MAX / 4))

/*
 * Mpool priorities from low to high.  Defined in terms of fractions of the
 * buffers in the pool.
 */
#define	MPOOL_PRI_VERY_LOW	-1	/* Dead duck.  Check and set to 0. */
#define	MPOOL_PRI_LOW		-2	/* Low. */
#define	MPOOL_PRI_DEFAULT	0	/* No adjustment -- special case.*/
#define	MPOOL_PRI_HIGH		10	/* With the dirty buffers. */
#define	MPOOL_PRI_DIRTY		10	/* Dirty gets a 10% boost. */
#define	MPOOL_PRI_VERY_HIGH	1	/* Add number of buffers in pool. */

/*
 * MPOOLFILE_IGNORE --
 *	Discard an MPOOLFILE and any buffers it references: update the flags
 *	so we never try to write buffers associated with the file, nor can we
 *	find it when looking for files to join.  In addition, clear the ftype
 *	field, there's no reason to post-process pages, they can be discarded
 *	by any thread.
 *
 * Expects the MPOOLFILE mutex to be held.
 */
#define	MPOOLFILE_IGNORE(mfp) {						\
	(mfp)->ftype = 0;						\
	F_SET(mfp, MP_DEADFILE);					\
}

/*
 * MPOOLFILE --
 *	Shared DB_MPOOLFILE information.
 */
struct __mpoolfile {
	DB_MUTEX mutex;

	/* Protected by MPOOLFILE mutex. */
	u_int32_t mpf_cnt;		/* Ref count: DB_MPOOLFILEs. */
	u_int32_t block_cnt;		/* Ref count: blocks in cache. */

	roff_t	  path_off;		/* File name location. */

	/* Protected by mpool cache 0 region lock. */
	SH_TAILQ_ENTRY q;		/* List of MPOOLFILEs */
	db_pgno_t last_pgno;		/* Last page in the file. */
	db_pgno_t orig_last_pgno;	/* Original last page in the file. */

	/*
	 * None of the following fields are thread protected.
	 *
	 * There are potential races with the ftype field because it's read
	 * without holding a lock.  However, it has to be set before adding
	 * any buffers to the cache that depend on it being set, so there
	 * would need to be incorrect operation ordering to have a problem.
	 *
	 * There are potential races with the priority field because it's read
	 * without holding a lock.  However, a collision is unlikely and if it
	 * happens is of little consequence.
	 *
	 * We do not protect the statistics in "stat" because of the cost of
	 * the mutex in the get/put routines.  There is a chance that a count
	 * will get lost.
	 *
	 * The remaining fields are initialized at open and never subsequently
	 * modified, except for the MP_DEADFILE, which is only set and never
	 * unset.  (If there was more than one flag that was subsequently set,
	 * there might be a race, but with a single flag there can't be.)
	 */
	int32_t	  ftype;		/* File type. */

	int32_t   priority;		/* Priority when unpinning buffer. */

	DB_MPOOL_FSTAT stat;		/* Per-file mpool statistics. */

	int32_t	  lsn_off;		/* Page's LSN offset. */
	u_int32_t clear_len;		/* Bytes to clear on page create. */

	roff_t	  fileid_off;		/* File ID string location. */

	roff_t	  pgcookie_len;		/* Pgin/pgout cookie length. */
	roff_t	  pgcookie_off;		/* Pgin/pgout cookie location. */

#define	MP_CAN_MMAP	0x01		/* If the file can be mmap'd. */
#define	MP_DEADFILE	0x02		/* Dirty pages can simply be trashed. */
#define	MP_DIRECT	0x04		/* No OS buffering. */
#define	MP_EXTENT	0x08		/* Extent file. */
#define	MP_TEMP		0x10		/* Backing file is a temporary. */
#define	MP_UNLINK	0x20		/* Unlink file on last close. */
	u_int32_t  flags;
};

/*
 * BH --
 *	Buffer header.
 */
struct __bh {
	DB_MUTEX	mutex;		/* Buffer thread/process lock. */

	u_int16_t	ref;		/* Reference count. */
	u_int16_t	ref_sync;	/* Sync wait-for reference count. */

#define	BH_CALLPGIN	0x001		/* Convert the page before use. */
#define	BH_DIRTY	0x002		/* Page was modified. */
#define	BH_DIRTY_CREATE	0x004		/* Page created, must be written. */
#define	BH_DISCARD	0x008		/* Page is useless. */
#define	BH_LOCKED	0x010		/* Page is locked (I/O in progress). */
#define	BH_TRASH	0x020		/* Page is garbage. */
	u_int16_t	flags;

	u_int32_t	priority;	/* LRU priority. */
	SH_TAILQ_ENTRY	hq;		/* MPOOL hash bucket queue. */

	db_pgno_t pgno;			/* Underlying MPOOLFILE page number. */
	roff_t	  mf_offset;		/* Associated MPOOLFILE offset. */

	/*
	 * !!!
	 * This array must be at least size_t aligned -- the DB access methods
	 * put PAGE and other structures into it, and then access them directly.
	 * (We guarantee size_t alignment to applications in the documentation,
	 * too.)
	 */
	u_int8_t   buf[1];		/* Variable length data. */
};

#include "dbinc_auto/mp_ext.h"
#endif /* !_DB_MP_H_ */
