/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_stat.c,v 11.82 2004/10/15 16:59:43 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"

#ifdef HAVE_STATISTICS
static void __memp_print_bh
		__P((DB_ENV *, DB_MPOOL *, BH *, roff_t *, u_int32_t));
static int  __memp_print_all __P((DB_ENV *, u_int32_t));
static int  __memp_print_stats __P((DB_ENV *, u_int32_t));
static void __memp_print_hash __P((DB_ENV *,
		DB_MPOOL *, REGINFO *, roff_t *, u_int32_t));
static int  __memp_stat __P((DB_ENV *,
		DB_MPOOL_STAT **, DB_MPOOL_FSTAT ***, u_int32_t));
static void __memp_stat_wait __P((
		REGINFO *, MPOOL *, DB_MPOOL_STAT *, u_int32_t));

/*
 * __memp_stat_pp --
 *	DB_ENV->memp_stat pre/post processing.
 *
 * PUBLIC: int __memp_stat_pp
 * PUBLIC:     __P((DB_ENV *, DB_MPOOL_STAT **, DB_MPOOL_FSTAT ***, u_int32_t));
 */
int
__memp_stat_pp(dbenv, gspp, fspp, flags)
	DB_ENV *dbenv;
	DB_MPOOL_STAT **gspp;
	DB_MPOOL_FSTAT ***fspp;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "DB_ENV->memp_stat", DB_INIT_MPOOL);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->memp_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_stat(dbenv, gspp, fspp, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

/*
 * __memp_stat --
 *	DB_ENV->memp_stat
 */
static int
__memp_stat(dbenv, gspp, fspp, flags)
	DB_ENV *dbenv;
	DB_MPOOL_STAT **gspp;
	DB_MPOOL_FSTAT ***fspp;
	u_int32_t flags;
{
	DB_MPOOL *dbmp;
	DB_MPOOL_FSTAT **tfsp, *tstruct;
	DB_MPOOL_STAT *sp;
	MPOOL *c_mp, *mp;
	MPOOLFILE *mfp;
	size_t len, nlen;
	u_int32_t pages, pagesize, i;
	int ret;
	char *name, *tname;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	/* Global statistics. */
	if (gspp != NULL) {
		*gspp = NULL;

		if ((ret = __os_umalloc(dbenv, sizeof(**gspp), gspp)) != 0)
			return (ret);
		memset(*gspp, 0, sizeof(**gspp));
		sp = *gspp;

		/*
		 * Initialization and information that is not maintained on
		 * a per-cache basis.  Note that configuration information
		 * may be modified at any time, and so we have to lock.
		 */
		c_mp = dbmp->reginfo[0].primary;
		sp->st_gbytes = c_mp->stat.st_gbytes;
		sp->st_bytes = c_mp->stat.st_bytes;
		sp->st_ncache = dbmp->nreg;
		sp->st_regsize = dbmp->reginfo[0].rp->size;

		R_LOCK(dbenv, dbmp->reginfo);
		sp->st_mmapsize = mp->mp_mmapsize;
		sp->st_maxopenfd = mp->mp_maxopenfd;
		sp->st_maxwrite = mp->mp_maxwrite;
		sp->st_maxwrite_sleep = mp->mp_maxwrite_sleep;
		R_UNLOCK(dbenv, dbmp->reginfo);

		/* Walk the cache list and accumulate the global information. */
		for (i = 0; i < mp->nreg; ++i) {
			c_mp = dbmp->reginfo[i].primary;

			sp->st_map += c_mp->stat.st_map;
			sp->st_cache_hit += c_mp->stat.st_cache_hit;
			sp->st_cache_miss += c_mp->stat.st_cache_miss;
			sp->st_page_create += c_mp->stat.st_page_create;
			sp->st_page_in += c_mp->stat.st_page_in;
			sp->st_page_out += c_mp->stat.st_page_out;
			sp->st_ro_evict += c_mp->stat.st_ro_evict;
			sp->st_rw_evict += c_mp->stat.st_rw_evict;
			sp->st_page_trickle += c_mp->stat.st_page_trickle;
			sp->st_pages += c_mp->stat.st_pages;
			/*
			 * st_page_dirty	calculated by __memp_stat_hash
			 * st_page_clean	calculated here
			 */
			__memp_stat_hash(
			    &dbmp->reginfo[i], c_mp, &sp->st_page_dirty);
			sp->st_page_clean = sp->st_pages - sp->st_page_dirty;
			sp->st_hash_buckets += c_mp->stat.st_hash_buckets;
			sp->st_hash_searches += c_mp->stat.st_hash_searches;
			sp->st_hash_longest += c_mp->stat.st_hash_longest;
			sp->st_hash_examined += c_mp->stat.st_hash_examined;
			/*
			 * st_hash_nowait	calculated by __memp_stat_wait
			 * st_hash_wait
			 */
			__memp_stat_wait(&dbmp->reginfo[i], c_mp, sp, flags);
			sp->st_region_nowait +=
			    dbmp->reginfo[i].rp->mutex.mutex_set_nowait;
			sp->st_region_wait +=
			    dbmp->reginfo[i].rp->mutex.mutex_set_wait;
			sp->st_alloc += c_mp->stat.st_alloc;
			sp->st_alloc_buckets += c_mp->stat.st_alloc_buckets;
			if (sp->st_alloc_max_buckets <
			    c_mp->stat.st_alloc_max_buckets)
				sp->st_alloc_max_buckets =
				    c_mp->stat.st_alloc_max_buckets;
			sp->st_alloc_pages += c_mp->stat.st_alloc_pages;
			if (sp->st_alloc_max_pages <
			    c_mp->stat.st_alloc_max_pages)
				sp->st_alloc_max_pages =
				    c_mp->stat.st_alloc_max_pages;

			if (LF_ISSET(DB_STAT_CLEAR)) {
				MUTEX_CLEAR(&dbmp->reginfo[i].rp->mutex);

				R_LOCK(dbenv, dbmp->reginfo);
				pages = c_mp->stat.st_pages;
				memset(&c_mp->stat, 0, sizeof(c_mp->stat));
				c_mp->stat.st_hash_buckets = c_mp->htab_buckets;
				c_mp->stat.st_pages = pages;
				R_UNLOCK(dbenv, dbmp->reginfo);
			}
		}

		/*
		 * We have duplicate statistics fields in per-file structures
		 * and the cache.  The counters are only incremented in the
		 * per-file structures, except if a file is flushed from the
		 * mpool, at which time we copy its information into the cache
		 * statistics.  We added the cache information above, now we
		 * add the per-file information.
		 */
		R_LOCK(dbenv, dbmp->reginfo);
		for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
		    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
			sp->st_map += mfp->stat.st_map;
			sp->st_cache_hit += mfp->stat.st_cache_hit;
			sp->st_cache_miss += mfp->stat.st_cache_miss;
			sp->st_page_create += mfp->stat.st_page_create;
			sp->st_page_in += mfp->stat.st_page_in;
			sp->st_page_out += mfp->stat.st_page_out;
			if (fspp == NULL && LF_ISSET(DB_STAT_CLEAR)) {
				pagesize = mfp->stat.st_pagesize;
				memset(&mfp->stat, 0, sizeof(mfp->stat));
				mfp->stat.st_pagesize = pagesize;
			}
		}
		R_UNLOCK(dbenv, dbmp->reginfo);
	}

	/* Per-file statistics. */
	if (fspp != NULL) {
		*fspp = NULL;

		/* Count the MPOOLFILE structures. */
		R_LOCK(dbenv, dbmp->reginfo);
		for (i = 0, len = 0,
		    mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
		    mfp != NULL;
		    ++i, mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile))
			len += sizeof(DB_MPOOL_FSTAT *) +
			    sizeof(DB_MPOOL_FSTAT) +
			    strlen(__memp_fns(dbmp, mfp)) + 1;
		len += sizeof(DB_MPOOL_FSTAT *);	/* Trailing NULL */
		R_UNLOCK(dbenv, dbmp->reginfo);

		if (i == 0)
			return (0);

		/* Allocate space */
		if ((ret = __os_umalloc(dbenv, len, fspp)) != 0)
			return (ret);

		/*
		 * Build each individual entry.  We assume that an array of
		 * pointers are aligned correctly to be followed by an array
		 * of structures, which should be safe (in this particular
		 * case, the first element of the structure is a pointer, so
		 * we're doubly safe).  The array is followed by space for
		 * the text file names.
		 *
		 * Add 1 to i because we need to skip over the NULL.
		 */
		tfsp = *fspp;
		tstruct = (DB_MPOOL_FSTAT *)(tfsp + i + 1);
		tname = (char *)(tstruct + i);

		/*
		 * Files may have been opened since we counted, don't walk
		 * off the end of the allocated space.
		 */
		R_LOCK(dbenv, dbmp->reginfo);
		for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
		    mfp != NULL && i-- > 0;
		    ++tfsp, ++tstruct, tname += nlen,
		    mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
			name = __memp_fns(dbmp, mfp);
			nlen = strlen(name) + 1;
			*tfsp = tstruct;
			*tstruct = mfp->stat;
			if (LF_ISSET(DB_STAT_CLEAR)) {
				pagesize = mfp->stat.st_pagesize;
				memset(&mfp->stat, 0, sizeof(mfp->stat));
				mfp->stat.st_pagesize = pagesize;
			}
			tstruct->file_name = tname;
			memcpy(tname, name, nlen);
		}
		R_UNLOCK(dbenv, dbmp->reginfo);

		*tfsp = NULL;
	}
	return (0);
}

