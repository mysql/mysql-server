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
SQLFreeHandle(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle)
{
    driver_enter(SQL_API_SQLFREEHANDLE);
    const char* const sqlFunction = "SQLFreeHandle";
    HandleRoot* const pRoot = HandleRoot::instance();
    SQLSMALLINT parentType = pRoot->findParentType(HandleType);
    if (parentType == -1) {
	driver_exit(SQL_API_SQLFREEHANDLE);
        return SQL_INVALID_HANDLE;
    }
    HandleBase* pChild = pRoot->findBase(HandleType, Handle);
    if (pChild == 0) {
	driver_exit(SQL_API_SQLFREEHANDLE);
        return SQL_INVALID_HANDLE;
    }
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    HandleBase* pParent = pChild->getParent();
    ctx_assert(pParent != 0);
    try {
	pParent->sqlFreeHandle(ctx, HandleType, pChild);
    } catch (CtxAssert& ctxAssert) {
	ctx.handleEx(ctxAssert);
    }
    if (! ctx.ok()) {
	pChild->saveCtx(ctx);
	ctx.logSqlExit();
	SQLRETURN ret = ctx.getCode();
	driver_exit(SQL_API_SQLFREEHANDLE);
	return ret;
    }
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    delete &ctx;
    driver_exit(SQL_API_SQLFREEHANDLE);
    return ret;
}
#endif // ODBCVER >= 0x0300
