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

#define ROWS 100
#define DESC_LEN 50


// Template for a row
typedef struct {
   SQLINTEGER   sPartID;
   SQLINTEGER   cbPartID;
   SQLUCHAR     szDescription[DESC_LENGTH];
   SQLINTEGER   cbDescription;
   REAL         sPrice;
   SQLINTEGER   cbPrice;
} PartsSource;

PartsSource    rget[ROWS];          // rowset buffer
SQLUSMALLINT   sts_ptr[ROWS];       // status pointer
SQLHSTMT       hstmt0, hstmt1;
SQLHDESC       hArd0, hIrd0, hApd1, hIpd1;


SQLHSTMT    hstmt;
SQLHDESC    hdesc;

SQLSMALLINT RecNumber;
SQLCHAR     szSalesPerson[SALES_PERSON_LEN];

SQLCHAR     Sqlstate[5], Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
SQLINTEGER  NativeError;
SQLRETURN retcode, SQLSTATEs;

SQLINTEGER   ValuePtr1;
SQLCHAR      ValuePtr2;
SQLSMALLINT  ValuePtr3;


SQLINTEGER  StringLengthPtr;

SQLSMALLINT i, MsgLen;

void DisplayError(SQLSMALLINT HandleType, SQLHDESC InputHandle);

int SQLCopyDescTest ()
{


  // We can create the table and insert rows in NDB by program TestDirectSQL. 
  // In this test program(SQLGetCopyRecTest),we only have three rows in table ORDERS


// ARD and IRD of hstmt0
SQLGetStmtAttr(hstmt0, SQL_ATTR_APP_ROW_DESC, &hArd0, 0, NULL);
SQLGetStmtAttr(hstmt0, SQL_ATTR_IMP_ROW_DESC, &hIrd0, 0, NULL);

// APD and IPD of hstmt1
SQLGetStmtAttr(hstmt1, SQL_ATTR_APP_PARAM_DESC, &hApd1, 0, NULL);
SQLGetStmtAttr(hstmt1, SQL_ATTR_IMP_PARAM_DESC, &hIpd1, 0, NULL);

// Use row-wise binding on hstmt0 to fetch rows
SQLSetStmtAttr(hstmt0, SQL_ATTR_ROW_BIND_TYPE, (SQLPOINTER) sizeof(PartsSource), 0);

// Set rowset size for hstmt0
SQLSetStmtAttr(hstmt0, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER) ROWS, 0);

// Execute a select statement
SQLExecDirect(hstmt0, "SELECT PARTID, DESCRIPTION, PRICE FROM PARTS ORDER BY 3, 1, 2"",
               SQL_NTS);

// Bind
SQLBindCol(hstmt0, 1, SQL_C_SLONG, rget[0].sPartID, 0, 
   &rget[0].cbPartID);
SQLBindCol(hstmt0, 2, SQL_C_CHAR, &rget[0].szDescription, DESC_LEN, 
   &rget[0].cbDescription);
SQLBindCol(hstmt0, 3, SQL_C_FLOAT, rget[0].sPrice, 
   0, &rget[0].cbPrice);

  // Perform parameter bindings on hstmt1. 
  /* If SourceDeschandle does not identify an allocated CLI descriptor area */
  retcode1 = SQLCopyDesc(hArd0, hApd1);
  retcode2 = SQLCopyDesc(hIrd0, hIpd1);

if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
   DisplayError(SQL_HANDLE_DESC, hdesc);


  /* If TargetDeschandle does not identify an allocated CLI descriptor area */
  retcode = SQLCopyDesc(hdesc, );
  if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
      DisplayError(SQL_HANDLE_DESC, hdesc);


  return 0;

 }


void DisplayError(SQLSMALLINT HandleType, SQLHDESC InputHandle)
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



