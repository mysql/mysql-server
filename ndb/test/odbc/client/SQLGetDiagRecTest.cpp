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

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLINTEGER  strangehandle;
SQLRETURN   retcode, SQLSTATEs;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;
SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
       
void DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

int SQLGetDiagRecTest()
{

  strangehandle = 67;
  /* hstmt */
  // Execute a statement to retrieve rows from the Customers table. We can create the table and inside rows in 
  // NDB by another program TestDirectSQL 
  //  const SQLCHAR *StatementText = "SELECT CustID, Name, Address, Phone FROM Customers";

  //  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", 56);

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(67, 67, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
                                                         }
                                                                  }

 
  /* HandleType indicates ENVIRNMENT HANDLE and Handle does not identify an allocated SQL_environment */

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(SQL_HANDLE_ENV, hdbc, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
                                                         }
                                                                  }


  /*  HandleType indicates CONNECTION HANDLE and Handle does not identify an allocated SQL_connection */

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(SQL_HANDLE_DBC, henv, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
                                                         }
                                                                  }

  /*  HandleType indicates STATEMENT HANDLE and Handle does not identify an allocated SQL_statement */

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(SQL_HANDLE_STMT, hdbc, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
                                                         }
                                                                  }

  /*  HandleType indicates DESCRIPTOR HANDLE and Handle does not identify an allocated SQL_descriptor */

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(SQL_HANDLE_DESC, hdbc, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
                                                         }
                                                                  }


  /*  RecordNumber is less than one */

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     
     where ((SQLSTATEs = SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, -1, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
                                                         }
                                                                  }
  /*  RecordNumber is greater than N */

  retcode = SQLPrepare(hstmt, (SQLCHAR*)"SELECT CustID, Name, Address, Phone FROM Customers", SQL_NTS);

  if (retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_SUCCESS) {
     
     where ((SQLSTATEs = SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, 9999, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     if (SQLSTATEs == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 
                                                                     }
     i ++;
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



