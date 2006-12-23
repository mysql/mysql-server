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
 * @file SQLSetCursorNameTest.cpp
 */
#include <common.hpp>
using namespace std;

#define SCN_MESSAGE_LENGTH 50

SQLHSTMT    SCN_hstmt;
SQLHDESC    SCN_hdesc;
SQLHENV     SCN_henv;
SQLHDBC     SCN_hdbc;

void SCN_DisplayError(SQLSMALLINT SCN_HandleType, 
		      SQLHDESC SCN_InputHandle);

/** 
 * Test to assign a user-defined name to a cursor that is 
 * associated with an active SQL statement handle
 *
 * Tests:
 * -# set user-defined cursor name to zero
 * -# set user-defined cursor name in normal case
 *
 * @return Zero, if test succeeded
 */

int SQLSetCursorNameTest()
{
  SQLRETURN   retcode;
  SQLCHAR     SQLStmt [120];
  SQLCHAR     CursorName [80];
  SQLSMALLINT CNameSize;

  ndbout << endl << "Start SQLSetCursorName Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &SCN_henv);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(SCN_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   SCN_henv, 
			   &SCN_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(SCN_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
  ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }
  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  retcode = SQLAllocHandle(SQL_HANDLE_STMT, 
			   SCN_hdbc, 
			   &SCN_hstmt); 

  if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
 
  //************************
  //** Define a statement **
  //************************

  strcpy((char *) SQLStmt, 
	 "SELECT * FROM Customers WHERE Address = 'LM Vag 8'");

  //*************************
  //** Prepare a statement **
  //************************* 

  retcode = SQLPrepare(SCN_hstmt, 
		       SQLStmt, 
		       SQL_NTS);

  //**********************************
  //** Set the cursor name with zero**
  //********************************** 
  retcode = SQLSetCursorName(SCN_hstmt, 
			     (char *)"", 
			     SQL_NTS);

  if (retcode != SQL_SUCCESS)
    {
      ndbout << endl << "retcode =" << retcode << endl;
      SCN_DisplayError(SQL_HANDLE_STMT, SCN_hstmt);
    }

  //*************************
  //** Set the cursor name **
  //*************************
  retcode = SQLSetCursorName(SCN_hstmt, 
			     (char *)"Customer_CURSOR", 
			     SQL_NTS);

  if (retcode != SQL_SUCCESS)
    {
      ndbout << endl << "retcode =" << retcode << endl;
      SCN_DisplayError(SQL_HANDLE_STMT, SCN_hstmt);
    }
  //***************************
  //** Execute the statement **
  //***************************
  retcode = SQLExecute(SCN_hstmt);

  //**********************************************
  //** retrieve and display the new cursor name **
  //**********************************************
  retcode = SQLGetCursorName(SCN_hstmt, 
			     CursorName, 
			     sizeof(CursorName), 
			     &CNameSize);

  ndbout << endl << "The cursor name is : " << (char *) CursorName << endl;

  //****************
  // Free Handles **
  //****************
  SQLDisconnect(SCN_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, SCN_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, SCN_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, SCN_henv);
  
  return NDBT_OK;

 }


void SCN_DisplayError(SQLSMALLINT SCN_HandleType, SQLHDESC SCN_InputHandle)
{

  SQLINTEGER  NativeError;
  SQLCHAR     Sqlstate[5], Msg[SCN_MESSAGE_LENGTH];
  SQLRETURN SQLSTATEs;
  SQLSMALLINT i, MsgLen;
  i = 1;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((SQLSTATEs = SQLGetDiagRec(SCN_HandleType, 
				    SCN_InputHandle, i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
    {

      ndbout << "the HandleType is:" << SCN_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)SCN_InputHandle << endl;
      ndbout << "the Msg is: " << (char *) Msg << endl;
      ndbout << "the output state is:" << (char *)Sqlstate << endl; 
      
      i ++;
    }
  ndbout << "-------------------------------------------------" << endl;
}



