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
 * @file SQLGetFunctionsTest.cpp
 */


#include <common.hpp>
#define GF_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     GF_hdbc;
SQLHSTMT    GF_hstmt;
SQLHENV     GF_henv;
       
void SQLGetFunctions_DisplayError(SQLSMALLINT GF_HandleType, 
				  SQLHDBC GF_InputHandle);

/** 
 * Test whether a specific ODBC API function is supported by
 * the driver an application is currently connected to.
 *
 * In this test program, we can change ODBC function values in order to
 * know different which fuction is supported by ODBC drivers
 * Tests:
 * -# Test1 There is no established SQL-connection
 * -# Test2 ConnectionHandle does not identify an allocated SQL-connection 
 * -# Test3 The value of FunctionId is not in table 27
 * -# Test4 Normal case test
 * @return Zero, if test succeeded
 */

int SQLGetFunctionsTest()
{
  SQLUSMALLINT TableExists, Supported;
  SQLCHAR SQLStmt [120];
  SQLRETURN retcode;

  ndbout << endl << "Start SQLGetFunctions Testing" << endl;

  //**********************************************************
  //** Test 1                                               **
  //** If there is no established SQL-connection associated **
  //** with allocated SQL-connection                        **
  //**********************************************************

  retcode = SQLGetFunctions(GF_hdbc, SQL_API_SQLTABLES, &TableExists);
  if (retcode == -2)
{
  ndbout << endl << "Test 1" << endl;
  ndbout << "retcode = " << retcode << endl;
  ndbout << endl << "There is no established SQL-connection" << endl;
  ndbout << "associated with allocated SQL_connection" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}
  else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
{
  ndbout << endl << "Test 1" << endl;
  ndbout << "retcode = " << retcode << endl;
  ndbout << endl << "There is no established SQL-connection" << endl;
  ndbout << "associated with allocated SQL_connection" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}
  else
{
  ndbout << endl << "Test 1" << endl;
  ndbout << "retcode = " << retcode << endl;
  ndbout << endl << "There is no established SQL-connection" << endl;
  ndbout << "associated with allocated SQL_connection" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &GF_henv);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(GF_henv,
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.X!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   GF_henv, 
			   &GF_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(GF_hdbc, 
		       (SQLCHAR*) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR*) "", 
		       SQL_NTS,
		       (SQLCHAR*) "", 
		       SQL_NTS);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
{

    //*************************************************************
    //** Test 2                                                  **
    //** If ConnectionHandle does not identify an allocated      **
    //** SQL-connection, then an exception condition is raised   **
    //*************************************************************
    retcode = SQLGetFunctions(GF_hdbc, SQL_API_SQLTABLES, &TableExists);
    if (retcode == -2)
{
  ndbout << endl << "Test 2" << endl;
  ndbout << "retcode = " << retcode << endl;
  ndbout << "If ConnectionHandle does not identify an allocated" << endl;
  ndbout << "SQL-connection,an exception condition is raised" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}
  else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
{
  ndbout << endl << "Test 2" << endl;
  ndbout << "retcode = " << retcode << endl;
  ndbout << "If ConnectionHandle does not identify an allocated" << endl;
  ndbout << "SQL-connection,an exception condition is raised" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}
   else
{
  ndbout << endl << "Test 2 :" << endl;
  ndbout << "retcode = " << retcode << endl;
  ndbout << "If ConnectionHandle does not identify an allocated" << endl;
  ndbout << "SQL-connection,an exception condition is raised" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}

    //*************************************************************
    //** Test 3                                                  **
    //** If the value of FunctionId is not in table 27, "Codes   **
    //** used to identify SQL/CLI routines"                      **
    //*************************************************************
    
 retcode = SQLGetFunctions(GF_hdbc, 88888, &TableExists);
 ndbout<< "TableExists = " << TableExists << endl;
 if (retcode == -2)
{
  ndbout << "retcode = " << retcode << endl;
  ndbout << "Test 3 : The value of FunctionId is not in table 27" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}
 else if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
{
  ndbout << "retcode = " << retcode << endl;
  ndbout << "Test 3 : The value of FunctionId is not in table 27" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}
 else
{
  ndbout << "retcode = " << retcode << endl;
  ndbout << "Test 3 : The value of FunctionId is not in table 27" << endl;
  SQLGetFunctions_DisplayError(SQL_HANDLE_STMT, GF_hstmt);
}

  //******************
  //** Test 4       **
  //** Normal case  **
  //******************
  ndbout << "Test 4:" << endl;
  retcode = SQLGetFunctions(GF_hdbc, SQL_API_SQLBROWSECONNECT, &Supported);
  ndbout << "retcode = " << retcode << endl;
  if (Supported == TRUE)
{
  ndbout << "Supported = " << Supported << endl;
  ndbout << "SQLBrowseConnect is supported by the current data source" 
	 << endl;
}
  else
{
  ndbout << "Supported = " << Supported << endl;
  ndbout << "SQLBrowseConnect isn't supported by the current data source" 
	 << endl;
}


  //******************
  //** Test 5       **
  //** Normal case  **
  //******************
  ndbout << endl << "Test 5:" << endl;
  retcode = SQLGetFunctions(GF_hdbc, SQL_API_SQLFETCH, &Supported);
  ndbout << "retcode = " << retcode << endl;
  if (Supported == TRUE)
{
  ndbout << "Supported = " << Supported << endl;
  ndbout << "SQLFETCH is supported by the current data source" << endl;
}
  else
{
  ndbout << "Supported = " << Supported << endl;
  ndbout << "SQLFETCH isn't supported by the current data source" << endl;
}

}

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(GF_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, GF_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, GF_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, GF_henv);

 return NDBT_OK;

}


void SQLGetFunctions_DisplayError(SQLSMALLINT GF_HandleType, 
				  SQLHDBC GF_InputHandle)
{
  SQLRETURN   SQLSTATEs;
  SQLCHAR Sqlstate[50];
  SQLINTEGER    NativeError;
  SQLSMALLINT   i, MsgLen;
  SQLCHAR   Msg[GF_MESSAGE_LENGTH];
  i = 1;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  Msg[0] = 0;
  while ((SQLSTATEs = SQLGetDiagRec(GF_HandleType, 
				    GF_InputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
    {
    
      ndbout << "the HandleType is:" << GF_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)GF_InputHandle << endl;
      ndbout << "the Msg is :" << (char *) Msg << endl;
      ndbout << "the output state is:" << (char *)Sqlstate << endl; 
      
      i ++;
      Msg[0] = 0;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;  
}



