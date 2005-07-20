/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: env_region.c,v 11.103 2004/10/15 16:59:41 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/crypto.h"
#include "dbinc/lock.h"
#include "dbinc/log.h"
#include "dbinc/mp.h"
#include "dbinc/txn.h"

static int  __db_des_destroy __P((DB_ENV *, REGION *, int));
static int  __db_des_get __P((DB_ENV *, REGINFO *, REGINFO *, REGION **));
static int  __db_e_remfile __P((DB_ENV *));
static int  __db_faultmem __P((DB_ENV *, void *, size_t, int));
static void __db_region_destroy __P((DB_ENV *, REGINFO *));

/*
 * __db_e_attach
 *	Join/create the environment
 *
 * PUBLIC: int __db_e_attach __P((DB_ENV *, u_int32_t *));
 */
int
__db_e_attach(dbenv, init_flagsp)
	DB_ENV *dbenv;
	u_int32_t *init_flagsp;
{
	REGENV *renv;
	REGENV_REF ref;
	REGINFO *infop;
	REGION *rp, tregion;
	size_t size;
	size_t nrw;
	u_int32_t mbytes, bytes;
	u_int retry_cnt;
	int ret, segid;
	char buf[sizeof(DB_REGION_FMT) + 20];

#if !defined(HAVE_MUTEX_THREADS)
	/*
	 * !!!
	 * If we don't have spinlocks, we need a file descriptor for fcntl(2)
	 * locking.  We use the file handle from the REGENV file for this
	 * purpose.
	 *
	 * Since we may be using shared memory regions, e.g., shmget(2), and
	 * not a mapped-in regular file, the backing file may be only a few
	 * bytes in length.  So, this depends on the ability to call fcntl to
	 * lock file offsets much larger than the actual physical file.  I
	 * think that's safe -- besides, very few systems actually need this
	 * kind of support, SunOS is the only one still in wide use of which
	 * I'm aware.
	 *
	 * The error case is if an application lacks spinlocks and wants to be
	 * threaded.  That doesn't work because fcntl may lock the underlying
	 * process, including all its threads.
	 */
	if (F_ISSET(dbenv, DB_ENV_THREAD)) {
		__db_err(dbenv,
	    "architecture lacks fast mutexes: applications cannot be threaded");
		return (EINVAL);
	}
#endif

	/* Initialization */
	retry_cnt = 0;

	/* Repeated initialization. */
loop:	renv = NULL;

	/* Set up the DB_ENV's REG_INFO structure. */
	if ((ret = __os_calloc(dbenv, 1, sizeof(REGINFO), &infop)) != 0)
		return (ret);
	infop->dbenv = dbenv;
	infop->type = REGION_TYPE_ENV;
	infop->id = REGION_ID_ENV;
	infop->flags = REGION_JOIN_OK;
	if (F_ISSET(dbenv, DB_ENV_CREATE))
		F_SET(infop, REGION_CREATE_OK);

	/*
	 * We have to single-thread the creation of the REGENV region.  Once
	 * it exists, we can do locking using locks in the region, but until
	 * then we have to be the only player in the game.
	 *
	 * If this is a private environment, we are only called once and there
	 * are no possible race conditions.
	 *
	 * If this is a public environment, we use the filesystem to ensure
	 * the creation of the environment file is single-threaded.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		if ((ret = __os_strdup(dbenv,
		    "process-private", &infop->name)) != 0)
			goto err;
		goto creation;
	}

	/* Build the region name. */
	(void)snprintf(buf, sizeof(buf), "%s", DB_REGION_ENV);
	if ((ret = __db_appname(dbenv,
	    DB_APP_NONE, buf, 0, NULL, &infop->name)) != 0)
		goto err;

	/*
	 * Try to create the file, if we have the authority.  We have to ensure
	 * that multiple threads/processes attempting to simultaneously create
	 * the file are properly ordered.  Open using the O_CREAT and O_EXCL
	 * flags so that multiple attempts to create the region will return
	 * failure in all but one.  POSIX 1003.1 requires that EEXIST be the
	 * errno return value -- I sure hope they're right.
	 */
	if (F_ISSET(dbenv, DB_ENV_CREATE)) {
		if ((ret = __os_open(dbenv, infop->name,
		    DB_OSO_CREATE | DB_OSO_EXCL | DB_OSO_REGION,
		    dbenv->db_mode, &dbenv->lockfhp)) == 0)
			goto creation;
		if (ret != EEXIST) {
			__db_err(dbenv,
			    "%s: %s", infop->name, db_strerror(ret));
			goto err;
		}
	}

	/*
	 * If we couldn't create the file, try and open it.  (If that fails,
	 * we're done.)
	 */
	if ((ret = __os_open(
	    dbenv, infop->name, DB_OSO_REGION, 0, &dbenv->lockfhp)) != 0)
		goto err;

	/* The region exists, it's not okay to recreate it. */
	F_CLR(infop, REGION_CREATE_OK);

	/*
	 * !!!
	 * The region may be in system memory not backed by the filesystem
	 * (more specifically, not backed by this file), and we're joining
	 * it.  In that case, the process that created it will have written
	 * out a REGENV_REF structure as its only contents.  We read that
	 * structure before we do anything further, e.g., we can't just map
	 * that file in and then figure out what's going on.
	 *
	 * All of this noise is because some systems don't have a coherent VM
	 * and buffer cache, and what's worse, when you mix operations on the
	 * VM and buffer cache, half the time you hang the system.
	 *
	 * If the file is the size of an REGENV_REF structure, then we know
	 * the real region is in some other memory.  (The only way you get a
	 * file that size is to deliberately write it, as it's smaller than
	 * any possible disk sector created by writing a file or mapping the
	 * file into memory.)  In which case, retrieve the structure from the
	 * file and use it to acquire the referenced memory.
	 *
	 * If the structure is larger than a REGENV_REF structure, then this
	 * file is backing the shared memory region, and we just map it into
	 * memory.
	 *
	 * And yes, this makes me want to take somebody and kill them.  (I
	 * digress -- but you have no freakin' idea.  This is unbelievably
	 * stupid and gross, and I've probably spent six months of my life,
	 * now, trying to make different versions of it work.)
	 */
	if ((ret = __os_ioinfo(dbenv, infop->name,
	    dbenv->lockfhp, &mbytes, &bytes, NULL)) != 0) {
		__db_err(dbenv, "%s: %s", infop->name, db_strerror(ret));
		goto err;
	}

	/*
	 * !!!
	 * A size_t is OK -- regions get mapped into memory, and so can't
	 * be larger than a size_t.
	 */
	size = mbytes * MEGABYTE + bytes;

	/*
	 * If the size is less than the size of a REGENV_REF structure, the
	 * region (or, possibly, the REGENV_REF structure) has not yet been
	 * completely written.  Wait awhile and try again.
	 *
	 * Otherwise, if the size is the size of a REGENV_REF structure,
	 * read it into memory and use it as a reference to the real region.
	 */
	if (size <= sizeof(ref)) {
		if (size != sizeof(ref))
			goto retry;

		if ((ret = __os_read(dbenv, dbenv->lockfhp, &ref,
		    sizeof(ref), &nrw)) != 0 || nrw < (size_t)sizeof(ref)) {
			if (ret == 0)
				ret = EIO;
			__db_err(dbenv,
		    "%s: unable to read system-memory information from: %s",
			    infop->name, db_strerror(ret));
			goto err;
		}
		size = ref.size;
		segid = ref.segid;

		F_SET(dbenv, DB_ENV_SYSTEM_MEM);
	} else if (F_ISSET(dbenv, DB_ENV_SYSTEM_MEM)) {
		ret = EINVAL;
		__db_err(dbenv,
		    "%s: existing environment not created in system memory: %s",
		    infop->name, db_strerror(ret));
		goto err;
	} else
		segid = INVALID_REGION_SEGID;

	/*
	 * If not doing thread locking, we need to save the file handle for
	 * fcntl(2) locking.  Otherwise, discard the handle, we no longer
	 * need it, and the less contact between the buffer cache and the VM,
	 * the better.
	 */
#ifdef HAVE_MUTEX_THREADS
	 (void)__os_closehandle(dbenv, dbenv->lockfhp);
	 dbenv->lockfhp = NULL;
#endif

	/* Call the region join routine to acquire the region. */
	memset(&tregion, 0, sizeof(tregion));
	tregion.size = (roff_t)size;
	tregion.segid = segid;
	if ((ret = __os_r_attach(dbenv, infop, &tregion)) != 0)
		goto err;

	/*
	 * The environment's REGENV structure has to live at offset 0 instead
	 * of the usual shalloc information.  Set the primary reference and
	 * correct the "addr" value to reference the shalloc region.  Note,
	 * this means that all of our offsets (R_ADDR/R_OFFSET) get shifted
	 * as well, but that should be fine.
	 */
	infop->primary = infop->addr;
	infop->addr = (u_int8_t *)infop->addr + sizeof(REGENV);
	renv = infop->primary;

	/* Make sure the region matches our build. */
	if (renv->majver != DB_VERSION_MAJOR ||
	    renv->minver != DB_VERSION_MINOR) {
		__db_err(dbenv,
	"Program version %d.%d doesn't match environment version",
		    DB_VERSION_MAJOR, DB_VERSION_MINOR);
		ret = DB_VERSION_MISMATCH;
		goto err;
	}

	/*
	 * Check if the environment has had a catastrophic failure.
	 *
	 * Check the magic number to ensure the region is initialized.  If the
	 * magic number isn't set, the lock may not have been initialized, and
	 * an attempt to use it could lead to random behavior.
	 *
	 * The panic and magic values aren't protected by any lock, so we never
	 * use them in any check that's more complex than set/not-set.
	 *
	 * !!!
	 * I'd rather play permissions games using the underlying file, but I
	 * can't because Windows/NT filesystems won't open files mode 0.
	 */
	if (renv->envpanic && !F_ISSET(dbenv, DB_ENV_NOPANIC)) {
		ret = __db_panic_msg(dbenv);
		goto err;
	}
	if (renv->magic != DB_REGION_MAGIC)
		goto retry;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex);

	/*
	 * Finally!  We own the environment now.  Repeat the panic check, it's
	 * possible that it was set while we waited for the lock.
	 */
	if (renv->envpanic && !F_ISSET(dbenv, DB_ENV_NOPANIC)) {
		ret = __db_panic_msg(dbenv);
		goto err_unlock;
	}

	/*
	 * Get a reference to the underlying REGION information for this
	 * environment.
	 */
	if ((ret = __db_des_get(dbenv, infop, infop, &rp)) != 0 || rp == NULL) {
		MUTEX_UNLOCK(dbenv, &renv->mutex);
		goto find_err;
	}
	infop->rp = rp;

	/*
	 * There's still a possibility for inconsistent data.  When we acquired
	 * the size of the region and attached to it, it might have still been
	 * growing as part of its creation.  We can detect this by checking the
	 * size we originally found against the region's current size.  (The
	 * region's current size has to be final, the creator finished growing
	 * it before releasing the environment for us to lock.)
	 */
	if (rp->size != size) {
err_unlock:	MUTEX_UNLOCK(dbenv, &renv->mutex);
		goto retry;
	}

	/* Increment the reference count. */
	++renv->refcnt;

	/*
	 * Add configuration flags from our caller; return the total set of
	 * configuration flags for later DB_JOINENV calls.
	 */
	if (init_flagsp != NULL) {
		renv->init_flags |= *init_flagsp;
		*init_flagsp = renv->init_flags;
	}

	/* Discard our lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/*
	 * Fault the pages into memory.  Note, do this AFTER releasing the
	 * lock, because we're only reading the pages, not writing them.
	 */
	(void)__db_faultmem(dbenv, infop->primary, rp->size, 0);

	/* Everything looks good, we're done. */
	dbenv->reginfo = infop;
	return (0);

