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

#include sqlcli.h;
#include stdio.h;

#define SQL_MAX_MESSAGE_LENGTH 200;

SQLHDBC     hdbc;
SQLHSTMT    hstmt;
SQLHENV     henv;
SQLHDESC    hdesc;
SQLRETURN   retcode, SQLSTATEs;

SQLCHAR       SqlState[6], SQLStmt[100], Msg[SQL_MAX_MESSAGE_LENGTH];
SQLINTEGER    NativeError;
SQLSMALLINT   i, MsgLen;

struct handle_set
{
SQLHDBC     hdbc_varible;
SQLHSTMT    hstmt_varible;
SQLHENV     henv_varible;
SQLHDESC    hdesc_varible;
INTEGER     strangehandle;
}

static int
check(
    SQLSMALLINT HandleType,
    SQLHANDLE inputhandle,
    SQLHANDLE *outputhandle,
    SQLRETURN wantret,
    char *wantSqlstate)
{
    SQLRETURN ret;
    SQLCHAR Sqlstate[20];

    ret = SQLAllocHandle(handletype, inputhandle, outputhandle);
    if (ret != wantret) {
	// report error
	return -1;
    }
    if (ret == SQL_INVALID_HANDLE) {
	// cannot get diag
	return 0;
    }
    // TODO
    ret = SQLGetDiagRec(HandleType, InputHandle, 1, Sqlstate, &NativeError, Msg, sizeof(Msg), &MsgLen);
    if (strcmp(Sqlstate, wantSqlstate) != 0) {
	// report error;
	return -1;
    }
    return 0;
}

int
Test_SQLAllocHandle()
{
    SQLRETURN ret;
    SQLHENV henv;
    SQLDBC dbc;
    int i;

    // env
    check(SQL_HANDLE_ENV, SQL_NULL_HANDLE, 0, SQL_ERROR, "HY009");
    for (i = 0; i < 1000; i++) {
	if (i != SQL_NULL_HANDLE)
	    check(SQL_HANDLE_ENV, i, &henv, SQL_INVALID_HANDLE, 0);
    }
    if (check(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv, SQL_SUCCESS, "00000") < 0)
	return -1;

    // dbc
    check(SQL_HANDLE_DBC, henv, 0, SQL_ERROR, "HY009");
    for (i = 0; i < 1000; i++) {
	if (i != henv)
	    check(SQL_HANDLE_DBC, i, &dbc, SQL_INVALID_HANDLE, 0);
    }
    if (check(SQL_HANDLE_DBC, henv, &dbc, SQL_SUCCESS, "00000") < 0)
	return -1;

    //??
    check(SQL_HANDLE_ENV, dbc, 0, SQL_ERROR, "HY092");

    // TODO
    // stmt

    return 0;
}


handle_set handlevalue;
 
handlevalue.hdbc_varible = hdbc;
handlevalue.hstmt_varible = hstmt;
handlevalue.henv_varible = henv;
handlevalue.hdesc_varible = hdesc;
handlevalue.stranghandle = 67;

      /*Allocate environment handle */
//retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

