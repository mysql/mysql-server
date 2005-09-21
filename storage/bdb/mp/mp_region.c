/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_region.c,v 11.68 2004/10/15 16:59:43 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

static int	__memp_init __P((DB_ENV *, DB_MPOOL *, u_int, u_int32_t));
static void	__memp_init_config __P((DB_ENV *, MPOOL *));
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
static size_t	__memp_region_maint __P((REGINFO *));
#endif

/*
 * __memp_open --
 *	Internal version of memp_open: only called from DB_ENV->open.
 *
 * PUBLIC: int __memp_open __P((DB_ENV *));
 */
int
__memp_open(dbenv)
	DB_ENV *dbenv;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	REGINFO reginfo;
	roff_t reg_size;
	u_int i;
	u_int32_t htab_buckets, *regids;
	int ret;

	/* Figure out how big each cache region is. */
	reg_size = (dbenv->mp_gbytes / dbenv->mp_ncache) * GIGABYTE;
	reg_size += ((dbenv->mp_gbytes %
	    dbenv->mp_ncache) * GIGABYTE) / dbenv->mp_ncache;
	reg_size += dbenv->mp_bytes / dbenv->mp_ncache;

	/*
	 * Figure out how many hash buckets each region will have.  Assume we
	 * want to keep the hash chains with under 10 pages on each chain.  We
	 * don't know the pagesize in advance, and it may differ for different
	 * files.  Use a pagesize of 1K for the calculation -- we walk these
	 * chains a lot, they must be kept short.
	 */
	htab_buckets = __db_tablesize((u_int32_t)(reg_size / (1 * 1024)) / 10);

	/* Create and initialize the DB_MPOOL structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(*dbmp), &dbmp)) != 0)
		return (ret);
	LIST_INIT(&dbmp->dbregq);
	TAILQ_INIT(&dbmp->dbmfq);
	dbmp->dbenv = dbenv;

	/* Join/create the first mpool region. */
	memset(&reginfo, 0, sizeof(REGINFO));
	reginfo.dbenv = dbenv;
	reginfo.type = REGION_TYPE_MPOOL;
	reginfo.id = INVALID_REGION_ID;
	reginfo.flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(&reginfo, REGION_CREATE_OK);
	if ((ret = __db_r_attach(dbenv, &reginfo, reg_size)) != 0)
		goto err;

	/*
	 * If we created the region, initialize it.  Create or join any
	 * additional regions.
	 */
	if (F_ISSET(&reginfo, REGION_CREATE)) {
		/*
		 * We define how many regions there are going to be, allocate
		 * the REGINFO structures and create them.  Make sure we don't
		 * clear the wrong entries on error.
		 */
		dbmp->nreg = dbenv->mp_ncache;
		if ((ret = __os_calloc(dbenv,
		    dbmp->nreg, sizeof(REGINFO), &dbmp->reginfo)) != 0)
			goto err;
		/* Make sure we don't clear the wrong entries on error. */
		for (i = 0; i < dbmp->nreg; ++i)
			dbmp->reginfo[i].id = INVALID_REGION_ID;
		dbmp->reginfo[0] = reginfo;

		/* Initialize the first region. */
		if ((ret = __memp_init(dbenv, dbmp, 0, htab_buckets)) != 0)
			goto err;

		/*
		 * Create/initialize remaining regions and copy their IDs into
		 * the first region.
		 */
		mp = R_ADDR(dbmp->reginfo, dbmp->reginfo[0].rp->primary);
		regids = R_ADDR(dbmp->reginfo, mp->regids);
		for (i = 1; i < dbmp->nreg; ++i) {
			dbmp->reginfo[i].dbenv = dbenv;
			dbmp->reginfo[i].type = REGION_TYPE_MPOOL;
			dbmp->reginfo[i].id = INVALID_REGION_ID;
			dbmp->reginfo[i].flags = REGION_CREATE_OK;
			if ((ret = __db_r_attach(
			    dbenv, &dbmp->reginfo[i], reg_size)) != 0)
				goto err;
			if ((ret =
			    __memp_init(dbenv, dbmp, i, htab_buckets)) != 0)
				goto err;
			R_UNLOCK(dbenv, &dbmp->reginfo[i]);

			regids[i] = dbmp->reginfo[i].id;
		}

		__memp_init_config(dbenv, mp);

		R_UNLOCK(dbenv, dbmp->reginfo);
	} else {
		/*
		 * Determine how many regions there are going to be, allocate
		 * the REGINFO structures and fill in local copies of that
		 * information.
		 */
		mp = R_ADDR(&reginfo, reginfo.rp->primary);
		dbmp->nreg = mp->nreg;
		if ((ret = __os_calloc(dbenv,
		    dbmp->nreg, sizeof(REGINFO), &dbmp->reginfo)) != 0)
			goto err;
		/* Make sure we don't clear the wrong entries on error. */
		for (i = 0; i < dbmp->nreg; ++i)
			dbmp->reginfo[i].id = INVALID_REGION_ID;
		dbmp->reginfo[0] = reginfo;

		__memp_init_config(dbenv, mp);

		/*
		 * We have to unlock the primary mpool region before we attempt
		 * to join the additional mpool regions.  If we don't, we can
		 * deadlock.  The scenario is that we hold the primary mpool
		 * region lock.  We then try to attach to an additional mpool
		 * region, which requires the acquisition/release of the main
		 * region lock (to search the list of regions).  If another
		 * thread of control already holds the main region lock and is
		 * waiting on our primary mpool region lock, we'll deadlock.
		 * See [#4696] for more information.
		 */
		R_UNLOCK(dbenv, dbmp->reginfo);

		/* Join remaining regions. */
		regids = R_ADDR(dbmp->reginfo, mp->regids);
		for (i = 1; i < dbmp->nreg; ++i) {
			dbmp->reginfo[i].dbenv = dbenv;
			dbmp->reginfo[i].type = REGION_TYPE_MPOOL;
			dbmp->reginfo[i].id = regids[i];
			dbmp->reginfo[i].flags = REGION_JOIN_OK;
			if ((ret = __db_r_attach(
			    dbenv, &dbmp->reginfo[i], 0)) != 0)
				goto err;
			R_UNLOCK(dbenv, &dbmp->reginfo[i]);
		}
	}

	/* Set the local addresses for the regions. */
	for (i = 0; i < dbmp->nreg; ++i)
		dbmp->reginfo[i].primary =
		    R_ADDR(&dbmp->reginfo[i], dbmp->reginfo[i].rp->primary);

	/* If the region is threaded, allocate a mutex to lock the handles. */
	if (F_ISSET(dbenv, DB_ENV_THREAD) &&
	    (ret = __db_mutex_setup(dbenv, dbmp->reginfo, &dbmp->mutexp,
	    MUTEX_ALLOC | MUTEX_THREAD)) != 0)
		goto err;

	dbenv->mp_handle = dbmp;
	return (0);

