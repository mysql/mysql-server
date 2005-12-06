/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2005
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: mp_region.c,v 12.7 2005/08/08 14:30:03 bostic Exp $
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
static int	__memp_init_config __P((DB_ENV *, MPOOL *));
static void	__memp_region_size __P((DB_ENV *, roff_t *, u_int32_t *));

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

	/* Calculate the region size and hash bucket count. */
	__memp_region_size(dbenv, &reg_size, &htab_buckets);

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

			regids[i] = dbmp->reginfo[i].id;
		}
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
		}
	}

	/* Set the local addresses for the regions. */
	for (i = 0; i < dbmp->nreg; ++i)
		dbmp->reginfo[i].primary =
		    R_ADDR(&dbmp->reginfo[i], dbmp->reginfo[i].rp->primary);

	/* If the region is threaded, allocate a mutex to lock the handles. */
	if ((ret = __mutex_alloc(
	    dbenv, MTX_MPOOL_HANDLE, DB_MUTEX_THREAD, &dbmp->mutex)) != 0)
		goto err;

	dbenv->mp_handle = dbmp;

	/* A process joining the region may reset the mpool configuration. */
	if ((ret = __memp_init_config(dbenv, mp)) != 0)
		return (ret);

	return (0);

err:	dbenv->mp_handle = NULL;
	if (dbmp->reginfo != NULL && dbmp->reginfo[0].addr != NULL) {
		for (i = 0; i < dbmp->nreg; ++i)
			if (dbmp->reginfo[i].id != INVALID_REGION_ID)
				(void)__db_r_detach(
				    dbenv, &dbmp->reginfo[i], 0);
		__os_free(dbenv, dbmp->reginfo);
	}

	(void)__mutex_free(dbenv, &dbmp->mutex);
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
	u_int32_t i;
	int ret;
	void *p;

	reginfo = &dbmp->reginfo[reginfo_off];
	if ((ret = __db_shalloc(
	    reginfo, sizeof(MPOOL), 0, &reginfo->primary)) != 0)
		goto mem_err;
	reginfo->rp->primary = R_OFFSET(reginfo, reginfo->primary);
	mp = reginfo->primary;
	memset(mp, 0, sizeof(*mp));

	if ((ret =
	    __mutex_alloc(dbenv, MTX_MPOOL_REGION, 0, &mp->mtx_region)) != 0)
		return (ret);

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
	    htab_buckets * sizeof(DB_MPOOL_HASH), 0, &htab)) != 0)
		goto mem_err;
	mp->htab = R_OFFSET(reginfo, htab);
	for (i = 0; i < htab_buckets; i++) {
		if ((ret = __mutex_alloc(
		    dbenv, MTX_MPOOL_HASH_BUCKET, 0, &htab[i].mtx_hash)) != 0)
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
 * __memp_region_size --
 *	Size the region and figure out how many hash buckets we'll have.
 */
static void
__memp_region_size(dbenv, reg_sizep, htab_bucketsp)
	DB_ENV *dbenv;
	roff_t *reg_sizep;
	u_int32_t *htab_bucketsp;
{
	roff_t reg_size;

	/* Figure out how big each cache region is. */
	reg_size = (roff_t)(dbenv->mp_gbytes / dbenv->mp_ncache) * GIGABYTE;
	reg_size += ((roff_t)(dbenv->mp_gbytes %
	    dbenv->mp_ncache) * GIGABYTE) / dbenv->mp_ncache;
	reg_size += dbenv->mp_bytes / dbenv->mp_ncache;
	*reg_sizep = reg_size;

	/*
	 * Figure out how many hash buckets each region will have.  Assume we
	 * want to keep the hash chains with under 10 pages on each chain.  We
	 * don't know the pagesize in advance, and it may differ for different
	 * files.  Use a pagesize of 1K for the calculation -- we walk these
	 * chains a lot, they must be kept short.
	 *
	 * XXX
	 * Cache sizes larger than 10TB would cause 32-bit wrapping in the
	 * calculation of the number of hash buckets.  This probably isn't
	 * something we need to worry about right now, but is checked when the
	 * cache size is set.
	 */
	*htab_bucketsp = __db_tablesize((u_int32_t)(reg_size / (10 * 1024)));
}

/*
 * __memp_region_mutex_count --
 *	Return the number of mutexes the mpool region will need.
 *
 * PUBLIC: u_int32_t __memp_region_mutex_count __P((DB_ENV *));
 */
u_int32_t
__memp_region_mutex_count(dbenv)
	DB_ENV *dbenv;
{
	roff_t reg_size;
	u_int32_t htab_buckets;

	__memp_region_size(dbenv, &reg_size, &htab_buckets);

	/*
	 * We need a couple of mutexes for the region itself, and one for each
	 * file handle (MPOOLFILE).  More importantly, each configured cache
	 * has one mutex per hash bucket and buffer header.  Hash buckets are
	 * configured to have 10 pages or fewer on each chain, but we don't
	 * want to fail if we have a large number of 512 byte pages, so double
	 * the guess.
	 */
	return (dbenv->mp_ncache * htab_buckets * 21 + 50);
}

/*
 * __memp_init_config --
 *	Initialize shared configuration information.
 */
static int
__memp_init_config(dbenv, mp)
	DB_ENV *dbenv;
	MPOOL *mp;
{
	MPOOL_SYSTEM_LOCK(dbenv);

	if (dbenv->mp_mmapsize != 0)
		mp->mp_mmapsize = dbenv->mp_mmapsize;
	if (dbenv->mp_maxopenfd != 0)
		mp->mp_maxopenfd = dbenv->mp_maxopenfd;
	if (dbenv->mp_maxwrite != 0)
		mp->mp_maxwrite = dbenv->mp_maxwrite;
	if (dbenv->mp_maxwrite_sleep != 0)
		mp->mp_maxwrite_sleep = dbenv->mp_maxwrite_sleep;

	MPOOL_SYSTEM_UNLOCK(dbenv);

	return (0);
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
					if ((t_ret = __memp_bhfree(
					    dbmp, hp, bhp,
					    BH_FREE_FREEMEM |
					    BH_FREE_UNLOCKED)) != 0 && ret == 0)
						ret = t_ret;
		}

	/* Discard DB_MPOOLFILEs. */
	while ((dbmfp = TAILQ_FIRST(&dbmp->dbmfq)) != NULL)
		if ((t_ret = __memp_fclose(dbmfp, 0)) != 0 && ret == 0)
			ret = t_ret;

	/* Discard DB_MPREGs. */
	if (dbmp->pg_inout != NULL)
		__os_free(dbenv, dbmp->pg_inout);
	while ((mpreg = LIST_FIRST(&dbmp->dbregq)) != NULL) {
		LIST_REMOVE(mpreg, q);
		__os_free(dbenv, mpreg);
	}

	/* Discard the DB_MPOOL thread mutex. */
	if ((t_ret = __mutex_free(dbenv, &dbmp->mutex)) != 0 && ret == 0)
		ret = t_ret;

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