while (int j = 0; j++; j < 6) {
   if ( j = 0 ) 
      handle_deal_with(SQL_HANDLE_ENV, SQL_NULL_HANDLE, );
          
   else if ( j = 1 ) 
      handle_deal_with(SQL_HANDLE_ENV, handlevalue.henv_varible, );
                           
   else if ( j = 2 ) 
     handle_deal_with(SQL_HANDLE_ENV, handlevalue.hdbc_varible,  );

   else if ( j = 3 ) 
     handle_deal_with(SQL_HANDLE_ENV, handlevalue.hstmt_varible,  );
           
   else if ( j = 4 )  
     handle_deal_with(SQL_HANDLE_ENV, handlevalue.hdesc_varible,  );
     
  else 
     handle_deal_with(SQL_HANDLE_ENV, handlevalue.stranghandle,  );
        
                              }
                           
 while (int j = 0; j++; j < 6) {
   if ( j = 0 ) 
      handle_deal_with(SQL_HANDLE_DBC, SQL_NULL_HANDLE,  );
          
   else if ( j = 1 ) 
      handle_deal_with(SQL_HANDLE_DBC, handlevalue.henv_varible,  );
                           
   else if ( j = 2 ) 
      handle_deal_with(SQL_HANDLE_DBC, handlevalue.hdbc_varible,  );

   else if ( j = 3 ) 
      handle_deal_with(SQL_HANDLE_DBC, handlevalue.hstmt_varible,  );
           
   else if ( j = 4 )  
      handle_deal_with(SQL_HANDLE_DBC, handlevalue.hdesc_varible,  );
     
  else 
      handle_deal_with(SQL_HANDLE_DBC, handlevalue.stranghandle,  );
         
                              }
 

 while (int j = 0; j++; j < 6) {
   if ( j = 0 ) 
      handle_deal_with(SQL_HANDLE_STMT, SQL_NULL_HANDLE,  );
          
   else if ( j = 1 ) 
      handle_deal_with(SQL_HANDLE_STMT, handlevalue.henv_varible,  );
                           
   else if ( j = 2 ) 
      handle_deal_with(SQL_HANDLE_STMT, handlevalue.hdbc_varible,  );

   else if ( j = 3 ) 
      handle_deal_with(SQL_HANDLE_STMT, handlevalue.hstmt_varible,  );
           
   else if ( j = 4 )  
      handle_deal_with(SQL_HANDLE_STMT, handlevalue.hdesc_varible,  );
     
  else 
      handle_deal_with(SQL_HANDLE_STMT, handlevalue.stranghandle,  );
        
                              }



  while (int j = 0; j++; j < 6) {
   if ( j = 0 ) 
      handle_deal_with(SQL_HANDLE_DESC, SQL_NULL_HANDLE,  );
          
   else if ( j = 1 ) 
      handle_deal_with(SQL_HANDLE_DESC, handlevalue.henv_varible,  );
                           
   else if ( j = 2 ) 
      handle_deal_with(SQL_HANDLE_DESC, handlevalue.hdbc_varible,  );

   else if ( j = 3 ) 
      handle_deal_with(SQL_HANDLE_DESC, handlevalue.hstmt_varible,  );
           
   else if ( j = 4 )  
      handle_deal_with(SQL_HANDLE_DESC, handlevalue.hdesc_varible,  );
     
  else 
      handle_deal_with(SQL_HANDLE_DESC, handlevalue.stranghandle,  );
        
                              }

  while (int j = 0; j++; j < 6) {
   if ( j = 0 ) 
      handle_deal_with(handlevalue.stranghandle, SQL_NULL_HANDLE,  );
          
   else if ( j = 1 ) 
      handle_deal_with(handlevalue.stranghandle, handlevalue.henv_varible,  );
                           
   else if ( j = 2 ) 
      handle_deal_with(handlevalue.stranghandle, handlevalue.hdbc_varible,  );

   else if ( j = 3 ) 
      handle_deal_with(handlevalue.stranghandle, handlevalue.hstmt_varible,  );
           
   else if ( j = 4 )  
      handle_deal_with(handlevalue.stranghandle  handlevalue.hdesc_varible,  );
     
  else 
      handle_deal_with(handlevalue.stranghandle, handlevalue.stranghandle,  );
        
                              }
   

}


void DisplayError(SQLCHAR SqlState[6], string SQLSTATE, string flag, SQLSMALLINT HandleType, SQLHANDLE InputHandle)
{
cout << "the operation is: " << flag << endl;
cout << "the HandleType is:" << HandleType << endl;
cout << "the InputHandle is :"<< InputHandle <<endl;
cout << "the correct state is:" << SQLSTATE << endl;
cout << "the output state is:" << Sqlstate << endl;
}

}


void handle_deal_with(SQLSMALLINT HandleType, SQLHANDLE InputHandle, string SQLSTATE) 
{
     retcode = SQLAllocHandle(HandleType, InputHandle, OutputHandlePtr);
     if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
     i = 1;
     while ((SQLSTATEs = SQLGetDiagRec(HandleType, InputHandle, i, 
             Sqlstate, &NativeError, Msg, sizeof(Msg), 
             &MsgLen)) != SQL_NO_DATE)                   {
  
     if (SQLSTATE = Sqlstate )
     DisplayError(SqlState, SQLSTATE, 'OK');
  
     else
     DisplayError(SqlState, SQLSTATE, 'failure');
  
     i ++;
                                                         }
                                                                   }
 }