creation:
	/* Create the environment region. */
	F_SET(infop, REGION_CREATE);

	/*
	 * Allocate room for 100 REGION structures plus overhead (we're going
	 * to use this space for last-ditch allocation requests), although we
	 * should never need anything close to that.
	 *
	 * Encryption passwds are stored in the env region.  Add that in too.
	 */
	memset(&tregion, 0, sizeof(tregion));
	tregion.size = (roff_t)(100 * sizeof(REGION) +
	    dbenv->passwd_len + 4096);
	tregion.segid = INVALID_REGION_SEGID;
	if ((ret = __os_r_attach(dbenv, infop, &tregion)) != 0)
		goto err;

	/*
	 * Fault the pages into memory.  Note, do this BEFORE we initialize
	 * anything, because we're writing the pages, not just reading them.
	 */
	(void)__db_faultmem(dbenv, infop->addr, tregion.size, 1);

	/*
	 * The first object in the region is the REGENV structure.  This is
	 * different from the other regions, and, from everything else in
	 * this region, where all objects are allocated from the pool, i.e.,
	 * there aren't any fixed locations.  The remaining space is made
	 * available for later allocation.
	 *
	 * The allocation space must be size_t aligned, because that's what
	 * the initialization routine is going to store there.  To make sure
	 * that happens, the REGENV structure was padded with a final size_t.
	 * No other region needs to worry about it because all of them treat
	 * the entire region as allocation space.
	 *
	 * Set the primary reference and correct the "addr" value to reference
	 * the shalloc region.  Note, this requires that we "uncorrect" it at
	 * region detach, and that all of our offsets (R_ADDR/R_OFFSET) will be
	 * shifted as well, but that should be fine.
	 */
	infop->primary = infop->addr;
	infop->addr = (u_int8_t *)infop->addr + sizeof(REGENV);
	__db_shalloc_init(infop, tregion.size - sizeof(REGENV));

	/*
	 * Initialize the rest of the REGENV structure, except for the magic
	 * number which validates the file/environment.
	 */
	renv = infop->primary;
	renv->envpanic = 0;
	__os_unique_id(dbenv, &renv->envid);
	(void)db_version(&renv->majver, &renv->minver, &renv->patch);
	SH_LIST_INIT(&renv->regionq);
	renv->refcnt = 1;
	renv->cipher_off = INVALID_ROFF;
	renv->rep_off = INVALID_ROFF;

	/*
	 * Initialize init_flags to store the flags that any other environment
	 * handle that uses DB_JOINENV to join this environment will need.
	 */
	renv->init_flags = (init_flagsp == NULL) ? 0 : *init_flagsp;

	/*
	 * Lock the environment.
	 *
	 * Check the lock call return.  This is the first lock we initialize
	 * and acquire, and we have to know if it fails.  (It CAN fail, e.g.,
	 * SunOS, when using fcntl(2) for locking and using an in-memory
	 * filesystem as the database home.  But you knew that, I'm sure -- it
	 * probably wasn't even worth mentioning.)
	 */
	if ((ret = __db_mutex_setup(dbenv, infop, &renv->mutex,
	    MUTEX_NO_RECORD | MUTEX_NO_RLOCK)) != 0) {
		__db_err(dbenv, "%s: unable to initialize environment lock: %s",
		    infop->name, db_strerror(ret));
		goto err;
	}

	if (!F_ISSET(&renv->mutex, MUTEX_IGNORE) &&
	    (ret = __db_mutex_lock(dbenv, &renv->mutex)) != 0) {
		__db_err(dbenv, "%s: unable to acquire environment lock: %s",
		    infop->name, db_strerror(ret));
		goto err;
	}

	/*
	 * Get the underlying REGION structure for this environment.  Note,
	 * we created the underlying OS region before we acquired the REGION
	 * structure, which is backwards from the normal procedure.  Update
	 * the REGION structure.
	 */
	if ((ret = __db_des_get(dbenv, infop, infop, &rp)) != 0) {
find_err:	__db_err(dbenv,
		    "%s: unable to find environment", infop->name);
		if (ret == 0)
			ret = EINVAL;
		goto err;
	}
	infop->rp = rp;
	rp->size = tregion.size;
	rp->segid = tregion.segid;

	/*
	 * !!!
	 * If we create an environment where regions are public and in system
	 * memory, we have to inform processes joining the environment how to
	 * attach to the shared memory segment.  So, we write the shared memory
	 * identifier into the file, to be read by those other processes.
	 *
	 * XXX
	 * This is really OS-layer information, but I can't see any easy way
	 * to move it down there without passing down information that it has
	 * no right to know, e.g., that this is the one-and-only REGENV region
	 * and not some other random region.
	 */
	if (tregion.segid != INVALID_REGION_SEGID) {
		ref.size = tregion.size;
		ref.segid = tregion.segid;
		if ((ret = __os_write(
		    dbenv, dbenv->lockfhp, &ref, sizeof(ref), &nrw)) != 0) {
			__db_err(dbenv,
			    "%s: unable to write out public environment ID: %s",
			    infop->name, db_strerror(ret));
			goto err;
		}
	}

	/*
	 * If not doing thread locking, we need to save the file handle for
	 * fcntl(2) locking.  Otherwise, discard the handle, we no longer
	 * need it, and the less contact between the buffer cache and the VM,
	 * the better.
	 */
