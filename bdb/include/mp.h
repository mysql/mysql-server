/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp.h,v 11.16 2001/01/10 04:50:53 ubell Exp $
 */

struct __bh;		typedef struct __bh BH;
struct __db_mpool;	typedef struct __db_mpool DB_MPOOL;
struct __db_mpreg;	typedef struct __db_mpreg DB_MPREG;
struct __mpool;		typedef struct __mpool MPOOL;
struct __mpoolfile;	typedef struct __mpoolfile MPOOLFILE;

/* We require at least 40K of cache. */
#define	DB_CACHESIZE_MIN	(20 * 1024)

/*
 * DB_MPOOL --
 *	Per-process memory pool structure.
 */
struct __db_mpool {
	/* These fields need to be protected for multi-threaded support. */
	MUTEX	   *mutexp;		/* Structure thread lock. */

					/* List of pgin/pgout routines. */
	LIST_HEAD(__db_mpregh, __db_mpreg) dbregq;

					/* List of DB_MPOOLFILE's. */
	TAILQ_HEAD(__db_mpoolfileh, __db_mpoolfile) dbmfq;

	/* These fields are not thread-protected. */
	DB_ENV     *dbenv;		/* Reference to error information. */

	u_int32_t   nreg;		/* N underlying cache regions. */
	REGINFO	   *reginfo;		/* Underlying cache regions. */
};

/*
 * DB_MPREG --
 *	DB_MPOOL registry of pgin/pgout functions.
 */
struct __db_mpreg {
	LIST_ENTRY(__db_mpreg) q;	/* Linked list. */

	int ftype;			/* File type. */
					/* Pgin, pgout routines. */
	int (*pgin) __P((DB_ENV *, db_pgno_t, void *, DBT *));
	int (*pgout) __P((DB_ENV *, db_pgno_t, void *, DBT *));
};

/*
 * DB_MPOOLFILE --
 *	Per-process DB_MPOOLFILE information.
 */
struct __db_mpoolfile {
	/* These fields need to be protected for multi-threaded support. */
	MUTEX	  *mutexp;		/* Structure thread lock. */

	DB_FH	   fh;			/* Underlying file handle. */

	u_int32_t ref;			/* Reference count. */

	/*
	 * !!!
	 * This field is a special case -- it's protected by the region lock
	 * NOT the thread lock.  The reason for this is that we always have
	 * the region lock immediately before or after we modify the field,
	 * and we don't want to use the structure lock to protect it because
	 * then I/O (which is done with the structure lock held because of
	 * the race between the seek and write of the file descriptor) will
	 * block any other put/get calls using this DB_MPOOLFILE structure.
	 */
	u_int32_t pinref;		/* Pinned block reference count. */

	/*
	 * !!!
	 * This field is a special case -- it's protected by the region lock
	 * since it's manipulated only when new files are added to the list.
	 */
	TAILQ_ENTRY(__db_mpoolfile) q;	/* Linked list of DB_MPOOLFILE's. */

	/* These fields are not thread-protected. */
	DB_MPOOL  *dbmp;		/* Overlying DB_MPOOL. */
	MPOOLFILE *mfp;			/* Underlying MPOOLFILE. */

	void	  *addr;		/* Address of mmap'd region. */
	size_t	   len;			/* Length of mmap'd region. */

	/* These fields need to be protected for multi-threaded support. */
#define	MP_READONLY	0x01		/* File is readonly. */
#define	MP_UPGRADE	0x02		/* File descriptor is readwrite. */
#define	MP_UPGRADE_FAIL	0x04		/* Upgrade wasn't possible. */
	u_int32_t  flags;
};

/*
 * NCACHE --
 *	Select a cache based on the page number.  This assumes accesses are
 *	uniform across pages, which is probably OK -- what we really want to
 *	avoid is anything that puts all the pages for any single file in the
 *	same cache, as we expect that file access will be bursty.
 */
