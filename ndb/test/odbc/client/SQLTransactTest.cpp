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
 * @file SQLTransactTest.cpp
 */
#include <common.hpp>
#define STR_MESSAGE_LENGTH 200
#define STR_NAME_LEN 20
#define STR_PHONE_LEN 20
#define STR_ADDRESS_LEN 20
using namespace std;

SQLHDBC     STR_hdbc;
SQLHSTMT    STR_hstmt;
SQLHENV     STR_henv;
SQLHDESC    STR_hdesc;
       
void Transact_DisplayError(SQLSMALLINT STR_HandleType, 
			   SQLHSTMT STR_InputHandle);

int STR_Display_Result(SQLHSTMT EXDR_InputHandle);

/** 
 * Test:
 * -#Test to request a commit or a rollback operation for
 *   all active transactions associated with a specific
 *   environment or connection handle
 *
 * @return Zero, if test succeeded
 */

int SQLTransactTest()
{
  SQLRETURN   STR_ret;

  ndbout << endl << "Start SQLTransact Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  STR_ret = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &STR_henv);
  
  if (STR_ret == SQL_SUCCESS || STR_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  STR_ret = SQLSetEnvAttr(STR_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3,
			  SQL_IS_UINTEGER);
  
  if (STR_ret == SQL_SUCCESS || STR_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  STR_ret = SQLAllocHandle(SQL_HANDLE_DBC, 
			   STR_henv, 
			   &STR_hdbc);

  if (STR_ret == SQL_SUCCESS || STR_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  STR_ret = SQLConnect(STR_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);
  
  if (STR_ret == SQL_SUCCESS || STR_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }

  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  STR_ret = SQLAllocHandle(SQL_HANDLE_STMT, 
			   STR_hdbc, 
			   &STR_hstmt); 
  if(STR_ret == SQL_SUCCESS || STR_ret == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;

  //********************************
  //** Turn Manual-Commit Mode On **
  //********************************
  STR_ret = SQLSetConnectOption(STR_hdbc, 
				SQL_AUTOCOMMIT, 
				(UDWORD) SQL_AUTOCOMMIT_OFF);

  //**********************************************
  //** Prepare and Execute a prepared statement **
  //**********************************************
  STR_ret = SQLExecDirect(STR_hstmt, 
			  (SQLCHAR*)"SELECT * FROM Customers", 
			  SQL_NTS);

  if (STR_ret == SQL_INVALID_HANDLE)
  {   
    ndbout << "Handle Type is SQL_HANDLE_STMT, but SQL_INVALID_HANDLE" 
	   << endl;
    ndbout << "still appeared. Please check program" << endl;
  }

  if (STR_ret == SQL_ERROR || STR_ret == SQL_SUCCESS_WITH_INFO)
    Transact_DisplayError(SQL_HANDLE_STMT, STR_hstmt);

  //*************************
  //** Display the results **
  //*************************

  STR_Display_Result(STR_hstmt);

  //****************************
  //** Commit the transaction **
  //****************************
  STR_ret = SQLTransact(STR_henv, 
			STR_hdbc, 
			SQL_COMMIT);

  //****************
  // Free Handles **
  //****************
  SQLDisconnect(STR_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, STR_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, STR_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, STR_henv);

  return NDBT_OK;

 }

void Transact_DisplayError(SQLSMALLINT STR_HandleType, 
			     SQLHSTMT STR_InputHandle)
{
     SQLCHAR STR_Sqlstate[5];
     SQLINTEGER   STR_NativeError;
     SQLSMALLINT  STR_i, STR_MsgLen;
     SQLCHAR      STR_Msg[STR_MESSAGE_LENGTH];
     SQLRETURN    SQLSTATEs;
     STR_i = 1;

     ndbout << "-------------------------------------------------" << endl;
     ndbout << "Error diagnostics:" << endl;
  
     while ((SQLSTATEs = SQLGetDiagRec(STR_HandleType, 
				       STR_InputHandle, 
				       STR_i, 
				       STR_Sqlstate, 
				       &STR_NativeError, 
				       STR_Msg, 
				       sizeof(STR_Msg), 
				       &STR_MsgLen)) 
	    != SQL_NO_DATA)                   {

     ndbout << "the HandleType is:" << STR_HandleType << endl;
     ndbout << "the InputHandle is :" << (long)STR_InputHandle << endl;
     ndbout << "the STR_Msg is: " << (char *) STR_Msg << endl;
     ndbout << "the output state is:" << (char *)STR_Sqlstate << endl; 

     STR_i ++;
     //     break;
                                                       }
     ndbout << "-------------------------------------------------" << endl;
}

int STR_Display_Result(SQLHSTMT STR_InputHandle)
{
  SQLRETURN   STR_retcode;
  unsigned long  STR_CustID;
  SQLCHAR     STR_Name[STR_NAME_LEN], STR_Phone[STR_PHONE_LEN];
  SQLCHAR     STR_Address[STR_ADDRESS_LEN];

  //*********************
  //** Bind columns  1 **
  //*********************
  STR_retcode =SQLBindCol(STR_InputHandle, 
			  1, 
			  SQL_C_ULONG, 
			  &STR_CustID, 
			  sizeof(STR_CustID),
			  NULL);
      if (STR_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  Transact_DisplayError(SQL_HANDLE_STMT, STR_InputHandle);
	  return NDBT_FAILED;
	}

  //*********************
  //** Bind columns  2 **
  //*********************

  STR_retcode =SQLBindCol(STR_InputHandle, 
			  2, 
			  SQL_C_CHAR, 
			  &STR_Name, 
			  STR_NAME_LEN,
			  NULL);
      if (STR_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  Transact_DisplayError(SQL_HANDLE_STMT, STR_InputHandle);
	  return NDBT_FAILED;
	}

  //*********************
  //** Bind columns 3  **
  //*********************

      STR_retcode = SQLBindCol(STR_InputHandle, 
			       3,
			       SQL_C_CHAR, 
			       &STR_Address, 
			       STR_ADDRESS_LEN, 
			       NULL);

      if (STR_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  Transact_DisplayError(SQL_HANDLE_STMT, STR_InputHandle);
	  return NDBT_FAILED;
	}

  //*********************
  //** Bind columns 4  **
  //*********************

      STR_retcode = SQLBindCol(STR_InputHandle, 
			       4, 
			       SQL_C_CHAR, 
			       &STR_Phone, 
			       STR_PHONE_LEN,
			       NULL);

      if (STR_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  Transact_DisplayError(SQL_HANDLE_STMT, STR_InputHandle);
	  return NDBT_FAILED;
	}

  //*****************************************
  //* Fetch and print each row of data. On **
  //* an error, display a message and exit **
  //*****************************************
 
  if (STR_retcode != SQL_ERROR)
  STR_retcode = SQLFetch(STR_InputHandle);

  ndbout << endl << "STR_retcode = SQLFetch(STR_InputHandle) = " 
	 << STR_retcode << endl;

  if (STR_retcode == SQL_ERROR)
    { 
      ndbout << "Executing SQLFetch, SQL_ERROR happened!" << endl;
      Transact_DisplayError(SQL_HANDLE_STMT, STR_InputHandle);
      return NDBT_FAILED;
    }
  else if (STR_retcode == SQL_SUCCESS_WITH_INFO) 
    {
    ndbout << "CustID = " << (int)STR_CustID << endl;
    ndbout << "Name = " << (char *)STR_Name << endl;
    ndbout << "Address = " << (char *)STR_Address << endl;
    ndbout << "Phone = " << (char *)STR_Phone << endl; 
    Transact_DisplayError(SQL_HANDLE_STMT, STR_InputHandle);
    }
  else
   {
    ndbout << "CustID = " << (int)STR_CustID << endl;
    ndbout << "Name = " << (char *)STR_Name << endl;
    ndbout << "Address = " << (char *)STR_Address << endl;
    ndbout << "Phone = " << (char *)STR_Phone << endl;     
   }
 return 0;
}
