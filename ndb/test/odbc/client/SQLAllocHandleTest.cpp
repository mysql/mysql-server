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

using namespace std;

#define SQL_MAXIMUM_MESSAGE_LENGTH 200

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLRETURN   retcode, SQLSTATEs;

SQLCHAR Sqlstate[5];

SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;
long        strangehandle;

void handle_deal_with_HSTMT(SQLSMALLINT HandleType, SQLHSTMT InputHandle);
void handle_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle);
void handle_deal_with_HDESC(SQLSMALLINT HandleType, SQLHDESC InputHandle);
void handle_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle);
//void handle_deal_with_int(SQLSMALLINT HandleType, long InputHandle);
        
void DisplayError_HDBC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDBC InputHandle);
void DisplayError_HSTMT(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHSTMT InputHandle);
void DisplayError_HENV(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHENV InputHandle);
void DisplayError_HDESC(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, SQLHDESC InputHandle);
//void DisplayError_int(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, int InputHandle);

int SQLAllocHandleTest()
{

strangehandle = 6;

/*Allocate environment handle */

//retcode = SQLFreeHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE);

/* ENV */ 
ndbout << endl;
ndbout << "The HandleType: Allocate Environment handle" << endl;
ndbout << endl;

handle_deal_with_HENV(SQL_HANDLE_ENV, SQL_NULL_HANDLE);
          
handle_deal_with_HENV(SQL_HANDLE_ENV, henv);
      
handle_deal_with_HDBC(SQL_HANDLE_ENV, hdbc);
 
handle_deal_with_HSTMT(SQL_HANDLE_ENV, hstmt);
            
handle_deal_with_HDESC(SQL_HANDLE_ENV, hdesc);
     
//handle_deal_with_int(SQL_HANDLE_ENV, strangehandle);
        
/* DBC */
ndbout << endl;
ndbout << "The HandleType: Allocate Connection handle" << endl;
ndbout << endl;

handle_deal_with_HDBC(SQL_HANDLE_DBC, SQL_NULL_HANDLE);
          
handle_deal_with_HENV(SQL_HANDLE_DBC, henv);
                           
handle_deal_with_HDBC(SQL_HANDLE_DBC, hdbc);

handle_deal_with_HSTMT(SQL_HANDLE_DBC, hstmt);
           
handle_deal_with_HDESC(SQL_HANDLE_DBC, hdesc);
     
//handle_deal_with_int(SQL_HANDLE_DBC, strangehandle);
         
/* STMT */
ndbout << endl;
ndbout << "The HandleType: Allocate Statement handle" << endl;
ndbout << endl;

handle_deal_with_HSTMT(SQL_HANDLE_STMT, SQL_NULL_HANDLE);
          
handle_deal_with_HENV(SQL_HANDLE_STMT, henv);
                           
handle_deal_with_HDBC(SQL_HANDLE_STMT, hdbc);

handle_deal_with_HSTMT(SQL_HANDLE_STMT, hstmt);
           
handle_deal_with_HDESC(SQL_HANDLE_STMT, hdesc);
     
//handle_deal_with_int(SQL_HANDLE_STMT, strangehandle);
        

/* DESC */
ndbout << endl;
ndbout << "The HandType: Allocate Descriptor handle" << endl;
ndbout << endl;

handle_deal_with_HDESC(SQL_HANDLE_DESC, SQL_NULL_HANDLE);
          
handle_deal_with_HENV(SQL_HANDLE_DESC, henv);
                         
handle_deal_with_HDBC(SQL_HANDLE_DESC, hdbc);

handle_deal_with_HSTMT(SQL_HANDLE_DESC, hstmt);
           
handle_deal_with_HDESC(SQL_HANDLE_DESC, hdesc);
 
//handle_deal_with_int(SQL_HANDLE_DESC, strangehandle);
    
                              
/* strangehandle */
ndbout << endl;
ndbout << "The HandType: strangehandle" << endl;
ndbout << endl;

//handle_deal_with_int(strangehandle, SQL_NULL_HANDLE);

handle_deal_with_HENV(strangehandle, henv);
                           
handle_deal_with_HDBC(strangehandle, hdbc);

handle_deal_with_HSTMT(strangehandle, hstmt);
           
handle_deal_with_HDESC(strangehandle, hdesc);
     
// handle_deal_with_int(strangehandle, strangehandle);

return 0;

}        