err:	if (dbmp->reginfo != NULL && dbmp->reginfo[0].addr != NULL) {
		if (F_ISSET(dbmp->reginfo, REGION_CREATE))
			ret = __db_panic(dbenv, ret);

		R_UNLOCK(dbenv, dbmp->reginfo);

		for (i = 0; i < dbmp->nreg; ++i)
			if (dbmp->reginfo[i].id != INVALID_REGION_ID)
				(void)__db_r_detach(
				    dbenv, &dbmp->reginfo[i], 0);
		__os_free(dbenv, dbmp->reginfo);
	}
	if (dbmp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmp->mutexp);
	__os_free(dbenv, dbmp);
	return (ret);
}

/*
 * __memp_init --
 *	Initialize a MPOOL structure in shared memory.
 */
static int
__memp_init(dbenv, dbmp, reginfo_off, htab_buckets)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	u_int reginfo_off;
	u_int32_t htab_buckets;
{
	DB_MPOOL_HASH *htab;
	MPOOL *mp;
	REGINFO *reginfo;
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	size_t maint_size;
#endif
	u_int32_t i;
	int ret;
	void *p;

	reginfo = &dbmp->reginfo[reginfo_off];
	if ((ret = __db_shalloc(reginfo,
	    sizeof(MPOOL), MUTEX_ALIGN, &reginfo->primary)) != 0)
		goto mem_err;
	reginfo->rp->primary = R_OFFSET(reginfo, reginfo->primary);
	mp = reginfo->primary;
	memset(mp, 0, sizeof(*mp));

#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
	maint_size = __memp_region_maint(reginfo);
	/* Allocate room for the maintenance info and initialize it. */
	if ((ret = __db_shalloc(reginfo,
	    sizeof(REGMAINT) + maint_size, 0, &p)) != 0)
		goto mem_err;
	__db_maintinit(reginfo, p, maint_size);
	mp->maint_off = R_OFFSET(reginfo, p);
#endif

	if (reginfo_off == 0) {
		SH_TAILQ_INIT(&mp->mpfq);

		ZERO_LSN(mp->lsn);

		mp->nreg = dbmp->nreg;
		if ((ret = __db_shalloc(&dbmp->reginfo[0],
		    dbmp->nreg * sizeof(u_int32_t), 0, &p)) != 0)
			goto mem_err;
		mp->regids = R_OFFSET(dbmp->reginfo, p);
	}

	/* Allocate hash table space and initialize it. */
	if ((ret = __db_shalloc(reginfo,
	    htab_buckets * sizeof(DB_MPOOL_HASH), MUTEX_ALIGN, &htab)) != 0)
		goto mem_err;
	mp->htab = R_OFFSET(reginfo, htab);
	for (i = 0; i < htab_buckets; i++) {
		if ((ret = __db_mutex_setup(dbenv,
		    reginfo, &htab[i].hash_mutex, MUTEX_NO_RLOCK)) != 0)
			return (ret);
		SH_TAILQ_INIT(&htab[i].hash_bucket);
		htab[i].hash_page_dirty = htab[i].hash_priority = 0;
	}
	mp->htab_buckets = mp->stat.st_hash_buckets = htab_buckets;

	/*
	 * Only the environment creator knows the total cache size, fill in
	 * those statistics now.
	 */
	mp->stat.st_gbytes = dbenv->mp_gbytes;
	mp->stat.st_bytes = dbenv->mp_bytes;
	return (0);

mem_err:__db_err(dbenv, "Unable to allocate memory for mpool region");
	return (ret);
}

