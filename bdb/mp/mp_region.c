/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_region.c,v 11.49 2002/05/07 18:42:20 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

static int __mpool_init __P((DB_ENV *, DB_MPOOL *, int, int));
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
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
 * __mpool_init --
 *	Initialize a MPOOL structure in shared memory.
 */
static int
__mpool_init(dbenv, dbmp, reginfo_off, htab_buckets)
	DB_ENV *dbenv;
	DB_MPOOL *dbmp;
	int reginfo_off, htab_buckets;
{
	DB_MPOOL_HASH *htab;
	MPOOL *mp;
	REGINFO *reginfo;
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	size_t maint_size;
#endif
	int i, ret;
	void *p;

	mp = NULL;

	reginfo = &dbmp->reginfo[reginfo_off];
	if ((ret = __db_shalloc(reginfo->addr,
	    sizeof(MPOOL), MUTEX_ALIGN, &reginfo->primary)) != 0)
		goto mem_err;
	reginfo->rp->primary = R_OFFSET(reginfo, reginfo->primary);
	mp = reginfo->primary;
	memset(mp, 0, sizeof(*mp));

#ifdef	HAVE_MUTEX_SYSTEM_RESOURCES
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

		ZERO_LSN(mp->lsn);

		mp->nreg = dbmp->nreg;
		if ((ret = __db_shalloc(dbmp->reginfo[0].addr,
		    dbmp->nreg * sizeof(int), 0, &p)) != 0)
			goto mem_err;
		mp->regids = R_OFFSET(dbmp->reginfo, p);
	}

	/* Allocate hash table space and initialize it. */
	if ((ret = __db_shalloc(reginfo->addr,
	    htab_buckets * sizeof(DB_MPOOL_HASH), 0, &htab)) != 0)
		goto mem_err;
	mp->htab = R_OFFSET(reginfo, htab);
	for (i = 0; i < htab_buckets; i++) {
		if ((ret = __db_mutex_setup(dbenv,
		    reginfo, &htab[i].hash_mutex,
		    MUTEX_NO_RLOCK)) != 0)
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
 * __memp_dbenv_refresh --
 *	Clean up after the mpool system on a close or failed open.
 *
 * PUBLIC: int __memp_dbenv_refresh __P((DB_ENV *));
 */
int
__memp_dbenv_refresh(dbenv)
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
		__os_free(dbenv, mpreg);
	}

	/* Discard DB_MPOOLFILEs. */
	while ((dbmfp = TAILQ_FIRST(&dbmp->dbmfq)) != NULL)
		if ((t_ret = __memp_fclose_int(dbmfp, 0)) != 0 && ret == 0)
			ret = t_ret;

	/* Discard the thread mutex. */
	if (dbmp->mutexp != NULL)
		__db_mutex_free(dbenv, dbmp->reginfo, dbmp->mutexp);

	/* Detach from the region(s). */
	for (i = 0; i < dbmp->nreg; ++i)
		if ((t_ret = __db_r_detach(
		    dbenv, &dbmp->reginfo[i], 0)) != 0 && ret == 0)
			ret = t_ret;

	__os_free(dbenv, dbmp->reginfo);
	__os_free(dbenv, dbmp);

	dbenv->mp_handle = NULL;
	return (ret);
}

#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
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
	__db_shlocks_destroy(infop, (REGMAINT *)R_ADDR(infop,
	    ((MPOOL *)R_ADDR(infop, infop->rp->primary))->maint_off));

	COMPQUIET(dbenv, NULL);
	COMPQUIET(infop, NULL);
}

/*
 * __memp_nameop
 *	Remove or rename a file in the pool.
 *
 * PUBLIC: int  __memp_nameop __P((DB_ENV *,
 * PUBLIC:     u_int8_t *, const char *, const char *, const char *));
 *
 * XXX
 * Undocumented interface: DB private.
 */
int
__memp_nameop(dbenv, fileid, newname, fullold, fullnew)
	DB_ENV *dbenv;
	u_int8_t *fileid;
	const char *newname, *fullold, *fullnew;
{
	DB_MPOOL *dbmp;
	MPOOL *mp;
	MPOOLFILE *mfp;
	roff_t newname_off;
	int locked, ret;
	void *p;

	locked = 0;
	dbmp = NULL;

	if (!MPOOL_ON(dbenv))
		goto fsop;

	dbmp = dbenv->mp_handle;
	mp = dbmp->reginfo[0].primary;

	/*
	 * Remove or rename a file that the mpool might know about.  We assume
	 * that the fop layer has the file locked for exclusive access, so we
	 * don't worry about locking except for the mpool mutexes.  Checkpoint
	 * can happen at any time, independent of file locking, so we have to
	 * do the actual unlink or rename system call to avoid any race.
	 *
	 * If this is a rename, allocate first, because we can't recursively
	 * grab the region lock.
	 */
	if (newname == NULL)
		p = NULL;
	else {
		if ((ret = __memp_alloc(dbmp, dbmp->reginfo,
		    NULL, strlen(newname) + 1, &newname_off, &p)) != 0)
			return (ret);
		memcpy(p, newname, strlen(newname) + 1);
	}

	locked = 1;
	R_LOCK(dbenv, dbmp->reginfo);

	/*
	 * Find the file -- if mpool doesn't know about this file, that's not
	 * an error-- we may not have it open.
	 */
	for (mfp = SH_TAILQ_FIRST(&mp->mpfq, __mpoolfile);
	    mfp != NULL; mfp = SH_TAILQ_NEXT(mfp, q, __mpoolfile)) {
		/* Ignore non-active files. */
		if (F_ISSET(mfp, MP_DEADFILE | MP_TEMP))
			continue;

		/* Ignore non-matching files. */
		if (memcmp(fileid, R_ADDR(
		    dbmp->reginfo, mfp->fileid_off), DB_FILE_ID_LEN) != 0)
			continue;

		/* If newname is NULL, we're removing the file. */
		if (newname == NULL) {
			MUTEX_LOCK(dbenv, &mfp->mutex);
			MPOOLFILE_IGNORE(mfp);
			MUTEX_UNLOCK(dbenv, &mfp->mutex);
		} else {
			/*
			 * Else, it's a rename.  We've allocated memory
			 * for the new name.  Swap it with the old one.
			 */
			p = R_ADDR(dbmp->reginfo, mfp->path_off);
			mfp->path_off = newname_off;
		}
		break;
	}

	/* Delete the memory we no longer need. */
	if (p != NULL)
		__db_shalloc_free(dbmp->reginfo[0].addr, p);

fsop:	if (newname == NULL)
		(void)__os_unlink(dbenv, fullold);
	else
		(void)__os_rename(dbenv, fullold, fullnew, 1);

	if (locked)
		R_UNLOCK(dbenv, dbmp->reginfo);

	return (0);
}
