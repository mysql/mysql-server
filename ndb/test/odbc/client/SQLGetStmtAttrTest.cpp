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
SQLINTEGER GetStmtAttr_StringLengthPtr;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;
SQLCHAR   Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
       
void GetStmtAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle);

int SQLGetStmtAttrTest()
{
  /* SQL/CLI attributes */
  /*  SQL_ATTR_APP_PARAM_DESC */
  //  char PtrValue1[1] = {'SQL_NULL_DESC'};
  //  for (i=0; i < 1; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_PARAM_DESC, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//} 

  /* SQL_ATTR_APP_ROW_DESC */
  //  char PtrValue2[1] = {'SQL_NULL_DESC'}; /* ? */
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_APP_ROW_DESC, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//} 

  /*  SQL_ATTR_CURSOR_SCROLLABLE  */
  //  char PtrValue3[2] = {'SQL_NONSCROLLABLE', 'SQL_SCROLLABLE'}; /* ? */
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//} 

  /*  SQL_ATTR_CURSOR_SENSITIVITY  */
  //  char PtrValue4[3] = {'SQL_UNSPECIFIED', 'SQL_INSENSITIVE', 'SQL_SENSITIVE'}; /* ? */
  //  for (i=0; i < 3; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_SENSITIVITY, ValuePtr, 36,  &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//} 

  /* SQL_ATTR_METADATA_ID */
  //  char PtrValue5[2] = {'SQL_TRUE', 'SQL_FALSE'}; /* ? */
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//} 
  
  /* SQL_ATTR_IMP_ROW_DESC */
  //  char PtrValue6[2] = {'TRUE', 'FALSE'}; /* ? */
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_ROW_DESC, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//}


  /* SQL_ATTR_IMP_PARAM_DESC */
  //  char PtrValue6[2] = {'TRUE', 'FALSE'}; /* ? */
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_IMP_PARAM_DESC, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//}


  /* SQL_ATTR_METADATA_ID */
  // char PtrValue6[2] = {'TRUE', 'FALSE'}; /* ? */
  //  for (i=0; i < 2; i++) {
  retcode = SQLGetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, ValuePtr, 36, &GetStmtAttr_StringLengthPtr);

  if (retcode == SQL_INVALID_HANDLE)
  ndbout << "Handle Type is SQL_HANDLE_STMT, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    GetStmtAttr_DisplayError(SQL_HANDLE_STMT, hstmt);//}

  return 0;


 }


void GetStmtAttr_DisplayError(SQLSMALLINT HandleType, SQLHENV InputHandle)
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



