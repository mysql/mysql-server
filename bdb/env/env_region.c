/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: env_region.c,v 11.28 2000/12/12 17:36:10 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <string.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "db_shash.h"
#include "lock.h"
#include "lock_ext.h"
#include "log.h"
#include "log_ext.h"
#include "mp.h"
#include "mp_ext.h"
#include "txn.h"
#include "txn_ext.h"

static int  __db_des_destroy __P((DB_ENV *, REGION *));
static int  __db_des_get __P((DB_ENV *, REGINFO *, REGINFO *, REGION **));
static int  __db_e_remfile __P((DB_ENV *));
static int  __db_faultmem __P((void *, size_t, int));
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
	int retry_cnt, ret, segid;
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
	infop->type = REGION_TYPE_ENV;
	infop->id = REGION_ID_ENV;
	infop->mode = dbenv->db_mode;
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
	if (F_ISSET(dbenv, DB_ENV_PRIVATE))
		goto creation;

	/* Build the region name. */
	(void)snprintf(buf, sizeof(buf), "%s", DB_REGION_ENV);
	if ((ret = __db_appname(dbenv,
	    DB_APP_NONE, NULL, buf, 0, NULL, &infop->name)) != 0)
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
		if ((ret = __os_open(dbenv,
		    infop->name, DB_OSO_REGION | DB_OSO_CREATE | DB_OSO_EXCL,
		    dbenv->db_mode, dbenv->lockfhp)) == 0)
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
	if ((ret = __os_open(dbenv, infop->name,
	    DB_OSO_REGION, dbenv->db_mode, dbenv->lockfhp)) != 0)
		goto err;

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
	 __os_closehandle(dbenv->lockfhp);
