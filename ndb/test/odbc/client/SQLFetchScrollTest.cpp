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
#include <stdio.h>
#include <sqlext.h>

using namespace std;

#define NAME_LEN 50
#define PHONE_LEN 10
#define SALES_PERSON_LEN 10
#define STATUS_LEN 6
#define SQL_MAXIMUM_MESSAGE_LENGTH 200

SQLHSTMT    hstmt;
SQLSMALLINT RecNumber;
SQLCHAR     szSalesPerson[SALES_PERSON_LEN];

SQLCHAR     Sqlstate[5], Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
SQLINTEGER  NativeError;
SQLRETURN retcode, SQLSTATEs;

SQLINTEGER   ValuePtr1;
SQLCHAR      ValuePtr2;
SQLSMALLINT  ValuePtr3;
SQLSMALLINT i, MsgLen;

void SFCT_DisplayError(SQLSMALLINT HandleType, SQLHDESC InputHandle);

int SQLFetchScrollTest ()
{

  // FetchScroll a statement to retrieve rows from the Customers table. We can 
  // create the table and insert rows in NDB by program TestDirectSQL 

  /* There is no executed statement associated with the allocated SQL-statement identified by StatementHandle */
retcode = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 1);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   SFCT_DisplayError(SQL_HANDLE_DESC, hstmt);

  /* FetchOrientation is not one of the code values in Table24 */
retcode = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 8);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   SFCT_DisplayError(SQL_HANDLE_DESC, hstmt);

  return 0;

 }


void SFCT_DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle)
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



