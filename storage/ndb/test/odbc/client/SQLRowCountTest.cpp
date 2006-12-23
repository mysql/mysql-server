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
 * @file SQLRowCountTest.cpp
 */

#include <common.hpp>
using namespace std;

#define NAME_LEN 50
#define PHONE_LEN 10
#define SALES_PERSON_LEN 10
#define STATUS_LEN 6
#define RC_MESSAGE_LENGTH 200

SQLHSTMT    RC_hstmt;
SQLHDBC     RC_hdbc;
SQLHENV     RC_henv;
SQLHDESC    RC_hdesc;

void SQLRowCountTest_DisplayError(SQLSMALLINT RC_HandleType, 
				  SQLHSTMT RC_InputHandle);

/** 
 * Test to obtain a count of the number of rows
 * in a table
 *
 * -# Call SQLRowCount without executed statement
 * -# Call SQLRowCount with normal case
 *
 * @return Zero, if test succeeded
 */

int SQLRowCountTest()
{
  SQLRETURN   retcode;
  unsigned long  RowCount;
  SQLCHAR SQLStmt [120];

  ndbout << endl << "Start SQLRowCount Testing" << endl;

  //************************************************************************
  //* If there is no executed statement, an execption condotion is raised **
  //************************************************************************

      retcode = SQLRowCount(RC_hstmt, &RowCount);
      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
	{

         SQLRowCountTest_DisplayError(SQL_HANDLE_STMT, RC_hstmt);
	}

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &RC_henv);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(RC_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   RC_henv, 
			   &RC_hdbc);

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
 retcode = SQLConnect(RC_hdbc, 
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
			   RC_hdbc, 
			   &RC_hstmt); 
  if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
  
  //************************
  //** Define a statement **
  //************************
   strcpy((char *) SQLStmt, "INSERT INTO Customers (CustID, Name, Address,Phone) VALUES(588, 'HeYong','LM888','919888')");

  //*******************************
  //* Prepare  the SQL statement **
  //*******************************

  retcode = SQLPrepare(RC_hstmt, 
		       SQLStmt, 
		       SQL_NTS);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

  //******************************
  //* Execute the SQL statement **
  //******************************
    retcode = SQLExecute(RC_hstmt);
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
 
  //***************
  // Normal test **
  //***************
    retcode = SQLRowCount(RC_hstmt, &RowCount);
    if  (retcode == SQL_ERROR )
      SQLRowCountTest_DisplayError(SQL_HANDLE_STMT, RC_hstmt);
    else
      ndbout << endl << "Number of the rows in the table Customers: " 
	     << (int)RowCount << endl;
    }
  }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(RC_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, RC_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, RC_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, RC_henv);

  return NDBT_OK;
  
}

void SQLRowCountTest_DisplayError(SQLSMALLINT RC_HandleType, 
				  SQLHSTMT RC_InputHandle)
{
  SQLRETURN   SQLSTATEs;
  SQLSMALLINT i, MsgLen;
  SQLCHAR     Sqlstate[5], Msg[RC_MESSAGE_LENGTH];
  SQLINTEGER  NativeError;
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(RC_HandleType, 
				       RC_InputHandle, 
				       i, 
				       Sqlstate, 
				       &NativeError, 
				       Msg, 
				       sizeof(Msg), 
				       &MsgLen)) 
	    != SQL_NO_DATA)                   
{

     ndbout << "the HandleType is:" << RC_HandleType << endl;
     ndbout << "the InputHandle is :" << (long)RC_InputHandle << endl;
     ndbout << "the Msg:" << (char *)Msg << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 

     i ++;
}

}



