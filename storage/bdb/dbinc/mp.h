/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp.h,v 11.61 2004/09/17 22:00:27 mjc Exp $
 */

#ifndef	_DB_MP_H_
#define	_DB_MP_H_

struct __bh;		typedef struct __bh BH;
struct __db_mpool_hash; typedef struct __db_mpool_hash DB_MPOOL_HASH;
struct __db_mpreg;	typedef struct __db_mpreg DB_MPREG;
struct __mpool;		typedef struct __mpool MPOOL;

				/* We require at least 20KB of cache. */
#define	DB_CACHESIZE_MIN	(20 * 1024)

/*
 * DB_MPOOLFILE initialization methods cannot be called after open is called,
 * other methods cannot be called before open is called
 */
#define	MPF_ILLEGAL_AFTER_OPEN(dbmfp, name)				\
	if (F_ISSET(dbmfp, MP_OPEN_CALLED))				\
		return (__db_mi_open((dbmfp)->dbenv, name, 1));
#define	MPF_ILLEGAL_BEFORE_OPEN(dbmfp, name)				\
	if (!F_ISSET(dbmfp, MP_OPEN_CALLED))				\
		return (__db_mi_open((dbmfp)->dbenv, name, 0));

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
	(((pgno) ^ ((u_int32_t)(mf_offset) >> 3)) % ((MPOOL *)mp)->nreg)

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

	/* Configuration information: protected by the region lock. */
	size_t mp_mmapsize;		/* Maximum file size for mmap. */
	int    mp_maxopenfd;		/* Maximum open file descriptors. */
	int    mp_maxwrite;		/* Maximum buffers to write. */
	int    mp_maxwrite_sleep;	/* Sleep after writing max buffers. */

	/*
	 * The nreg, regids and maint_off fields are not thread protected,
	 * as they are initialized during mpool creation, and not modified
	 * again.
	 */
	u_int32_t nreg;			/* Number of underlying REGIONS. */
	roff_t	  regids;		/* Array of underlying REGION Ids. */

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	roff_t	  maint_off;		/* Maintenance information offset */
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
	u_int32_t htab_buckets;	/* Number of hash table entries. */
	roff_t	  htab;		/* Hash table offset. */
	u_int32_t last_checked;	/* Last bucket checked for free. */
	u_int32_t lru_count;		/* Counter for buffer LRU */

	/*
	 * The stat fields are generally not thread protected, and cannot be
	 * trusted.  Note that st_pages is an exception, and is always updated
	 * inside a region lock (although it is sometimes read outside of the
	 * region lock).
	 */
	DB_MPOOL_STAT stat;		/* Per-cache mpool statistics. */

	/*
	 * We track page puts so that we can decide when allocation is never
	 * going to succeed.  We don't lock the field, all we care about is
	 * if it changes.
	 */
	u_int32_t  put_counter;		/* Count of page put calls. */
};

struct __db_mpool_hash {
	DB_MUTEX	hash_mutex;	/* Per-bucket mutex. */

	DB_HASHTAB	hash_bucket;	/* Head of bucket. */

	u_int32_t	hash_page_dirty;/* Count of dirty pages. */
	u_int32_t	hash_priority;	/* Minimum priority of bucket buffer. */

#ifdef	HPUX_MUTEX_PAD
	/*
	 * !!!
	 * We allocate the mpool hash buckets as an array, which means that
	 * they are not individually aligned.  This fails on one platform:
	 * HPUX 10.20, where mutexes require 16 byte alignment.   This is a
	 * grievous hack for that single platform.
	 */
	u_int8_t	pad[HPUX_MUTEX_PAD];
#endif
};

/*
 * The base mpool priority is 1/4th of the name space, or just under 2^30.
 * When the LRU counter wraps, we shift everybody down to a base-relative
 * value.
 */
#define	MPOOL_BASE_DECREMENT	(UINT32_MAX - (UINT32_MAX / 4))

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
 * MPOOLFILE --
 *	Shared DB_MPOOLFILE information.
 */
struct __mpoolfile {
	DB_MUTEX mutex;

