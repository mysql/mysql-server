/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "driver.hpp"

#if ODBCVER >= 0x0000
SQLRETURN SQL_API
SQLTransact(
    SQLHENV EnvironmentHandle,
    SQLHDBC ConnectionHandle,
    SQLUSMALLINT CompletionType)
{
    driver_enter(SQL_API_SQLTRANSACT);
    const char* const sqlFunction = "SQLTransact";
    HandleRoot* const pRoot = HandleRoot::instance();
    // check connection first and ignore environment
    if (ConnectionHandle != SQL_NULL_HANDLE) {
	HandleDbc* pDbc = pRoot->findDbc(ConnectionHandle);
	if (pDbc == 0) {
	    driver_exit(SQL_API_SQLTRANSACT);
	    return SQL_INVALID_HANDLE;
	}
	Ctx& ctx = *new Ctx;
	ctx.logSqlEnter(sqlFunction);
	try {
	    pDbc->sqlTransact(ctx, CompletionType);
	} catch (CtxAssert& ctxAssert) {
	    ctx.handleEx(ctxAssert);
	}
	pDbc->saveCtx(ctx);
	ctx.logSqlExit();
	SQLRETURN ret = ctx.getCode();
	driver_exit(SQL_API_SQLTRANSACT);
	return ret;
    }
    if (EnvironmentHandle != SQL_NULL_HANDLE) {
	HandleEnv* pEnv = pRoot->findEnv(EnvironmentHandle);
	if (pEnv == 0) {
	    driver_exit(SQL_API_SQLTRANSACT);
	    return SQL_INVALID_HANDLE;
	}
	Ctx& ctx = *new Ctx;
	ctx.logSqlEnter(sqlFunction);
	try {
	    pEnv->sqlTransact(ctx, CompletionType);
	} catch (CtxAssert& ctxAssert) {
	    ctx.handleEx(ctxAssert);
	}
	pEnv->saveCtx(ctx);
	ctx.logSqlExit();
	SQLRETURN ret = ctx.getCode();
	driver_exit(SQL_API_SQLTRANSACT);
	return ret;
    }
    driver_exit(SQL_API_SQLTRANSACT);
    return SQL_INVALID_HANDLE;
}
#endif // ODBCVER >= 0x0000
