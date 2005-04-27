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
SQLGetDiagField(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle,
    SQLSMALLINT RecNumber,
    SQLSMALLINT DiagIdentifier,
    SQLPOINTER DiagInfo,
    SQLSMALLINT BufferLength,
    SQLSMALLINT* StringLength)
{
    driver_enter(SQL_API_SQLGETDIAGFIELD);
    const char* const sqlFunction = "SQLGetDiagField";
    HandleRoot* const pRoot = HandleRoot::instance();
    HandleBase* pBase = pRoot->findBase(HandleType, Handle);
    if (pBase == 0) {
	driver_exit(SQL_API_SQLGETDIAGFIELD);
        return SQL_INVALID_HANDLE;
    }
    Ctx ctx;				// discard diagnostics
    ctx.logSqlEnter(sqlFunction);
    try {
	pBase->sqlGetDiagField(ctx, RecNumber, DiagIdentifier, DiagInfo, BufferLength, StringLength);
    } catch (CtxAssert& ctxAssert) {
	ctx.handleEx(ctxAssert);
    }
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    driver_exit(SQL_API_SQLGETDIAGFIELD);
    return ret;
}
#endif // ODBCVER >= 0x0300
