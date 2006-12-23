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
 * @file SQLTablesTest.cpp
 */
#include <common.hpp>
using namespace std;

#define Tables_NAME_LEN 12
#define Tables_PHONE_LEN 12
#define Tables_ADDRESS_LEN 12
#define Tables_SQL_MAXIMUM_MESSAGE_LENGTH 200

SQLHDBC     Tables_hdbc;
SQLHSTMT    Tables_hstmt;
SQLHENV     Tables_henv;
SQLHDESC    Tables_hdesc;

void Tables_DisplayError(SQLSMALLINT Tables_HandleType, 
			  SQLHSTMT Tables_InputHandle);

/** 
 * Test to retrieve a list of table names stored in aspecified
 * data source's system
 *
 * -# Normal case test: print out the table name in the data result set
 * @return Zero, if test succeeded
 */

int SQLTablesTest()
{
  SQLRETURN   Tables_retcode;
  SQLCHAR     Tables_Name[Tables_NAME_LEN], Tables_Phone[Tables_PHONE_LEN];
  SQLCHAR     Tables_Address[Tables_ADDRESS_LEN];
  SQLINTEGER  Tables_CustID;

  ndbout << endl << "Start SQLTables Testing" << endl;

  //*******************************************************************
  //** hstmt 
  //** Execute a statement to retrieve rows from the Customers table **
  //** We can create the table and insert rows into Customers        **
  //*******************************************************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  Tables_retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
		        	   SQL_NULL_HANDLE, 
				   &Tables_henv);
  
if (Tables_retcode == SQL_SUCCESS || Tables_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  Tables_retcode = SQLSetEnvAttr(Tables_henv, 
			          SQL_ATTR_ODBC_VERSION, 
			          (SQLPOINTER) SQL_OV_ODBC3, 
			          SQL_IS_UINTEGER);
  
if (Tables_retcode == SQL_SUCCESS || Tables_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  Tables_retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
				   Tables_henv, 
				   &Tables_hdbc);

if (Tables_retcode == SQL_SUCCESS || Tables_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  Tables_retcode = SQLConnect(Tables_hdbc, 
			       (SQLCHAR *) connectString(), 
			       SQL_NTS, 
			       (SQLCHAR *) "", 
			       SQL_NTS, 
			       (SQLCHAR *) "", 
			       SQL_NTS);
  
if (Tables_retcode == SQL_SUCCESS || Tables_retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }

  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  Tables_retcode = SQLAllocHandle(SQL_HANDLE_STMT, 
				  Tables_hdbc, 
				  &Tables_hstmt); 

if (Tables_retcode == SQL_SUCCESS || Tables_retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
  
  //**************************************************************
  //** Retrieve information about the tables in the data source **
  //**************************************************************
 Tables_retcode = SQLTables(Tables_hstmt,
			    NULL,
			    0,
			    NULL,
			    0,
			    (SQLCHAR *)"%",
			    128,
			    (SQLCHAR *)"TABLES", 
			    128);

 ndbout <<"Tables_retcode = SQLTables() =" << Tables_retcode; 

 if (Tables_retcode == SQL_ERROR)
   Tables_DisplayError(SQL_HANDLE_STMT, Tables_hstmt);
  //*******************************************
  //** Bind columns 3 in the result data set **
  //*******************************************

  Tables_retcode = SQLBindCol(Tables_hstmt, 
			      3,
			      SQL_C_CHAR, 
			      &Tables_Name, 
			      Tables_NAME_LEN, 
			      NULL);

 ndbout <<"Tables_retcode = SQLBindCol() =" << Tables_retcode;

  //**********************************************
  //* Fetch and print out data in the result On **
  //* an error, display a message and exit      **
  //**********************************************
 
  Tables_retcode = SQLFetch(Tables_hstmt);


 ndbout <<"Tables_retcode = SQLFetch() =" << Tables_retcode;

  ndbout << endl << "Tables_retcode = SQLFetch(Tables_hstmt) = " 
	 << Tables_retcode << endl;

 if (Tables_retcode == SQL_ERROR)
    { 
      Tables_DisplayError(SQL_HANDLE_STMT, Tables_hstmt);
      return NDBT_FAILED;
    }
  else if (Tables_retcode == SQL_SUCCESS_WITH_INFO) 
    {
      ndbout << "Table Name = " << (char *)Tables_Name << endl;
      Tables_DisplayError(SQL_HANDLE_STMT, Tables_hstmt);
    }
 else if (Tables_retcode == SQL_NO_DATA)
     Tables_DisplayError(SQL_HANDLE_STMT, Tables_hstmt);
 else
   {
     ndbout << "TableName = " << (char *)Tables_Name << endl;  
     Tables_DisplayError(SQL_HANDLE_STMT, Tables_hstmt);
   }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(Tables_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, Tables_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, Tables_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, Tables_henv);

 return NDBT_OK;
}

void Tables_DisplayError(SQLSMALLINT Tables_HandleType, 
			 SQLHSTMT Tables_InputHandle)
{
  SQLINTEGER  NativeError;
  SQLSMALLINT Tables_i = 1;
  SQLRETURN Tables__SQLSTATEs;
  SQLCHAR Tables_Sqlstate[5];
  SQLCHAR Tables_Msg[Tables_SQL_MAXIMUM_MESSAGE_LENGTH];
  SQLSMALLINT Tables_MsgLen;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((Tables__SQLSTATEs = SQLGetDiagRec(Tables_HandleType, 
					     Tables_InputHandle, 
					     Tables_i, 
					     Tables_Sqlstate, 
					     &NativeError, 
					     Tables_Msg, 
					     sizeof(Tables_Msg), 
					     &Tables_MsgLen)
	  ) != SQL_NO_DATA)
    {

      ndbout << "the HandleType is:" << Tables_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)Tables_InputHandle << endl;
      ndbout << "the Tables_Msg is: " << (char *) Tables_Msg << endl;
      ndbout << "the output state is:" << (char *)Tables_Sqlstate << endl; 

      Tables_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}

