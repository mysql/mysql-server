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

#include <common.hpp>
#include <string.h>

using namespace std;

SQLHDBC     driconn_hdbc;
SQLHSTMT    driconn_hstmt;
SQLHENV     driconn_henv;
SQLHDESC    driconn_hdesc;
SQLRETURN   driconn_retcode, driconn_SQLSTATEs;

#define driconn_SQL_MAXIMUM_MESSAGE_LENGTH 200
SQLCHAR driconn_Sqlstate[5];

SQLINTEGER    driconn_NativeError;
SQLSMALLINT   driconn_i, driconn_MsgLen;
SQLCHAR   driconn_Msg[driconn_SQL_MAXIMUM_MESSAGE_LENGTH], driconn_ConnectIn[30];
       
void SQLDriverConnectTest_DisplayError_HDBC(SQLSMALLINT driconn_HandleType, SQLHDBC driconn_InputHandle);

int SQLDriverConnectTest()
{
     ndbout << endl << "Start SQLDriverConnect Testing" << endl;
  // Allocate An Environment Handle
     driconn_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &driconn_henv);

  // Set the ODBC application Version to 3.x
     driconn_retcode = SQLSetEnvAttr(driconn_henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);

     if (driconn_retcode == SQL_SUCCESS || driconn_retcode == SQL_SUCCESS_WITH_INFO)
       ndbout << "Set the ODBC application Version to 3.x" << endl;

  // Allocate A Connection Handle
     driconn_retcode = SQLAllocHandle(SQL_HANDLE_DBC, driconn_henv, &driconn_hdbc);

     if (driconn_retcode == SQL_SUCCESS || driconn_retcode == SQL_SUCCESS_WITH_INFO)
         ndbout << "Allocation A Connection Handle" << endl;

  // Build A Connection String
     strcpy((char*) driconn_ConnectIn, "DSN=ndb;UID=x;PWD=y");

  // Connect to NDB
     driconn_retcode = SQLDriverConnect(driconn_hdbc, NULL, driconn_ConnectIn, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
     ndbout << "retcode = " << driconn_retcode << endl;
     ndbout << "Before pringing out information about connection, we print out retcode = " << driconn_retcode << endl;

     if (driconn_retcode == SQL_SUCCESS || driconn_retcode == SQL_SUCCESS_WITH_INFO)
         ndbout << "Connected to NDB" << endl;

     if (driconn_retcode == SQL_INVALID_HANDLE)
 ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

     else 
       { if (driconn_retcode == SQL_ERROR || driconn_retcode == SQL_SUCCESS_WITH_INFO)
         SQLDriverConnectTest_DisplayError_HDBC(SQL_HANDLE_DBC, driconn_hdbc);}

  // Free the Connection Handle
  SQLFreeHandle(SQL_HANDLE_DBC, driconn_hdbc);

  // Free the Environment Handle
  SQLFreeHandle(SQL_HANDLE_ENV, driconn_henv);

  return 0;
 }

void SQLDriverConnectTest_DisplayError_HDBC(SQLSMALLINT driconn_HandleType, SQLHDBC driconn_InputHandle)
{
     driconn_i = 1;
     while ((driconn_SQLSTATEs = SQLGetDiagRec(driconn_HandleType, driconn_InputHandle, driconn_i, 
             driconn_Sqlstate, &driconn_NativeError, driconn_Msg, sizeof(driconn_Msg), 
             &driconn_MsgLen)) != SQL_NO_DATA)                   {

     ndbout << "the HandleType is:" << driconn_HandleType << endl;
     ndbout << "the InputHandle is :" << (long)driconn_InputHandle << endl;
     ndbout << "the output state is:" << (char *)driconn_Sqlstate << endl; 

     driconn_i ++;
                                                         }

}