#endif

	/* Call the region join routine to acquire the region. */
	memset(&tregion, 0, sizeof(tregion));
	tregion.size = size;
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
	infop->primary = R_ADDR(infop, 0);
	infop->addr = (u_int8_t *)infop->addr + sizeof(REGENV);

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
	renv = infop->primary;
	if (renv->panic) {
		ret = __db_panic_msg(dbenv);
		goto err;
	}
	if (renv->magic != DB_REGION_MAGIC)
		goto retry;

	/* Make sure the region matches our build. */
	if (renv->majver != DB_VERSION_MAJOR ||
	    renv->minver != DB_VERSION_MINOR ||
	    renv->patch != DB_VERSION_PATCH) {
		__db_err(dbenv,
	"Program version %d.%d.%d doesn't match environment version %d.%d.%d",
		    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
		    renv->majver, renv->minver, renv->patch);
#ifndef DIAGNOSTIC
		ret = EINVAL;
		goto err;
#endif
	}

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex, dbenv->lockfhp);

	/*
	 * Finally!  We own the environment now.  Repeat the panic check, it's
	 * possible that it was set while we waited for the lock.
	 */
	if (renv->panic) {
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
	 * If our caller wants them, return the flags this environment was
	 * initialized with.
	 */
	if (init_flagsp != NULL)
		*init_flagsp = renv->init_flags;

	/* Discard our lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/*
	 * Fault the pages into memory.  Note, do this AFTER releasing the
	 * lock, because we're only reading the pages, not writing them.
	 */
	(void)__db_faultmem(infop->primary, rp->size, 0);

	/* Everything looks good, we're done. */
	dbenv->reginfo = infop;
	return (0);

creation:
	/* Create the environment region. */
	F_SET(infop, REGION_CREATE);

	/*
	 * Allocate room for 50 REGION structures plus overhead (we're going
	 * to use this space for last-ditch allocation requests), although we
	 * should never need anything close to that.
	 */
	memset(&tregion, 0, sizeof(tregion));
	tregion.size = 50 * sizeof(REGION) + 50 * sizeof(MUTEX) + 2048;
	tregion.segid = INVALID_REGION_SEGID;
	if ((ret = __os_r_attach(dbenv, infop, &tregion)) != 0)
		goto err;

	/*
	 * Fault the pages into memory.  Note, do this BEFORE we initialize
	 * anything, because we're writing the pages, not just reading them.
	 */
	(void)__db_faultmem(infop->addr, tregion.size, 1);

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
	infop->primary = R_ADDR(infop, 0);
	infop->addr = (u_int8_t *)infop->addr + sizeof(REGENV);
	__db_shalloc_init(infop->addr, tregion.size - sizeof(REGENV));

	/*
	 * Initialize the rest of the REGENV structure, except for the magic
	 * number which validates the file/environment.
	 */
	renv = infop->primary;
	renv->panic = 0;
	db_version(&renv->majver, &renv->minver, &renv->patch);
	SH_LIST_INIT(&renv->regionq);
	renv->refcnt = 1;

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
	if ((ret =
	    __db_mutex_init(dbenv, &renv->mutex, DB_FCNTL_OFF_GEN, 0)) != 0) {
		__db_err(dbenv, "%s: unable to initialize environment lock: %s",
		    infop->name, db_strerror(ret));
		goto err;
	}

	if (!F_ISSET(&renv->mutex, MUTEX_IGNORE) &&
	    (ret = __db_mutex_lock(dbenv, &renv->mutex, dbenv->lockfhp)) != 0) {
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
		if ((ret = __os_write(dbenv, dbenv->lockfhp,
		    &ref, sizeof(ref), &nrw)) != 0 || nrw != sizeof(ref)) {
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
	if (F_ISSET(dbenv->lockfhp, DB_FH_VALID))
		 __os_closehandle(dbenv->lockfhp);
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
	if (F_ISSET(dbenv->lockfhp, DB_FH_VALID))
		(void)__os_closehandle(dbenv->lockfhp);

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
		__os_freestr(infop->name);
	__os_free(infop, sizeof(REGINFO));

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

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex, dbenv->lockfhp);

	/* Decrement the reference count. */
	if (renv->refcnt == 0) {
		__db_err(dbenv,
		    "region %lu (environment): reference count went negative",
		    infop->rp->id);
	} else
		--renv->refcnt;

	/* Release the lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/* Close the locking file handle. */
	if (F_ISSET(dbenv->lockfhp, DB_FH_VALID))
		(void)__os_closehandle(dbenv->lockfhp);

	/* Reset the addr value that we "corrected" above. */
	infop->addr = infop->primary;

	/*
	 * If we are destroying the environment, we need to
	 * destroy any system resources backing the mutex.
	 * Do that now before we free the memory in __os_r_detach.
	 */
	if (destroy)
		__db_mutex_destroy(&renv->mutex);

	/*
	 * Release the region, and kill our reference.
	 *
	 * We set the DBENV->reginfo field to NULL here and discard its memory.
	 * DBENV->remove calls __dbenv_remove to do the region remove, and
	 * __dbenv_remove attached and then detaches from the region.  We don't
	 * want to return to DBENV->remove with a non-NULL DBENV->reginfo field
	 * because it will attempt to detach again as part of its cleanup.
	 */
	(void)__os_r_detach(dbenv, infop, destroy);

	if (infop->name != NULL)
		__os_free(infop->name, 0);
	__os_free(dbenv->reginfo, sizeof(REGINFO));
	dbenv->reginfo = NULL;

	return (0);
}

/*
 * __db_e_remove --
 *	Discard an environment if it's not in use.
 *
 * PUBLIC: int __db_e_remove __P((DB_ENV *, int));
 */
int
__db_e_remove(dbenv, force)
	DB_ENV *dbenv;
	int force;
{
	REGENV *renv;
	REGINFO *infop, reginfo;
	REGION *rp;
	int ret;

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
	if (force)
		dbenv->db_mutexlocks = 0;

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
		goto err;
	}

	infop = dbenv->reginfo;
	renv = infop->primary;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex, dbenv->lockfhp);

	/* If it's in use, we're done. */
	if (renv->refcnt == 1 || force) {
		/*
		 * Set the panic flag and overwrite the magic number.
		 *
		 * !!!
		 * From this point on, there's no going back, we pretty
		 * much ignore errors, and just whack on whatever we can.
		 */
		renv->panic = 1;
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
restart:	for (rp = SH_LIST_FIRST(&renv->regionq, __db_region);
		    rp != NULL; rp = SH_LIST_NEXT(rp, q, __db_region)) {
			if (rp->type == REGION_TYPE_ENV)
				continue;

			reginfo.id = rp->id;
			reginfo.flags = REGION_CREATE_OK;
			if ((ret = __db_r_attach(dbenv, &reginfo, 0)) != 0) {
				__db_err(dbenv,
				    "region %s attach: %s", db_strerror(ret));
				continue;
			}
			R_UNLOCK(dbenv, &reginfo);
			if ((ret = __db_r_detach(dbenv, &reginfo, 1)) != 0) {
				__db_err(dbenv,
				    "region detach: %s", db_strerror(ret));
				continue;
			}
			/*
			 * If we have an error, we continue so we eventually
			 * reach the end of the list.  If we succeed, restart
			 * the list because it was relinked when we destroyed
			 * the entry.
			 */
			goto restart;
		}

		/* Destroy the environment's region. */
		(void)__db_e_detach(dbenv, 1);

		/* Discard the physical files. */
remfiles:	(void)__db_e_remfile(dbenv);
	} else {
		/* Unlock the environment. */
		MUTEX_UNLOCK(dbenv, &renv->mutex);

		/* Discard the environment. */
		(void)__db_e_detach(dbenv, 0);

		ret = EBUSY;
	}

