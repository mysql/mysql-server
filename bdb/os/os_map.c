/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_map.c,v 11.32 2000/11/30 00:58:42 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif

#ifdef HAVE_SHMGET
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_ext.h"
#include "os_jump.h"

#ifdef HAVE_MMAP
static int __os_map __P((DB_ENV *, char *, DB_FH *, size_t, int, int, void **));
#endif
#ifndef HAVE_SHMGET
static int __db_nosystemmem __P((DB_ENV *));
#endif

/*
 * __os_r_sysattach --
 *	Create/join a shared memory region.
 *
 * PUBLIC: int __os_r_sysattach __P((DB_ENV *, REGINFO *, REGION *));
 */
int
__os_r_sysattach(dbenv, infop, rp)
	DB_ENV *dbenv;
	REGINFO *infop;
	REGION *rp;
{
	if (F_ISSET(dbenv, DB_ENV_SYSTEM_MEM)) {
		/*
		 * If the region is in system memory on UNIX, we use shmget(2).
		 *
		 * !!!
		 * There exist spinlocks that don't work in shmget memory, e.g.,
		 * the HP/UX msemaphore interface.  If we don't have locks that
		 * will work in shmget memory, we better be private and not be
		 * threaded.  If we reach this point, we know we're public, so
		 * it's an error.
		 */
#if defined(MUTEX_NO_SHMGET_LOCKS)
		__db_err(dbenv,
	    "architecture does not support locks inside system shared memory");
		return (EINVAL);
#endif
#if defined(HAVE_SHMGET)
		{
		key_t segid;
		int id, ret;

		/*
		 * We could potentially create based on REGION_CREATE_OK, but
		 * that's dangerous -- we might get crammed in sideways if
		 * some of the expected regions exist but others do not.  Also,
		 * if the requested size differs from an existing region's
		 * actual size, then all sorts of nasty things can happen.
		 * Basing create solely on REGION_CREATE is much safer -- a
		 * recovery will get us straightened out.
		 */
		if (F_ISSET(infop, REGION_CREATE)) {
			/*
			 * The application must give us a base System V IPC key
			 * value.  Adjust that value based on the region's ID,
			 * and correct so the user's original value appears in
			 * the ipcs output.
			 */
			if (dbenv->shm_key == INVALID_REGION_SEGID) {
				__db_err(dbenv,
			    "no base system shared memory ID specified");
				return (EINVAL);
			}
			segid = (key_t)(dbenv->shm_key + (infop->id - 1));

			/*
			 * If map to an existing region, assume the application
			 * crashed and we're restarting.  Delete the old region
			 * and re-try.  If that fails, return an error, the
			 * application will have to select a different segment
			 * ID or clean up some other way.
			 */
			if ((id = shmget(segid, 0, 0)) != -1) {
				(void)shmctl(id, IPC_RMID, NULL);
				if ((id = shmget(segid, 0, 0)) != -1) {
					__db_err(dbenv,
		"shmget: key: %ld: shared system memory region already exists",
					    (long)segid);
					return (EAGAIN);
				}
			}
			if ((id =
			    shmget(segid, rp->size, IPC_CREAT | 0600)) == -1) {
				ret = __os_get_errno();
				__db_err(dbenv,
	"shmget: key: %ld: unable to create shared system memory region: %s",
				    (long)segid, strerror(ret));
				return (ret);
			}
			rp->segid = id;
		} else
			id = rp->segid;

		if ((infop->addr = shmat(id, NULL, 0)) == (void *)-1) {
			infop->addr = NULL;
			ret = __os_get_errno();
			__db_err(dbenv,
	"shmat: id %d: unable to attach to shared system memory region: %s",
			    id, strerror(ret));
			return (ret);
		}

		return (0);
		}
#else
		return (__db_nosystemmem(dbenv));
#endif
	}

#ifdef HAVE_MMAP
	{
	DB_FH fh;
	int ret;

	/*
	 * Try to open/create the shared region file.  We DO NOT need to
	 * ensure that multiple threads/processes attempting to
	 * simultaneously create the region are properly ordered,
	 * our caller has already taken care of that.
	 */
	if ((ret = __os_open(dbenv, infop->name, DB_OSO_REGION |
	    (F_ISSET(infop, REGION_CREATE_OK) ? DB_OSO_CREATE : 0),
	    infop->mode, &fh)) != 0)
		__db_err(dbenv, "%s: %s", infop->name, db_strerror(ret));

	/*
	 * If we created the file, grow it to its full size before mapping
	 * it in.  We really want to avoid touching the buffer cache after
	 * mmap(2) is called, doing anything else confuses the hell out of
	 * systems without merged VM/buffer cache systems, or, more to the
	 * point, *badly* merged VM/buffer cache systems.
	 */
	if (ret == 0 && F_ISSET(infop, REGION_CREATE))
		ret = __os_finit(dbenv,
		    &fh, rp->size, DB_GLOBAL(db_region_init));

	/* Map the file in. */
	if (ret == 0)
		ret = __os_map(dbenv,
		    infop->name, &fh, rp->size, 1, 0, &infop->addr);

	 (void)__os_closehandle(&fh);

	return (ret);
	}
#else
	COMPQUIET(infop, NULL);
	COMPQUIET(rp, NULL);
	__db_err(dbenv,
	    "architecture lacks mmap(2), shared environments not possible");
	return (__db_eopnotsup(dbenv));
#endif
}