#if defined(HAVE_MUTEX_THREADS)
	if (dbenv->lockfhp != NULL) {
		 (void)__os_closehandle(dbenv, dbenv->lockfhp);
		 dbenv->lockfhp = NULL;
	}
#endif

	/* Validate the file. */
	renv->magic = DB_REGION_MAGIC;

	/* Discard our lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/* Everything looks good, we're done. */
	dbenv->reginfo = infop;
	return (0);

err:
retry:	/* Close any open file handle. */
	if (dbenv->lockfhp != NULL) {
		(void)__os_closehandle(dbenv, dbenv->lockfhp);
		dbenv->lockfhp = NULL;
	}

	/*
	 * If we joined or created the region, detach from it.  If we created
	 * it, destroy it.  Note, there's a path in the above code where we're
	 * using a temporary REGION structure because we haven't yet allocated
	 * the real one.  In that case the region address (addr) will be filled
	 * in, but the REGION pointer (rp) won't.  Fix it.
	 */
	if (infop->addr != NULL) {
		if (infop->rp == NULL)
			infop->rp = &tregion;

		/* Reset the addr value that we "corrected" above. */
		infop->addr = infop->primary;
		(void)__os_r_detach(dbenv,
		    infop, F_ISSET(infop, REGION_CREATE));
	}

	/* Free the allocated name and/or REGINFO structure. */
	if (infop->name != NULL)
		__os_free(dbenv, infop->name);
	__os_free(dbenv, infop);

	/* If we had a temporary error, wait awhile and try again. */
	if (ret == 0) {
		if (++retry_cnt > 3) {
			__db_err(dbenv, "unable to join the environment");
			ret = EAGAIN;
		} else {
			__os_sleep(dbenv, retry_cnt * 3, 0);
			goto loop;
		}
	}

	return (ret);
}

