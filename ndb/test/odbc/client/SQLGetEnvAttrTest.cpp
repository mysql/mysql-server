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
#define SQL_MAXIMUM_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLRETURN   retcode, SQLSTATEs;

SQLPOINTER  ValuePtr;
SQLINTEGER GetEnvAttr_StringLengthPtr;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;
SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
       
void GetEnvAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle);

int SQLGetEnvAttrTest()
{
  /* ODBC attributres */

  /*
  //  char PtrValue1[3] = {'SQL_CP_OFF', 'SQL_CP_ONE_DRIVER', 'SQL_CP_ONE_PER_HENV'};
  //  for (i=0; i < 3; i++) {
  retcode = SQLGetEnvAttr(henv, SQL_ATTR_CONNECTION_POOLING, ValuePtr, 36, &GetEnvAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_ENV, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetEnvAttr_DisplayError(SQL_HANDLE_ENV, henv); // }


  //  char PtrValue2[2] = {'SQL_CP_STRICT_MATCH', 'SQL_CP_RELAXED_MATCH'};
  // for (i=0; i < 2; i++) {
  retcode = SQLGetEnvAttr(henv, SQL_ATTR_CP_MATCH, ValuePtr, 36, &GetEnvAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_ENV, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetEnvAttr_DisplayError(SQL_HANDLE_ENV, henv); // } 


  //  char PtrValue3[2] = {'SQL_OV_ODBC3', 'SQL_OV_ODBC2'};
  // for (i=0; i < 2; i++) {
  retcode = SQLGetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, ValuePtr, 36, &GetEnvAttr_StringLengthPtr );

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_ENV, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetEnvAttr_DisplayError(SQL_HANDLE_ENV, henv); // } 

  */

  //  char PtrValue4[2] = {'SQL_TRUE', 'SQL_FALSE'};
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetEnvAttr(henv, SQL_ATTR_OUTPUT_NTS, ValuePtr, 36, &GetEnvAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_ENV, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetEnvAttr_DisplayError(SQL_HANDLE_ENV, henv); // }

  return 0;

 }


void GetEnvAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle)
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



