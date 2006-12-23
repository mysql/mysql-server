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
 * @file SQLNumResultColsTest.cpp
 */
#include <common.hpp>
using namespace std;

#define NRC_MESSAGE_LENGTH 200

SQLHSTMT    NRC_hstmt;
SQLHSTMT    NRC_hdbc;
SQLHENV     NRC_henv;
SQLHDESC    NRC_hdesc;

void SQLNumResultColsTest_DisplayError(SQLSMALLINT NRC_HandleType, 
				       SQLHSTMT NRC_InputHandle);

/** 
 * Test returning descriptor information
 *
 * Tests:
 * -# Testing how many columns exist in the result data set
 * 
 * @return Zero, if test succeeded
 */

int SQLNumResultColsTest()
{
  SQLRETURN retcode;
  SQLSMALLINT NumColumns;
  SQLCHAR SQLStmt[NRC_MESSAGE_LENGTH];

  ndbout << endl << "Start SQLNumResultCols Testing" << endl << endl;

  //**************************************************************
  //** If there is no prepared or executed statement associated **
  //** with SQL-statement                                       **
  //**************************************************************

  retcode = SQLNumResultCols(NRC_hstmt, &NumColumns);
  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    {

    SQLNumResultColsTest_DisplayError(SQL_HANDLE_STMT, NRC_hstmt);
    }

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &NRC_henv);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(NRC_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   NRC_henv, 
			   &NRC_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(NRC_hdbc, 
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
			   NRC_hdbc, 
			   &NRC_hstmt); 
  if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;

  //************************
  //** Define a statement **
  //************************

  strcpy((char *) SQLStmt, "SELECT * FROM Customers");

  //    strcpy((char *) SQLStmt, 
  //    "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES (7, 'pet', 'LM vag 8', '88888')");

  //*******************************************
  //** Prepare and Execute the SQL statement **
  //*******************************************

  retcode = SQLExecDirect(NRC_hstmt, 
			  SQLStmt, 
			  SQL_NTS);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

    //*****************************************************
    //** Only general error test. It is not in test rule **
    //*****************************************************

    retcode = SQLNumResultCols(NRC_hstmt, &NumColumns);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
      {
      ndbout << endl << "Number of columns in the result data set" << endl;
      ndbout << NumColumns << endl;
      }
    else 
      SQLNumResultColsTest_DisplayError(SQL_HANDLE_STMT, NRC_hstmt);
  }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(NRC_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, NRC_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, NRC_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, NRC_henv);

  return NDBT_OK;
  
}

void SQLNumResultColsTest_DisplayError(SQLSMALLINT NRC_HandleType, 
				       SQLHSTMT NRC_InputHandle)
{
  SQLRETURN   SQLSTATEs;
  SQLINTEGER  NativeError;
  SQLSMALLINT i, MsgLen;
  SQLCHAR     Msg[NRC_MESSAGE_LENGTH],Sqlstate[5];
  i = 1;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((SQLSTATEs = SQLGetDiagRec(NRC_HandleType, 
				    NRC_InputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
    {
    
    ndbout << "the HandleType is:" << NRC_HandleType << endl;
    ndbout << "the InputHandle is :" << (long)NRC_InputHandle << endl;
    ndbout << "the Msg is: " << (char *)Msg << endl;
    ndbout << "the output state is:" << (char *)Sqlstate << endl; 
    
    i ++;
    break;
    }
  ndbout << "-------------------------------------------------" << endl;  
}



