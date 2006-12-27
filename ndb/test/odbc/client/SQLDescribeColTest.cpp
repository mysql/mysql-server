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
 * @file SQLDescribeColTest.cpp
 */
#include <common.hpp>

using namespace std;

#define DC_Column_NAME_LEN 50
#define DC_MESSAGE_LENGTH 200

SQLHSTMT    DC_hstmt;
SQLHDBC     DC_hdbc;
SQLHENV     DC_henv;
SQLHDESC    DC_hdesc;

void DescribeCol_DisplayError(SQLSMALLINT DC_HandleType, 
			      SQLHSTMT DC_InputHandle);

/** 
 * Test to retrieve  basic result data set metadata information
 * (specifically, column name, SQL data type, column size, decimal
 * precision, and nullability) for a specified column in a result
 * data set
 * -# No prepared or executed statement when executing
 * -# ColumnNumber is less than 1
 * -#  ColumnNumber is greater than the value of the TOP_LEVEL_COUNT field of IRD
 * @return Zero, if test succeeded
 */

int SQLDescribeColTest()
{
  SQLCHAR     SQLStmt [120];
  SQLRETURN   retcode;
  SQLCHAR     ColumnName[DC_Column_NAME_LEN];
  SQLSMALLINT NameLength, DataTypePtr, DecimalDigitsPtr, NullablePtr;
  SQLUINTEGER ColumnSizePtr;

  ndbout << "Start SQLDescribeCol Test " << endl;
  //******************************************************************
  //** Test1                                                        **
  //** There is no prepared or executed statement associated with   **
  //** StatementHandle                                              **
  //******************************************************************

  retcode = SQLDescribeCol(DC_hstmt, 
			   (SQLUSMALLINT)1, 
			   ColumnName, 
			   sizeof(ColumnName), 
			   &NameLength, 
			   &DataTypePtr, 
			   &ColumnSizePtr, 
			   &DecimalDigitsPtr, 
			   &NullablePtr);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    DescribeCol_DisplayError(SQL_HANDLE_STMT, DC_hstmt);


  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &DC_henv);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(DC_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   DC_henv, 
			   &DC_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(DC_hdbc, 
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
			   DC_hdbc, 
			   &DC_hstmt); 
  if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
  
  //************************
  //** Define a statement **
  //************************

  strcpy((char *) SQLStmt, 
	 "SELECT * FROM Customers");

  //***********************************************
  //** Prepare and Execute the SQL statement     **
  //***********************************************

  retcode = SQLExecDirect(DC_hstmt, 
			  SQLStmt, 
			  SQL_NTS);

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

  //*********************************
  //** ColumnNumber is from 1 to 4 **
  //*********************************
  ndbout << endl << "ColumnNumber is from 1 to 4" << endl;
  
  for (int ii = 1; ii <= 4; ii++)
 {  
  retcode = SQLDescribeCol(DC_hstmt, 
			   ii, 
			   ColumnName, 
			   sizeof(ColumnName), 
			   &NameLength, 
			   &DataTypePtr, 
			   &ColumnSizePtr, 
			   &DecimalDigitsPtr, 
			   &NullablePtr);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Column Name = " << (char *)ColumnName << endl;

 }

  //*********************************
  //** Test2                       **
  //** ColumnNumber is less than 1 **
  //*********************************
 
  retcode = SQLDescribeCol(DC_hstmt, 
			   (SQLUSMALLINT)-1, 
			   ColumnName, 
			   sizeof(ColumnName), 
			   &NameLength, 
			   &DataTypePtr, 
			   &ColumnSizePtr, 
			   &DecimalDigitsPtr, 
			   &NullablePtr);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << endl << "ColumnNumber is less than 1" << endl;
    DescribeCol_DisplayError(SQL_HANDLE_STMT, DC_hstmt);

  //*********************************************************************
  //** Test3                                                           **
  //** ColumnNumber is greater than N(the value of the TOP_LEVEL_COUNT ** 
  //** field of IRD)                                                   **
  //*********************************************************************
  
    ndbout << endl <<"ColumnNumber is greater than N(the value" 
	   << "of the TOP_LEVEL_COUNTfield of IRD)" << endl;

    retcode = SQLDescribeCol(DC_hstmt, 
			     (SQLUSMALLINT)1045, 
			     ColumnName, 
			     sizeof(ColumnName), 
			     &NameLength, 
			     &DataTypePtr, 
			     &ColumnSizePtr, 
			     &DecimalDigitsPtr, 
			     &NullablePtr);

    if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
      DescribeCol_DisplayError(SQL_HANDLE_STMT, DC_hstmt);
    
}

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(DC_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, DC_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, DC_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, DC_henv);

 return NDBT_OK;
}

void DescribeCol_DisplayError(SQLSMALLINT DC_HandleType, 
			      SQLHSTMT DC_InputHandle)
{
  SQLCHAR    Sqlstate[5], Msg[DC_MESSAGE_LENGTH];
  SQLINTEGER NativeError;
  SQLSMALLINT DC_i, MsgLen;
  SQLRETURN  SQLSTATEs;

  DC_i = 1;

  while ((SQLSTATEs = SQLGetDiagRec(DC_HandleType, 
				    DC_InputHandle, 
				    DC_i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   {

    ndbout << "the HandleType is:" << DC_HandleType << endl;
    ndbout << "the InputHandle is :" << (long)DC_InputHandle << endl;
    ndbout << "the return message is:" << (char *)Msg << endl;
    ndbout << "the output state is:" << (char *)Sqlstate << endl; 
    
    DC_i ++;
    break;
  }

}



