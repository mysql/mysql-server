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
SQLHENV     henv;
SQLRETURN   retcode, SQLSTATEs;

SQLCHAR Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;

void sqlallocenv_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle);
void sqlallocenv_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle);

void DisplayError_HDBC_free(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle);
void DisplayError_HENV_free(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle);


int SQLAllocEnvTest()
{

/* Environment test for SQLAllocEnv() */
ndbout << "Environment test for SQLAllocEnv()" << endl;
//SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

sqlallocenv_deal_with_HENV(SQL_HANDLE_ENV, henv);
        
//SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
//SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
sqlallocenv_deal_with_HDBC(SQL_HANDLE_DBC, hdbc);
         
return 0;

}        

void sqlallocenv_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle) 
{
     SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
     retcode = SQLAllocHandle(HandleType, henv, &InputHandle);

     ndbout << "the HandleType is : " << HandleType << endl;
     ndbout << "the InputHandle is SQLHDBC:" << InputHandle << endl;
     ndbout << "retcode = " << retcode << endl;

     /* ***
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HDBC_free(Sqlstate, HandleType, InputHandle);

     i ++;
                                                         }
                                                                   }
     *** */
}

void sqlallocenv_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle) 
{
     retcode = SQLAllocEnv(&InputHandle);

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


void DisplayError_HENV_free(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;  
}

void DisplayError_HDBC_free(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle)
{
     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl;
}

