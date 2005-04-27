/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_method.c,v 11.29 2002/03/27 04:32:27 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef HAVE_RPC
#include <rpc/rpc.h>
#endif
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/mp.h"

#ifdef HAVE_RPC
#include "dbinc_auto/db_server.h"
#include "dbinc_auto/rpc_client_ext.h"
#endif

static int __memp_set_cachesize __P((DB_ENV *, u_int32_t, u_int32_t, int));
static int __memp_set_mp_mmapsize __P((DB_ENV *, size_t));

/*
 * __memp_dbenv_create --
 *	Mpool specific creation of the DB_ENV structure.
 *
 * PUBLIC: void __memp_dbenv_create __P((DB_ENV *));
 */
void
__memp_dbenv_create(dbenv)
	DB_ENV *dbenv;
{
	/*
	 * !!!
	 * Our caller has not yet had the opportunity to reset the panic
	 * state or turn off mutex locking, and so we can neither check
	 * the panic state or acquire a mutex in the DB_ENV create path.
	 *
	 * We default to 32 8K pages.  We don't default to a flat 256K, because
	 * some systems require significantly more memory to hold 32 pages than
	 * others.  For example, HP-UX with POSIX pthreads needs 88 bytes for
	 * a POSIX pthread mutex and almost 200 bytes per buffer header, while
	 * Solaris needs 24 and 52 bytes for the same structures.  The minimum
	 * number of hash buckets is 37.  These contain a mutex also.
	 */
	dbenv->mp_bytes =
	    32 * ((8 * 1024) + sizeof(BH)) + 37 * sizeof(DB_MPOOL_HASH);
	dbenv->mp_ncache = 1;

#ifdef HAVE_RPC
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->set_cachesize = __dbcl_env_cachesize;
		dbenv->set_mp_mmapsize = __dbcl_set_mp_mmapsize;
		dbenv->memp_dump_region = NULL;
		dbenv->memp_fcreate = __dbcl_memp_fcreate;
		dbenv->memp_nameop = NULL;
		dbenv->memp_register = __dbcl_memp_register;
		dbenv->memp_stat = __dbcl_memp_stat;
		dbenv->memp_sync = __dbcl_memp_sync;
		dbenv->memp_trickle = __dbcl_memp_trickle;
	} else
#endif
	{
		dbenv->set_cachesize = __memp_set_cachesize;
		dbenv->set_mp_mmapsize = __memp_set_mp_mmapsize;
		dbenv->memp_dump_region = __memp_dump_region;
		dbenv->memp_fcreate = __memp_fcreate;
		dbenv->memp_nameop = __memp_nameop;
		dbenv->memp_register = __memp_register;
		dbenv->memp_stat = __memp_stat;
		dbenv->memp_sync = __memp_sync;
		dbenv->memp_trickle = __memp_trickle;
	}
}

/*
 * __memp_set_cachesize --
 *	Initialize the cache size.
 */
static int
__memp_set_cachesize(dbenv, gbytes, bytes, ncache)
	DB_ENV *dbenv;
	u_int32_t gbytes, bytes;
	int ncache;
{
	ENV_ILLEGAL_AFTER_OPEN(dbenv, "set_cachesize");

	/* Normalize the values. */
	if (ncache == 0)
		ncache = 1;

	/*
	 * You can only store 4GB-1 in an unsigned 32-bit value, so correct for
	 * applications that specify 4GB cache sizes -- we know what they meant.
	 */
	if (gbytes / ncache == 4 && bytes == 0) {
		--gbytes;
		bytes = GIGABYTE - 1;
	} else {
		gbytes += bytes / GIGABYTE;
		bytes %= GIGABYTE;
	}

	/* Avoid too-large cache sizes, they result in a region size of zero. */
	if (gbytes / ncache > 4 || (gbytes / ncache == 4 && bytes != 0)) {
		__db_err(dbenv, "individual cache size too large");
		return (EINVAL);
	}

	/*
	 * If the application requested less than 500Mb, increase the cachesize
	 * by 25% and factor in the size of the hash buckets to account for our
	 * overhead.  (I'm guessing caches over 500Mb are specifically sized,
	 * that is, it's a large server and the application actually knows how
	 * much memory is available.  We only document the 25% overhead number,
	 * not the hash buckets, but I don't see a reason to confuse the issue,
	 * it shouldn't matter to an application.)
	 *
	 * There is a minimum cache size, regardless.
	 */
	if (gbytes == 0) {
		if (bytes < 500 * MEGABYTE)
			bytes += (bytes / 4) + 37 * sizeof(DB_MPOOL_HASH);
		if (bytes / ncache < DB_CACHESIZE_MIN)
			bytes = ncache * DB_CACHESIZE_MIN;
	}

	dbenv->mp_gbytes = gbytes;
	dbenv->mp_bytes = bytes;
	dbenv->mp_ncache = ncache;

	return (0);
}

/*
 * __memp_set_mp_mmapsize --
 *	Set the maximum mapped file size.
 */
static int
__memp_set_mp_mmapsize(dbenv, mp_mmapsize )
	DB_ENV *dbenv;
	size_t mp_mmapsize;
{
	dbenv->mp_mmapsize = mp_mmapsize;
	return (0);
}
