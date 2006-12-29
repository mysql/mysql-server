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

#define SQL_MAXIMUM_MESSAGE_LENGTH 200

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLINTEGER  strangehandle;
SQLRETURN   retcode, retcodeprepare, SQLSTATEs;
SQLCHAR Sqlstate[5];

SQLINTEGER  NativeError;
SQLSMALLINT i, MsgLen;
SQLSMALLINT Not_In_Table13;

SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];

       
void SQLEndTran_DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

int SQLEndTranTest()
{

  strangehandle = 67;
  /* hstmt */
  // Execute a statement to retrieve rows from the Customers table. We can create the table and 
  // inside rows into NDB by program TestDirectSQL 
  // retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", 56);

  retcodeprepare = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcodeprepare == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
  retcode = SQLExecute(hstmt);
  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {

  /*  HandleType is not in Table 13 */
    Not_In_Table13 = 67;
    SQLSTATEs = SQLEndTran(Not_In_Table13, (void*)strangehandle , SQL_COMMIT); 
    if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
       i = 1;
       while ((SQLSTATEs = SQLGetDiagRec(67, 67, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     ndbout << "the HandleType is:67" << endl;
     ndbout << "the InputHandle is :67" << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 

     i ++;
                                                         }
 
                                                                     }

  /*  HandleType is STATEMENT HANDLE, if the value of Handle does not identity an allocated SQL_statement */
    SQLSTATEs = SQLEndTran(SQL_HANDLE_STMT, hdbc, SQL_COMMIT); 
    if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
        SQLEndTran_DisplayError(SQL_HANDLE_STMT, hstmt);

  /* The value of CompletionType is not in Table 14 */
    SQLSTATEs = SQLEndTran(SQL_HANDLE_STMT, hstmt, 8888); 
    if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
        SQLEndTran_DisplayError(SQL_HANDLE_STMT, hstmt);

                                                                   }

                                                                   }
  return 0;

 }


void SQLEndTran_DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle)
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



