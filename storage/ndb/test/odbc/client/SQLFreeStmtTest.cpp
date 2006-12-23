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
int strange_handle;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;

struct handle_set
{
SQLHDBC     hdbc_varible;
SQLHSTMT    hstmt_varible;
SQLHENV     henv_varible;
SQLHDESC    hdesc_varible;
int         strangehandle;
};
handle_set handlevalue;

void handle_deal_with_HSTMT(SQLSMALLINT HandleType, SQLHSTMT InputHandle);
void handle_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle);
void handle_deal_with_HDESC(SQLSMALLINT HandleType, SQLHDESC InputHandle);
void handle_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle);
        
void DisplayError_HDBC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle);
void DisplayError_HSTMT(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHSTMT InputHandle);
void DisplayError_HENV(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle);
void DisplayError_HDESC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDESC InputHandle);

int SQLFreeStmtTest ()
{
 
handlevalue.hdbc_varible = hdbc;
handlevalue.hstmt_varible = hstmt;
handlevalue.henv_varible = henv;
handlevalue.hdesc_varible = hdesc;
handlevalue.strangehandle = 67;


/* ENV */ 
handle_deal_with_HENV(SQL_HANDLE_ENV, SQL_NULL_HANDLE);
        
/* DBC */
handle_deal_with_HDBC(SQL_HANDLE_DBC, SQL_NULL_HANDLE);
         
/* STMT */
handle_deal_with_HSTMT(SQL_HANDLE_STMT, SQL_NULL_HANDLE);

/* DESC */
handle_deal_with_HDESC(SQL_HANDLE_DESC, SQL_NULL_HANDLE);
        
return 0;

}        


void handle_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HDBC(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
 }


void handle_deal_with_HSTMT(SQLSMALLINT HandleType, SQLHSTMT InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HSTMT(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
 }

void handle_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     DisplayError_HENV(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
 }

void handle_deal_with_HDESC(SQLSMALLINT HandleType, SQLHDESC InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLFreeHandle(HandleType, InputHandle);

     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     DisplayError_HDESC(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
 }


void DisplayError_HENV(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;  
}


void DisplayError_HDBC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}

void DisplayError_HSTMT(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHSTMT InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}

void DisplayError_HDESC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDESC InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}


