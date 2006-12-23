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
 * @file SQLGetDataTest.cpp
 */

#include <common.hpp>
using namespace std;

#define GD_MESSAGE_LENGTH 200

SQLHSTMT     GD_hstmt;
SQLHENV      GD_henv;
SQLHDBC      GD_hdbc;
SQLHDESC     GD_hdesc;

void GetData_DisplayError(SQLSMALLINT GD_HandleType, SQLHSTMT GD_InputHandle);

/** 
 * Test to retrieve data for a single unbound column
 * in the current row of a result data set
 *
 * Tests:
 * -# Test1 There is no fetched rowset associated with S
 * -# Test2 column number is less than zero
 * -# Test3 fetched rowset is empty
 * @return Zero, if test succeeded
 */

int SQLGetDataTest()
{
  SQLRETURN    retcode;
  SQLCHAR      ColumnName;
  SQLINTEGER   CustID;
  //  SQLCHAR      Name, Address, Phone;
  SQLCHAR SQLStmt [120];
  SQLCHAR SQLStmt1 [120];

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &GD_henv);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(GD_henv,
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.X!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   GD_henv, 
			   &GD_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(GD_hdbc, 
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
			   GD_hdbc, 
			   &GD_hstmt); 
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;

  //*****************************
  //** Define SELECT statement **
  //*****************************

  strcpy((char *) SQLStmt, "SELECT * FROM Customers");


  //***********************************
  //** Prepare SELECT SQL statement  **
  //***********************************

  retcode = SQLPrepare(GD_hstmt, 
		       SQLStmt, 
		       SQL_NTS);
  ndbout << endl << "Preparing SELECT, retcode = SQLprepare()= " 
	 << retcode << endl;

  //*********************************
  //** Execute prepared statement  **
  //*********************************

  //  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
  //{

  retcode = SQLExecute(GD_hstmt);

  ndbout << "Exexuting SELECT, retcode = SQLExecute()= " 
	 << retcode << endl;

  //  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
  //    {

      //*****************************************************************
      //** Test1                                                       **
      //** There is no fetched rowset associated with S(SQL-statement) **
      //*****************************************************************
      
      retcode = SQLGetData(GD_hstmt, 
			   1, 
			   SQL_C_SLONG, 
			   &CustID,
			   sizeof(CustID), 
			   NULL);
      ndbout << "retcode = SQLGetData()= " << retcode << endl;      

      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << endl << "Test 1:" << endl;
	  ndbout << "There is no fetched rowset associated with SQL" 
		 << " statement. But system reported SUCCESS or" 
		 << " SUCCESS_WITH_INFO. Please check the function!" << endl;
	  GetData_DisplayError(SQL_HANDLE_STMT, GD_hstmt);
	}
      else if (retcode == SQL_ERROR)
	{
	  ndbout << endl << "Test 1:" << endl;
	  ndbout << "There is no fetched rowset associated with SQL" 
		 << " statement. The system reported ERROR " 
		 << " The function is OK!" << endl;
	}      
      else 
	ndbout << endl;

      //*******************************
      //** Fetch Data from database  **
      //*******************************

      retcode = SQLFetch(GD_hstmt);

      ndbout << endl 
	     << "Fetching after Executing SELECT, retcode = SQLFetch()= " 
	     << retcode << endl;  

      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
	  
	  //**************************************
	  //** Test2                            **
	  //** column number is less than zero  **
	  //**************************************
	  
	  retcode = SQLGetData(GD_hstmt, 
			       0, 
			       SQL_C_ULONG, 
			       &CustID, 
			       sizeof(CustID), 
			       NULL);
	  
	  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	    {
	      ndbout << "Test 2:" <<"Column number is less than zero" 
		     << " The system reported SUCCESS or SUCCESS_WITH_INFO." 
		     << " Check the function, please!" <<endl;
	      GetData_DisplayError(SQL_HANDLE_STMT, GD_hstmt);
	    }    
	  else if (retcode == SQL_ERROR)
	    {
	      ndbout << "Test 2:" << "Column number is less than zero." 
		     << " The system reported SQL_ERROR." 
		     << " The function is OK!" << endl; 
	    }
	  else
	    ndbout << endl;
	}
      //    } 
    
      //  } 

  //*****************************
  //** Define DELETE statement **
  //*****************************

      //  strcpy((char *) SQLStmt1, "DELETE FROM Customers");
  strcpy((char *) SQLStmt1, "DELETE FROM Customers WHERE CustID = 568 AND Name = 'Hans  Peter'");

  //***********************************
  //** Prepare DELETE SQL statement  **
  //***********************************

  retcode = SQLPrepare(GD_hstmt, 
		       SQLStmt1, 
		       SQL_NTS);
  ndbout << endl << "Preparing DELETE, retcode = SQLPrepare()= " 
	 << retcode << endl;

  //****************************************
  //** Execute prepared DELETE statement **
  //****************************************

  //  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
  //    {
 
     retcode = SQLExecute(GD_hstmt);

     ndbout << "Executing DELETE, retcode = SQLExecute()= " 
	    << retcode << endl;
      
      //      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
      //	{
	  
	  retcode = SQLFetch(GD_hstmt);

	  ndbout << "Fetching after Executing DELETE, retcode = SQLExecute()= " 
		 << retcode << endl;

	  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
	    {
	      
	      //******************************************************
	      //** Test3                                            **
	      //** If the fetched rowset associated with            **
	      //** Statement is empty, condition is raised: NO DATA **
	      //** We can delete all rows in table Customers for    **
	      //** this case                                        **
	      //******************************************************
	      
	      retcode = SQLGetData(GD_hstmt, 
				   1, 
				   SQL_C_ULONG, 
				   &CustID, 
				   sizeof(CustID), 
				   NULL);
	      
	      if (retcode == SQL_ERROR)
		{
		  ndbout << "Test 3:" << endl;
		  ndbout << "The fetched rowset associated" 
			 << "with Statementhandle is empty. The system" 
			 << " reported SQL_ERROR. Check the function!" << endl;
		  GetData_DisplayError(SQL_HANDLE_STMT, GD_hstmt);        
		}
	      else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
		  ndbout << "Test 3:" << endl;
		  ndbout << "The fetched rowset associated" 
			 << "with Statementhandle is empty. The system" 
			 << " reported SUCCESS. Check the function!" << endl;
		  GetData_DisplayError(SQL_HANDLE_STMT, GD_hstmt);        
		}
	      else if (retcode == 100)
		{
		  ndbout << "Test 3:" << endl;
		  ndbout << "The fetched rowset associated" 
			 << "with Statementhandle is empty. The system" 
			 << " reported SQL_NO_DATA. The function is OK!" << endl;
		}
	    } 
	  else if (retcode == SQL_ERROR)
	    { 
	      ndbout << "Test 3 falied!" << endl;
	      GetData_DisplayError(SQL_HANDLE_STMT, GD_hstmt);
	    }
	  else 
	    ndbout << " " << endl;

	  //	}      
	  //    }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(GD_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, GD_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, GD_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, GD_henv);
  
  return NDBT_OK;
}

void GetData_DisplayError(SQLSMALLINT GD_HandleType, SQLHSTMT GD_InputHandle)
{
  
  SQLSMALLINT  i, MsgLen;
  SQLRETURN    SQLSTATEs;
  SQLCHAR      Sqlstate[5], Msg[GD_MESSAGE_LENGTH];
  SQLINTEGER   NativeError;
  i = 1;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((SQLSTATEs = SQLGetDiagRec(GD_HandleType, 
				    GD_InputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)  
    {
      
      ndbout << "the HandleType is:" << GD_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)GD_InputHandle << endl;
      ndbout << "Phone = " << (char *)Msg << endl;
      ndbout << "the output state is:" << (char *)Sqlstate << endl; 
      
      i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}