/*
 * __os_r_sysdetach --
 *	Detach from a shared memory region.
 *
 * PUBLIC: int __os_r_sysdetach __P((DB_ENV *, REGINFO *, int));
 */
int
__os_r_sysdetach(dbenv, infop, destroy)
	DB_ENV *dbenv;
	REGINFO *infop;
	int destroy;
{
	REGION *rp;

	rp = infop->rp;

	if (F_ISSET(dbenv, DB_ENV_SYSTEM_MEM)) {
#ifdef HAVE_SHMGET
		int ret, segid;

		/*
		 * We may be about to remove the memory referenced by rp,
		 * save the segment ID, and (optionally) wipe the original.
		 */
		segid = rp->segid;
		if (destroy)
			rp->segid = INVALID_REGION_SEGID;

		if (shmdt(infop->addr) != 0) {
			ret = __os_get_errno();
			__db_err(dbenv, "shmdt: %s", strerror(ret));
			return (ret);
		}

		if (destroy && shmctl(segid, IPC_RMID,
		    NULL) != 0 && (ret = __os_get_errno()) != EINVAL) {
			__db_err(dbenv,
	    "shmctl: id %ld: unable to delete system shared memory region: %s",
			    segid, strerror(ret));
			return (ret);
		}

		return (0);
#else
		return (__db_nosystemmem(dbenv));
#endif
	}

#ifdef HAVE_MMAP
#ifdef HAVE_MUNLOCK
	if (F_ISSET(dbenv, DB_ENV_LOCKDOWN))
		(void)munlock(infop->addr, rp->size);
#endif
	if (munmap(infop->addr, rp->size) != 0) {
		int ret;

		ret = __os_get_errno();
		__db_err(dbenv, "munmap: %s", strerror(ret));
		return (ret);
	}

	if (destroy && __os_region_unlink(dbenv, infop->name) != 0)
		return (__os_get_errno());

	return (0);
#else
	COMPQUIET(destroy, 0);
	return (EINVAL);
#endif
}

/*
 * __os_mapfile --
 *	Map in a shared memory file.
 *
 * PUBLIC: int __os_mapfile __P((DB_ENV *,
 * PUBLIC:     char *, DB_FH *, size_t, int, void **));
 */
int
__os_mapfile(dbenv, path, fhp, len, is_rdonly, addrp)
	DB_ENV *dbenv;
	char *path;
	DB_FH *fhp;
	int is_rdonly;
	size_t len;
	void **addrp;
{
#if defined(HAVE_MMAP) && !defined(HAVE_QNX)
	return (__os_map(dbenv, path, fhp, len, 0, is_rdonly, addrp));
#else
	COMPQUIET(dbenv, NULL);
	COMPQUIET(path, NULL);
	COMPQUIET(fhp, NULL);
	COMPQUIET(is_rdonly, 0);
	COMPQUIET(len, 0);
	COMPQUIET(addrp, NULL);
	return (EINVAL);
#endif
}

/*
 * __os_unmapfile --
 *	Unmap the shared memory file.
 *
 * PUBLIC: int __os_unmapfile __P((DB_ENV *, void *, size_t));
 */
