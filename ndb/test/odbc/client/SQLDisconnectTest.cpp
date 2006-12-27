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
 * @file SQLDisconnectTest.cpp
 */

#include <common.hpp>
#define disc_SQL_MAXIMUM_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     disc_hdbc;
SQLHSTMT    disc_hstmt;
SQLHENV     disc_henv;
SQLHDESC    disc_hdesc;
       
void Disconnect_DisplayError_HDBC(SQLSMALLINT disc_HandleType, 
				  SQLHDBC disc_InputHandle);
/** 
 * Test to close the data source connection associated with
 * a specific connection handle
 *
 * -# Normal case testing 
 * @return Zero, if test succeeded
 */

int SQLDisconnectTest()
{
  SQLRETURN   disc_retcode;
  ndbout << endl << "Start SQLDisconnect Testing" << endl;
  
  // ************************************
  // ** Allocate an environment handle **
  // ************************************
  disc_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &disc_henv);

  if(disc_retcode == SQL_SUCCESS || disc_retcode == SQL_SUCCESS_WITH_INFO) {
    ndbout << "Allocated an environment handle!" << endl;
  } else {
    ndbout << "Failed to allocate environment handle!" << endl;
    return NDBT_FAILED;
  }

  // *********************************************
  // ** Set the ODBC application Version to 3.x **
  // *********************************************
  disc_retcode = SQLSetEnvAttr(disc_henv, 
			       SQL_ATTR_ODBC_VERSION, 
			       (SQLPOINTER) SQL_OV_ODBC3, 
			       SQL_IS_UINTEGER);
  if (disc_retcode == SQL_SUCCESS || disc_retcode == SQL_SUCCESS_WITH_INFO) {
    ndbout << "Set ODBC application version to 3.x" << endl;
  } else {
    ndbout << "Failed to set application version!" << endl;
    return NDBT_FAILED;
  }
  
  // **********************************
  // ** Allocate a connection handle **
  // **********************************
  disc_retcode = SQLAllocHandle(SQL_HANDLE_DBC, disc_henv, &disc_hdbc);

  if (disc_retcode == SQL_SUCCESS || disc_retcode == SQL_SUCCESS_WITH_INFO) {
    ndbout << "Allocated a connection handle!" << endl;
  } else {
    ndbout << "Failed to allocate connection handle!" << endl;
    return NDBT_FAILED;
  }

  // *******************
  // ** connect to DB **
  // *******************
  disc_retcode = SQLConnect(disc_hdbc, 
			    (SQLCHAR *) connectString(),
			    SQL_NTS, 
			    (SQLCHAR *) "",
			    SQL_NTS, 
			    (SQLCHAR *) "",
			    SQL_NTS);

  // **********************
  // ** Disconnect to DB **
  // **********************
  disc_retcode = SQLDisconnect(disc_hdbc);

  if (disc_retcode == SQL_INVALID_HANDLE)
{
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE"
	 << " still appeared. Please check program" << endl;
  Disconnect_DisplayError_HDBC(SQL_HANDLE_DBC, disc_hdbc);
}

  if (disc_retcode == SQL_ERROR || disc_retcode == SQL_SUCCESS_WITH_INFO)
{
  ndbout << "disconnect retcode = " << disc_retcode << endl;
  Disconnect_DisplayError_HDBC(SQL_HANDLE_DBC, disc_hdbc);
}
  // ******************
  // ** Free Handles **
  // ******************  
  SQLFreeHandle(SQL_HANDLE_STMT, disc_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, disc_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, disc_henv);

  return NDBT_OK;

 }

void Disconnect_DisplayError_HDBC(SQLSMALLINT disc_HandleType, 
				  SQLHDBC disc_InputHandle)
{
  SQLCHAR   disc_Msg[disc_SQL_MAXIMUM_MESSAGE_LENGTH];
  SQLSMALLINT   disc_i, disc_MsgLen;
  SQLINTEGER    disc_NativeError;
  SQLRETURN   disc_SQLSTATEs;
  disc_i = 1;
  SQLCHAR disc_Sqlstate[5];
  
  while ((disc_SQLSTATEs = SQLGetDiagRec(disc_HandleType, 
					 disc_InputHandle, 
					 disc_i, 
					 disc_Sqlstate, 
					 &disc_NativeError, 
					 disc_Msg, 
					 sizeof(disc_Msg), 
					 &disc_MsgLen)) 
	 != SQL_NO_DATA)
    {
    
      ndbout << "the HandleType is:" << disc_HandleType << endl;
      ndbout << "the InputHandle is :" <<(long)disc_InputHandle << endl;
      ndbout << "the output state is:" << (char *)disc_Sqlstate << endl; 
    
      disc_i ++;
      break;
    }
  
}



