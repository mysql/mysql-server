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
#include <NdbMutex.h>

#undef NDB_ODBC_SIG_DFL
#ifdef NDB_ODBC_SIG_DFL
#include <signal.h>
#endif

// The big mutex (just in case).

#ifdef NDB_WIN32
static NdbMutex & driver_mutex = * NdbMutex_Create();
#else
static NdbMutex driver_mutex = NDB_MUTEX_INITIALIZER;
#endif

static void
driver_lock()
{
    NdbMutex_Lock(&driver_mutex);
}

static void
driver_unlock()
{
    NdbMutex_Unlock(&driver_mutex);
}

// Hooks for function entry and exit.

static inline void
driver_enter(SQLUSMALLINT functionId)
{
    switch (functionId) {
    default:
	break;
    }
#ifdef NDB_ODBC_SIG_DFL
    // XXX need to restore old sig
    for (int i = 1; i <= 30; i++)
	signal(i, SIG_DFL);
#endif
}

static inline void
driver_exit(SQLUSMALLINT functionId)
{
    switch (functionId) {
    default:
	break;
    }
}

// Some C++ compilers (like gcc) cannot merge template code
// in different files.  So compile all in one file.

#include "SQLAllocConnect.cpp"
#include "SQLAllocEnv.cpp"
#include "SQLAllocHandle.cpp"
#include "SQLAllocHandleStd.cpp"
#include "SQLAllocStmt.cpp"
#include "SQLBindCol.cpp"
#include "SQLBindParam.cpp"
#include "SQLBindParameter.cpp"
#include "SQLBrowseConnect.cpp"
#include "SQLBulkOperations.cpp"
#include "SQLCancel.cpp"
#include "SQLCloseCursor.cpp"
#include "SQLColAttribute.cpp"
#include "SQLColAttributes.cpp"
#include "SQLColumnPrivileges.cpp"
#include "SQLColumns.cpp"
#include "SQLConnect.cpp"
#include "SQLCopyDesc.cpp"
#include "SQLDataSources.cpp"
#include "SQLDescribeCol.cpp"
#include "SQLDescribeParam.cpp"
#include "SQLDisconnect.cpp"
#include "SQLDriverConnect.cpp"
#include "SQLDrivers.cpp"
#include "SQLEndTran.cpp"
#include "SQLError.cpp"
#include "SQLExecDirect.cpp"
#include "SQLExecute.cpp"
#include "SQLExtendedFetch.cpp"
#include "SQLFetch.cpp"
#include "SQLFetchScroll.cpp"
#include "SQLForeignKeys.cpp"
#include "SQLFreeConnect.cpp"
#include "SQLFreeEnv.cpp"
#include "SQLFreeHandle.cpp"
#include "SQLFreeStmt.cpp"
#include "SQLGetConnectAttr.cpp"
#include "SQLGetConnectOption.cpp"
#include "SQLGetCursorName.cpp"
#include "SQLGetData.cpp"
#include "SQLGetDescField.cpp"
#include "SQLGetDescRec.cpp"
#include "SQLGetDiagField.cpp"
#include "SQLGetDiagRec.cpp"
#include "SQLGetEnvAttr.cpp"
#include "SQLGetFunctions.cpp"
#include "SQLGetInfo.cpp"
#include "SQLGetStmtAttr.cpp"
#include "SQLGetStmtOption.cpp"
#include "SQLGetTypeInfo.cpp"
#include "SQLMoreResults.cpp"
#include "SQLNativeSql.cpp"
#include "SQLNumParams.cpp"
#include "SQLNumResultCols.cpp"
#include "SQLParamData.cpp"
#include "SQLParamOptions.cpp"
#include "SQLPrepare.cpp"
#include "SQLPrimaryKeys.cpp"
#include "SQLProcedureColumns.cpp"
#include "SQLProcedures.cpp"
#include "SQLPutData.cpp"
#include "SQLRowCount.cpp"
#include "SQLSetConnectAttr.cpp"
#include "SQLSetConnectOption.cpp"
#include "SQLSetCursorName.cpp"
#include "SQLSetDescField.cpp"
#include "SQLSetDescRec.cpp"
#include "SQLSetEnvAttr.cpp"
#include "SQLSetParam.cpp"
#include "SQLSetPos.cpp"
#include "SQLSetScrollOptions.cpp"
#include "SQLSetStmtAttr.cpp"
#include "SQLSetStmtOption.cpp"
#include "SQLSpecialColumns.cpp"
#include "SQLStatistics.cpp"
#include "SQLTablePrivileges.cpp"
#include "SQLTables.cpp"
#include "SQLTransact.cpp"
