/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_region.c,v 11.26 2000/11/30 00:58:41 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

static int __mpool_init __P((DB_ENV *, DB_MPOOL *, int, int));
#ifdef MUTEX_SYSTEM_RESOURCES
static size_t __mpool_region_maint __P((REGINFO *));
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
	roff_t reg_size, *regids;
	u_int32_t i;
	int htab_buckets, ret;

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
	htab_buckets = __db_tablesize((reg_size / (1 * 1024)) / 10);

	/* Create and initialize the DB_MPOOL structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(*dbmp), &dbmp)) != 0)
		return (ret);
	LIST_INIT(&dbmp->dbregq);
	TAILQ_INIT(&dbmp->dbmfq);
	dbmp->dbenv = dbenv;

	/* Join/create the first mpool region. */
	memset(&reginfo, 0, sizeof(REGINFO));
	reginfo.type = REGION_TYPE_MPOOL;
	reginfo.id = INVALID_REGION_ID;
	reginfo.mode = dbenv->db_mode;
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
		if ((ret = __mpool_init(dbenv, dbmp, 0, htab_buckets)) != 0)
			goto err;

		/*
		 * Create/initialize remaining regions and copy their IDs into
		 * the first region.
		 */
		mp = R_ADDR(dbmp->reginfo, dbmp->reginfo[0].rp->primary);
		regids = R_ADDR(dbmp->reginfo, mp->regids);
		for (i = 1; i < dbmp->nreg; ++i) {
			dbmp->reginfo[i].type = REGION_TYPE_MPOOL;
			dbmp->reginfo[i].id = INVALID_REGION_ID;
			dbmp->reginfo[i].mode = dbenv->db_mode;
			dbmp->reginfo[i].flags = REGION_CREATE_OK;
			if ((ret = __db_r_attach(
			    dbenv, &dbmp->reginfo[i], reg_size)) != 0)
				goto err;
			if ((ret =
			    __mpool_init(dbenv, dbmp, i, htab_buckets)) != 0)
				goto err;
			R_UNLOCK(dbenv, &dbmp->reginfo[i]);

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
			dbmp->reginfo[i].type = REGION_TYPE_MPOOL;
			dbmp->reginfo[i].id = regids[i];
			dbmp->reginfo[i].mode = 0;
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
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		if ((ret = __db_mutex_alloc(
		    dbenv, dbmp->reginfo, &dbmp->mutexp)) != 0) {
			goto err;
		}
		if ((ret =
		    __db_mutex_init(dbenv, dbmp->mutexp, 0, MUTEX_THREAD)) != 0)
			goto err;
	}

	R_UNLOCK(dbenv, dbmp->reginfo);

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
		__os_free(dbmp->reginfo,
		    dbmp->nreg * sizeof(*dbmp->reginfo));
	}
	if (dbmp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmp->mutexp);
	__os_free(dbmp, sizeof(*dbmp));
	return (ret);
}

/*
 * __mpool_init --
 *	Initialize a MPOOL structure in shared memory.
 */
