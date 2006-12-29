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
 * @file SQLGetTypeInfoTest.cpp
 */

#include <common.hpp>
#define GT_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     GT_hdbc;
SQLHSTMT    GT_hstmt;
SQLHENV     GT_henv;
       
void SQLGetTypeInfoTest_DisplayError(SQLSMALLINT GT_HandleType, 
				     SQLHDBC GT_InputHandle);

/** 
 * Test to retrieve general information about the data types
 * supported by the data source an application is currently connected to.
 *
 * Tests:
 * -# Test The value of FunctionId is not in table 37
 * @return Zero, if test succeeded
 */
int SQLGetTypeInfoTest()
{
  SQLRETURN   retcode;
  SQLSMALLINT ColumnSize;
  unsigned long TypeName;
  //  SQLCHAR TypeName[128];

  ndbout << endl << "Start SQLGetTypeInfo Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &GT_henv);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(GT_henv,
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.X!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   GT_henv, 
			   &GT_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(GT_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);


  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  retcode = SQLAllocHandle(SQL_HANDLE_STMT, 
			   GT_hdbc, 
			   &GT_hstmt); 


  //***********************************************
  //** Get DataType From the Current Application **
  //***********************************************
  retcode = SQLGetTypeInfo(GT_hstmt, SQL_CHAR);
  ndbout << "retcode =SQLGetTypeInfo()= " << retcode << endl;
  if (retcode == SQL_SUCCESS)
    {
      retcode =SQLBindCol(GT_hstmt, 
			  2, 
			  SQL_C_ULONG, 
			  TypeName, 
			  sizeof(TypeName),
			  NULL);
      ndbout << "retcode = SQLBindCol()= " << retcode << endl;

      //      retcode =SQLBindCol(GT_hstmt, 
      //			  1, 
      //			  SQL_C_DEFAULT, 
      //			  ColumnSize, 
      //			  sizeof(ColumnSize),
      //			  NULL);

      retcode = SQLFetch(GT_hstmt);
      ndbout << "retcode = SQLFETCH()=" << retcode << endl;
      ndbout << "DataType = " << TypeName << endl;

    }

  //*******************************************************
  //** If the Value of DataType is not in Table 37, then **
  //** an exception condition is raised                  **
  //*******************************************************
  retcode = SQLGetTypeInfo(GT_hstmt, 8888888);
  if (retcode == -2)
   {
     ndbout << "retcode = " << retcode << endl;
     ndbout << "The value of DataType is not in table 37" << endl;
     SQLGetTypeInfoTest_DisplayError(SQL_HANDLE_STMT, GT_hstmt);
   }
  else if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "retcode = " << retcode << endl;
      ndbout << endl << "The value of DataType is not in Table 37" << endl;
      SQLGetTypeInfoTest_DisplayError(SQL_HANDLE_STMT, GT_hstmt);
    }
  else 
    {
      ndbout << "retcode = " << retcode << endl;
      ndbout << endl << "The value of DataType is not in Table 37" << endl;
      SQLGetTypeInfoTest_DisplayError(SQL_HANDLE_STMT, GT_hstmt);
    }
  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(GT_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, GT_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, GT_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, GT_henv);

  return NDBT_OK;

 }

void SQLGetTypeInfoTest_DisplayError(SQLSMALLINT GT_HandleType, 
				     SQLHDBC GT_InputHandle)
{
  SQLINTEGER    NativeError;
  SQLSMALLINT   i, MsgLen;
  SQLCHAR   Msg[GT_MESSAGE_LENGTH];
  SQLRETURN   SQLSTATEs;
  SQLCHAR Sqlstate[50];
  i = 1;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;

  while ((SQLSTATEs = SQLGetDiagRec(GT_HandleType, 
				    GT_InputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
{

     ndbout << "the HandleType is:" << GT_HandleType << endl;
     ndbout << "the InputHandle is :" << (long)GT_InputHandle << endl;
     ndbout << "the Msg is :" << (char *) Msg << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 

     i ++;
     break;
                                                         }
  ndbout << "-------------------------------------------------" << endl;
}



