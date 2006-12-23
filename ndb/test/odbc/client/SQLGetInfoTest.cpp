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
 * @file SQLGetInfoTest.cpp
 */

#include <common.hpp>

using namespace std;

SQLHDBC     GI_hdbc;
SQLHSTMT    GI_hstmt;
SQLHENV     GI_henv;

#define GI_MESSAGE_LENGTH 200

SQLCHAR   Msg[GI_MESSAGE_LENGTH];
       
void SQLGetInfoTest_DisplayError(SQLSMALLINT GI_HandleType, 
				 SQLHDBC GI_InputHandle);

/** 
 * Test to retrieve general information about the driver and
 * the data source an application is currently connected to.
 *
 * Tests:
 * -# Test The value of FunctionId is not in table 27
 * @return Zero, if test succeeded
 */

int SQLGetInfoTest()
{
  SQLRETURN retcode;
  SQLINTEGER    InfoValuePtr;
  SQLSMALLINT   SLPStringLengthPtr;

  ndbout << endl << "Start SQLGetInfo Testing" << endl;

  //******************************************************
  //** The value of FunctionId is not in Table 27, then **
  //** an exception condition is raised                 **
  //******************************************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &GI_henv);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(GI_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   GI_henv, 
			   &GI_hdbc);

  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(GI_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);
  

  // **********************
  // ** GET INFO FROM DB **
  // *********************

  retcode = SQLGetInfo(GI_hdbc, 
		       SQL_DATABASE_NAME, 
		       &InfoValuePtr, 
		       sizeof(InfoValuePtr), 
		       &SLPStringLengthPtr);

  if (retcode == SQL_SUCCESS)
    ndbout << endl << "Database Name:" << InfoValuePtr << endl;
  else
    {
      ndbout << endl << "retcode = SQLGetInfo() = " << retcode <<endl;
      SQLGetInfoTest_DisplayError(SQL_HANDLE_STMT, GI_hstmt);
    }

  retcode = SQLGetInfo(GI_hdbc, 
		       SQL_DRIVER_NAME, 
		       &InfoValuePtr, 
		       sizeof(InfoValuePtr), 
		       &SLPStringLengthPtr);

  if (retcode == SQL_SUCCESS)
    ndbout << endl << "Driver Name:" << InfoValuePtr << endl;
  else
    {
      ndbout << endl << "retcode = SQLGetInfo() = " << retcode <<endl;
      SQLGetInfoTest_DisplayError(SQL_HANDLE_STMT, GI_hstmt);
    }

  // **************************
  // ** INPUT WRONG InfoType **
  // **************************
  retcode = SQLGetInfo(GI_hdbc, 
		       8888, 
		       &InfoValuePtr, 
		       sizeof(InfoValuePtr), 
		       &SLPStringLengthPtr);
 if (retcode == -2)
   {
     ndbout << endl <<"retcode = " << retcode << endl;
     ndbout << "System reported -2. Please check your test programme" 
	    << " about the connectionhandle." << endl;
     SQLGetInfoTest_DisplayError(SQL_HANDLE_STMT, GI_hstmt);
   }
  else if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << endl << "retcode = " << retcode << endl;
      ndbout << "The information of InfoType is not in Table 28," 
	     << " but SQLGetInfo() executeed succeddfully." 
	     << " Check the function!" <<endl;
      SQLGetInfoTest_DisplayError(SQL_HANDLE_STMT, GI_hstmt);
    }
 else if (retcode == SQL_ERROR)
    {
      ndbout << endl << "retcode = " << retcode << endl;
      ndbout << "Input a wrong InfoType. The system found the" 
	     << " information of InfoType is not in Table 28." 
	     << " Test successful!" << endl;
    }
  else 
    ndbout << endl;
  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(GI_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, GI_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, GI_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, GI_henv);

  return NDBT_OK;

 }


void SQLGetInfoTest_DisplayError(SQLSMALLINT GI_HandleType, 
				 SQLHDBC GI_InputHandle)
{
  SQLRETURN   SQLSTATEs;
  SQLINTEGER    NativeError;
  SQLCHAR Sqlstate[50];
  SQLSMALLINT   i, MsgLen;
  i = 1;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  

  while ((SQLSTATEs = SQLGetDiagRec(GI_HandleType, 
				    GI_InputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
{

     ndbout << "the GI_HandleType is:" << GI_HandleType << endl;
     ndbout << "the GI_InputHandle is :" << (long)GI_InputHandle << endl;
     ndbout << "the Msg is :" << (char *) Msg << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 

     i ++;
     break;
                                                         }
  ndbout << "-------------------------------------------------" << endl;
}