void handle_deal_with_HDBC(SQLSMALLINT HandleType, SQLHDBC InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLAllocHandle(HandleType, InputHandle, &hdbc);

     ndbout << "the HandleType is : " << HandleType << endl;
 
     ndbout << "the InputHandle is SQLHDBC:" << InputHandle << endl;

     ndbout << "return &hdbc: " << (long)&hdbc << endl;
     ndbout << "the retcode state is:" << retcode << endl;
     ndbout << endl;

     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HDBC(Sqlstate, HandleType, InputHandle);
     i ++;
                                                         }
                                                                   }
    */
 }


void handle_deal_with_HSTMT(SQLSMALLINT HandleType, SQLHSTMT InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLAllocHandle(HandleType, InputHandle, &hstmt);

     ndbout << "the HandleType is : " << HandleType << endl;
      
     ndbout << "the InputHandle is SQLHSTMT:" << InputHandle << endl;

     ndbout << "return &hstmt: " << (long)&hstmt << endl;
     ndbout << "the output retcode is:" << retcode << endl;
     ndbout << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {
     DisplayError_HSTMT(Sqlstate, HandleType, InputHandle);
     i ++;
                                                         }
                                                                   }
     */
 }

void handle_deal_with_HENV(SQLSMALLINT HandleType, SQLHENV InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLAllocHandle(HandleType, InputHandle, &henv);

     ndbout << "the HandleType is : " << HandleType << endl;

     ndbout << "the InputHandle is SQLHENV:" << InputHandle << endl;

     ndbout << "return &henv: " << (long)&henv << endl;
     ndbout << "the output retcode is:" << retcode << endl;
     ndbout << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     DisplayError_HENV(Sqlstate, HandleType, InputHandle);
     i ++;
                                                         }
                                                                   }
     */
 }

void handle_deal_with_HDESC(SQLSMALLINT HandleType, SQLHDESC InputHandle) 
{
     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
     retcode = SQLAllocHandle(HandleType, InputHandle, &hdesc);
     
     ndbout << "the HandleType is : " << HandleType << endl;

     ndbout << "the InputHandle is SQLHDESC:" << InputHandle << endl;

     ndbout << "return &hdesc: " << (long)&hdesc << endl;
     ndbout << "the output retcode is:" << retcode << endl;  
     ndbout << endl;
     /*
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     DisplayError_HDESC(Sqlstate, HandleType, InputHandle);
     i ++;
                                                         }
                                                                   }
     */
 }


//void handle_deal_with_int(SQLSMALLINT HandleType, long InputHandle) 
//{
//     SQLCHAR     Msg[SQL_MAXIMUM_MESSAGE_LENGTH];
//     retcode = SQLAllocHandle(HandleType, InputHandle, &InputHandle); 
//
//     ndbout << "the HandleType is: " << HandleType << endl;
//
//     ndbout << "the InputHandle is stranghandle:" << InputHandle << endl;
//
//     ndbout << "return &InputHandle: " << (long)&InputHandle << endl;
//     ndbout << "the output retcode is:" << retcode << endl;  
//     ndbout << endl; 
//     /*
//     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
//     i = 1;
//     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
//             Sqlstate, &NativeError, Msg, sizeof(Msg), 
//             &MsgLen)) != SQL_NO_DATA)                   {
//
//     DisplayError_int(Sqlstate, HandleType, InputHandle);
//
//     i ++;
//                                                         }
//                                                                   }
//     */
// }


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

//void DisplayError_int(SQLCHAR Sqlstate[6], SQLSMALLINT HandleType, int InputHandle)
//{
//     ndbout << "the HandleType is:" << HandleType << endl;
//     ndbout << "the InputHandle is :" << InputHandle << endl;
//     ndbout << "the output state is:" << (char *)Sqlstate << endl;
//}
