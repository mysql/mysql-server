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
#include <NdbTest.hpp>
#include <NdbMain.h>

SQLRETURN retcode, SQLSTATEs;
SQLHENV     henv;
SQLHDBC     hdbc;

void NDBT_Connect_DisplayError(SQLSMALLINT HandleType, SQLHSTMT InputHandle);

int NDBT_SQLConnect()
{

      /*****************************SQLConnect AutoTest*****************************/

     // Allocate An Environment Handle
        SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

     // This part does not include in sqlcli.h, it is only in ODBC
     // Set the ODBC application Version to 3.x
     // SQLSetEnvattr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTERGER);

     // Allocate A Connection Handle
        SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

     // Connect to NDB
        retcode = SQLConnect(hdbc, 
			     (SQLCHAR*) "Sales", 
			     5, 
			     (SQLCHAR*) "JohnS", 
			     5, 
			     (SQLCHAR*) "Sesame", 
			     6);

        if (retcode == SQL_INVALID_HANDLE)
            ndbout << "Handle Type is SQL_HANDLE_DBC, but string SQL_INVALID_HANDLE still appeared. Please check programm" << endl;
        else 
            { if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
              NDBT_Connect_DisplayError(SQL_HANDLE_DBC, hdbc);}

     // Free the Connection Handle
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);

     // Free the Environment Handle
        SQLFreeHandle(SQL_HANDLE_ENV, henv);

	return 0;
}


void NDBT_Connect_DisplayError(SQLSMALLINT HandleType, SQLHDBC InputHandle)
{
     SQLRETURN  Sqlstate;
     int i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATA)                   {

     ndbout << "the HandleType is:" << HandleType << endl;
     ndbout << "the InputHandle is :" << InputHandle << endl;
     ndbout << "the output state is:" << (char *)Sqlstate << endl; 

     i ++;
                                                         }

}