/*
 * __memp_init_config --
 *	Initialize shared configuration information.
 */
static void
__memp_init_config(dbenv, mp)
	DB_ENV *dbenv;
	MPOOL *mp;
{
	/* A process joining the region may reset the mpool configuration. */
	if (dbenv->mp_mmapsize != 0)
		mp->mp_mmapsize = dbenv->mp_mmapsize;
	if (dbenv->mp_maxopenfd != 0)
		mp->mp_maxopenfd = dbenv->mp_maxopenfd;
	if (dbenv->mp_maxwrite != 0)
		mp->mp_maxwrite = dbenv->mp_maxwrite;
	if (dbenv->mp_maxwrite_sleep != 0)
		mp->mp_maxwrite_sleep = dbenv->mp_maxwrite_sleep;
}

/*
 * __memp_dbenv_refresh --
 *	Clean up after the mpool system on a close or failed open.
 *
 * PUBLIC: int __memp_dbenv_refresh __P((DB_ENV *));
 */
int
__memp_dbenv_refresh(dbenv)
	DB_ENV *dbenv;
{
	BH *bhp;
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	DB_MPOOL_HASH *hp;
	DB_MPREG *mpreg;
	MPOOL *mp;
	REGINFO *reginfo;
	u_int32_t bucket, i;
	int ret, t_ret;

	ret = 0;
	dbmp = dbenv->mp_handle;

	/*
	 * If a private region, return the memory to the heap.  Not needed for
	 * filesystem-backed or system shared memory regions, that memory isn't
	 * owned by any particular process.
	 *
	 * Discard buffers.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		for (i = 0; i < dbmp->nreg; ++i) {
			reginfo = &dbmp->reginfo[i];
			mp = reginfo->primary;
			for (hp = R_ADDR(reginfo, mp->htab), bucket = 0;
			    bucket < mp->htab_buckets; ++hp, ++bucket)
				while ((bhp = SH_TAILQ_FIRST(
				    &hp->hash_bucket, __bh)) != NULL)
					__memp_bhfree(dbmp, hp, bhp,
					    BH_FREE_FREEMEM | BH_FREE_UNLOCKED);
		}

	/* Discard DB_MPOOLFILEs. */
	while ((dbmfp = TAILQ_FIRST(&dbmp->dbmfq)) != NULL)
		if ((t_ret = __memp_fclose(dbmfp, 0)) != 0 && ret == 0)
			ret = t_ret;

	/* Discard DB_MPREGs. */
	while ((mpreg = LIST_FIRST(&dbmp->dbregq)) != NULL) {
		LIST_REMOVE(mpreg, q);
		__os_free(dbenv, mpreg);
	}

	/* Discard the DB_MPOOL thread mutex. */
	if (dbmp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmp->mutexp);

	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		/* Discard REGION IDs. */
		reginfo = &dbmp->reginfo[0];
		mp = dbmp->reginfo[0].primary;
		__db_shalloc_free(reginfo, R_ADDR(reginfo, mp->regids));

		/* Discard Hash tables. */
		for (i = 0; i < dbmp->nreg; ++i) {
			reginfo = &dbmp->reginfo[i];
			mp = reginfo->primary;
			__db_shalloc_free(reginfo, R_ADDR(reginfo, mp->htab));
		}
	}

	/* Detach from the region. */
	for (i = 0; i < dbmp->nreg; ++i) {
		reginfo = &dbmp->reginfo[i];
		if ((t_ret = __db_r_detach(dbenv, reginfo, 0)) != 0 && ret == 0)
			ret = t_ret;
	}

	/* Discard DB_MPOOL. */
	__os_free(dbenv, dbmp->reginfo);
	__os_free(dbenv, dbmp);

	dbenv->mp_handle = NULL;
	return (ret);
}

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
/*
 * __memp_region_maint --
 *	Return the amount of space needed for region maintenance info.
 *
 */