err:
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
	static char *old_region_names[] = {
		"__db_lock.share",
		"__db_log.share",
		"__db_mpool.share",
		"__db_txn.share",
		NULL,
	};
	int cnt, fcnt, lastrm, ret;
	u_int8_t saved_byte;
	const char *dir;
	char *p, **names, *path, buf[sizeof(DB_REGION_FMT) + 20];

	/* Get the full path of a file in the environment. */
	(void)snprintf(buf, sizeof(buf), "%s", DB_REGION_ENV);
	if ((ret =
	    __db_appname(dbenv, DB_APP_NONE, NULL, buf, 0, NULL, &path)) != 0)
		return (ret);

	/* Get the parent directory for the environment. */
	if ((p = __db_rpath(path)) == NULL) {
		p = path;
		saved_byte = *p;

		dir = PATH_DOT;
	} else {
		saved_byte = *p;
		*p = '\0';

		dir = path;
	}

	/* Get the list of file names. */
	ret = __os_dirlist(dbenv, dir, &names, &fcnt);

	/* Restore the path, and free it. */
	*p = saved_byte;
	__os_freestr(path);

	if (ret != 0) {
		__db_err(dbenv, "%s: %s", dir, db_strerror(ret));
		return (ret);
	}

	/*
	 * Search for valid region names, and remove them.  We remove the
	 * environment region last, because it's the key to this whole mess.
	 */
	for (lastrm = -1, cnt = fcnt; --cnt >= 0;) {
		if (strlen(names[cnt]) != DB_REGION_NAME_LENGTH ||
		    memcmp(names[cnt], DB_REGION_FMT, DB_REGION_NAME_NUM) != 0)
			continue;
		if (strcmp(names[cnt], DB_REGION_ENV) == 0) {
			lastrm = cnt;
			continue;
		}
		for (p = names[cnt] + DB_REGION_NAME_NUM;
		    *p != '\0' && isdigit((int)*p); ++p)
			;
		if (*p != '\0')
			continue;

		if (__db_appname(dbenv,
		    DB_APP_NONE, NULL, names[cnt], 0, NULL, &path) == 0) {
			(void)__os_unlink(dbenv, path);
			__os_freestr(path);
		}
	}

	if (lastrm != -1)
		if (__db_appname(dbenv,
		    DB_APP_NONE, NULL, names[lastrm], 0, NULL, &path) == 0) {
			(void)__os_unlink(dbenv, path);
			__os_freestr(path);
		}
	__os_dirfree(names, fcnt);

	/*
	 * !!!
	 * Backward compatibility -- remove region files from releases
	 * before 2.8.XX.
	 */
	for (names = (char **)old_region_names; *names != NULL; ++names)
		if (__db_appname(dbenv,
		    DB_APP_NONE, NULL, *names, 0, NULL, &path) == 0) {
			(void)__os_unlink(dbenv, path);
			__os_freestr(path);
		}

	return (0);
}

/*
 * __db_e_stat
 *	Statistics for the environment.
 *
 * PUBLIC: int __db_e_stat __P((DB_ENV *, REGENV *, REGION *, int *));
 */
