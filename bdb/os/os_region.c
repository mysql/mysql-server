/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_region.c,v 11.9 2000/11/30 00:58:42 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#endif

#include "db_int.h"
#include "os_jump.h"

/*
 * __os_r_attach --
 *	Attach to a shared memory region.
 *
 * PUBLIC: int __os_r_attach __P((DB_ENV *, REGINFO *, REGION *));
 */
int
__os_r_attach(dbenv, infop, rp)
	DB_ENV *dbenv;
	REGINFO *infop;
	REGION *rp;
{
	int ret;
	/* Round off the requested size for the underlying VM. */
	OS_VMROUNDOFF(rp->size);

#ifdef DB_REGIONSIZE_MAX
	/* Some architectures have hard limits on the maximum region size. */
	if (rp->size > DB_REGIONSIZE_MAX) {
		__db_err(dbenv, "region size %lu is too large; maximum is %lu",
		    (u_long)rp->size, (u_long)DB_REGIONSIZE_MAX);
		return (EINVAL);
	}
#endif

	/*
	 * If a region is private, malloc the memory.
	 *
	 * !!!
	 * If this fails because the region is too large to malloc, mmap(2)
	 * using the MAP_ANON or MAP_ANONYMOUS flags would be an alternative.
	 * I don't know of any architectures (yet!) where malloc is a problem.
	 */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
#if defined(MUTEX_NO_MALLOC_LOCKS)
		/*
		 * !!!
		 * There exist spinlocks that don't work in malloc memory, e.g.,
		 * the HP/UX msemaphore interface.  If we don't have locks that
		 * will work in malloc memory, we better not be private or not
		 * be threaded.
		 */
		if (F_ISSET(dbenv, DB_ENV_THREAD)) {
			__db_err(dbenv, "%s",
    "architecture does not support locks inside process-local (malloc) memory");
			__db_err(dbenv, "%s",
    "application may not specify both DB_PRIVATE and DB_THREAD");
			return (EINVAL);
		}
#endif
		if ((ret =
		    __os_malloc(dbenv, rp->size, NULL, &infop->addr)) != 0)
			return (ret);
#if defined(UMRW) && !defined(DIAGNOSTIC)
		memset(infop->addr, CLEAR_BYTE, rp->size);
#endif
		return (0);
	}

	/* If the user replaced the map call, call through their interface. */
	if (__db_jump.j_map != NULL)
		return (__db_jump.j_map(infop->name,
		    rp->size, 1, 0, &infop->addr));

	return (__os_r_sysattach(dbenv, infop, rp));
}

/*
 * __os_r_detach --
 *	Detach from a shared memory region.
 *
 * PUBLIC: int __os_r_detach __P((DB_ENV *, REGINFO *, int));
 */
int
__os_r_detach(dbenv, infop, destroy)
	DB_ENV *dbenv;
	REGINFO *infop;
	int destroy;
{
	REGION *rp;

	rp = infop->rp;

	/* If a region is private, free the memory. */
	if (F_ISSET(dbenv, DB_ENV_PRIVATE)) {
		__os_free(infop->addr, rp->size);
		return (0);
	}

	/* If the user replaced the map call, call through their interface. */
	if (__db_jump.j_unmap != NULL)
		return (__db_jump.j_unmap(infop->addr, rp->size));

	return (__os_r_sysdetach(dbenv, infop, destroy));
}