/*
 * __db_e_detach --
 *	Detach from the environment.
 *
 * PUBLIC: int __db_e_detach __P((DB_ENV *, int));
 */
int
__db_e_detach(dbenv, destroy)
	DB_ENV *dbenv;
	int destroy;
{
	REGENV *renv;
	REGINFO *infop;

	infop = dbenv->reginfo;
	renv = infop->primary;

	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		destroy = 1;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex);

	/* Decrement the reference count. */
	if (renv->refcnt == 0) {
		__db_err(dbenv,
		    "region %lu (environment): reference count went negative",
		    (u_long)infop->rp->id);
	} else
		--renv->refcnt;

	/* Release the lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/* Close the locking file handle. */
	if (dbenv->lockfhp != NULL) {
		(void)__os_closehandle(dbenv, dbenv->lockfhp);
		dbenv->lockfhp = NULL;
	}

	/*
	 * If we are destroying the environment, destroy any system resources
	 * the crypto and replication systems may have acquired and put in the
	 * main region.
	 */
	if (destroy) {
#ifdef HAVE_CRYPTO
		(void)__crypto_region_destroy(dbenv);
#endif
		(void)__rep_region_destroy(dbenv);
	}

	/*
	 * Release the region, and kill our reference.
	 *
	 * If we are destroying the environment, destroy any system resources
	 * backing the mutex.
	 */
	if (destroy) {
		(void)__db_mutex_destroy(&renv->mutex);
		(void)__db_mutex_destroy(&infop->rp->mutex);

		/*
		 * Only free the REGION structure itself if it was separately
		 * allocated from the heap.
		 */
		if (F_ISSET(dbenv, DB_ENV_PRIVATE))
			__db_shalloc_free(infop, infop->rp);
	}

	/* Reset the addr value that we "corrected" above. */
	infop->addr = infop->primary;

	(void)__os_r_detach(dbenv, infop, destroy);
	if (infop->name != NULL)
		__os_free(dbenv, infop->name);

	/*
	 * We set the DB_ENV->reginfo field to NULL here and discard its memory.
	 * DB_ENV->remove calls __dbenv_remove to do the region remove, and
	 * __dbenv_remove attached and then detaches from the region.  We don't
	 * want to return to DB_ENV->remove with a non-NULL DB_ENV->reginfo
	 * field because it will attempt to detach again as part of its cleanup.
	 */
	__os_free(dbenv, dbenv->reginfo);
	dbenv->reginfo = NULL;

	return (0);
}

