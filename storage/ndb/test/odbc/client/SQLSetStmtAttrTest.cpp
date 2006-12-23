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
       
void SetStmtAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle);

int SQLSetStmtAttrTest()
{
  /* SQL/CLI attributes */
  /* SQL_ATTR_APP_PARAM_DESC */
  char PtrValue1[13] = {'SQL_NULL_DESC'};
  for (i=0; i < 1; i++) {
  retcode = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, (void*)PtrValue1[i], sizeof(PtrValue1[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);} 

  /* SQL_ATTR_APP_ROW_DESC */
  char PtrValue2[1] = {'SQL_NULL_DESC'}; /* ? */
  for (i=0; i < 2; i++) {
  retcode = SQLSetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, (void*)PtrValue2[i], sizeof(PtrValue2[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);} 

  /* SQL_ATTR_CURSOR_SCROLLABLE */
  char PtrValue3[2] = {'SQL_NONSCROLLABLE', 'SQL_SCROLLABLE'}; /* ? */
  for (i=0; i < 2; i++) {
  retcode = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE, (void*)PtrValue3[i], sizeof(PtrValue3[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);} 

  /* SQL_ATTR_CURSOR_SENSITIVITY  */
  char PtrValue4[3] = {'SQL_UNSPECIFIED', 'SQL_INSENSITIVE', 'SQL_SENSITIVE'}; /* ? */
  for (i=0; i < 3; i++) {
  retcode = SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_SENSITIVITY, (void*)PtrValue4[i], sizeof(PtrValue4[i]));

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    SetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);} 

  return 0;

 }


void SetStmtAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle)
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



