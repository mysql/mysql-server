/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_mpool.cpp,v 11.11 2000/09/21 15:05:45 dda Exp $";
#endif /* not lint */

#include <errno.h>

#include "db_cxx.h"
#include "cxx_int.h"

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbMpoolFile                             //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbMpoolFile::DbMpoolFile()
:	imp_(0)
{
}

DbMpoolFile::~DbMpoolFile()
{
}

int DbMpoolFile::open(DbEnv *envp, const char *file,
		      u_int32_t flags, int mode, size_t pagesize,
		      DB_MPOOL_FINFO *finfop, DbMpoolFile **result)
{
	int err;

	DB_MPOOLFILE *mpf;
	DB_ENV *env = unwrap(envp);

	if ((err = ::memp_fopen(env, file, flags, mode, pagesize,
				finfop, &mpf)) != 0) {
		DB_ERROR("DbMpoolFile::open", err, envp->error_policy());
		return (err);
	}
	*result = new DbMpoolFile();
	(*result)->imp_ = wrap(mpf);
	return (0);
}

int DbMpoolFile::close()
{
	DB_MPOOLFILE *mpf = unwrap(this);
	int err = 0;
	if (!mpf) {
		err = EINVAL;
	}
	else if ((err = ::memp_fclose(mpf)) != 0) {
		DB_ERROR("DbMpoolFile::close", err, ON_ERROR_UNKNOWN);
		return (err);
	}
	imp_ = 0;                   // extra safety

	// This may seem weird, but is legal as long as we don't access
	// any data before returning.
	//
	delete this;
	return (0);
}

int DbMpoolFile::get(db_pgno_t *pgnoaddr, u_int32_t flags, void *pagep)
{
	DB_MPOOLFILE *mpf = unwrap(this);
	int err = 0;
	if (!mpf) {
		err = EINVAL;
	}
	else if ((err = ::memp_fget(mpf, pgnoaddr, flags, pagep)) != 0) {
		DB_ERROR("DbMpoolFile::get", err, ON_ERROR_UNKNOWN);
	}
	return (err);
}

int DbMpoolFile::put(void *pgaddr, u_int32_t flags)
{
	DB_MPOOLFILE *mpf = unwrap(this);
	int err = 0;
	if (!mpf) {
		err = EINVAL;
	}
	else if ((err = ::memp_fput(mpf, pgaddr, flags)) != 0) {
		DB_ERROR("DbMpoolFile::put", err, ON_ERROR_UNKNOWN);
	}
	return (err);
}

int DbMpoolFile::set(void *pgaddr, u_int32_t flags)
{
	DB_MPOOLFILE *mpf = unwrap(this);
	int err = 0;
	if (!mpf) {
		err = EINVAL;
	}
	else if ((err = ::memp_fset(mpf, pgaddr, flags)) != 0) {
		DB_ERROR("DbMpoolFile::set", err, ON_ERROR_UNKNOWN);
	}
	return (err);
}

int DbMpoolFile::sync()
{
	DB_MPOOLFILE *mpf = unwrap(this);
	int err = 0;
	if (!mpf) {
		err = EINVAL;
	}
	else if ((err = ::memp_fsync(mpf)) != 0 && err != DB_INCOMPLETE) {
		DB_ERROR("DbMpoolFile::sync", err, ON_ERROR_UNKNOWN);
	}
	return (err);
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbMpool                                 //
//                                                                    //
////////////////////////////////////////////////////////////////////////

int DbEnv::memp_register(int ftype,
			 pgin_fcn_type pgin_fcn,
			 pgout_fcn_type pgout_fcn)
{
	DB_ENV *env = unwrap(this);
	int err = 0;

	if ((err = ::memp_register(env, ftype, pgin_fcn, pgout_fcn)) != 0) {
		DB_ERROR("DbEnv::memp_register", err, error_policy());
		return (err);
	}
	return (err);
}

int DbEnv::memp_stat(DB_MPOOL_STAT **gsp, DB_MPOOL_FSTAT ***fsp,
		     db_malloc_fcn_type db_malloc_fcn)
{
	DB_ENV *env = unwrap(this);
	int err = 0;

	if ((err = ::memp_stat(env, gsp, fsp, db_malloc_fcn)) != 0) {
		DB_ERROR("DbEnv::memp_stat", err, error_policy());
		return (err);
	}
	return (err);
}

int DbEnv::memp_sync(DbLsn *sn)
{
	DB_ENV *env = unwrap(this);
	int err = 0;

	if ((err = ::memp_sync(env, sn)) != 0 && err != DB_INCOMPLETE) {
		DB_ERROR("DbEnv::memp_sync", err, error_policy());
		return (err);
	}
	return (err);
}

int DbEnv::memp_trickle(int pct, int *nwrotep)
{
	DB_ENV *env = unwrap(this);
	int err = 0;

	if ((err = ::memp_trickle(env, pct, nwrotep)) != 0) {
		DB_ERROR("DbEnv::memp_trickle", err, error_policy());
		return (err);
	}
	return (err);
}