/*
 * __db_e_remove --
 *	Discard an environment if it's not in use.
 *
 * PUBLIC: int __db_e_remove __P((DB_ENV *, u_int32_t));
 */
int
__db_e_remove(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	REGENV *renv;
	REGINFO *infop, reginfo;
	REGION *rp;
	u_int32_t db_env_reset;
	int force, ret;

	force = LF_ISSET(DB_FORCE) ? 1 : 0;
	/*
	 * This routine has to walk a nasty line between not looking into
	 * the environment (which may be corrupted after an app or system
	 * crash), and removing everything that needs removing.  What we
	 * do is:
	 *	1. Connect to the environment (so it better be OK).
	 *	2. If the environment is in use (reference count is non-zero),
	 *	   return EBUSY.
	 *	3. Overwrite the magic number so that any threads of control
	 *	   attempting to connect will backoff and retry.
	 *	4. Walk the list of regions.  Connect to each region and then
	 *	   disconnect with the destroy flag set.  This shouldn't cause
	 *	   any problems, even if the region is corrupted, because we
	 *	   should never be looking inside the region.
	 *	5. Walk the list of files in the directory, unlinking any
	 *	   files that match a region name.  Unlink the environment
	 *	   file last.
	 *
	 * If the force flag is set, we do not acquire any locks during this
	 * process.
	 */
	db_env_reset = F_ISSET(dbenv, DB_ENV_NOLOCKING | DB_ENV_NOPANIC);
	if (force)
		F_SET(dbenv, DB_ENV_NOLOCKING);
	F_SET(dbenv, DB_ENV_NOPANIC);

	/* Join the environment. */
	if ((ret = __db_e_attach(dbenv, NULL)) != 0) {
		/*
		 * If we can't join it, we assume that's because it doesn't
		 * exist.  It would be better to know why we failed, but it
		 * probably isn't important.
		 */
		ret = 0;
		if (force)
			goto remfiles;
		goto done;
	}

	infop = dbenv->reginfo;
	renv = infop->primary;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex);

	/*
	 * If it's in use, we're done unless we're forcing the issue or the
	 * environment has panic'd.  (Presumably, if the environment panic'd,
	 * the thread holding the reference count may not have cleaned up.)
	 */
	if (renv->refcnt == 1 || renv->envpanic == 1 || force) {
		/*
		 * Set the panic flag and overwrite the magic number.
		 *
		 * !!!
		 * From this point on, there's no going back, we pretty
		 * much ignore errors, and just whack on whatever we can.
		 */
		renv->envpanic = 1;
		renv->magic = 0;

		/*
		 * Unlock the environment.  We should no longer need the lock
		 * because we've poisoned the pool, but we can't continue to
		 * hold it either, because other routines may want it.
		 */
		MUTEX_UNLOCK(dbenv, &renv->mutex);

		/*
		 * Attach to each sub-region and destroy it.
		 *
		 * !!!
		 * The REGION_CREATE_OK flag is set for Windows/95 -- regions
		 * are zero'd out when the last reference to the region goes
		 * away, in which case the underlying OS region code requires
		 * callers be prepared to create the region in order to join it.
		 */
		memset(&reginfo, 0, sizeof(reginfo));
		for (rp = SH_LIST_FIRST(&renv->regionq, __db_region);
		    rp != NULL; rp = SH_LIST_NEXT(rp, q, __db_region)) {
			if (rp->type == REGION_TYPE_ENV)
				continue;

			/*
			 * If we get here and can't attach and/or detach to the
			 * region, it's a mess.  Ignore errors, there's nothing
			 * we can do about them.
			 */
			reginfo.id = rp->id;
			reginfo.flags = REGION_CREATE_OK;
			if (__db_r_attach(dbenv, &reginfo, 0) == 0) {
				R_UNLOCK(dbenv, &reginfo);
				(void)__db_r_detach(dbenv, &reginfo, 1);
			}
		}

		/* Destroy the environment's region. */
		(void)__db_e_detach(dbenv, 1);

		/* Discard any remaining physical files. */
remfiles:	(void)__db_e_remfile(dbenv);
	} else {
		/* Unlock the environment. */
		MUTEX_UNLOCK(dbenv, &renv->mutex);

		/* Discard the environment. */
		(void)__db_e_detach(dbenv, 0);

		ret = EBUSY;
	}

