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
 * @file SQLExecDirectTest.cpp
 */
#include <common.hpp>
#define EXD_MESSAGE_LENGTH 200
#define EXD_NAME_LEN 10
#define EXD_PHONE_LEN 10
#define EXD_ADDRESS_LEN 10
using namespace std;

SQLHDBC     EXD_hdbc;
SQLHSTMT    EXD_hstmt;
SQLHENV     EXD_henv;
SQLHDESC    EXD_hdesc;
SQLRETURN   EXD_ret, SQLSTATEs;
       
void ExecDirect_DisplayError(SQLSMALLINT EXD_HandleType, 
			     SQLHSTMT EXD_InputHandle);

int EXD_Display_Result(SQLHSTMT EXDR_InputHandle);

/** 
 * Test to execute a prepared ststement
 *
 * -# Normal case: Prepare and Execute a prepared statement
 * -# Prepare and Execute an empty statement
 * -# Prepare and Execute a statement with wrong henv handle
 * -# Prepare and Execute a statement with wrong hdbc handle
 * -# Prepare and Execute a statement with wrong hdesc handle
 * @return Zero, if test succeeded
 */

int SQLExecDirectTest()
{
  ndbout << endl << "Start ExecDirect Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  EXD_ret = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &EXD_henv);
  
if (EXD_ret == SQL_SUCCESS || EXD_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  EXD_ret = SQLSetEnvAttr(EXD_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
if (EXD_ret == SQL_SUCCESS || EXD_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  EXD_ret = SQLAllocHandle(SQL_HANDLE_DBC, 
			   EXD_henv, 
			   &EXD_hdbc);

if (EXD_ret == SQL_SUCCESS || EXD_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  EXD_ret = SQLConnect(EXD_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);
  
if (EXD_ret == SQL_SUCCESS || EXD_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }

  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  EXD_ret = SQLAllocHandle(SQL_HANDLE_STMT, 
			   EXD_hdbc, 
			   &EXD_hstmt); 
if(EXD_ret == SQL_SUCCESS || EXD_ret == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;

  //**********************************************
  //** Test1                                    **
  //** Prepare and Execute a prepared statement **
  //**********************************************
  EXD_ret = SQLExecDirect(EXD_hstmt, 
			  (SQLCHAR*)"SELECT * FROM Customers", 
			  SQL_NTS);

  if (EXD_ret == SQL_INVALID_HANDLE)
  {   
    ndbout << "Handle Type is SQL_HANDLE_STMT, but SQL_INVALID_HANDLE" << endl;
    ndbout << "still appeared. Please check program" << endl;
  }

  if (EXD_ret == SQL_ERROR || EXD_ret == SQL_SUCCESS_WITH_INFO)
    ExecDirect_DisplayError(SQL_HANDLE_STMT, EXD_hstmt);

  //*************************
  //** Display the results **
  //*************************

  EXD_Display_Result(EXD_hstmt);

  //*******************************************
  //** Test2                                 **
  //** Prepare and Execute an empty statement**
  //** in order to see what will happen      **
  //******************************************* 
  EXD_ret = SQLExecDirect(EXD_hstmt, 
			  (SQLCHAR*)" ", 
			  SQL_NTS);

  if (EXD_ret == SQL_ERROR || EXD_ret == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "Prepare and Execute an empty statement," << endl;
      ndbout << "The following case happened!" << endl;
      ExecDirect_DisplayError(SQL_HANDLE_STMT, EXD_hstmt);
    }

  //***************************************************************
  //** Test3                                                     **         
  //** Prepare and Execute a statement with wrong henv handle    **
  //** in order to see what will happen                          **
  //***************************************************************
  EXD_ret = SQLExecDirect(EXD_henv,  
			  (SQLCHAR*)"SELECT * FROM Customers", 
			  SQL_NTS);

  if (EXD_ret == SQL_SUCCESS_WITH_INFO || EXD_ret == SQL_SUCCESS)
  { ndbout << "Handle Type is SQL_HANDLE_HENV, but SQL_INVALID_HANDLE" << endl;
    ndbout << "still appeared. Please check programm" << endl;
    ExecDirect_DisplayError(SQL_HANDLE_ENV, EXD_henv);
  }

  //******************************************************************
  //** Test4                                                        **
  //** Prepare and Execute a statement with wrong hdbc handle       **
  //** in order to see what will happen                             **
  //******************************************************************

  EXD_ret = SQLExecDirect(EXD_hdbc,  
			  (SQLCHAR*)"SELECT * FROM Customers", 
			  SQL_NTS);

  if (EXD_ret == SQL_SUCCESS_WITH_INFO || EXD_ret == SQL_SUCCESS)
  ExecDirect_DisplayError(SQL_HANDLE_DBC, EXD_hdbc);

  //*******************************************************************
  //** Test5                                                         **
  //** Prepare and Execute a statement with wrong hdesc handle       **
  //** in order to see what will happen                              **
  //*******************************************************************

  EXD_ret = SQLExecDirect(EXD_hdesc,  
			  (SQLCHAR*)"SELECT * FROM Customers", 
			  SQL_NTS);

  if (EXD_ret == SQL_SUCCESS_WITH_INFO || EXD_ret == SQL_SUCCESS)
  {  
  ndbout << "Handle Type is SQL_HANDLE_DESC, but SQL_SUCCESS_WITH_INFO" <<endl;
  ndbout << "appeared. Please check program" << endl;
  ExecDirect_DisplayError(SQL_HANDLE_DESC, EXD_hdesc);
  }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(EXD_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, EXD_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, EXD_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, EXD_henv);

  return NDBT_OK;

 }


void ExecDirect_DisplayError(SQLSMALLINT EXD_HandleType, 
			     SQLHSTMT EXD_InputHandle)
{
     SQLCHAR EXD_Sqlstate[5];
     SQLINTEGER   EXD_NativeError;
     SQLSMALLINT  EXD_i, EXD_MsgLen;
     SQLCHAR      EXD_Msg[EXD_MESSAGE_LENGTH];
     SQLRETURN    SQLSTATEs;
     EXD_i = 1;

     ndbout << "-------------------------------------------------" << endl;
     ndbout << "Error diagnostics:" << endl;
  
     while ((SQLSTATEs = SQLGetDiagRec(EXD_HandleType, 
				       EXD_InputHandle, 
				       EXD_i, 
				       EXD_Sqlstate, 
				       &EXD_NativeError, 
				       EXD_Msg, 
				       sizeof(EXD_Msg), 
				       &EXD_MsgLen)) 
	    != SQL_NO_DATA)                   {

     ndbout << "the HandleType is:" << EXD_HandleType << endl;
     ndbout << "the InputHandle is :" << (long)EXD_InputHandle << endl;
     ndbout << "the ColAtt_Msg is: " << (char *) EXD_Msg << endl;
     ndbout << "the output state is:" << (char *)EXD_Sqlstate << endl; 

     EXD_i ++;
     //     break;
                                                       }
     ndbout << "-------------------------------------------------" << endl;
}

int EXD_Display_Result(SQLHSTMT EXDR_InputHandle)
{
  SQLRETURN   EXD_retcode;
  unsigned long  EXD_CustID;
  SQLCHAR     EXD_Name[EXD_NAME_LEN], EXD_Phone[EXD_PHONE_LEN];
  SQLCHAR     EXD_Address[EXD_ADDRESS_LEN];

  //*********************
  //** Bind columns  1 **
  //*********************
  EXD_retcode =SQLBindCol(EXDR_InputHandle, 
			  1, 
			  SQL_C_ULONG, 
			  &EXD_CustID, 
			  sizeof(EXD_CustID),
			  NULL);
      if (EXD_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  ExecDirect_DisplayError(SQL_HANDLE_STMT, EXDR_InputHandle);
	  return NDBT_FAILED;
	}
  //*********************
  //** Bind columns  2 **
  //*********************

  EXD_retcode =SQLBindCol(EXDR_InputHandle, 
			  2, 
			  SQL_C_CHAR, 
			  &EXD_Name, 
			  EXD_NAME_LEN,
			  NULL);
      if (EXD_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  ExecDirect_DisplayError(SQL_HANDLE_STMT, EXDR_InputHandle);
	  return NDBT_FAILED;
	}

  //*********************
  //** Bind columns 3  **
  //*********************

      EXD_retcode = SQLBindCol(EXDR_InputHandle, 
			       3,
			       SQL_C_CHAR, 
			       &EXD_Address, 
			       EXD_ADDRESS_LEN, 
			       NULL);

      if (EXD_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  ExecDirect_DisplayError(SQL_HANDLE_STMT, EXDR_InputHandle);
	  return NDBT_FAILED;
	}

  //*********************
  //** Bind columns 4  **
  //*********************

      EXD_retcode = SQLBindCol(EXDR_InputHandle, 
			       4, 
			       SQL_C_CHAR, 
			       &EXD_Phone, 
			       EXD_PHONE_LEN,
			       NULL);

      if (EXD_retcode == SQL_ERROR)
	{ 
	  ndbout << "Executing SQLBindCol, SQL_ERROR happened!" << endl;
	  ExecDirect_DisplayError(SQL_HANDLE_STMT, EXDR_InputHandle);
	  return NDBT_FAILED;
	}

  //*****************************************
  //* Fetch and print each row of data. On **
  //* an error, display a message and exit **
  //*****************************************
 
  if (EXD_retcode != SQL_ERROR)
  EXD_retcode = SQLFetch(EXDR_InputHandle);

  ndbout << endl << "EXD_retcode = SQLFetch(EXDR_InputHandle) = " 
	 << EXD_retcode << endl;

 if (EXD_retcode == SQL_ERROR)
    { 
      ndbout << "Executing SQLFetch, SQL_ERROR happened!" << endl;
      ExecDirect_DisplayError(SQL_HANDLE_STMT, EXDR_InputHandle);
      return NDBT_FAILED;
    }
  else if (EXD_retcode == SQL_SUCCESS_WITH_INFO) 
    {
    ndbout << "CustID = " << (int)EXD_CustID << endl;
    ndbout << "Name = " << (char *)EXD_Name << endl;
    ndbout << "Address = " << (char *)EXD_Address << endl;
    ndbout << "Phone = " << (char *)EXD_Phone << endl; 
    ExecDirect_DisplayError(SQL_HANDLE_STMT, EXDR_InputHandle);
    }
  else
   {
    ndbout << "CustID = " << (int)EXD_CustID << endl;
    ndbout << "Name = " << (char *)EXD_Name << endl;
    ndbout << "Address = " << (char *)EXD_Address << endl;
    ndbout << "Phone = " << (char *)EXD_Phone << endl;     
   }
 return 0;
}
