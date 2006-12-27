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

#include <common.h>

using namespace std;

#define NAME_LEN 50
#define PHONE_LEN 10
#define SALES_PERSON_LEN 10
#define STATUS_LEN 6
#define SQL_MAXIMUM_MESSAGE_LENGTH 200

SQLHSTMT    hstmt;
SQLHDESC    hdesc;

SQLSMALLINT RecNumber;
SQLCHAR     szSalesPerson[SALES_PERSON_LEN];

SQLCHAR     Sqlstate[5], Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
SQLINTEGER  NativeError;
SQLRETURN   retcode, SQLSTATEs;

SQLCHAR     Name;
SQLINTEGER  LengthPtr;
SQLSMALLINT SGDR_StringLengthPtr, TypePtr, SubTypePtr, PrecisionPtr, ScalePtr, NullablePtr;

SQLSMALLINT i, MsgLen;

void SQLGetDescRec_DisplayError(SQLSMALLINT HandleType, SQLHDESC InputHandle);

int SQLGetDescRecTest()
{

  /* hstmt */
  // SQLPrepare a statement to select rows from the ORDERS Table. We can create the table and inside rows in 
  // NDB by another program TestDirectSQL. In this test program(SQLGetDescRecTest),we only have three rows in
  // table ORDERS

/* Prepare the SQL statement with parameter markers. */
retcode = SQLPrepare(hstmt, (SQLCHAR *)"SELECT ORDERID, CUSTID, OPENDATE, SALESPERSON, STATUS FROM ORDERS)", SQL_NTS);

/* SELECT OrderID, CustID, OpenDate, SalesPerson from Table ORDERS */

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

  /* RecoderNumber is less than 1 */
retcode = SQLGetDescRec(hdesc, -1, &Name, sizeof(Name), &SGDR_StringLengthPtr, &TypePtr, &SubTypePtr, &LengthPtr, &PrecisionPtr, &ScalePtr, &NullablePtr);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   SQLGetDescRec_DisplayError(SQL_HANDLE_DESC, hdesc);

  /* RecoderNumber is greater than N, N be the value of the COUNT field of D, D be the allocated CLI */
  /* descriptor area identified by DescriptorHandle */
retcode = SQLGetDescRec(hdesc, 4, &Name, sizeof(Name), &SGDR_StringLengthPtr, &TypePtr, &SubTypePtr, &LengthPtr, &PrecisionPtr, &ScalePtr, &NullablePtr);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   SQLGetDescRec_DisplayError(SQL_HANDLE_DESC, hdesc);

}

  return 0;

 }


void SQLGetDescRec_DisplayError(SQLSMALLINT HandleType, SQLHDESC InputHandle)
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