done:	F_CLR(dbenv, DB_ENV_NOLOCKING | DB_ENV_NOPANIC);
	F_SET(dbenv, db_env_reset);

	return (ret);
}

/*
 * __db_e_remfile --
 *	Discard any region files in the filesystem.
 */
static int
__db_e_remfile(dbenv)
	DB_ENV *dbenv;
{
	int cnt, fcnt, lastrm, ret;
	const char *dir;
	char saved_char, *p, **names, *path, buf[sizeof(DB_REGION_FMT) + 20];

	/* Get the full path of a file in the environment. */
	(void)snprintf(buf, sizeof(buf), "%s", DB_REGION_ENV);
	if ((ret = __db_appname(dbenv, DB_APP_NONE, buf, 0, NULL, &path)) != 0)
		return (ret);

	/* Get the parent directory for the environment. */
	if ((p = __db_rpath(path)) == NULL) {
		p = path;
		saved_char = *p;

		dir = PATH_DOT;
	} else {
		saved_char = *p;
		*p = '\0';

		dir = path;
	}

	/* Get the list of file names. */
	if ((ret = __os_dirlist(dbenv, dir, &names, &fcnt)) != 0)
		__db_err(dbenv, "%s: %s", dir, db_strerror(ret));

	/* Restore the path, and free it. */
	*p = saved_char;
	__os_free(dbenv, path);

	if (ret != 0)
		return (ret);

	/*
	 * Remove files from the region directory.
	 */
	for (lastrm = -1, cnt = fcnt; --cnt >= 0;) {
		/* Skip anything outside our name space. */
		if (strncmp(names[cnt],
		    DB_REGION_PREFIX, sizeof(DB_REGION_PREFIX) - 1))
			continue;

		/* Skip queue extent files. */
		if (strncmp(names[cnt], "__dbq.", 6) == 0)
			continue;

		/* Skip replication files. */
		if (strncmp(names[cnt], "__db.rep.", 9) == 0)
			continue;

		/*
		 * Remove the primary environment region last, because it's
		 * the key to this whole mess.
		 */
		if (strcmp(names[cnt], DB_REGION_ENV) == 0) {
			lastrm = cnt;
			continue;
		}

		/* Remove the file. */
		if (__db_appname(dbenv,
		    DB_APP_NONE, names[cnt], 0, NULL, &path) == 0) {
			/*
			 * Overwrite region files.  Temporary files would have
			 * been maintained in encrypted format, so there's no
			 * reason to overwrite them.  This is not an exact
			 * check on the file being a region file, but it's
			 * not likely to be wrong, and the worst thing that can
			 * happen is we overwrite a file that didn't need to be
			 * overwritten.
			 */
			if (F_ISSET(dbenv, DB_ENV_OVERWRITE) &&
			    strlen(names[cnt]) == DB_REGION_NAME_LENGTH)
				(void)__db_overwrite(dbenv, path);
			(void)__os_unlink(dbenv, path);
			__os_free(dbenv, path);
		}
	}

	if (lastrm != -1)
		if (__db_appname(dbenv,
		    DB_APP_NONE, names[lastrm], 0, NULL, &path) == 0) {
			if (F_ISSET(dbenv, DB_ENV_OVERWRITE))
				(void)__db_overwrite(dbenv, path);
			(void)__os_unlink(dbenv, path);
			__os_free(dbenv, path);
		}
	__os_dirfree(dbenv, names, fcnt);

	return (0);
}

/*
 * __db_r_attach
 *	Join/create a region.
 *
 * PUBLIC: int __db_r_attach __P((DB_ENV *, REGINFO *, size_t));
 */