	/* Protected by MPOOLFILE mutex. */
	u_int32_t mpf_cnt;		/* Ref count: DB_MPOOLFILEs. */
	u_int32_t block_cnt;		/* Ref count: blocks in cache. */

	roff_t	  path_off;		/* File name location. */

	/*
	 * We normally don't lock the deadfile field when we read it since we
	 * only care if the field is zero or non-zero.  We do lock on read when
	 * searching for a matching MPOOLFILE -- see that code for more detail.
	 */
	int32_t	  deadfile;		/* Dirty pages can be discarded. */

	/* Protected by mpool cache 0 region lock. */
	SH_TAILQ_ENTRY q;		/* List of MPOOLFILEs */
	db_pgno_t last_pgno;		/* Last page in the file. */
	db_pgno_t orig_last_pgno;	/* Original last page in the file. */
	db_pgno_t maxpgno;		/* Maximum page number. */

	/*
	 * None of the following fields are thread protected.
	 *
	 * There are potential races with the ftype field because it's read
	 * without holding a lock.  However, it has to be set before adding
	 * any buffers to the cache that depend on it being set, so there
	 * would need to be incorrect operation ordering to have a problem.
	 */
	int32_t	  ftype;		/* File type. */

	/*
	 * There are potential races with the priority field because it's read
	 * without holding a lock.  However, a collision is unlikely and if it
	 * happens is of little consequence.
	 */
	int32_t   priority;		/* Priority when unpinning buffer. */

	/*
	 * There are potential races with the file_written field (many threads
	 * may be writing blocks at the same time), and with no_backing_file
	 * and unlink_on_close fields, as they may be set while other threads
	 * are reading them.  However, we only care if the field value is zero
	 * or non-zero, so don't lock the memory.
	 *
	 * !!!
	 * Theoretically, a 64-bit architecture could put two of these fields
	 * in a single memory operation and we could race.  I have never seen
	 * an architecture where that's a problem, and I believe Java requires
	 * that to never be the case.
	 *
	 * File_written is set whenever a buffer is marked dirty in the cache.
	 * It can be cleared in some cases, after all dirty buffers have been
	 * written AND the file has been flushed to disk.
	 */
	int32_t	  file_written;		/* File was written. */
	int32_t	  no_backing_file;	/* Never open a backing file. */
	int32_t	  unlink_on_close;	/* Unlink file on last close. */

	/*
	 * We do not protect the statistics in "stat" because of the cost of
	 * the mutex in the get/put routines.  There is a chance that a count
	 * will get lost.
	 */
	DB_MPOOL_FSTAT stat;		/* Per-file mpool statistics. */

	/*
	 * The remaining fields are initialized at open and never subsequently
	 * modified.
	 */
	int32_t	  lsn_off;		/* Page's LSN offset. */
	u_int32_t clear_len;		/* Bytes to clear on page create. */

	roff_t	  fileid_off;		/* File ID string location. */

	roff_t	  pgcookie_len;		/* Pgin/pgout cookie length. */
	roff_t	  pgcookie_off;		/* Pgin/pgout cookie location. */

	/*
	 * The flags are initialized at open and never subsequently modified.
	 */
#define	MP_CAN_MMAP		0x001	/* If the file can be mmap'd. */
#define	MP_DIRECT		0x002	/* No OS buffering. */
#define	MP_DURABLE_UNKNOWN	0x004	/* We don't care about durability. */
#define	MP_EXTENT		0x008	/* Extent file. */
#define	MP_FAKE_DEADFILE	0x010	/* Deadfile field: fake flag. */
#define	MP_FAKE_FILEWRITTEN	0x020	/* File_written field: fake flag. */
#define	MP_FAKE_NB		0x040	/* No_backing_file field: fake flag. */
#define	MP_FAKE_UOC		0x080	/* Unlink_on_close field: fake flag. */
#define	MP_NOT_DURABLE		0x100	/* File is not durable. */
#define	MP_TEMP			0x200	/* Backing file is a temporary. */
	u_int32_t  flags;
};

/*
 * Flags to __memp_bh_free.
 */
#define	BH_FREE_FREEMEM		0x01
#define	BH_FREE_UNLOCKED	0x02

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