/*
 * __memp_stat_print_pp --
 *	DB_ENV->memp_stat_print pre/post processing.
 *
 * PUBLIC: int __memp_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__memp_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int rep_check, ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "DB_ENV->memp_stat_print", DB_INIT_MPOOL);

#define	DB_STAT_MEMP_FLAGS						\
	(DB_STAT_ALL | DB_STAT_CLEAR | DB_STAT_MEMP_HASH)
	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->memp_stat_print", flags, DB_STAT_MEMP_FLAGS)) != 0)
		return (ret);

	rep_check = IS_ENV_REPLICATED(dbenv) ? 1 : 0;
	if (rep_check)
		__env_rep_enter(dbenv);
	ret = __memp_stat_print(dbenv, flags);
	if (rep_check)
		__env_db_rep_exit(dbenv);
	return (ret);
}

#define	FMAP_ENTRIES	200			/* Files we map. */

/*
 * __memp_stat_print --
 *	DB_ENV->memp_stat_print method.
 *
 * PUBLIC: int  __memp_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__memp_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __memp_print_stats(dbenv, orig_flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL | DB_STAT_MEMP_HASH) &&
	    (ret = __memp_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __memp_print_stats --
 *	Display default mpool region statistics.
 */
static int
__memp_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_MPOOL_FSTAT **fsp, **tfsp;
	DB_MPOOL_STAT *gsp;
	int ret;

	if ((ret = __memp_stat(dbenv, &gsp, &fsp, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default cache region information:");
	__db_dlbytes(dbenv, "Total cache size",
	    (u_long)gsp->st_gbytes, (u_long)0, (u_long)gsp->st_bytes);
	__db_dl(dbenv, "Number of caches", (u_long)gsp->st_ncache);
	__db_dlbytes(dbenv, "Pool individual cache size",
	    (u_long)0, (u_long)0, (u_long)gsp->st_regsize);
	__db_dlbytes(dbenv, "Maximum memory-mapped file size",
	    (u_long)0, (u_long)0, (u_long)gsp->st_mmapsize);
	STAT_LONG("Maximum open file descriptors", gsp->st_maxopenfd);
	STAT_LONG("Maximum sequential buffer writes", gsp->st_maxwrite);
	STAT_LONG("Sleep after writing maximum sequential buffers",
	    gsp->st_maxwrite_sleep);
	__db_dl(dbenv,
	    "Requested pages mapped into the process' address space",
	    (u_long)gsp->st_map);
	__db_dl_pct(dbenv, "Requested pages found in the cache",
	    (u_long)gsp->st_cache_hit, DB_PCT(
	    gsp->st_cache_hit, gsp->st_cache_hit + gsp->st_cache_miss), NULL);
	__db_dl(dbenv, "Requested pages not found in the cache",
	    (u_long)gsp->st_cache_miss);
	__db_dl(dbenv,
	    "Pages created in the cache", (u_long)gsp->st_page_create);
	__db_dl(dbenv, "Pages read into the cache", (u_long)gsp->st_page_in);
	__db_dl(dbenv, "Pages written from the cache to the backing file",
	    (u_long)gsp->st_page_out);
	__db_dl(dbenv, "Clean pages forced from the cache",
	    (u_long)gsp->st_ro_evict);
	__db_dl(dbenv, "Dirty pages forced from the cache",
	    (u_long)gsp->st_rw_evict);
	__db_dl(dbenv, "Dirty pages written by trickle-sync thread",
	    (u_long)gsp->st_page_trickle);
	__db_dl(dbenv, "Current total page count",
	    (u_long)gsp->st_pages);
	__db_dl(dbenv, "Current clean page count",
	    (u_long)gsp->st_page_clean);
	__db_dl(dbenv, "Current dirty page count",
	    (u_long)gsp->st_page_dirty);
	__db_dl(dbenv, "Number of hash buckets used for page location",
	    (u_long)gsp->st_hash_buckets);
	__db_dl(dbenv,
	    "Total number of times hash chains searched for a page",
	    (u_long)gsp->st_hash_searches);
	__db_dl(dbenv, "The longest hash chain searched for a page",
	    (u_long)gsp->st_hash_longest);
	__db_dl(dbenv,
	    "Total number of hash buckets examined for page location",
	    (u_long)gsp->st_hash_examined);
	__db_dl_pct(dbenv,
	    "The number of hash bucket locks that required waiting",
	    (u_long)gsp->st_hash_wait, DB_PCT(
	    gsp->st_hash_wait, gsp->st_hash_wait + gsp->st_hash_nowait), NULL);
	__db_dl(dbenv,
    "The maximum number of times any hash bucket lock was waited for",
	    (u_long)gsp->st_hash_max_wait);
	__db_dl_pct(dbenv,
	    "The number of region locks that required waiting",
	    (u_long)gsp->st_region_wait, DB_PCT(gsp->st_region_wait,
	    gsp->st_region_wait + gsp->st_region_nowait), NULL);
	__db_dl(dbenv, "The number of page allocations", (u_long)gsp->st_alloc);
	__db_dl(dbenv,
	    "The number of hash buckets examined during allocations",
	    (u_long)gsp->st_alloc_buckets);
	__db_dl(dbenv,
	    "The maximum number of hash buckets examined for an allocation",
	    (u_long)gsp->st_alloc_max_buckets);
	__db_dl(dbenv, "The number of pages examined during allocations",
	    (u_long)gsp->st_alloc_pages);
	__db_dl(dbenv, "The max number of pages examined for an allocation",
	    (u_long)gsp->st_alloc_max_pages);

	for (tfsp = fsp; fsp != NULL && *tfsp != NULL; ++tfsp) {
		if (LF_ISSET(DB_STAT_ALL))
			__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Pool File: %s", (*tfsp)->file_name);
		__db_dl(dbenv, "Page size", (u_long)(*tfsp)->st_pagesize);
		__db_dl(dbenv,
		    "Requested pages mapped into the process' address space",
		    (u_long)(*tfsp)->st_map);
		__db_dl_pct(dbenv, "Requested pages found in the cache",
		    (u_long)(*tfsp)->st_cache_hit, DB_PCT((*tfsp)->st_cache_hit,
		    (*tfsp)->st_cache_hit + (*tfsp)->st_cache_miss), NULL);
		__db_dl(dbenv, "Requested pages not found in the cache",
		    (u_long)(*tfsp)->st_cache_miss);
		__db_dl(dbenv, "Pages created in the cache",
		    (u_long)(*tfsp)->st_page_create);
		__db_dl(dbenv, "Pages read into the cache",
		    (u_long)(*tfsp)->st_page_in);
		__db_dl(dbenv,
		    "Pages written from the cache to the backing file",
		    (u_long)(*tfsp)->st_page_out);
	}

	__os_ufree(dbenv, fsp);
	__os_ufree(dbenv, gsp);
	return (0);
}

/*
 * __memp_print_all --
 *	Display debugging mpool region statistics.
 */
static int
__memp_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ MP_CAN_MMAP,		"MP_CAN_MMAP" },
		{ MP_DIRECT,		"MP_DIRECT" },
		{ MP_EXTENT,		"MP_EXTENT" },
		{ MP_FAKE_DEADFILE,	"deadfile" },
		{ MP_FAKE_FILEWRITTEN,	"file written" },
		{ MP_FAKE_NB,		"no backing file" },
		{ MP_FAKE_UOC,		"unlink on close" },
		{ MP_NOT_DURABLE,	"not durable" },
		{ MP_TEMP,		"MP_TEMP" },
		{ 0,			NULL }
	};
	static const FN cfn[] = {
		{ DB_MPOOL_NOFILE,	"DB_MPOOL_NOFILE" },
		{ DB_MPOOL_UNLINK,	"DB_MPOOL_UNLINK" },
		{ 0,			NULL }
	};
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	roff_t fmap[FMAP_ENTRIES + 1];
	u_int32_t i, mfp_flags;
	int cnt;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	R_LOCK(dbenv, dbmp->reginfo);

	__db_print_reginfo(dbenv, dbmp->reginfo, "Mpool");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "MPOOL structure:");
	STAT_LSN("Maximum checkpoint LSN", &mp->lsn);
	STAT_ULONG("Hash table entries", mp->htab_buckets);
	STAT_ULONG("Hash table last-checked", mp->last_checked);
	STAT_ULONG("Hash table LRU count", mp->lru_count);
	STAT_ULONG("Put counter", mp->put_counter);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_MPOOL handle information:");
	__db_print_mutex(
	    dbenv, NULL, dbmp->mutexp, "DB_MPOOL handle mutex", flags);
	STAT_ULONG("Underlying cache regions", dbmp->nreg);

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_MPOOLFILE structures:");
	for (cnt = 0, dbmfp = TAILQ_FIRST(&dbmp->dbmfq);
	    dbmfp != NULL; dbmfp = TAILQ_NEXT(dbmfp, q), ++cnt) {
		__db_msg(dbenv, "File #%d: %s: per-process, %s",
		    cnt + 1, __memp_fn(dbmfp),
		    F_ISSET(dbmfp, MP_READONLY) ? "readonly" : "read/write");
		STAT_ULONG("Reference count", dbmfp->ref);
		STAT_ULONG("Pinned block reference count", dbmfp->ref);
		STAT_ULONG("Clear length", dbmfp->clear_len);
		__db_print_fileid(dbenv, dbmfp->fileid, "\tID");
		STAT_ULONG("File type", dbmfp->ftype);
		STAT_ULONG("LSN offset", dbmfp->lsn_offset);
		STAT_ULONG("Max gbytes", dbmfp->gbytes);
		STAT_ULONG("Max bytes", dbmfp->bytes);
		STAT_ULONG("Cache priority", dbmfp->priority);
		STAT_HEX("mmap address", dbmfp->addr);
		STAT_ULONG("mmap length", dbmfp->len);
		__db_prflags(dbenv, NULL, dbmfp->flags, cfn, NULL, "\tFlags");
		__db_print_fh(dbenv, dbmfp->fhp, flags);
	}

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "MPOOLFILE structures:");
	for (cnt = 0, mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile), ++cnt) {
		__db_msg(dbenv, "File #%d: %s", cnt + 1, __memp_fns(dbmp, mfp));
		__db_print_mutex(dbenv, NULL, &mfp->mutex, "Mutex", flags);

		MUTEX_LOCK(dbenv, &mfp->mutex);
		STAT_ULONG("Reference count", mfp->mpf_cnt);
		STAT_ULONG("Block count", mfp->block_cnt);
		STAT_ULONG("Last page number", mfp->last_pgno);
		STAT_ULONG("Original last page number", mfp->orig_last_pgno);
		STAT_ULONG("Maximum page number", mfp->maxpgno);
		STAT_LONG("Type", mfp->ftype);
		STAT_LONG("Priority", mfp->priority);
		STAT_LONG("Page's LSN offset", mfp->lsn_off);
		STAT_LONG("Page's clear length", mfp->clear_len);

		__db_print_fileid(dbenv,
		    R_ADDR(dbmp->reginfo, mfp->fileid_off), "\tID");

		mfp_flags = 0;
		if (mfp->deadfile)
			FLD_SET(mfp_flags, MP_FAKE_DEADFILE);
		if (mfp->file_written)
			FLD_SET(mfp_flags, MP_FAKE_FILEWRITTEN);
		if (mfp->no_backing_file)
			FLD_SET(mfp_flags, MP_FAKE_NB);
		if (mfp->unlink_on_close)
			FLD_SET(mfp_flags, MP_FAKE_UOC);
		__db_prflags(dbenv, NULL, mfp_flags, fn, NULL, "\tFlags");

		if (cnt < FMAP_ENTRIES)
			fmap[cnt] = R_OFFSET(dbmp->reginfo, mfp);
		MUTEX_UNLOCK(dbenv, &mfp->mutex);
	}
	R_UNLOCK(dbenv, dbmp->reginfo);

	if (cnt < FMAP_ENTRIES)
		fmap[cnt] = INVALID_ROFF;
	else
		fmap[FMAP_ENTRIES] = INVALID_ROFF;

	/* Dump the individual caches. */
	for (i = 0; i < mp->nreg; ++i) {
		__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
		__db_msg(dbenv, "Cache #%d:", i + 1);
		__memp_print_hash(dbenv, dbmp, &dbmp->reginfo[i], fmap, flags);
	}

	return (0);
}

