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
SQLDataSources(
    SQLHENV EnvironmentHandle,
    SQLUSMALLINT Direction,
    SQLCHAR* ServerName,
    SQLSMALLINT BufferLength1,
    SQLSMALLINT* NameLength1,
    SQLCHAR* Description,
    SQLSMALLINT BufferLength2,
    SQLSMALLINT* NameLength2)
{
#ifndef auto_SQLDataSources
    const char* const sqlFunction = "SQLDataSources";
    Ctx ctx;
    ctx_log1(("*** not implemented: %s", sqlFunction));
    return SQL_ERROR;
#else
    driver_enter(SQL_API_SQLDATASOURCES);
    const char* const sqlFunction = "SQLDataSources";
    HandleRoot* const pRoot = HandleRoot::instance();
    HandleEnv* pEnv = pRoot->findEnv(EnvironmentHandle);
    if (pEnv == 0) {
	driver_exit(SQL_API_SQLDATASOURCES);
        return SQL_INVALID_HANDLE;
    }
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    if (ctx.ok())
        pEnv->sqlDataSources(
            ctx,
            Direction,
            &ServerName,
            BufferLength1,
            &NameLength1,
            &Description,
            BufferLength2,
            &NameLength2
        );
    pEnv->saveCtx(ctx);
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    driver_exit(SQL_API_SQLDATASOURCES);
    return ret;
#endif
}
#endif // ODBCVER >= 0x0000
