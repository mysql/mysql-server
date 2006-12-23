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
 * @file SQLExecuteTest.cpp
 */

#include <common.hpp>
#define ESQL_MAXIMUM_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     Ehdbc;
SQLHSTMT    Ehstmt;
SQLHENV     Ehenv;
SQLHDESC    Ehdesc;
       
void Execute_DisplayError(SQLSMALLINT EHandleType, 
			  SQLHSTMT EInputHandle);

/** 
 * Test to execute a SQL statement in a data result set
 *
 * Tests:
 * -# Test1 There is no executed statement 
 * @return Zero, if test succeeded
 */
int SQLExecuteTest()
{

  SQLRETURN   retcode;
  /* hstmt */
  retcode = SQLExecute(Ehstmt);

  if (retcode == SQL_INVALID_HANDLE)
    ndbout << "Handle Type is SQL_HANDLE_STMT, but SQL_INVALID_HANDLE" << endl;
    ndbout << "still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
  Execute_DisplayError(SQL_HANDLE_STMT, Ehstmt);

  /* henv */
  retcode = SQLExecute(Ehenv);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS)
    ndbout << "Handle Type is SQL_HANDLE_ENV, but SQL_SUCCESS_WITH_INFO" 
	   << "still appeared. Please check programm" << endl;
  //  Execute_DisplayError(SQL_HANDLE_ENV, Ehenv);

  /* hdbc */
  retcode = SQLExecute(Ehdbc);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS)
    ndbout << "Handle Type is SQL_HANDLE_DBC, but SQL_SUCCESS_WITH_INFO" 
	   <<"still appeared. Please check programm" << endl;
  //  Execute_DisplayError(SQL_HANDLE_DBC, Ehdbc);

  /* hdesc */
  retcode = SQLExecute(Ehdesc);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS)
    ndbout << "Handle Type is SQL_HANDLE_DESC, but SQL_SUCCESS_WITH_INFO" 
	   << "still appeared. Please check programm" << endl;
  //  Execute_DisplayError(SQL_HANDLE_DESC, Ehdesc);

  return NDBT_OK;

 }


void Execute_DisplayError(SQLSMALLINT EHandleType, 
			  SQLHSTMT EInputHandle)
{
  SQLCHAR Sqlstate[5];

  SQLINTEGER    NativeError;
  SQLSMALLINT   i, MsgLen;
  SQLCHAR   Msg[ESQL_MAXIMUM_MESSAGE_LENGTH];
  SQLRETURN   SQLSTATEs;
  i = 1;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
 
  while ((SQLSTATEs = SQLGetDiagRec(EHandleType, 
				    EInputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)
    {
   
      ndbout << "the HandleType is:" << EHandleType << endl;
      ndbout << "the InputHandle is :" << EInputHandle << endl;
      ndbout << "the Msg is :" << (char *)Msg << endl;
      ndbout << "the output state is:" << (char *)Sqlstate << endl; 
      
      i ++;     
      break;

    }
  ndbout << "-------------------------------------------------" << endl;  
}



