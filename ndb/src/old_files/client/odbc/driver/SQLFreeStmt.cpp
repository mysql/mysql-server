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
SQLFreeStmt(
    SQLHSTMT StatementHandle,
    SQLUSMALLINT Option)
{
    driver_enter(SQL_API_SQLFREESTMT);
    const char* const sqlFunction = "SQLFreeStmt";
    HandleRoot* const pRoot = HandleRoot::instance();
    HandleStmt* pStmt = pRoot->findStmt(StatementHandle);
    if (pStmt == 0) {
	driver_exit(SQL_API_SQLFREESTMT);
        return SQL_INVALID_HANDLE;
    }
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    HandleDbc* pDbc = pStmt->getDbc();
    try {
	pDbc->sqlFreeStmt(ctx, pStmt, Option);
    } catch (CtxAssert& ctxAssert) {
	ctx.handleEx(ctxAssert);
    }
    if (! ctx.ok() || Option != SQL_DROP) {
	pStmt->saveCtx(ctx);
	ctx.logSqlExit();
	SQLRETURN ret = ctx.getCode();
	driver_exit(SQL_API_SQLFREESTMT);
	return ret;
    }
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    delete &ctx;
    driver_exit(SQL_API_SQLFREESTMT);
    return ret;
}
#endif // ODBCVER >= 0x0000
