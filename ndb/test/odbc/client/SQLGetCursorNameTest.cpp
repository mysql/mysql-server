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
 * @file SQLGetCursorNameTest.cpp
 */

#include <common.hpp>
using namespace std;

#define GCN_MESSAGE_LENGTH 50

SQLHSTMT    GCN_hstmt;
SQLHDESC    GCN_hdesc;
SQLHENV     GCN_henv;
SQLHDBC     GCN_hdbc;

void GCN_DisplayError(SQLSMALLINT GCN_HandleType, 
		      SQLHDESC GCN_InputHandle);

/** 
 * Test to assign a user-defined name to a cursor that is 
 * associated with an active SQL statement handle
 *
 * Tests:
 * -# if there is no user-defined cursor name, then try to 
 *    get user-definedcursor name 
 * -# get cursor name in normal case
 *
 * @return Zero, if test succeeded
 */

int SQLGetCursorNameTest()
{
  SQLRETURN   retcode;
  SQLCHAR     SQLStmt [120];
  SQLCHAR     CursorName [80];
  SQLSMALLINT CNameSize;

  ndbout << endl << "Start SQLGetCursorName Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &GCN_henv);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(GCN_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   GCN_henv, 
			   &GCN_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(GCN_hdbc, 
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
			   GCN_hdbc, 
			   &GCN_hstmt); 

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

  retcode = SQLPrepare(GCN_hstmt, 
		       SQLStmt, 
		       SQL_NTS);

  //*************************************************************************
  //** if there is no user-defined cursor name, try to get the cursor name **
  //*************************************************************************
  retcode = SQLGetCursorName(GCN_hstmt, 
			     CursorName, 
			     sizeof(CursorName), 
			     &CNameSize);

  if (retcode != SQL_SUCCESS)
    {
      ndbout << endl << "retcode =" << retcode << endl;
      GCN_DisplayError(SQL_HANDLE_STMT, GCN_hstmt);
    }
  else 
      ndbout << endl << "The cursor name is : " << (char *) CursorName << endl;

  //*************************
  //** Set the cursor name **
  //*************************
  retcode = SQLSetCursorName(GCN_hstmt, 
			     (char *)"Customer_CURSOR", 
			     SQL_NTS);

  //***************************
  //** Execute the statement **
  //***************************
  retcode = SQLExecute(GCN_hstmt);

  //**********************************************
  //** retrieve and display the new cursor name **
  //**********************************************
  retcode = SQLGetCursorName(GCN_hstmt, 
			     CursorName, 
			     sizeof(CursorName), 
			     &CNameSize);

  if (retcode != SQL_SUCCESS)
    {
      ndbout << endl << "retcode =" << retcode << endl;
      GCN_DisplayError(SQL_HANDLE_STMT, GCN_hstmt);
    }
  else 
      ndbout << endl << "The cursor name is : " << (char *) CursorName << endl;

  //****************
  // Free Handles **
  //****************
  SQLDisconnect(GCN_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, GCN_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, GCN_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, GCN_henv);
  
  return NDBT_OK;

 }


void GCN_DisplayError(SQLSMALLINT GCN_HandleType, SQLHDESC GCN_InputHandle)
{

  SQLINTEGER  NativeError;
  SQLCHAR     Sqlstate[5], Msg[GCN_MESSAGE_LENGTH];
  SQLRETURN SQLSTATEs;
  SQLSMALLINT i, MsgLen;
  i = 1;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((SQLSTATEs = SQLGetDiagRec(GCN_HandleType, 
				    GCN_InputHandle, i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
    {

      ndbout << "the HandleType is:" << GCN_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)GCN_InputHandle << endl;
      ndbout << "the Msg is: " << (char *) Msg << endl;
      ndbout << "the output state is:" << (char *)Sqlstate << endl; 
      
      i ++;
    }
  ndbout << "-------------------------------------------------" << endl;
}