static size_t
__memp_region_maint(infop)
	REGINFO *infop;
{
	size_t s;
	int numlocks;

	/*
	 * For mutex maintenance we need one mutex per possible page.
	 * Compute the maximum number of pages this cache can have.
	 * Also add in an mpool mutex and mutexes for all dbenv and db
	 * handles.
	 */
	numlocks = ((infop->rp->size / DB_MIN_PGSIZE) + 1);
	numlocks += DB_MAX_HANDLES;
	s = sizeof(roff_t) * numlocks;
	return (s);
}
#endif

/*
 * __memp_region_destroy
 *	Destroy any region maintenance info.
 *
 * PUBLIC: void __memp_region_destroy __P((DB_ENV *, REGINFO *));
 */
void
__memp_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	/*
	 * This routine is called in two cases: when discarding the mutexes
	 * from a previous Berkeley DB run, during recovery, and two, when
	 * discarding the mutexes as we shut down the database environment.
	 * In the latter case, we also need to discard shared memory segments,
	 * this is the last time we use them, and the last region-specific
	 * call we make.
	 */
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	MPOOL *mp;

	mp = R_ADDR(infop, infop->rp->primary);

	/* Destroy mutexes. */
	__db_shlocks_destroy(infop, R_ADDR(infop, mp->maint_off));
	if (infop->primary != NULL && F_ISSET(dbenv, DB_ENV_PRIVATE))
		__db_shalloc_free(infop, R_ADDR(infop, mp->maint_off));
#endif
	if (infop->primary != NULL && F_ISSET(dbenv, DB_ENV_PRIVATE))
		__db_shalloc_free(infop, infop->primary);
}
