/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_stat.c,v 11.51 2002/08/06 06:13:47 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_shash.h"
#include "dbinc/db_am.h"
#include "dbinc/mp.h"

static void __memp_dumpcache __P((DB_ENV *,
		DB_MPOOL *, REGINFO *, size_t *, FILE *, u_int32_t));
static void __memp_pbh __P((DB_MPOOL *, BH *, size_t *, FILE *));
static void __memp_stat_wait __P((REGINFO *, MPOOL *, DB_MPOOL_STAT *, int));

/*
 * __memp_stat --
 *	Display MPOOL statistics.
 *
 * PUBLIC: int __memp_stat
 * PUBLIC:     __P((DB_ENV *, DB_MPOOL_STAT **, DB_MPOOL_FSTAT ***, u_int32_t));
 */
int
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
	size_t len, nlen, pagesize;
	u_int32_t pages, i;
	int ret;
	char *name, *tname;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_stat", DB_INIT_MPOOL);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->memp_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

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
		 * a per-cache basis.
		 */
		c_mp = dbmp->reginfo[0].primary;
		sp->st_gbytes = c_mp->stat.st_gbytes;
		sp->st_bytes = c_mp->stat.st_bytes;
		sp->st_ncache = dbmp->nreg;
		sp->st_regsize = dbmp->reginfo[0].rp->size;

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
				dbmp->reginfo[i].rp->mutex.mutex_set_wait = 0;
				dbmp->reginfo[i].rp->mutex.mutex_set_nowait = 0;
				pages = c_mp->stat.st_pages;
				memset(&c_mp->stat, 0, sizeof(c_mp->stat));
				c_mp->stat.st_hash_buckets = c_mp->htab_buckets;
				c_mp->stat.st_pages = pages;
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

#define	FMAP_ENTRIES	200			/* Files we map. */

#define	MPOOL_DUMP_HASH	0x01			/* Debug hash chains. */
#define	MPOOL_DUMP_MEM	0x04			/* Debug region memory. */
#define	MPOOL_DUMP_ALL	0x07			/* Debug all. */

/*
 * __memp_dump_region --
 *	Display MPOOL structures.
 *
 * PUBLIC: int __memp_dump_region __P((DB_ENV *, char *, FILE *));
 */
int
__memp_dump_region(dbenv, area, fp)
	DB_ENV *dbenv;
	char *area;
	FILE *fp;
{
	static const FN fn[] = {
		{ MP_CAN_MMAP,	"mmapped" },
		{ MP_DEADFILE,	"dead" },
		{ MP_DIRECT,	"no buffer" },
		{ MP_EXTENT,	"extent" },
		{ MP_TEMP,	"temporary" },
		{ MP_UNLINK,	"unlink" },
		{ 0,		NULL }
	};
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	size_t fmap[FMAP_ENTRIES + 1];
	u_int32_t i, flags;
	int cnt;
	u_int8_t *p;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->mp_handle, "memp_dump_region", DB_INIT_MPOOL);

	dbmp = dbenv->mp_handle;

	/* Make it easy to call from the debugger. */
	if (fp == NULL)
		fp = stderr;

	for (flags = 0; *area != '\0'; ++area)
		switch (*area) {
		case 'A':
			LF_SET(MPOOL_DUMP_ALL);
			break;
		case 'h':
			LF_SET(MPOOL_DUMP_HASH);
			break;
		case 'm':
			LF_SET(MPOOL_DUMP_MEM);
			break;
		}

	mp = dbmp->reginfo[0].primary;

	/* Display MPOOL structures. */
	(void)fprintf(fp, "%s\nPool (region addr 0x%lx)\n",
	    DB_LINE, P_TO_ULONG(dbmp->reginfo[0].addr));

	/* Display the MPOOLFILE structures. */
	R_LOCK(dbenv, dbmp->reginfo);
	for (cnt = 0, mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile), ++cnt) {
		(void)fprintf(fp, "File #%d: %s: pagesize %lu\n", cnt + 1,
		    __memp_fns(dbmp, mfp), (u_long)mfp->stat.st_pagesize);
		(void)fprintf(fp, "\t type %ld; ref %lu; blocks %lu; last %lu;",
		    (long)mfp->ftype, (u_long)mfp->mpf_cnt,
		    (u_long)mfp->block_cnt, (u_long)mfp->last_pgno);
		__db_prflags(mfp->flags, fn, fp);

		(void)fprintf(fp, "\n\t UID: ");
		p = R_ADDR(dbmp->reginfo, mfp->fileid_off);
		for (i = 0; i < DB_FILE_ID_LEN; ++i, ++p) {
			(void)fprintf(fp, "%x", (u_int)*p);
			if (i < DB_FILE_ID_LEN - 1)
				(void)fprintf(fp, " ");
		}
		(void)fprintf(fp, "\n");
		if (cnt < FMAP_ENTRIES)
			fmap[cnt] = R_OFFSET(dbmp->reginfo, mfp);
	}
	R_UNLOCK(dbenv, dbmp->reginfo);

	MUTEX_THREAD_LOCK(dbenv, dbmp->mutexp);
	for (dbmfp = TAILQ_FIRST(&dbmp->dbmfq);
	    dbmfp != NULL; dbmfp = TAILQ_NEXT(dbmfp, q), ++cnt) {
		(void)fprintf(fp, "File #%d: %s: per-process, %s\n",
		    cnt + 1, __memp_fn(dbmfp),
		    F_ISSET(dbmfp, MP_READONLY) ? "readonly" : "read/write");
		    if (cnt < FMAP_ENTRIES)
			fmap[cnt] = R_OFFSET(dbmp->reginfo, mfp);
	}
	MUTEX_THREAD_UNLOCK(dbenv, dbmp->mutexp);
	if (cnt < FMAP_ENTRIES)
		fmap[cnt] = INVALID_ROFF;
	else
		fmap[FMAP_ENTRIES] = INVALID_ROFF;

	/* Dump the memory pools. */
	for (i = 0; i < mp->nreg; ++i) {
		(void)fprintf(fp, "%s\nCache #%d:\n", DB_LINE, i + 1);
		__memp_dumpcache(
		    dbenv, dbmp, &dbmp->reginfo[i], fmap, fp, flags);
	}

	/* Flush in case we're debugging. */
	(void)fflush(fp);

	return (0);
}

