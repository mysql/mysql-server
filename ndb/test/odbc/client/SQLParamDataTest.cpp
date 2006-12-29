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
#include <sqlcli.h>
#include <stdio.h>

using namespace std;

#define NAME_LEN 50
#define PHONE_LEN 10
#define SALES_PERSON_LEN 10
#define STATUS_LEN 6

#define NAME_LEN 50
#define PHONE_LEN 50

SQLCHAR      szName[NAME_LEN], szPhone[PHONE_LEN];
SQLINTEGER   sCustID, cbName, cbAge, cbBirthday;

SQLHSTMT     hstmt;
SQLHENV      henv;

SQLCHAR      szSalesPerson[SALES_PERSON_LEN];

SQLCHAR      szStatus[STATUS_LEN], Sqlstate[5], Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
SQLINTEGER   cbOrderID = 0, cbCustID = 0, cbOpenDate = 0, cbSalesPerson = SQL_NTS, cbStatus = SQL_NTS, NativeError;
SQLRETURN    retcode, SQLSTATEs;

SQLSMALLINT     sOrderID;

SQLSMALLINT  i, MsgLen;

SQLCHAR      ColumnName;
SQLSMALLINT  TargetValuePtr;
SQLINTEGER   StrLen_or_IndPtr;
SQLPOINTER   ValuePtrPtr;

void DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

int SQLParamDataTest()
{

  /* hstmt */
  // We can create the table ORDERS and insert rows into ORDERS    
  // NDB by program TestDirectSQL. In this test program, We only have three rows in table ORDERS

/* Prepare the SQL statement with parameter markers. */
retcode = SQLPrepare(hstmt, (SQLCHAR *)"SELECT ORDERID, CUSTID, OPENDATE, SALESPERSON, STATUS FROM ORDERS", SQL_NTS);

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
   retcode = SQLExecute(hstmt);

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
   retcode = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_INTEGER, 16, 0, &sOrderID, 16, &cbOrderID);

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

while (retcode == SQL_NEED_DATA) {
      retcode = SQLParamData(hstmt, &ValuePtrPtr);
      if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO){
         DisplayError(SQL_HANDLE_STMT, hstmt);
                                                                     }
                                                                 }


                                  }
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