int
__db_r_attach(dbenv, infop, size)
	DB_ENV *dbenv;
	REGINFO *infop;
	size_t size;
{
	REGENV *renv;
	REGION *rp;
	int ret;
	char buf[sizeof(DB_REGION_FMT) + 20];

	renv = ((REGINFO *)dbenv->reginfo)->primary;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex);

	/*
	 * Find or create a REGION structure for this region.  If we create
	 * it, the REGION_CREATE flag will be set in the infop structure.
	 */
	F_CLR(infop, REGION_CREATE);
	if ((ret = __db_des_get(dbenv, dbenv->reginfo, infop, &rp)) != 0) {
		MUTEX_UNLOCK(dbenv, &renv->mutex);
		return (ret);
	}
	infop->dbenv = dbenv;
	infop->rp = rp;
	infop->type = rp->type;
	infop->id = rp->id;

	/* If we're creating the region, set the desired size. */
	if (F_ISSET(infop, REGION_CREATE))
		rp->size = (roff_t)size;

	/* Join/create the underlying region. */
	(void)snprintf(buf, sizeof(buf), DB_REGION_FMT, infop->id);
	if ((ret = __db_appname(dbenv,
	    DB_APP_NONE, buf, 0, NULL, &infop->name)) != 0)
		goto err;
	if ((ret = __os_r_attach(dbenv, infop, rp)) != 0)
		goto err;

	/*
	 * Fault the pages into memory.  Note, do this BEFORE we initialize
	 * anything because we're writing pages in created regions, not just
	 * reading them.
	 */
	(void)__db_faultmem(dbenv,
	    infop->addr, rp->size, F_ISSET(infop, REGION_CREATE));

	/*
	 * !!!
	 * The underlying layer may have just decided that we are going
	 * to create the region.  There are various system issues that
	 * can result in a useless region that requires re-initialization.
	 *
	 * If we created the region, initialize it for allocation.
	 */
	if (F_ISSET(infop, REGION_CREATE))
		__db_shalloc_init(infop, rp->size);

	/*
	 * If the underlying REGION isn't the environment, acquire a lock
	 * for it and release our lock on the environment.
	 */
	if (infop->type != REGION_TYPE_ENV) {
		MUTEX_LOCK(dbenv, &rp->mutex);
		MUTEX_UNLOCK(dbenv, &renv->mutex);
	}

	return (0);

err:	/* Discard the underlying region. */
	if (infop->addr != NULL)
		(void)__os_r_detach(dbenv,
		    infop, F_ISSET(infop, REGION_CREATE));
	infop->rp = NULL;
	infop->id = INVALID_REGION_ID;

	/* Discard the REGION structure if we created it. */
	if (F_ISSET(infop, REGION_CREATE)) {
		(void)__db_des_destroy(dbenv, rp, 1);
		F_CLR(infop, REGION_CREATE);
	}

	/* Release the environment lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	return (ret);
}

/*
 * __db_r_detach --
 *	Detach from a region.
 *
 * PUBLIC: int __db_r_detach __P((DB_ENV *, REGINFO *, int));
 */
int
__db_r_detach(dbenv, infop, destroy)
	DB_ENV *dbenv;
	REGINFO *infop;
	int destroy;
{
	REGENV *renv;
	REGION *rp;
	int ret, t_ret;

	renv = ((REGINFO *)dbenv->reginfo)->primary;
	rp = infop->rp;
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		destroy = 1;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex);

	/* Acquire the lock for the REGION. */
	MUTEX_LOCK(dbenv, &rp->mutex);

	/*
	 * We need to call destroy on per-subsystem info before we free the
	 * memory associated with the region.
	 */
	if (destroy)
		__db_region_destroy(dbenv, infop);

	/* Detach from the underlying OS region. */
	ret = __os_r_detach(dbenv, infop, destroy);

	/* Release the REGION lock. */
	MUTEX_UNLOCK(dbenv, &rp->mutex);

	/*
	 * If we destroyed the region, discard the REGION structure.  The only
	 * time this routine is called with the destroy flag set is when the
	 * environment is being removed, and it's likely that the only reason
	 * the environment is being removed is because we crashed.  Don't do
	 * any unnecessary shared memory manipulation.
	 */
	if (destroy &&
	    ((t_ret = __db_des_destroy(
		dbenv, rp, F_ISSET(dbenv, DB_ENV_PRIVATE))) != 0) && ret == 0)
		ret = t_ret;

	/* Release the environment lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/* Destroy the structure. */
	if (infop->name != NULL)
		__os_free(dbenv, infop->name);

	return (ret);
}

/*
 * __db_des_get --
 *	Return a reference to the shared information for a REGION,
 *	optionally creating a new entry.
 */