#define	NCACHE(mp, pgno)						\
	((pgno) % ((MPOOL *)mp)->nreg)

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
	 * The first of these pieces/files describes the entire pool, all
	 * subsequent ones only describe a part of the cache.
	 *
	 * We single-thread memp_sync and memp_fsync calls.
	 *
	 * This mutex is intended *only* to single-thread access to the call,
	 * it is not used to protect the lsn and lsn_cnt fields, the region
	 * lock is used to protect them.
	 */
	MUTEX	  sync_mutex;		/* Checkpoint lock. */
	DB_LSN	  lsn;			/* Maximum checkpoint LSN. */
	u_int32_t lsn_cnt;		/* Checkpoint buffers left to write. */

	SH_TAILQ_HEAD(__mpfq) mpfq;	/* List of MPOOLFILEs. */

	u_int32_t nreg;			/* Number of underlying REGIONS. */
	roff_t	  regids;		/* Array of underlying REGION Ids. */

#define	MP_LSN_RETRY	0x01		/* Retry all BH_WRITE buffers. */
	u_int32_t  flags;

	/*
	 * The following structure fields only describe the cache portion of
	 * the region.
	 */
	SH_TAILQ_HEAD(__bhq) bhq;	/* LRU list of buffer headers. */

	int	    htab_buckets;	/* Number of hash table entries. */
	roff_t	    htab;		/* Hash table offset. */

	DB_MPOOL_STAT stat;		/* Per-cache mpool statistics. */
#ifdef MUTEX_SYSTEM_RESOURCES
	roff_t	    maint_off;		/* Maintenance information offset */
#endif
};

/*
 * MPOOLFILE --
 *	Shared DB_MPOOLFILE information.
 */
struct __mpoolfile {
	SH_TAILQ_ENTRY  q;		/* List of MPOOLFILEs */

	db_pgno_t mpf_cnt;		/* Ref count: DB_MPOOLFILEs. */
	db_pgno_t block_cnt;		/* Ref count: blocks in cache. */
	db_pgno_t lsn_cnt;		/* Checkpoint buffers left to write. */

	int	  ftype;		/* File type. */
	int32_t	  lsn_off;		/* Page's LSN offset. */
	u_int32_t clear_len;		/* Bytes to clear on page create. */

	roff_t	  path_off;		/* File name location. */
	roff_t	  fileid_off;		/* File identification location. */

	roff_t	  pgcookie_len;		/* Pgin/pgout cookie length. */
	roff_t	  pgcookie_off;		/* Pgin/pgout cookie location. */

	db_pgno_t last_pgno;		/* Last page in the file. */
	db_pgno_t orig_last_pgno;	/* Original last page in the file. */

	DB_MPOOL_FSTAT stat;		/* Per-file mpool statistics. */

#define	MP_CAN_MMAP	0x01		/* If the file can be mmap'd. */
#define	MP_DEADFILE	0x02		/* Dirty pages can simply be trashed. */
#define	MP_TEMP		0x04		/* Backing file is a temporary. */
#define	MP_UNLINK	0x08		/* Unlink file on last close. */
	u_int32_t  flags;
};

/*
 * BH_TO_CACHE --
 *	Return the cache where we can find the specified buffer header.
 */
#define	BH_TO_CACHE(dbmp, bhp)						\
	(dbmp)->reginfo[NCACHE((dbmp)->reginfo[0].primary, (bhp)->pgno)].primary

/*
 * BH --
 *	Buffer header.
 */
struct __bh {
	MUTEX	        mutex;		/* Buffer thread/process lock. */

	u_int16_t	ref;		/* Reference count. */

#define	BH_CALLPGIN	0x001		/* Page needs to be reworked... */
#define	BH_DIRTY	0x002		/* Page was modified. */
#define	BH_DISCARD	0x004		/* Page is useless. */
#define	BH_LOCKED	0x008		/* Page is locked (I/O in progress). */
#define	BH_SYNC		0x010		/* memp sync: write the page */
#define	BH_SYNC_LOGFLSH	0x020		/* memp sync: also flush the log */
#define	BH_TRASH	0x040		/* Page is garbage. */
	u_int16_t  flags;

	SH_TAILQ_ENTRY	q;		/* LRU queue. */
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

#include "mp_ext.h"
