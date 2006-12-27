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

SQLHSTMT    hstmt;

SQLSMALLINT sOrderID;
SQLSMALLINT sCustID;
DATE_STRUCT dsOpenDate;
SQLCHAR     szSalesPerson[SALES_PERSON_LEN];

SQLCHAR     szStatus[STATUS_LEN],Sqlstate[5], Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
SQLINTEGER  cbOrderID = 0, cbCustID = 0, cbOpenDate = 0, cbSalesPerson = SQL_NTS, cbStatus = SQL_NTS, NativeError;
SQLRETURN retcode, SQLSTATEs;

SQLSMALLINT i, MsgLen;

void DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

int SQLBindParameterTest ()
{

  /* hstmt */
  //** Execute a statement to retrieve rows from the Customers table. 
  //** We can create the table and inside rows in 
  //** NDB by another program TestDirectSQL. 
  //** In this test program(SQLBindParameterTest),we only have three rows in
  //** table ORDERS

  //************************
  //** Define a statement **
  //************************
  strcpy( (char *) SQLStmt, 
	  "INSERT INTO Customers (CUSTID, Name, Address, Phone) VALUES (2, 'paul, 'Alzato', '468719989');

/* Prepare the SQL statement with parameter markers. */
retcode = SQLPrepare(hstmt, SQLStmt, SQL_NTS);

/* Specify data types and buffers for OrderID, CustID, OpenDate, SalesPerson, */
/* Status parameter data. */

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

  /* ParameterNumber is less than 1 */
retcode = SQLBindParameter(hstmt, 
			   0, 
			   SQL_PARAM_INPUT, 
			   SQL_C_SSHORT, 
			   SQL_INTEGER, 
			   0, 
			   0, 
			   &sOrderID, 
			   0, 
			   &cbOrderID);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);

  /* InputOutputMode is not one of the code values in Table 11 */
retcode = SQLBindParameter(hstmt, 
			   1, 
			   3, 
			   SQL_C_SSHORT, 
			   SQL_INTEGER, 
			   0, 
			   0, 
			   &sOrderID, 
			   0, 
			   &cbOrderID);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);

  /* ParameterType is not one of the code values in Table 37 */
retcode = SQLBindParameter(hstmt, 
			   1, 
			   3, 
			   SQL_C_SSHORT, 
			   114, 
			   0, 
			   0, 
			   &sOrderID, 
			   0, 
			   &cbOrderID);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);

SQLBindParameter(hstmt, 
		 1, 
		 SQL_PARAM_INPUT, 
		 SQL_C_SSHORT, 
		 SQL_INTEGER, 
		 0, 
		 0, 
		 &sOrderID, 
		 0, 
		 &cbOrderID);
if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_STMT, hstmt);


SQLBindParameter(hstmt, 
		 2, 
		 SQL_PARAM_INPUT, 
		 SQL_C_SSHORT, 
		 SQL_INTEGER, 
		 0, 
		 0, 
		 &sCustID, 
		 0, 
		 &cbCustID);

SQLBindParameter(hstmt, 
		 3, 
		 SQL_PARAM_INPUT, 
		 SQL_C_TYPE_DATE, 
		 SQL_TYPE_DATE, 
		 0, 
		 0, 
		 &dsOpenDate, 
		 0, 
		 &cbOpenDate); 

SQLBindParameter(hstmt, 
		 4, 
		 SQL_PARAM_INPUT, 
		 SQL_C_CHAR, 
		 SQL_CHAR, 
		 SALES_PERSON_LEN, 
		 0, 
		 szSalesPerson, 
		 0, 
		 &cbSalesPerson); 

SQLBindParameter(hstmt, 
		 5, 
		 SQL_PARAM_INPUT, 
		 SQL_C_CHAR, 
		 SQL_CHAR, 
		 STATUS_LEN, 
		 0, 
		 szStatus, 
		 0, 
		 &cbStatus);

/*

/* Specify first row of parameter data. */
sOrderID = 1001;   
sCustID = 298;   
dsOpenDate.year = 1996;
dsOpenDate.month = 3;
dsOpenDate.day = 8;
strcpy(szSalesPerson, "Johnson");
strcpy(szStatus, "Closed");

/* Execute statement with first row. */
retcode = SQLExecute(hstmt);   

/* Specify second row of parameter data. */
sOrderID = 1002;   
sCustID = 501;         
dsOpenDate.year = 1996;
dsOpenDate.month = 3;
dsOpenDate.day = 9; 
strcpy(szSalesPerson, "Bailey");
strcpy(szStatus, "Open");

/* Execute statement with second row. */
retcode = SQLExecute(hstmt);      

*/

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



