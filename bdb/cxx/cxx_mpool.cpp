/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_mpool.cpp,v 11.20 2002/07/03 21:03:53 bostic Exp $";
#endif /* not lint */

#include <errno.h>

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

#include "db_int.h"

// Helper macros for simple methods that pass through to the
// underlying C method. It may return an error or raise an exception.
// Note this macro expects that input _argspec is an argument
// list element (e.g., "char *arg") and that _arglist is the arguments
// that should be passed through to the C method (e.g., "(mpf, arg)")
//
#define	DB_MPOOLFILE_METHOD(_name, _argspec, _arglist, _retok)		\
int DbMpoolFile::_name _argspec						\
{									\
	int ret;							\
	DB_MPOOLFILE *mpf = unwrap(this);				\
									\
	if (mpf == NULL)						\
		ret = EINVAL;						\
	else								\
		ret = mpf->_name _arglist;				\
	if (!_retok(ret))						\
		DB_ERROR("DbMpoolFile::"#_name, ret, ON_ERROR_UNKNOWN);	\
	return (ret);							\
}

#define	DB_MPOOLFILE_METHOD_VOID(_name, _argspec, _arglist)		\
void DbMpoolFile::_name _argspec					\
{									\
	DB_MPOOLFILE *mpf = unwrap(this);				\
									\
	mpf->_name _arglist;						\
}

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

int DbMpoolFile::close(u_int32_t flags)
{
	DB_MPOOLFILE *mpf = unwrap(this);
	int ret;

	if (mpf == NULL)
		ret = EINVAL;
	else
		ret = mpf->close(mpf, flags);

	imp_ = 0;                   // extra safety

	// This may seem weird, but is legal as long as we don't access
	// any data before returning.
	delete this;

	if (!DB_RETOK_STD(ret))
		DB_ERROR("DbMpoolFile::close", ret, ON_ERROR_UNKNOWN);

	return (ret);
}

DB_MPOOLFILE_METHOD(get, (db_pgno_t *pgnoaddr, u_int32_t flags, void *pagep),
    (mpf, pgnoaddr, flags, pagep), DB_RETOK_MPGET)
DB_MPOOLFILE_METHOD_VOID(last_pgno, (db_pgno_t *pgnoaddr), (mpf, pgnoaddr))
DB_MPOOLFILE_METHOD(open,
    (const char *file, u_int32_t flags, int mode, size_t pagesize),
    (mpf, file, flags, mode, pagesize), DB_RETOK_STD)
DB_MPOOLFILE_METHOD(put, (void *pgaddr, u_int32_t flags),
    (mpf, pgaddr, flags), DB_RETOK_STD)
DB_MPOOLFILE_METHOD_VOID(refcnt, (db_pgno_t *pgnoaddr), (mpf, pgnoaddr))
DB_MPOOLFILE_METHOD(set, (void *pgaddr, u_int32_t flags),
    (mpf, pgaddr, flags), DB_RETOK_STD)
DB_MPOOLFILE_METHOD(set_clear_len, (u_int32_t len),
    (mpf, len), DB_RETOK_STD)
DB_MPOOLFILE_METHOD(set_fileid, (u_int8_t *fileid),
    (mpf, fileid), DB_RETOK_STD)
DB_MPOOLFILE_METHOD(set_ftype, (int ftype),
    (mpf, ftype), DB_RETOK_STD)
DB_MPOOLFILE_METHOD(set_lsn_offset, (int32_t offset),
    (mpf, offset), DB_RETOK_STD)
DB_MPOOLFILE_METHOD(set_pgcookie, (DBT *dbt),
    (mpf, dbt), DB_RETOK_STD)
DB_MPOOLFILE_METHOD_VOID(set_unlink, (int ul), (mpf, ul))
DB_MPOOLFILE_METHOD(sync, (),
    (mpf), DB_RETOK_STD)
