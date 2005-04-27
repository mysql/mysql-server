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

#if ODBCVER >= 0x0300
SQLRETURN SQL_API
SQLCopyDesc(
    SQLHDESC SourceDescHandle,
    SQLHDESC TargetDescHandle)
{
#ifndef auto_SQLCopyDesc
    const char* const sqlFunction = "SQLCopyDesc";
    Ctx ctx;
    ctx_log1(("*** not implemented: %s", sqlFunction));
    return SQL_ERROR;
#else
    driver_enter(SQL_API_SQLCOPYDESC);
    const char* const sqlFunction = "SQLCopyDesc";
    HandleRoot* const pRoot = HandleRoot::instance();
    HandleDesc* pDesc = pRoot->findDesc(SourceDescHandle);
    if (pDesc == 0) {
	driver_exit(SQL_API_SQLCOPYDESC);
        return SQL_INVALID_HANDLE;
    }
    HandleDesc* pDesc = pRoot->findDesc(TargetDescHandle);
    if (pDesc == 0) {
	driver_exit(SQL_API_SQLCOPYDESC);
        return SQL_INVALID_HANDLE;
    }
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    if (ctx.ok())
        pDesc->sqlCopyDesc(
            ctx,
            pDesc
        );
    pDesc->saveCtx(ctx);
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    driver_exit(SQL_API_SQLCOPYDESC);
    return ret;
#endif
}
#endif // ODBCVER >= 0x0300