/*
 * __memp_print_hash --
 *	Display hash bucket statistics for a cache.
 */
static void
__memp_print_hash(dbenv, dbmp, reginfo, fmap, flags)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	REGINFO *reginfo;
	roff_t *fmap;
	u_int32_t flags;
{
	BH *bhp;
	DB_MPOOL_HASH *hp;
	DB_MSGBUF mb;
	MPOOL *c_mp;
	u_int32_t bucket;

	c_mp = reginfo->primary;
	DB_MSGBUF_INIT(&mb);

	/* Display the hash table list of BH's. */
	__db_msg(dbenv,
	    "BH hash table (%lu hash slots)", (u_long)c_mp->htab_buckets);
	__db_msg(dbenv, "bucket #: priority, mutex");
	__db_msg(dbenv,
	    "\tpageno, file, ref, LSN, mutex, address, priority, flags");

	for (hp = R_ADDR(reginfo, c_mp->htab),
	    bucket = 0; bucket < c_mp->htab_buckets; ++hp, ++bucket) {
		MUTEX_LOCK(dbenv, &hp->hash_mutex);
		if ((bhp =
		    SH_TAILQ_FIRST(&hp->hash_bucket, __bh)) != NULL) {
			__db_msgadd(dbenv, &mb, "bucket %lu: %lu, ",
			    (u_long)bucket, (u_long)hp->hash_priority);
			__db_print_mutex(
			    dbenv, &mb, &hp->hash_mutex, ":", flags);
			DB_MSGBUF_FLUSH(dbenv, &mb);
		}
		for (; bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh))
			__memp_print_bh(dbenv, dbmp, bhp, fmap, flags);

		MUTEX_UNLOCK(dbenv, &hp->hash_mutex);
	}
}