static int
__db_des_get(dbenv, env_infop, infop, rpp)
	DB_ENV *dbenv;
	REGINFO *env_infop, *infop;
	REGION **rpp;
{
	REGENV *renv;
	REGION *rp, *first_type;
	u_int32_t maxid;
	int ret;

	/*
	 * !!!
	 * Called with the environment already locked.
	 */
	*rpp = NULL;
	renv = env_infop->primary;

	/*
	 * If the caller wants to join a region, walk through the existing
	 * regions looking for a matching ID (if ID specified) or matching
	 * type (if type specified).  If we return based on a matching type
	 * return the "primary" region, that is, the first region that was
	 * created of this type.
	 *
	 * Track the maximum region ID so we can allocate a new region,
	 * note that we have to start at 1 because the primary environment
	 * uses ID == 1.
	 */
	maxid = REGION_ID_ENV;
	for (first_type = NULL,
	    rp = SH_LIST_FIRST(&renv->regionq, __db_region);
	    rp != NULL; rp = SH_LIST_NEXT(rp, q, __db_region)) {
		if (infop->id != INVALID_REGION_ID) {
			if (infop->id == rp->id)
				break;
			continue;
		}
		if (infop->type == rp->type &&
		    F_ISSET(infop, REGION_JOIN_OK) &&
		    (first_type == NULL || first_type->id > rp->id))
			first_type = rp;

		if (rp->id > maxid)
			maxid = rp->id;
	}
	if (rp == NULL)
		rp = first_type;

	/*
	 * If we didn't find a region and we can't create the region, fail.
	 * The caller generates any error message.
	 */
	if (rp == NULL && !F_ISSET(infop, REGION_CREATE_OK))
		return (ENOENT);

	/*
	 * If we didn't find a region, create and initialize a REGION structure
	 * for the caller.  If id was set, use that value, otherwise we use the
	 * next available ID.
	 */
	if (rp == NULL) {
		if ((ret = __db_shalloc(env_infop,
		    sizeof(REGION), MUTEX_ALIGN, &rp)) != 0) {
			__db_err(dbenv,
			    "unable to create new master region entry: %s",
			    db_strerror(ret));
			return (ret);
		}

		/* Initialize the region. */
		memset(rp, 0, sizeof(*rp));
		if ((ret = __db_mutex_setup(dbenv, env_infop, &rp->mutex,
		    MUTEX_NO_RECORD | MUTEX_NO_RLOCK)) != 0) {
			__db_shalloc_free(env_infop, rp);
			return (ret);
		}
		rp->segid = INVALID_REGION_SEGID;

		/*
		 * Set the type and ID; if no region ID was specified,
		 * allocate one.
		 */
		rp->type = infop->type;
		rp->id = infop->id == INVALID_REGION_ID ? maxid + 1 : infop->id;

		SH_LIST_INSERT_HEAD(&renv->regionq, rp, q, __db_region);
		F_SET(infop, REGION_CREATE);
	}

	*rpp = rp;
	return (0);
}

/*
 * __db_des_destroy --
 *	Destroy a reference to a REGION.
 */
static int
__db_des_destroy(dbenv, rp, shmem_safe)
	DB_ENV *dbenv;
	REGION *rp;
	int shmem_safe;
{
	REGINFO *infop;

	/*
	 * !!!
	 * Called with the environment already locked.
	 */
	infop = dbenv->reginfo;

	/*
	 * If we're calling during recovery, it may not be safe to access the
	 * shared memory, as the shared memory may have been corrupted during
	 * the crash.  If the shared memory is safe, remove the REGION entry
	 * from its linked list, destroy the mutex, and free the allocated
	 * memory.  On systems that require system mutex support, we don't
	 * have a choice -- safe or not, we have to destroy the mutex or we'll
	 * leak memory.
	 */
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
	(void)__db_mutex_destroy(&rp->mutex);
#else
	if (shmem_safe)
		(void)__db_mutex_destroy(&rp->mutex);
#endif
	if (shmem_safe) {
		SH_LIST_REMOVE(rp, q, __db_region);
		__db_shalloc_free(infop, rp);
	}

	return (0);
}

/*
 * __db_faultmem --
 *	Fault the region into memory.
 */
static int
__db_faultmem(dbenv, addr, size, created)
	DB_ENV *dbenv;
	void *addr;
	size_t size;
	int created;
{
	int ret;
	u_int8_t *p, *t;

	/* Ignore heap regions. */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		return (0);

	/*
	 * It's sometimes significantly faster to page-fault in all of the
	 * region's pages before we run the application, as we see nasty
	 * side-effects when we page-fault while holding various locks, i.e.,
	 * the lock takes a long time to acquire because of the underlying
	 * page fault, and the other threads convoy behind the lock holder.
	 *
	 * If we created the region, we write a non-zero value so that the
	 * system can't cheat.  If we're just joining the region, we can
	 * only read the value and try to confuse the compiler sufficiently
	 * that it doesn't figure out that we're never really using it.
	 */
	ret = 0;
	if (F_ISSET(dbenv, DB_ENV_REGION_INIT)) {
		if (created)
			for (p = addr, t = (u_int8_t *)addr + size;
			    p < t; p += OS_VMPAGESIZE)
				p[0] = 0xdb;
		else
			for (p = addr, t = (u_int8_t *)addr + size;
			    p < t; p += OS_VMPAGESIZE)
				ret |= p[0];
	}

	return (ret);
}

/*
 * __db_region_destroy --
 *	Destroy per-subsystem region information.
 *	Called with the region already locked.
 */
static void
__db_region_destroy(dbenv, infop)
	DB_ENV *dbenv;
	REGINFO *infop;
{
	switch (infop->type) {
	case REGION_TYPE_LOCK:
		__lock_region_destroy(dbenv, infop);
		break;
	case REGION_TYPE_LOG:
		__log_region_destroy(dbenv, infop);
		break;
	case REGION_TYPE_MPOOL:
		__memp_region_destroy(dbenv, infop);
		break;
	case REGION_TYPE_TXN:
		__txn_region_destroy(dbenv, infop);
		break;
	case REGION_TYPE_ENV:
	case REGION_TYPE_MUTEX:
		break;
	case INVALID_REGION_TYPE:
	default:
		DB_ASSERT(0);
		break;
	}
}
