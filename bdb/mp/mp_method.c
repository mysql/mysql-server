/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */
#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: mp_method.c,v 11.10 2000/04/04 20:12:04 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#ifdef  HAVE_RPC
#include "db_server.h"
#endif

#include "db_int.h"
#include "db_shash.h"
#include "mp.h"

#ifdef HAVE_RPC
#include "gen_client_ext.h"
#include "rpc_client_ext.h"
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
	 * We default to 32 8K pages.  We don't default to a flat 256K, because
	 * some systems require significantly more memory to hold 32 pages than
	 * others.  For example, HP-UX with POSIX pthreads needs 88 bytes for
	 * a POSIX pthread mutex and almost 200 bytes per buffer header, while
	 * Solaris needs 24 and 52 bytes for the same structures.
	 */
	dbenv->mp_bytes = 32 * ((8 * 1024) + sizeof(BH));
	dbenv->mp_ncache = 1;

	dbenv->set_mp_mmapsize = __memp_set_mp_mmapsize;
	dbenv->set_cachesize = __memp_set_cachesize;

#ifdef	HAVE_RPC
	/*
	 * If we have a client, overwrite what we just setup to
	 * point to client functions.
	 */
	if (F_ISSET(dbenv, DB_ENV_RPCCLIENT)) {
		dbenv->set_cachesize = __dbcl_env_cachesize;
		dbenv->set_mp_mmapsize = __dbcl_set_mp_mmapsize;
	}
#endif

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

	dbenv->mp_gbytes = gbytes + bytes / GIGABYTE;
	dbenv->mp_bytes = bytes % GIGABYTE;
	dbenv->mp_ncache = ncache == 0 ? 1 : ncache;

	/*
	 * If the application requested less than 500Mb, increase the
	 * cachesize by 25% to account for our overhead.  (I'm guessing
	 * that caches over 500Mb are specifically sized, i.e., it's
	 * a large server and the application actually knows how much
	 * memory is available.)
	 *
	 * There is a minimum cache size, regardless.
	 */
	if (dbenv->mp_gbytes == 0) {
		if (dbenv->mp_bytes < 500 * MEGABYTE)
			dbenv->mp_bytes += dbenv->mp_bytes / 4;
		if (dbenv->mp_bytes < DB_CACHESIZE_MIN)
			dbenv->mp_bytes = DB_CACHESIZE_MIN;
	}

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
