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

#if 0

#include <NdbOut.hpp>
#include <sqlcli.h>
#include <stdio.h>

using namespace std;

#define NAME_LEN 50
#define PHONE_LEN 10

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLRETURN   retcode, SQLSTATEs;

SQLCHAR Sqlstate[5];

SQLSMALLINT   i, MsgLen;
SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];

SQLCHAR      szName[NAME_LEN], szPhone[PHONE_LEN];
SQLINTEGER   sCustID, cbName, cbCustID, cbPhone, NativeError;

void DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

int SQLBindColTest ()
{

  /* hstmt */
  // Execute a statement to retrieve rows from the Customers table. We can create the table and inside rows in 
  // NDB by another program TestDirectSQL. In this test program(SQLBindColTest),we only have three rows in
  // table CUSTOMERS

retcode = SQLExecDirect(hstmt, (SQLCHAR*)"SELECT CUSTID, NAME, PHONE FROM CUSTOMERS ORDER BY 2, 1, 3", SQL_NTS);
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

SQLBindCol(hstmt, 0, SQL_C_ULONG, &sCustID, 0, &cbCustID);
while (TRUE) {
retcode = SQLFetch(hstmt);
if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);
      
             }


SQLBindCol(hstmt, 4, SQL_C_ULONG, &sCustID, 0, &cbCustID);
while (TRUE) {
retcode = SQLFetch(hstmt);
if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);
      
             }

/* Bind columns 1, 2, and 3 */
SQLBindCol(hstmt, 1, SQL_C_ULONG, &sCustID, 0, &cbCustID);
SQLBindCol(hstmt, 2, SQL_C_CHAR, szName, NAME_LEN, &cbName);
SQLBindCol(hstmt, 3, SQL_C_CHAR, szPhone, PHONE_LEN, &cbPhone);
/* Fetch and print each row of data. On */
/* an error, display a message and exit. */
while (TRUE) {
retcode = SQLFetch(hstmt);
if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);
      
             }
 }

  return 0;

 }


void DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle)
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

#endif
