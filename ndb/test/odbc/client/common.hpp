/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NdbOut.hpp>
#include <NDBT.hpp>
#include <sqlext.h>
#include <stdio.h>
#include <string.h>
#define FAILURE(msg) { ndbout << "TEST FAILURE: " << msg << endl \
  << "-- File: " << __FILE__ << ", Line: " << __LINE__ << endl; \
  return NDBT_FAILED; }

char* connectString();

int SQLFetchTest();
int SQLDisconnectTest();
int SQLTablesTest();
int SQLBindColTest();
int SQLGetInfoTest();
int SQLGetTypeInfoTest();
int SQLGetDataTest();
int SQLGetFunctionsTest();
int SQLColAttributeTest();
int SQLColAttributeTest1();
int SQLColAttributeTest2();
int SQLColAttributeTest3();
int SQLGetDiagRecSimpleTest();
int SQLDriverConnectTest();
int SQLAllocEnvTest();
int SQLFreeHandleTest();
int SQLFetchScrollTest();
int SQLFetchTest();
int SQLGetDescRecTest();
int SQLSetDescFieldTest();
int SQLGetDescFieldTest();
int SQLSetDescRecTest();
int SQLSetCursorNameTest();
int SQLGetCursorNameTest();
int SQLRowCountTest();
int SQLGetInfoTest();
int SQLTransactTest();
int SQLEndTranTest();
int SQLNumResultColsTest();
int SQLGetTypeInfoTest();
int SQLGetFunctionsTest();
int SQLDescribeColTest();
int SQLAllocHandleTest();
int SQLCancelTest();
int SQLCloseCursorTest();
int SQLConnectTest();
int SQLDisconnectTest();
int SQLExecDirectTest();
int SQLExecuteTest();
int SQLFreeHandleTest();
int SQLFreeStmtTest();
int SQLGetConnectAttrTest();
int SQLGetEnvAttrTest();
int SQLGetStmtAttrTest();
int SQLMoreResultsTest();
int SQLPrepareTest();
int SQLSetConnectAttrTest();
int SQLSetEnvAttrTest();
int SQLSetStmtAttrTest();

// int NDBT_ALLOCHANDLE();
// int NDBT_ALLOCHANDLE_HDBC();
// int NDBT_SQLPrepare();
// int NDBT_SQLConnect();
