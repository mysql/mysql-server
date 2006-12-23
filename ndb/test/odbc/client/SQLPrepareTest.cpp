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
 * @file SQLprepareTest.cpp
 */
#include <common.hpp>
#define pare_SQL_MAXIMUM_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     pare_hdbc;
SQLHSTMT    pare_hstmt;
SQLHENV     pare_henv;
SQLHDESC    pare_hdesc;
SQLRETURN   pare_retcode, pare_SQLSTATEs;

SQLCHAR pare_Sqlstate[5];

SQLINTEGER    pare_NativeError;
SQLSMALLINT   pare_i, pare_MsgLen;
SQLCHAR   pare_Msg[pare_SQL_MAXIMUM_MESSAGE_LENGTH];
       
void Prepare_DisplayError(SQLSMALLINT pare_HandleType, 
			  SQLHSTMT pare_InputHandle);

/** 
 * Test to prepare a statement with different handles
 *
 * -# Input correct hstmt handle
 * -# Input incorrect henv handle
 * -# Input incorrect hdbc handle
 * -# Input incorrect handle hdesc
 *
 * @return Zero, if test succeeded
 */
int SQLPrepareTest()
{
  SQLCHAR SQLStmt [120];
  ndbout << endl << "Start SQLPrepare Testing" << endl;
  ndbout << endl << "Test 1" << endl;  
  //*********************************
  //** Test1                       **
  //** Input correct hstmt handle  **
  //*********************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  pare_retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
				SQL_NULL_HANDLE, 
				&pare_henv);
  
  if(pare_retcode == SQL_SUCCESS || pare_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  pare_retcode = SQLSetEnvAttr(pare_henv, 
			       SQL_ATTR_ODBC_VERSION, 
			       (SQLPOINTER) SQL_OV_ODBC3, 
			       SQL_IS_UINTEGER);
  
  if (pare_retcode == SQL_SUCCESS || pare_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************
  pare_retcode = SQLAllocHandle(SQL_HANDLE_DBC, pare_henv, &pare_hdbc);
  if (pare_retcode == SQL_SUCCESS || pare_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  pare_retcode = SQLConnect(pare_hdbc, 
			    (SQLCHAR *) connectString(), 
			    SQL_NTS, 
			    (SQLCHAR *) "", 
			    SQL_NTS, 
			    (SQLCHAR *) "", 
			    SQL_NTS);
  
  if (pare_retcode == SQL_SUCCESS || pare_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    ndbout << "Failure to Connect DB!" << endl;
  
  //*******************************
  //** Allocate statement handle **
  //*******************************
  pare_retcode = SQLAllocHandle(SQL_HANDLE_STMT, pare_hdbc, &pare_hstmt); 
  if (pare_retcode == SQL_SUCCESS || pare_retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
  
  //************************
  //** Define a statement **
  //************************

   strcpy( (char *) SQLStmt, "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES(2, 'Hans  Peter', 'LM Vag8', '468719000')");

  pare_retcode = SQLPrepare(pare_hstmt, 
			    SQLStmt, 
			    SQL_NTS);
  
  if (pare_retcode == SQL_INVALID_HANDLE)
    {
      ndbout << "pare_retcode = " << pare_retcode << endl;
      ndbout << "HandleType is SQL_HANDLE_STMT, but SQL_INVALID_HANDLE" 
	     << endl;
      ndbout << "appeared. Please check program!" << endl;
    }
  else if (pare_retcode == SQL_ERROR || pare_retcode == SQL_SUCCESS_WITH_INFO)
    { 
      Prepare_DisplayError(SQL_HANDLE_STMT, pare_hstmt);
    } 
  else 
    { 
      //***********************
      //** Execute statement **
      //***********************
      pare_retcode = SQLExecute(pare_hstmt);
      if (pare_retcode != SQL_SUCCESS) 
	{
	  ndbout << "pare_retcode = " << pare_retcode << endl;
	  Prepare_DisplayError(SQL_HANDLE_STMT, pare_hstmt);
	}
      else
	ndbout << endl << "Test 1:Input correct HSTMT handle. OK!" << endl;
    }
  
  //*********************************
  //** Test2                       **
  //** Input incorrect henv handle **
  //*********************************

  strcpy( (char *) SQLStmt, "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES(3, 'Hans', 'LM8', '51888')");

  pare_retcode = SQLPrepare(pare_henv, 
			    SQLStmt, 
			    SQL_NTS);
  
  ndbout << endl << "Test 2" << endl;
  if (pare_retcode == SQL_SUCCESS_WITH_INFO || pare_retcode == SQL_SUCCESS)
    { 
      FAILURE("Wrong SQL_HANDLE_HENV, but success returned. Check it!");
    }
  else if (pare_retcode == SQL_INVALID_HANDLE) 
   { 
     ndbout << "Wrong SQL_HANDLE_HENV input and -2 appeared. OK!" << endl ;
   }
  else
    ;
  /*
    {
      ndbout << "Input wrong SQL_HANDLE_ENV, but SQL_SUCCESS_W_I" << endl;
      ndbout << "and SQL_SUCCESS appeared. Please check program!" << endl;
      return NDBT_FAILED;
    }
  */

  //*********************************
  //** Test3                       **
  //** Input incorrect hdbc handle **
  //*********************************

  strcpy( (char *) SQLStmt, "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES(4, 'HP', 'VÄG8', '90888')");

  pare_retcode = SQLPrepare(pare_hdbc, 
			    SQLStmt, 
			    SQL_NTS);

  ndbout << endl << "Test 3" << endl;
  if (pare_retcode == SQL_SUCCESS_WITH_INFO || pare_retcode == SQL_SUCCESS)
    {
      FAILURE("Wrong SQL_HANDLE_HDBC, but success returned. Check it!");
    }
  else if (pare_retcode == SQL_INVALID_HANDLE)
    {
     ndbout << "Wrong SQL_HANDLE_HDBC input and -2 appeared. OK!" << endl ;
    }
  else
    ;

    /*
    {
      ndbout << "Input wrong statement handle SQL_HANDLE_DBC" << endl;
      ndbout << "but SQL_SUCCESS_WITH_INFO" << endl;
      ndbout << "and SQL_SUCCESS still appeared. Please check program" << endl;
      return NDBT_FAILED;
    }

   */
  //**********************************
  //** Test4                        **
  //** Input incorrect handle hdesc **
  //**********************************

  strcpy( (char *) SQLStmt, "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES(5, 'Richard', 'VÄG8', '56888')");

  pare_retcode = SQLPrepare(pare_hdesc, 
			    SQLStmt, 
			    SQL_NTS);

  ndbout << endl << "Test 4" << endl;
  if (pare_retcode == SQL_SUCCESS_WITH_INFO || pare_retcode == SQL_SUCCESS)
    {
      FAILURE("Wrong SQL_HANDLE_DESC, but success returned");
    }
  else if (pare_retcode == SQL_INVALID_HANDLE)
    {
     ndbout << "Wrong SQL_HANDLE_DESC input and -2 appeared. OK!" << endl ;
    }
  else 
    ndbout << endl;

    /*
    {
      ndbout << "TEST FAILURE: Input wrong SQL_HANDLE_DESC, " 
	     << "but SQL_SUCCESS_WITH_INFO or SQL_SUCCESS was returned." 
	     << endl;
      return NDBT_FAILED;
    }
   */

  //****************
  // Free Handles **
  //****************
  SQLDisconnect(pare_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, pare_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, pare_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, pare_henv);
  
  return NDBT_OK;
  
}

void Prepare_DisplayError(SQLSMALLINT pare_HandleType, 
			  SQLHSTMT pare_InputHandle)
{
  SQLSMALLINT pare_i = 1;
  SQLRETURN pare_SQLSTATEs;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((pare_SQLSTATEs = SQLGetDiagRec(pare_HandleType, 
					 pare_InputHandle, 
					 pare_i,
					 pare_Sqlstate, 
					 &pare_NativeError, 
					 pare_Msg, 
					 sizeof(pare_Msg), 
					 &pare_MsgLen)
	  ) != SQL_NO_DATA)
    {
      ndbout << "SQLSTATE = " << pare_SQLSTATEs << endl;   
      ndbout << "the HandleType is:" << pare_HandleType << endl;
      ndbout << "the Handle is :" << (long)pare_InputHandle << endl;
      ndbout << "the conn_Msg is: " << (char *) pare_Msg << endl;
      ndbout << "the output state is:" << (char *)pare_Sqlstate << endl; 
      
      pare_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}