/*
 * __memp_dumpcache --
 *	Display statistics for a cache.
 */
static void
__memp_dumpcache(dbenv, dbmp, reginfo, fmap, fp, flags)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	REGINFO *reginfo;
	size_t *fmap;
	FILE *fp;
	u_int32_t flags;
{
	BH *bhp;
	DB_MPOOL_HASH *hp;
	MPOOL *c_mp;
	int bucket;

	c_mp = reginfo->primary;

	/* Display the hash table list of BH's. */
	if (LF_ISSET(MPOOL_DUMP_HASH)) {
		(void)fprintf(fp,
		    "%s\nBH hash table (%lu hash slots)\nbucket (priority):\n",
		    DB_LINE, (u_long)c_mp->htab_buckets);
		(void)fprintf(fp,
		    "\tpageno, file, ref, address [LSN] priority\n");

		for (hp = R_ADDR(reginfo, c_mp->htab),
		    bucket = 0; bucket < c_mp->htab_buckets; ++hp, ++bucket) {
			MUTEX_LOCK(dbenv, &hp->hash_mutex);
			if ((bhp =
			    SH_TAILQ_FIRST(&hp->hash_bucket, __bh)) != NULL)
				(void)fprintf(fp, "%lu (%u):\n",
				    (u_long)bucket, hp->hash_priority);
			for (; bhp != NULL; bhp = SH_TAILQ_NEXT(bhp, hq, __bh))
				__memp_pbh(dbmp, bhp, fmap, fp);
			MUTEX_UNLOCK(dbenv, &hp->hash_mutex);
		}
	}

	/* Dump the memory pool. */
	if (LF_ISSET(MPOOL_DUMP_MEM))
		__db_shalloc_dump(reginfo->addr, fp);
}

/*
 * __memp_pbh --
 *	Display a BH structure.
 */
static void
__memp_pbh(dbmp, bhp, fmap, fp)
	DB_MPOOL *dbmp;
	BH *bhp;
	size_t *fmap;
	FILE *fp;
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
	int i;

	for (i = 0; i < FMAP_ENTRIES; ++i)
		if (fmap[i] == INVALID_ROFF || fmap[i] == bhp->mf_offset)
			break;

	if (fmap[i] == INVALID_ROFF)
		(void)fprintf(fp, "\t%5lu, %lu, %2lu, %8lu [%lu,%lu] %lu",
		    (u_long)bhp->pgno, (u_long)bhp->mf_offset,
		    (u_long)bhp->ref, (u_long)R_OFFSET(dbmp->reginfo, bhp),
		    (u_long)LSN(bhp->buf).file, (u_long)LSN(bhp->buf).offset,
		    (u_long)bhp->priority);
	else
		(void)fprintf(fp, "\t%5lu,   #%d,  %2lu, %8lu [%lu,%lu] %lu",
		    (u_long)bhp->pgno, i + 1,
		    (u_long)bhp->ref, (u_long)R_OFFSET(dbmp->reginfo, bhp),
		    (u_long)LSN(bhp->buf).file, (u_long)LSN(bhp->buf).offset,
		    (u_long)bhp->priority);

	__db_prflags(bhp->flags, fn, fp);

	(void)fprintf(fp, "\n");
}

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
	u_int32_t dirty;
	int i;

	hp = R_ADDR(reginfo, mp->htab);
	for (i = 0, dirty = 0; i < mp->htab_buckets; i++, hp++)
		dirty += hp->hash_page_dirty;
	*dirtyp = dirty;
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
	int flags;
{
	DB_MPOOL_HASH *hp;
	DB_MUTEX *mutexp;
	int i;

	mstat->st_hash_max_wait = 0;
	hp = R_ADDR(reginfo, mp->htab);
	for (i = 0; i < mp->htab_buckets; i++, hp++) {
		mutexp = &hp->hash_mutex;
		mstat->st_hash_nowait += mutexp->mutex_set_nowait;
		mstat->st_hash_wait += mutexp->mutex_set_wait;
		if (mutexp->mutex_set_wait > mstat->st_hash_max_wait)
			mstat->st_hash_max_wait = mutexp->mutex_set_wait;

		if (LF_ISSET(DB_STAT_CLEAR)) {
			mutexp->mutex_set_wait = 0;
			mutexp->mutex_set_nowait = 0;
		}
	}
}
