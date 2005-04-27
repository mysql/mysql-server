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
SQLGetDescRec(
    SQLHDESC DescriptorHandle,
    SQLSMALLINT RecNumber,
    SQLCHAR* Name,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLength,
    SQLSMALLINT* Type,
    SQLSMALLINT* SubType,
    SQLINTEGER* Length,
    SQLSMALLINT* Precision,
    SQLSMALLINT* Scale,
    SQLSMALLINT* Nullable)
{
    driver_enter(SQL_API_SQLGETDESCREC);
    const char* const sqlFunction = "SQLGetDescRec";
    HandleRoot* const pRoot = HandleRoot::instance();
    HandleDesc* pDesc = pRoot->findDesc(DescriptorHandle);
    if (pDesc == 0) {
	driver_exit(SQL_API_SQLGETDESCREC);
        return SQL_INVALID_HANDLE;
    }
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    try {
	pDesc->sqlGetDescRec(ctx, RecNumber, Name, BufferLength, StringLength, Type, SubType, Length, Precision, Scale, Nullable);
    } catch (CtxAssert& ctxAssert) {
	ctx.handleEx(ctxAssert);
    }
    pDesc->saveCtx(ctx);
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    driver_exit(SQL_API_SQLGETDESCREC);
    return ret;
}
#endif // ODBCVER >= 0x0300