int
__os_unmapfile(dbenv, addr, len)
	DB_ENV *dbenv;
	void *addr;
	size_t len;
{
	/* If the user replaced the map call, call through their interface. */
	if (__db_jump.j_unmap != NULL)
		return (__db_jump.j_unmap(addr, len));

#ifdef HAVE_MMAP
#ifdef HAVE_MUNLOCK
	if (F_ISSET(dbenv, DB_ENV_LOCKDOWN))
		(void)munlock(addr, len);
#else
	COMPQUIET(dbenv, NULL);
#endif
	return (munmap(addr, len) ? __os_get_errno() : 0);
#else
	COMPQUIET(dbenv, NULL);

	return (EINVAL);
#endif
}

#ifdef HAVE_MMAP
/*
 * __os_map --
 *	Call the mmap(2) function.
 */
static int
__os_map(dbenv, path, fhp, len, is_region, is_rdonly, addrp)
	DB_ENV *dbenv;
	char *path;
	DB_FH *fhp;
	int is_region, is_rdonly;
	size_t len;
	void **addrp;
{
	void *p;
	int flags, prot, ret;

	/* If the user replaced the map call, call through their interface. */
	if (__db_jump.j_map != NULL)
		return (__db_jump.j_map
		    (path, len, is_region, is_rdonly, addrp));

	/*
	 * If it's read-only, it's private, and if it's not, it's shared.
	 * Don't bother with an additional parameter.
	 */
	flags = is_rdonly ? MAP_PRIVATE : MAP_SHARED;

#ifdef MAP_FILE
	/*
	 * Historically, MAP_FILE was required for mapping regular files,
	 * even though it was the default.  Some systems have it, some
	 * don't, some that have it set it to 0.
	 */
	flags |= MAP_FILE;
#endif

	/*
	 * I know of no systems that implement the flag to tell the system
	 * that the region contains semaphores, but it's not an unreasonable
	 * thing to do, and has been part of the design since forever.  I
	 * don't think anyone will object, but don't set it for read-only
	 * files, it doesn't make sense.
	 */
#ifdef MAP_HASSEMAPHORE
	if (is_region && !is_rdonly)
		flags |= MAP_HASSEMAPHORE;
#else
	COMPQUIET(is_region, 0);
#endif

	prot = PROT_READ | (is_rdonly ? 0 : PROT_WRITE);

	/*
	 * XXX
	 * Work around a bug in the VMS V7.1 mmap() implementation.  To map
	 * a file into memory on VMS it needs to be opened in a certain way,
	 * originally.  To get the file opened in that certain way, the VMS
	 * mmap() closes the file and re-opens it.  When it does this, it
	 * doesn't flush any caches out to disk before closing.  The problem
	 * this causes us is that when the memory cache doesn't get written
	 * out, the file isn't big enough to match the memory chunk and the
	 * mmap() call fails.  This call to fsync() fixes the problem.  DEC
	 * thinks this isn't a bug because of language in XPG5 discussing user
	 * responsibility for on-disk and in-memory synchronization.
	 */
#ifdef VMS
	if (__os_fsync(dbenv, fhp) == -1)
		return (__os_get_errno());
#endif

	/* MAP_FAILED was not defined in early mmap implementations. */
#ifndef MAP_FAILED
#define	MAP_FAILED	-1
#endif
	if ((p = mmap(NULL,
	    len, prot, flags, fhp->fd, (off_t)0)) == (void *)MAP_FAILED) {
		ret = __os_get_errno();
		__db_err(dbenv, "mmap: %s", strerror(ret));
		return (ret);
	}

#ifdef HAVE_MLOCK
	/*
	 * If it's a region, we want to make sure that the memory isn't paged.
	 * For example, Solaris will page large mpools because it thinks that
	 * I/O buffer memory is more important than we are.  The mlock system
	 * call may or may not succeed (mlock is restricted to the super-user
	 * on some systems).  Currently, the only other use of mmap in DB is
	 * to map read-only databases -- we don't want them paged, either, so
	 * the call isn't conditional.
	 */
	if (F_ISSET(dbenv, DB_ENV_LOCKDOWN) && mlock(p, len) != 0) {
		ret = __os_get_errno();
		(void)munmap(p, len);
		__db_err(dbenv, "mlock: %s", strerror(ret));
		return (ret);
	}
#else
	COMPQUIET(dbenv, NULL);
#endif

	*addrp = p;
	return (0);
}
#endif

#ifndef HAVE_SHMGET
/*
 * __db_nosystemmem --
 *	No system memory environments error message.
 */
static int
__db_nosystemmem(dbenv)
	DB_ENV *dbenv;
{
	__db_err(dbenv,
	    "architecture doesn't support environments in system memory");
	return (__db_eopnotsup(dbenv));
}
#endif
