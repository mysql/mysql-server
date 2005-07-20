/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: cxx_logc.cpp,v 11.13 2004/02/05 02:25:12 mjc Exp $
 */

#include "db_config.h"

#include <errno.h>
#include <string.h>

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc_auto/db_auto.h"
#include "dbinc_auto/crdel_auto.h"
#include "dbinc/db_dispatch.h"
#include "dbinc_auto/db_ext.h"
#include "dbinc_auto/common_ext.h"

// It's private, and should never be called,
// but some compilers need it resolved
//
DbLogc::~DbLogc()
{
}

// The name _flags prevents a name clash with __db_log_cursor::flags
int DbLogc::close(u_int32_t _flags)
{
	DB_LOGC *logc = this;
	int ret;
	DbEnv *dbenv2 = DbEnv::get_DbEnv(logc->dbenv);

	ret = logc->close(logc, _flags);

	if (!DB_RETOK_STD(ret))
		DB_ERROR(dbenv2, "DbLogc::close", ret, ON_ERROR_UNKNOWN);

	return (ret);
}

// The name _flags prevents a name clash with __db_log_cursor::flags
int DbLogc::get(DbLsn *lsn, Dbt *data, u_int32_t _flags)
{
	DB_LOGC *logc = this;
	int ret;

	ret = logc->get(logc, lsn, data, _flags);

	if (!DB_RETOK_LGGET(ret)) {
		if (ret == DB_BUFFER_SMALL)
			DB_ERROR_DBT(DbEnv::get_DbEnv(logc->dbenv),
				"DbLogc::get", data, ON_ERROR_UNKNOWN);
		else
			DB_ERROR(DbEnv::get_DbEnv(logc->dbenv),
				"DbLogc::get", ret, ON_ERROR_UNKNOWN);
	}

	return (ret);
}