/*
 * __memp_print_bh --
 *	Display a BH structure.
 */
static void
__memp_print_bh(dbenv, dbmp, bhp, fmap, flags)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	BH *bhp;
	roff_t *fmap;
	u_int32_t flags;
{
	static const FN fn[] = {
		{ BH_CALLPGIN,		"callpgin" },
		{ BH_DIRTY,		"dirty" },
		{ BH_DIRTY_CREATE,	"created" },
		{ BH_DISCARD,		"discard" },
		{ BH_LOCKED,		"locked" },
		{ BH_TRASH,		"trash" },
		{ 0,			NULL }
	};
	DB_MSGBUF mb;
	int i;

	DB_MSGBUF_INIT(&mb);

	for (i = 0; i < FMAP_ENTRIES; ++i)
		if (fmap[i] == INVALID_ROFF || fmap[i] == bhp->mf_offset)
			break;

	if (fmap[i] == INVALID_ROFF)
		__db_msgadd(dbenv, &mb, "\t%5lu, %lu, ",
		    (u_long)bhp->pgno, (u_long)bhp->mf_offset);
	else
		__db_msgadd(
		    dbenv, &mb, "\t%5lu, #%d, ", (u_long)bhp->pgno, i + 1);

	__db_msgadd(dbenv, &mb, "%2lu, %lu/%lu, ", (u_long)bhp->ref,
	    (u_long)LSN(bhp->buf).file, (u_long)LSN(bhp->buf).offset);
	__db_print_mutex(dbenv, &mb, &bhp->mutex, ", ", flags);
	__db_msgadd(dbenv, &mb, "%#08lx, %lu",
	    (u_long)R_OFFSET(dbmp->reginfo, bhp), (u_long)bhp->priority);
	__db_prflags(dbenv, &mb, bhp->flags, fn, " (", ")");
	DB_MSGBUF_FLUSH(dbenv, &mb);
}

