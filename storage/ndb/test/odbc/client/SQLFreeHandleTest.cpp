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

#include "common.h"
#define SQL_MAXIMUM_MESSAGE_LENGTH 200

using namespace std;

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLRETURN   retcode, SQLSTATEs;
int strangehandle;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;


void freehandle_deal_with_HSTMT(SQLSMALLINT HandleType, SQLHSTMT InputHandle);
void freehandle_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle);
void freehandle_deal_with_HDESC(SQLSMALLINT HandleType, SQLHDESC InputHandle);
void freehandle_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle);
        
void freehandle_DisplayError_HDBC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle);
void freehandle_DisplayError_HSTMT(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHSTMT InputHandle);
void freehandle_DisplayError_HENV(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle);
void freehandle_DisplayError_HDESC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDESC InputHandle);

int SQLFreeHandleTest ()
{

strangehandle = 67;

/* ENV */
ndbout << "Environment Handle" << endl;
SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
freehandle_deal_with_HENV(SQL_HANDLE_ENV, henv);
        
/* DBC */
ndbout << "Connection Handle" << endl;
SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
freehandle_deal_with_HDBC(SQL_HANDLE_DBC, hdbc);
         
/* STMT */
ndbout << "Statement Handle" << endl;
SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
freehandle_deal_with_HSTMT(SQL_HANDLE_STMT, hstmt);

/* DESC */
ndbout << "Descriptor Handle" << endl;
SQLAllocHandle(SQL_HANDLE_DESC, hdbc, &hdesc);
freehandle_deal_with_HDESC(SQL_HANDLE_DESC, hdesc);
 
return 0;

}        


void freehandle_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     ndbout << "the HandleType is : " << HandleType << endl;
     ndbout << "the InputHandle is SQLHDBC:" << InputHandle << endl;
     ndbout << "retcode = " << retcode << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HDBC_free(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
     */
 }


void freehandle_deal_with_HSTMT(SQLSMALLINT HandleType, SQLHSTMT InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     ndbout << "the HandleType is : " << HandleType << endl;
     ndbout << "the InputHandle is SQLHSTMT:" << InputHandle << endl;
     ndbout << "retcode = " << retcode << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HSTMT_free(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
     */
 }

void freehandle_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     ndbout << "the HandleType is : " << HandleType << endl;
     ndbout << "the InputHandle is SQLHENV:" << InputHandle << endl;
     ndbout << "retcode = " << retcode << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     DisplayError_HENV_free(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
     */
 }

void freehandle_deal_with_HDESC(SQLSMALLINT HandleType, SQLHDESC InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     ndbout << "the HandleType is : " << HandleType << endl;
     ndbout << "the InputHandle is SQLHDESC:" << InputHandle << endl;
     ndbout << "retcode = " << retcode << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     DisplayError_HDESC_free(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
     */
 }


void freehandle_DisplayError_HENV(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;  
}

void freehandle_DisplayError_HDBC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}

void freehandle_DisplayError_HSTMT(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHSTMT InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}

void freehandle_DisplayError_HDESC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDESC InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}
