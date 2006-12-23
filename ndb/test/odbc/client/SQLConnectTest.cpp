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

 /**
 * @file SQLConnectTest.cpp
 */
#include <common.hpp>
using namespace std;

SQLHDBC     conn_hdbc;
SQLHSTMT    conn_hstmt;
SQLHENV     conn_henv;
SQLHDESC    conn_hdesc;
SQLRETURN   conn_retcode;

#define conn_SQL_MAXIMUM_MESSAGE_LENGTH 200
SQLCHAR conn_Sqlstate[5];

SQLINTEGER    conn_NativeError;
SQLSMALLINT   conn_MsgLen;
SQLCHAR       conn_Msg[conn_SQL_MAXIMUM_MESSAGE_LENGTH];
       
void SQLConnectTest_DisplayError_HDBC(SQLSMALLINT conn_HandleType, 
				      SQLHDBC conn_InputHandle);

/** 
 * -# Test to make a connection to an ODBC data source
 *
 * @return Zero, if test succeeded
 */
int SQLConnectTest()
{
  ndbout << endl << "Start SQLConnect Testing" << endl;
  
  // ************************************
  // ** Allocate an environment handle **
  // ************************************
  conn_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &conn_henv);
  //conn_retcode = SQLAllocEnv(&conn_henv);
  if(conn_retcode == SQL_SUCCESS || conn_retcode == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "Allocated an environment handle!" << endl;
    } 
  else 
    {
      ndbout << "Failed to allocate environment handle!" << endl;
      return NDBT_FAILED;
    }

  // *********************************************
  // ** Set the ODBC application Version to 3.x **
  // *********************************************
  conn_retcode = SQLSetEnvAttr(conn_henv, 
			       SQL_ATTR_ODBC_VERSION, 
			       (SQLPOINTER) SQL_OV_ODBC3, 
			       SQL_IS_UINTEGER);
  if (conn_retcode == SQL_SUCCESS || conn_retcode == SQL_SUCCESS_WITH_INFO) {
    ndbout << "Set ODBC application version to 3.x" << endl;
  } else {
    ndbout << "Failed to set application version!" << endl;
    return NDBT_FAILED;
  }
  
  // **********************************
  // ** Allocate a connection handle **
  // **********************************
  conn_retcode = SQLAllocHandle(SQL_HANDLE_DBC, conn_henv, &conn_hdbc);
  //     retcode = SQLAllocConnect(conn_henv, &conn_hdbc);
  if (conn_retcode == SQL_SUCCESS || conn_retcode == SQL_SUCCESS_WITH_INFO) 
    {
      ndbout << "Allocated a connection handle!" << endl;
    } 
  else 
    {
      ndbout << "Failed to allocate connection handle!" << endl;
      return NDBT_FAILED;
    }
  
  // *******************
  // ** Connect to DB **
  // *******************
  conn_retcode = SQLConnect(conn_hdbc, 
			    (SQLCHAR *) connectString(),
			    SQL_NTS, 
			    (SQLCHAR *) "",
			    SQL_NTS, 
			    (SQLCHAR *) "",
			    SQL_NTS);
  ndbout << "conn_retcode = " << conn_retcode << endl;
  if (conn_retcode == SQL_SUCCESS) 
    {
      ndbout << "Connected to DB!" << endl;
    } 
  else if (conn_retcode == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "Connected to DB, but SQL_SUCCESS_WITH_INFO!" << endl;
      SQLConnectTest_DisplayError_HDBC(SQL_HANDLE_DBC, conn_hdbc);
    }
  else if (conn_retcode == SQL_INVALID_HANDLE) 
    {
      ndbout << "SQL_INVALID_HANDLE appeared. Please check program." << endl;
      return NDBT_FAILED;
    } 
  else if (conn_retcode == SQL_ERROR)
    { 
      ndbout << "Failed to connect!" << endl;
      SQLConnectTest_DisplayError_HDBC(SQL_HANDLE_DBC, conn_hdbc);
      return NDBT_FAILED;
    }
  else
    ;

  // ******************
  // ** Free Handles **
  // ******************  
  SQLDisconnect(conn_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, conn_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, conn_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, conn_henv);
  return NDBT_OK;
}


void SQLConnectTest_DisplayError_HDBC(SQLSMALLINT conn_HandleType, 
				      SQLHDBC conn_InputHandle) {
  SQLSMALLINT conn_i = 1;
  SQLRETURN   conn_SQLSTATE;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;

  while ((conn_SQLSTATE = SQLGetDiagRec(conn_HandleType, 
					conn_InputHandle, 
					conn_i, 
					conn_Sqlstate, 
					&conn_NativeError, 
					conn_Msg, 
					sizeof(conn_Msg), 
					&conn_MsgLen)
	  ) != SQL_NO_DATA)
    {
      ndbout << "SQLSTATE = " << conn_SQLSTATE << endl;
      ndbout << "the HandleType is: " << conn_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)conn_InputHandle << endl;
      ndbout << "the conn_Msg is: " << (char *) conn_Msg << endl;
      ndbout << "the output state is:" << (char *)conn_Sqlstate << endl; 
      
      conn_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}