static int
__mpool_init(dbenv, dbmp, reginfo_off, htab_buckets)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int reginfo_off, htab_buckets;
{
	DB_HASHTAB *htab;
	MPOOL *mp;
	REGINFO *reginfo;
#ifdef MUTEX_SYSTEM_RESOURCES
	size_t maint_size;
#endif
	int ret;
	void *p;

	mp = NULL;

	reginfo = &dbmp->reginfo[reginfo_off];
	if ((ret = __db_shalloc(reginfo->addr,
	    sizeof(MPOOL), MUTEX_ALIGN, &reginfo->primary)) != 0)
		goto mem_err;
	reginfo->rp->primary = R_OFFSET(reginfo, reginfo->primary);
	mp = reginfo->primary;
	memset(mp, 0, sizeof(*mp));

#ifdef	MUTEX_SYSTEM_RESOURCES
	maint_size = __mpool_region_maint(reginfo);
	/* Allocate room for the maintenance info and initialize it. */
	if ((ret = __db_shalloc(reginfo->addr,
	    sizeof(REGMAINT) + maint_size, 0, &p)) != 0)
		goto mem_err;
	__db_maintinit(reginfo, p, maint_size);
	mp->maint_off = R_OFFSET(reginfo, p);
#endif

	if (reginfo_off == 0) {
		SH_TAILQ_INIT(&mp->mpfq);

		if ((ret = __db_shmutex_init(dbenv, &mp->sync_mutex,
		    R_OFFSET(dbmp->reginfo, &mp->sync_mutex) +
		    DB_FCNTL_OFF_MPOOL, 0, dbmp->reginfo,
		    (REGMAINT *)R_ADDR(dbmp->reginfo, mp->maint_off))) != 0)
			goto err;

		ZERO_LSN(mp->lsn);
		mp->lsn_cnt = 0;

		mp->nreg = dbmp->nreg;
		if ((ret = __db_shalloc(dbmp->reginfo[0].addr,
		    dbmp->nreg * sizeof(int), 0, &p)) != 0)
			goto mem_err;
		mp->regids = R_OFFSET(dbmp->reginfo, p);
	}

	SH_TAILQ_INIT(&mp->bhq);

	/* Allocate hash table space and initialize it. */
	if ((ret = __db_shalloc(reginfo->addr,
	    htab_buckets * sizeof(DB_HASHTAB), 0, &htab)) != 0)
		goto mem_err;
	__db_hashinit(htab, htab_buckets);
	mp->htab = R_OFFSET(reginfo, htab);
	mp->htab_buckets = htab_buckets;

	return (0);

mem_err:__db_err(dbenv, "Unable to allocate memory for mpool region");
err:	if (reginfo->primary != NULL)
		__db_shalloc_free(reginfo->addr, reginfo->primary);
	return (ret);
}

/*
 * __memp_close --
 *	Internal version of memp_close: only called from DB_ENV->close.
 *
 * PUBLIC: int __memp_close __P((DB_ENV *));
 */
int
__memp_close(dbenv)
	DB_ENV *dbenv;
{
	DB_MPOOL *dbmp;
	DB_MPOOLFILE *dbmfp;
	DB_MPREG *mpreg;
	u_int32_t i;
	int ret, t_ret;

	ret = 0;
	dbmp = dbenv->mp_handle;

	/* Discard DB_MPREGs. */
	while ((mpreg = LIST_FIRST(&dbmp->dbregq)) != NULL) {
		LIST_REMOVE(mpreg, q);
		__os_free(mpreg, sizeof(DB_MPREG));
	}

	/* Discard DB_MPOOLFILEs. */
	while ((dbmfp = TAILQ_FIRST(&dbmp->dbmfq)) != NULL)
		if ((t_ret = memp_fclose(dbmfp)) != 0 && ret == 0)
			ret = t_ret;

	/* Discard the thread mutex. */
	if (dbmp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmp->mutexp);

	/* Detach from the region(s). */
	for (i = 0; i < dbmp->nreg; ++i)
		if ((t_ret = __db_r_detach(
		    dbenv, &dbmp->reginfo[i], 0)) != 0 && ret == 0)
			ret = t_ret;

	__os_free(dbmp->reginfo, dbmp->nreg * sizeof(*dbmp->reginfo));
	__os_free(dbmp, sizeof(*dbmp));

	dbenv->mp_handle = NULL;
	return (ret);
}

#ifdef MUTEX_SYSTEM_RESOURCES
/*
 * __mpool_region_maint --
 *	Return the amount of space needed for region maintenance info.
 *
 */
static size_t
__mpool_region_maint(infop)
	REGINFO *infop;
{
	size_t s;
	int numlocks;

	/*
	 * For mutex maintenance we need one mutex per possible page.
	 * Compute the maximum number of pages this cache can have.
	 * Also add in an mpool mutex.
	 */
	numlocks = ((infop->rp->size / DB_MIN_PGSIZE) + 1);
	s = sizeof(roff_t) * numlocks;
	return (s);
}
#endif

/*
 * __mpool_region_destroy
 *	Destroy any region maintenance info.
 *
 * PUBLIC: void __mpool_region_destroy __P((DB_ENV *, REGINFO *));
 */
void
__mpool_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	MPOOL *mp;

	COMPQUIET(dbenv, NULL);
	mp = R_ADDR(infop, infop->rp->primary);

	__db_shlocks_destroy(infop, (REGMAINT *)R_ADDR(infop, mp->maint_off));
	return;
}
