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
 * @file SQLGetDiagRecSimpleTest.cpp
 */
#include <common.hpp>
#include <string.h>

using namespace std;

SQLHDBC     GDS_hdbc;
SQLHSTMT    GDS_hstmt;
SQLHENV     GDS_henv;
SQLHDESC    GDS_hdesc;
SQLRETURN   GDS_retcode, GDS_RETURN;

#define GDS_SQL_MAXIMUM_MESSAGE_LENGTH 255
SQLCHAR GDS_Sqlstate[5];

SQLINTEGER    GDS_NativeError;
SQLSMALLINT   GDS_i = 1, GDS_MsgLen;
SQLCHAR   GDS_Msg[GDS_SQL_MAXIMUM_MESSAGE_LENGTH], GDS_ConnectIn[30];

/** 
 * Test SQLGetDiagRec return value
 *
 * -#Simply test Msg when return is SQL_NO_DATA
 * -#Simply test Msg when return is SQL_SUCCESS
 * -#Simply test Msg when return is SQL_SUCCESS_WITH_INFO
 * -#Simply test Msg when return is SQL_INVALID_HANDLE
 * -#Simply test Msg when return is SQL_ERROR
 *
 * @return Zero, if test succeeded
 */
       
int SQLGetDiagRecSimpleTest()
{
     ndbout << endl << "Start SQLGetDiagRec Simple Testing" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************

     GDS_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &GDS_henv);
     if (GDS_retcode == SQL_SUCCESS || GDS_retcode == SQL_SUCCESS_WITH_INFO)
         ndbout << "Allocated An Environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************

     GDS_retcode = SQLSetEnvAttr(GDS_henv, 
				 SQL_ATTR_ODBC_VERSION, 
				 (SQLPOINTER) SQL_OV_ODBC3, 
				 SQL_IS_UINTEGER);

     if (GDS_retcode == SQL_SUCCESS || GDS_retcode == SQL_SUCCESS_WITH_INFO)
     ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

     GDS_retcode = SQLAllocHandle(SQL_HANDLE_DBC, GDS_henv, &GDS_hdbc);

     if (GDS_retcode == SQL_SUCCESS || GDS_retcode == SQL_SUCCESS_WITH_INFO)
         ndbout << "Allocated A Connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************

    GDS_retcode = SQLConnect(GDS_hdbc, 
			    (SQLCHAR *) connectString(),
			    SQL_NTS, 
			    (SQLCHAR *) "", 
			    SQL_NTS, 
			    (SQLCHAR *) "", 
			    SQL_NTS);

     if (GDS_retcode == SQL_SUCCESS || GDS_retcode == SQL_SUCCESS_WITH_INFO){
       ndbout << "Success connection to DB!" << endl;
       ndbout << "GDS_retcode = " << GDS_retcode << endl; 
       ndbout << "SQL_SUCCESS = " << SQL_SUCCESS << endl;
       ndbout << "SQL_SUCCESS_WITH_INFO = " << SQL_SUCCESS_WITH_INFO << endl;}

       ndbout << endl;

     ndbout << "-------------------------------------------------" << endl;
     ndbout << "Error diagnostics:" << endl;

     if (GDS_retcode != SQL_SUCCESS || GDS_retcode != SQL_SUCCESS_WITH_INFO){
       ndbout << "GDS_retcode = " << GDS_retcode << endl;
       ndbout << "SQL_SUCCESS = " << SQL_SUCCESS << endl;
       ndbout << "SQL_SUCCESS_WITH_INFO = " << SQL_SUCCESS_WITH_INFO << endl;

     GDS_RETURN = SQLGetDiagRec(SQL_HANDLE_DBC, 
				GDS_hdbc, 
				GDS_i, 
				GDS_Sqlstate, 
				&GDS_NativeError, 
				GDS_Msg, 
				sizeof(GDS_Msg), 
				&GDS_MsgLen);

     if (GDS_RETURN == SQL_NO_DATA){
       ndbout << "GDS_SQLSTATES = SQL_NO_DATA" << endl;
       ndbout << "the HandleType is:" << SQL_HANDLE_DBC << endl;
       ndbout << "the Handle is :" << (long)GDS_hdbc << endl;
       ndbout << "the GDS_Msg is :" << (char *)GDS_Msg << endl;
       ndbout << "the sqlstate is:" << (char *)GDS_Sqlstate << endl;}

     else if (GDS_RETURN == SQL_SUCCESS){
       ndbout << "GDS_SQLSTATES = SQL_SUCCESS" << endl;
       ndbout << "the HandleType is:" << SQL_HANDLE_DBC << endl;
       ndbout << "the Handle is :" << (long)GDS_hdbc << endl;
       ndbout << "the GDS_Msg is :" << (char *)GDS_Msg << endl;
       ndbout << "the sqlstate is:" << (char *)GDS_Sqlstate << endl;}

     else if (GDS_RETURN == SQL_SUCCESS_WITH_INFO){
       ndbout << "GDS_SQLSTATES = SQL_SUCCESS_WITH_INFO" << endl;
       ndbout << "the HandleType is:" << SQL_HANDLE_DBC << endl;
       ndbout << "the Handle is :" << (long)GDS_hdbc << endl;
       ndbout << "the GDS_Msg is :" << (char *)GDS_Msg << endl;
       ndbout << "the sqlstate is:" << (char *)GDS_Sqlstate << endl;}

     else if (GDS_RETURN == SQL_INVALID_HANDLE){
       ndbout << "GDS_SQLSTATES = SQL_INVALID_HANDLE" << endl;
       ndbout << "the HandleType is:" << SQL_HANDLE_DBC << endl;
       ndbout << "the Handle is :" << (long)GDS_hdbc << endl;
       ndbout << "the GDS_Msg is :" << (char *)GDS_Msg << endl;
       ndbout << "the sqlstate is:" << (char *)GDS_Sqlstate << endl;}

     else{
       ndbout << "GDS_RETURN = SQL_ERROR" << endl;
       ndbout << "the HandleType is:" << SQL_HANDLE_DBC << endl;
       ndbout << "the Handle is :" << (long)GDS_hdbc << endl;
       ndbout << "the GDS_Msg is :" << (char *)GDS_Msg << endl;
       ndbout << "the sqlstate is:" << (char *)GDS_Sqlstate << endl; 
     }
                                                                            }
  ndbout << "-------------------------------------------------" << endl;

  //******************
  //** Free Handles **
  //******************
  SQLDisconnect(GDS_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, GDS_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, GDS_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, GDS_henv);
  return NDBT_OK;
 }

