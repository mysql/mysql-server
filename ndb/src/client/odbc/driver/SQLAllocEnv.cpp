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
SQLAllocEnv(
    SQLHENV* EnvironmentHandle)
{
    driver_enter(SQL_API_SQLALLOCENV);
    const char* const sqlFunction = "SQLAllocEnv";
    HandleRoot* const pRoot = HandleRoot::instance();
    Ctx& ctx = *new Ctx;
    ctx.logSqlEnter(sqlFunction);
    HandleEnv* pEnv = 0;
    HandleEnv** ppEnv = 0;
    if (EnvironmentHandle != 0)
	ppEnv = &pEnv;
    try {
	pRoot->sqlAllocEnv(ctx, ppEnv);
    } catch (CtxAssert& ctxAssert) {
	ctx.handleEx(ctxAssert);
    }
    if (EnvironmentHandle != 0)
        *EnvironmentHandle = static_cast<SQLHENV>(pEnv);
    pRoot->saveCtx(ctx);
    ctx.logSqlExit();
    SQLRETURN ret = ctx.getCode();
    driver_exit(SQL_API_SQLALLOCENV);
    return ret;
}
#endif // ODBCVER >= 0x0000
