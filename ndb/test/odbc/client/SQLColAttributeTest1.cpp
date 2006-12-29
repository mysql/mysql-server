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

/**
 * @file SQLColAttributeTest1.cpp
 */

#include <common.hpp>
using namespace std;

#define MAXIMUM_MESSAGE_LENGTH_Test1 200
#define BufferLenghTest1 156

SQLHSTMT    ColAtt_hstmtTest1;
SQLHSTMT    ColAtt_hdbcTest1;
SQLHENV     ColAtt_henvTest1;
SQLHDESC    ColAtt_hdescTest1;

SQLCHAR        CharacterAttributePtrTest1;
SQLINTEGER     NumericAttributePtrTest1;
SQLSMALLINT    StringLengthPtrTest1;

SQLRETURN ColAtt_retTest1;

void ColAtt_DisplayErrorTest1(SQLSMALLINT ColAtt_HandleType, 
			      SQLHSTMT ColAtt_InputHandle);

/** 
 * Test returning descriptor information
 *
 * Tests:
 * -# Execute SQLColAttribute without prepared or executed statement
 * 
 * @return Zero, if test succeeded
 */
int SQLColAttributeTest1()
{
  ndbout << endl << "Start SQLColAttribute Testing1" << endl;
  /********************************************************************
   ** Test :                                                         **
   **                                                                **
   ** Checks to execute SQLColAttribute, when there is no            **
   ** prepared or executed statement associated with StatementHandle **
   **                                                                **
   ** Intended result:CLI-specific condition-function sequence error **
   ********************************************************************/
  ColAtt_retTest1 = SQLColAttribute(ColAtt_hstmtTest1, 
				    1, 
				    SQL_DESC_AUTO_UNIQUE_VALUE, 
				    &CharacterAttributePtrTest1, 
				    BufferLenghTest1, 
				    &StringLengthPtrTest1, 
				    &NumericAttributePtrTest1);

  if (ColAtt_retTest1 == SQL_ERROR)
    {
      ndbout << "ColAtt_ret = " << ColAtt_retTest1 << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayErrorTest1(SQL_HANDLE_STMT, ColAtt_hstmtTest1);
    }
  else if (ColAtt_retTest1 == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "ColAtt_ret = " << ColAtt_retTest1 << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayErrorTest1(SQL_HANDLE_STMT, ColAtt_hstmtTest1);
    }
  else if (ColAtt_retTest1 == SQL_SUCCESS)
    {
      ndbout << "ColAtt_ret = " << ColAtt_retTest1 << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayErrorTest1(SQL_HANDLE_STMT, ColAtt_hstmtTest1);
    }
  else if (ColAtt_retTest1 == -2)
    {
      ndbout << "ColAtt_ret = " << ColAtt_retTest1 << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayErrorTest1(SQL_HANDLE_STMT, ColAtt_hstmtTest1);
    }
  else 
    {
      ndbout << "ColAtt_ret = " << ColAtt_retTest1 << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayErrorTest1(SQL_HANDLE_STMT, ColAtt_hstmtTest1);
    }

  return NDBT_OK;
}

void ColAtt_DisplayErrorTest1(SQLSMALLINT ColAtt_HandleType, 
			      SQLHSTMT ColAtt_InputHandle)
{
  SQLSMALLINT ColAtt_i = 1;
  SQLRETURN ColAtt_SQLSTATEs;
  SQLCHAR ColAtt_Sqlstate[5];
  SQLCHAR ColAtt_Msg[MAXIMUM_MESSAGE_LENGTH_Test1];
  SQLSMALLINT ColAtt_MsgLen;
  SQLINTEGER  ColAtt_NativeError;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((ColAtt_SQLSTATEs = SQLGetDiagRec(ColAtt_HandleType, 
					   ColAtt_InputHandle, 
					   ColAtt_i, 
					   ColAtt_Sqlstate, 
					   &ColAtt_NativeError, 
					   ColAtt_Msg, 
					   sizeof(ColAtt_Msg), 
					   &ColAtt_MsgLen)) 
	 != SQL_NO_DATA)                   
    {
      
      ndbout << "the HandleType is:" << ColAtt_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)ColAtt_InputHandle << endl;
      ndbout << "the ColAtt_Msg is: " << (char *) ColAtt_Msg << endl;
      ndbout << "the output state is:" << (char *)ColAtt_Sqlstate << endl; 
      
      ColAtt_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}



