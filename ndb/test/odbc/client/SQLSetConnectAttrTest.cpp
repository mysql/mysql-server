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
//SQLINTEGER StringLength;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;
SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
       
void SetConnectAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle);

int SQLSetConnectAttrTest ()
{
  /* SQL/CLI attributes */
  char PtrValue1[2] = {'SQL_TRUE', 'SQL_FALSE'};
  for (i=0; i < 2; i++) {
  retcode = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTO_IPD, (void*)PtrValue1[i], sizeof(PtrValue1[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetConnectAttr_DisplayError(SQL_HANDLE_DBC, hdbc);} 

  /* ODBC attributes */
  /*
  char PtrValue1[3] = {'SQL_MODE_READ_ONLY', 'SQL_MODE_READ_WRITE'};
  for (i=0; i < 3; i++) {
  retcode = SQLSetConnectAttr(hdbc, SQL_ATTR_ACCESS_MODE, (void*)PtrValue1[i], sizeof(PtrValue1[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetConnectAttr_DisplayError(SQL_HANDLE_DBC, hdbc);}


  char PtrValue2[2] = {'SQL_ASYNC_ENABLE_OFF', 'SQL_ASYNC_ENABLE_ON'};
  for (i=0; i < 2; i++) {
  retcode = SQLSetConnectAttr(hdbc, SQL_ATTR_ASYNC_ENABLE, (void*)PtrValue2[i], sizeof(PtrValue2[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetConnectAttr_DisplayError(SQL_HANDLE_DBC, hdbc);} 
  

  char PtrValue4[2] = {'SQL_AUTOCOMMIT_OFF', 'SQL_AUTOCOMMIT_ON'};
  for (i=0; i < 2; i++) {
  retcode = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (void*)PtrValue4[i], sizeof(PtrValue4[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetConnectAttr_DisplayError(SQL_HANDLE_DBC, hdbc);}

  char PtrValue5[2] = {'SQL_CD_TRUE', 'SQL_CD_FALSE'};
  for (i=0; i < 2; i++) {
  retcode = SQLSetConnectAttr(hdbc, SQL_ATTR_CONNECTION_DEAD, (void*)PtrValue4[i], sizeof(PtrValue5[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetConnectAttr_DisplayError(SQL_HANDLE_DBC, hdbc);}


  char PtrValue5[2] = {'SQL_CD_TRUE', 'SQL_CD_FALSE'};
  for (i=0; i < 2; i++) {
  retcode = SQLSetConnectAttr(hdbc, SQL_ATTR_CONNECTION_TIMEOUT, (void*)PtrValue4[i], sizeof(PtrValue5[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetConnectAttr_DisplayError(SQL_HANDLE_DBC, hdbc);}
  
  */
  
  return 0;

 }


void SetConnectAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle)
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