/*
 * __memp_stat_wait --
 *	Total hash bucket wait stats into the region.
 */
static void
__memp_stat_wait(reginfo, mp, mstat, flags)
	REGINFO *reginfo;
	MPOOL *mp;
	DB_MPOOL_STAT *mstat;
	u_int32_t flags;
{
	DB_MPOOL_HASH *hp;
	DB_MUTEX *mutexp;
	u_int32_t i;

	mstat->st_hash_max_wait = 0;
	hp = R_ADDR(reginfo, mp->htab);
	for (i = 0; i < mp->htab_buckets; i++, hp++) {
		mutexp = &hp->hash_mutex;
		mstat->st_hash_nowait += mutexp->mutex_set_nowait;
		mstat->st_hash_wait += mutexp->mutex_set_wait;
		if (mutexp->mutex_set_wait > mstat->st_hash_max_wait)
			mstat->st_hash_max_wait = mutexp->mutex_set_wait;

		if (LF_ISSET(DB_STAT_CLEAR))
			MUTEX_CLEAR(mutexp);
	}
}

#else /* !HAVE_STATISTICS */

int
__memp_stat_pp(dbenv, gspp, fspp, flags)
	DB_ENV *dbenv;
	DB_MPOOL_STAT **gspp;
	DB_MPOOL_FSTAT ***fspp;
	u_int32_t flags;
{
	COMPQUIET(gspp, NULL);
	COMPQUIET(fspp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__memp_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif

/*
 * __memp_stat_hash --
 *	Total hash bucket stats (other than mutex wait) into the region.
 *
 * PUBLIC: void __memp_stat_hash __P((REGINFO *, MPOOL *, u_int32_t *));
 */
void
__memp_stat_hash(reginfo, mp, dirtyp)
	REGINFO *reginfo;
	MPOOL *mp;
	u_int32_t *dirtyp;
{
	DB_MPOOL_HASH *hp;
	u_int32_t dirty, i;

	hp = R_ADDR(reginfo, mp->htab);
	for (i = 0, dirty = 0; i < mp->htab_buckets; i++, hp++)
		dirty += hp->hash_page_dirty;
	*dirtyp = dirty;
}
