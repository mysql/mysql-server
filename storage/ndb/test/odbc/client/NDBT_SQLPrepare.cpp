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

#include <NdbOut.hpp>
#include <sqlext.h>
#include <stdio.h>

#include <NdbTest.hpp>
#include <NdbMain.h>

using namespace std;

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLRETURN   SQLPrepare_retcode, SQLAllocHandl_retcode, SQLSTATEs;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;
SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
       
void NDBT_SQLPrepare_DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

  // Execute a statement to retrieve rows from the Customers table. We can 
  // create the table and inside rows into NDB by invoking SQLExecute() or 
  // another program called TestDirectSQL

int NDBT_SQLPrepare()
{
  // Allocate An Environment Handle
     SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

  // Allocate A Connection Handle
     SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

  // Allocate A Connection Handle
     SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

  // Connecte to database
     SQLConnect(hdbc, (SQLCHAR*) "Sales", 5, (SQLCHAR*) "JohnS", 5, (SQLCHAR*) "Sesame", 6);

  // Allocate A Statement Handle
     SQLAllocHandl_retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

  /*  We can change the SQL statement in the SQLPrepare() function according to the requirement of Johnny.  */
  /*  The order of the SQL statement could be CREATE, INSERT, UPDATE, SELECT, DELETE or another special SQL */

  if (SQLAllocHandl_retcode == SQL_SUCCESS){
  SQLPrepare_retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", 56);

  if (SQLPrepare_retcode == SQL_INVALID_HANDLE)
ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE and SQL_SUCCESS still appeared. Please check programm" << endl;

  if (SQLPrepare_retcode == SQL_ERROR || SQLPrepare_retcode == SQL_SUCCESS_WITH_INFO)
  NDBT_SQLPrepare_DisplayError(SQL_HANDLE_STMT, hstmt);

  SQLExecute(hstmt);

  SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
                                            }

  // Disconnect from the database before free Connection Handle and Environment Handle
     SQLDisconnect(hdbc);

  // Free the Connection Handle
     SQLFreeHandle(SQL_HANDLE_DBC, hdbc);

  // Free the Environment Handle
     SQLFreeHandle(SQL_HANDLE_ENV, henv);

  return 0;

 }


void NDBT_SQLPrepare_DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle)
{
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 

     i ++;
                                                         }

}