int
__db_e_stat(dbenv, arg_renv, arg_regions, arg_regions_cnt)
	DB_ENV *dbenv;
	REGENV *arg_renv;
	REGION *arg_regions;
	int *arg_regions_cnt;
{
	REGENV *renv;
	REGINFO *infop;
	REGION *rp;
	int n;

	infop = dbenv->reginfo;
	renv = infop->primary;
	rp = infop->rp;

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &rp->mutex, dbenv->lockfhp);

	*arg_renv = *renv;

	for (n = 0, rp = SH_LIST_FIRST(&renv->regionq, __db_region);
	    n < *arg_regions_cnt && rp != NULL;
	    ++n, rp = SH_LIST_NEXT(rp, q, __db_region))
		arg_regions[n] = *rp;

	/* Release the lock. */
	rp = infop->rp;
	MUTEX_UNLOCK(dbenv, &rp->mutex);

	*arg_regions_cnt = n == 0 ? n : n - 1;

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
	F_CLR(infop, REGION_CREATE);

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex, dbenv->lockfhp);

	/* Find or create a REGION structure for this region. */
	if ((ret = __db_des_get(dbenv, dbenv->reginfo, infop, &rp)) != 0) {
		MUTEX_UNLOCK(dbenv, &renv->mutex);
		return (ret);
	}
	infop->rp = rp;
	infop->type = rp->type;
	infop->id = rp->id;

	/* If we're creating the region, set the desired size. */
	if (F_ISSET(infop, REGION_CREATE))
		rp->size = size;

	/* Join/create the underlying region. */
	(void)snprintf(buf, sizeof(buf), DB_REGION_FMT, infop->id);
	if ((ret = __db_appname(dbenv,
	    DB_APP_NONE, NULL, buf, 0, NULL, &infop->name)) != 0)
		goto err;
	if ((ret = __os_r_attach(dbenv, infop, rp)) != 0)
		goto err;

	/*
	 * Fault the pages into memory.  Note, do this BEFORE we initialize
	 * anything because we're writing pages in created regions, not just
	 * reading them.
	 */
	(void)__db_faultmem(infop->addr,
	    rp->size, F_ISSET(infop, REGION_CREATE));

	/*
	 * !!!
	 * The underlying layer may have just decided that we are going
	 * to create the region.  There are various system issues that
	 * can result in a useless region that requires re-initialization.
	 *
	 * If we created the region, initialize it for allocation.
	 */
	if (F_ISSET(infop, REGION_CREATE)) {
		((REGION *)(infop->addr))->magic = DB_REGION_MAGIC;

		(void)__db_shalloc_init(infop->addr, rp->size);
	}

	/*
	 * If the underlying REGION isn't the environment, acquire a lock
	 * for it and release our lock on the environment.
	 */
	if (infop->type != REGION_TYPE_ENV) {
		MUTEX_LOCK(dbenv, &rp->mutex, dbenv->lockfhp);
		MUTEX_UNLOCK(dbenv, &renv->mutex);
	}

	return (0);

	/* Discard the underlying region. */
err:	if (infop->addr != NULL)
		(void)__os_r_detach(dbenv,
		    infop, F_ISSET(infop, REGION_CREATE));
	infop->rp = NULL;
	infop->id = INVALID_REGION_ID;

	/* Discard the REGION structure if we created it. */
	if (F_ISSET(infop, REGION_CREATE))
		(void)__db_des_destroy(dbenv, rp);

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

	/* Lock the environment. */
	MUTEX_LOCK(dbenv, &renv->mutex, dbenv->lockfhp);

	/* Acquire the lock for the REGION. */
	MUTEX_LOCK(dbenv, &rp->mutex, dbenv->lockfhp);

	/*
	 * We need to call destroy on per-subsystem info before
	 * we free the memory associated with the region.
	 */
	if (destroy)
		__db_region_destroy(dbenv, infop);

	/* Detach from the underlying OS region. */
	ret = __os_r_detach(dbenv, infop, destroy);

	/* Release the REGION lock. */
	MUTEX_UNLOCK(dbenv, &rp->mutex);

	/* If we destroyed the region, discard the REGION structure. */
	if (destroy &&
	    ((t_ret = __db_des_destroy(dbenv, rp)) != 0) && ret == 0)
		ret = t_ret;

	/* Release the environment lock. */
	MUTEX_UNLOCK(dbenv, &renv->mutex);

	/* Destroy the structure. */
	if (infop->name != NULL)
		__os_freestr(infop->name);

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
		if ((ret = __db_shalloc(env_infop->addr,
		    sizeof(REGION), MUTEX_ALIGN, &rp)) != 0)
			return (ret);

		/* Initialize the region. */
		memset(rp, 0, sizeof(*rp));
		if ((ret = __db_mutex_init(dbenv, &rp->mutex,
		    R_OFFSET(env_infop, &rp->mutex) + DB_FCNTL_OFF_GEN,
		    0)) != 0) {
			__db_shalloc_free(env_infop->addr, rp);
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
__db_des_destroy(dbenv, rp)
	DB_ENV *dbenv;
	REGION *rp;
{
	REGINFO *infop;

	/*
	 * !!!
	 * Called with the environment already locked.
	 */
	infop = dbenv->reginfo;

	SH_LIST_REMOVE(rp, q, __db_region);
	__db_mutex_destroy(&rp->mutex);
	__db_shalloc_free(infop->addr, rp);

	return (0);
}

/*
 * __db_faultmem --
 *	Fault the region into memory.
 */
static int
__db_faultmem(addr, size, created)
	void *addr;
	size_t size;
	int created;
{
	int ret;
	u_int8_t *p, *t;

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
	if (DB_GLOBAL(db_region_init)) {
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
	case REGION_TYPE_MPOOL:
		__mpool_region_destroy(dbenv, infop);
		break;
	case REGION_TYPE_ENV:
	case REGION_TYPE_LOG:
	case REGION_TYPE_MUTEX:
	case REGION_TYPE_TXN:
		break;
	default:
		DB_ASSERT(0);
		break;
	}
}
