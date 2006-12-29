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
 * @file SQLCancelTest.cpp
 */

#include <common.hpp>
#define Cancel_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     CC_hdbc;
SQLHSTMT    CC_hstmt;
SQLHENV     CC_henv;
SQLHDESC    CC_hdesc;
       
void Cancel_DisplayError(SQLSMALLINT Cancel_HandleType, 
			 SQLHSTMT Cancel_InputHandle);
/** 
 * Test to terminate SQL statement precessing
 *
 * Tests:
 * -# normal case test with correct hstmt handle
 * -# SQL_STILL_EXECUTING case test with hstmt handle
 * -# abnormal case test with incorrect hdbc, henv and hdesc handle
 * @return Zero, if test succeeded
 */

int SQLCancelTest()
{

  SQLRETURN   retcode;
  SQLCHAR SQLStmt [120];

  ndbout << endl << "Start SQLCancel Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &CC_henv);
  
  if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(CC_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************
  retcode = SQLAllocHandle(SQL_HANDLE_DBC, CC_henv, &CC_hdbc);
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(CC_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    ndbout << "Failure to Connect DB!" << endl;
  
  //*******************************
  //** Allocate statement handle **
  //*******************************

  retcode = SQLAllocHandle(SQL_HANDLE_STMT, CC_hdbc, &CC_hstmt); 
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
  
  //************************
  //** Define a statement **
  //************************

  strcpy((char *) SQLStmt, 
         "select * FROM Customers");

  //*************************
  //** Prepare a statement **
  //*************************

  retcode = SQLPrepare(CC_hstmt, 
		       SQLStmt, 
		       SQL_NTS);
  
  //***********************
  //** Execute statement **
  //***********************

  retcode = SQLExecute(CC_hstmt);
  
  //************************************************
  //** Test 1                                     **
  //** Input correct hstmt handle for normal test **
  //************************************************

  retcode = SQLCancel(CC_hstmt);

  if (retcode == SQL_INVALID_HANDLE)
    {
      ndbout << "Test 1" << endl;
      ndbout << "Handle Type is SQL_HANDLE_STMT, but SQL_INVALID_HANDLE" 
	     << "still appeared. Please check program" << endl;
    }

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "Test 1" << endl;
      Cancel_DisplayError(SQL_HANDLE_STMT, CC_hstmt);
    }
  //************************************************
  //** Test 2                                     **
  //** SQL_STILL_EXECUTING is not defined         **
  //************************************************

  if (retcode == SQL_STILL_EXECUTING)
    {
      ndbout << "Test 2" << endl;
    ndbout << "The function is still processing." << endl;
    }

  if (retcode == SQL_ERROR)
    {
      ndbout << "Test 2" << endl;
      ndbout << "The Asynchronous processing was successfully canceled!" 
	     << endl;
    }
  //*********************************
  //** Test 3                      **
  //** Input incorrect henv handle **
  //*********************************

  retcode = SQLCancel(CC_henv);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS)
    { 
      ndbout << "Test 3" << endl;
      ndbout << "Handle Type is SQL_HANDLE_ENV, but SQL_SUCCESS_WITH_INFO" 
	     << " still appeared. Please check program" << endl;
      Cancel_DisplayError(SQL_HANDLE_ENV, CC_henv);
    }

  //*********************************
  //** Test 4                      **
  //** Input incorrect hdbc handle **
  //*********************************

  retcode = SQLCancel(CC_hdbc);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS)
  {
   ndbout << "Test 4" << endl;
   ndbout << "Handle Type is SQL_HANDLE_DBC, but SQL_SUCCESS_WITH_INFO" 
	   << "still appeared. Please check programm" << endl;
   Cancel_DisplayError(SQL_HANDLE_DBC, CC_hdbc);
  }

  //**********************************
  //** Test 5                       **
  //** Input incorrect handle hdesc **
  //**********************************

  retcode = SQLCancel(CC_hdesc);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS)
    {
      ndbout << endl 
	     << "Handle Type is SQL_HANDLE_DESC, but SQL_SUCCESS_WITH_INFO" 
	     << "still appeared. Please check program" << endl;
      ndbout << "Test 5" << endl;
      Cancel_DisplayError(SQL_HANDLE_DESC, CC_hdesc);
    }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(CC_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, CC_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, CC_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, CC_henv);

  return NDBT_OK;

 }

void Cancel_DisplayError(SQLSMALLINT Cancel_HandleType, 
			 SQLHSTMT Cancel_InputHandle)
{
  SQLCHAR   Sqlstate[5];
  SQLRETURN SQLSTATEs;
  SQLINTEGER    NativeError;
  SQLSMALLINT   i, MsgLen;
  SQLCHAR   Msg[Cancel_MESSAGE_LENGTH];
  i = 1;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
     while ((SQLSTATEs = SQLGetDiagRec(Cancel_HandleType, 
				       Cancel_InputHandle, 
				       i, 
				       Sqlstate, 
				       &NativeError, 
				       Msg, 
				       sizeof(Msg), 
				       &MsgLen)) 
	    != SQL_NO_DATA)
       {

	 ndbout << "the HandleType is:" << Cancel_HandleType << endl;
	 ndbout << "the InputHandle is :" << (long)Cancel_InputHandle << endl;
         ndbout << "the Msg is: " << (char *) Msg << endl;
	 ndbout << "the output state is:" << (char *)Sqlstate << endl; 
	 
	 i ++;
	 break;
       }
  ndbout << "-------------------------------------------------" << endl;     
}



