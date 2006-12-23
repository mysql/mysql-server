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
 * @file SQLColAttributeTest2.cpp
 */

#include <common.hpp>
using namespace std;

#define MAXIMUM_MESSAGE_LENGTH_Test2 200
#define BufferLengthTest2 156

SQLHSTMT    ColAtt_hstmtTest2;
SQLHSTMT    ColAtt_hdbcTest2;
SQLHENV     ColAtt_henvTest2;
SQLHDESC    ColAtt_hdescTest2;

SQLCHAR        CharacterAttributePtrTest2;
SQLINTEGER     NumericAttributePtrTest2;
SQLSMALLINT    StringLengthPtrTest2;

SQLRETURN ColAtt_retTest2;

void ColAtt_DisplayErrorTest2(SQLSMALLINT ColAttTest2_HandleType, 
			      SQLHSTMT ColAttTest2_InputHandle);

/** 
 * Test returning descriptor information
 *
 * Test:
 * -# Call SQLColAttribute without preceeding SQLExecute
 * -# Let TYPE is 'ITEM' in Table 20, FieldIdentifer is zero
 * -# Let TYPE is 'ITEM' in Table 20, ColumnNumber is less than one
 * -# FieldIdentifer is not one of the code valuess in Table 20
 * -# Let TYPE is 'ITEM' in Table 20, ColumnNumber is greater than 1044
 * 
 * @return Zero, if test succeeded
 */
int SQLColAttributeTest2()
{
  ndbout << endl << "Start SQLColAttribute Testing2" << endl;

  SQLCHAR SQLStmt [120];

  //*******************************************************************
  //** Test                                                          **
  //**                                                               **
  //** hstmt                                                         **
  //** Prepare a statement without executing the statement           **
  //**                                                               **
  //** Intended result:  table Customer should not have new row      **
  //*******************************************************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  ColAtt_retTest2 = SQLAllocHandle(SQL_HANDLE_ENV, 
				   SQL_NULL_HANDLE, 
				   &ColAtt_henvTest2);
  
  if (ColAtt_retTest2 == SQL_SUCCESS || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  ColAtt_retTest2 = SQLSetEnvAttr(ColAtt_henvTest2, 
				  SQL_ATTR_ODBC_VERSION, 
				  (SQLPOINTER) SQL_OV_ODBC3, 
				  SQL_IS_UINTEGER);
  
  if (ColAtt_retTest2 == SQL_SUCCESS || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 2.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  ColAtt_retTest2 = SQLAllocHandle(SQL_HANDLE_DBC, 
				   ColAtt_henvTest2, 
				   &ColAtt_hdbcTest2);

  if (ColAtt_retTest2 == SQL_SUCCESS || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  ColAtt_retTest2 = SQLConnect(ColAtt_hdbcTest2, 
			       (SQLCHAR *) connectString(), 
			       SQL_NTS, 
			       (SQLCHAR *) "", 
			       SQL_NTS, 
			       (SQLCHAR *) "", 
			       SQL_NTS);
  
  if (ColAtt_retTest2 == SQL_SUCCESS || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }

  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  ColAtt_retTest2 = SQLAllocHandle(SQL_HANDLE_STMT, 
				   ColAtt_hdbcTest2, 
				   &ColAtt_hstmtTest2); 
  if(ColAtt_retTest2 == SQL_SUCCESS || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;

  //************************
  //** Define a statement **
  //************************

  /*
  strcpy((char *) SQLStmt, 
	 "DELETE FROM Customers WHERE CustID = 6");
   */

    strcpy((char *) SQLStmt, 
    "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES (6, 'Jan', 'LM vag 8', '969696')");

    /*
    strcpy((char *) SQLStmt, 
    "INSERT INTO Customers (CustID, Name, Address, Phone) VALUES (?, ?, ?, ?)");
    */

  //********************************
  //** Prepare  SQL statement     **
  //********************************
  ColAtt_retTest2 = SQLPrepare(ColAtt_hstmtTest2, 
			  SQLStmt, 
			  SQL_NTS);
  
  if (ColAtt_retTest2 == SQL_SUCCESS || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO) 
    {
      //**************************************************************
      //** FieldIdentifer is not one of the code valuess in Table 20, 
      //** "Codes used for descriptor fields" 
      //**************************************************************
      ColAtt_retTest2 = SQLColAttribute(ColAtt_hstmtTest2, 
					2, 
					9999, 
					&CharacterAttributePtrTest2, 
					BufferLengthTest2, 
					&StringLengthPtrTest2, 
					&NumericAttributePtrTest2);
      
      if (ColAtt_retTest2 == SQL_ERROR || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << endl << "FieldIdentifer is not one of the" << endl;  
	  ndbout << "code valuess in Table 20, Codes used for" << endl;
	  ndbout << "descriptor fields" <<endl;
	  ColAtt_DisplayErrorTest2(SQL_HANDLE_STMT, ColAtt_hstmtTest2);
	}

      //****************************************************************
      //** Let TYPE is 'ITEM' in Table 20, ColumnNumber is less than one
      //****************************************************************
      ColAtt_retTest2 = SQLColAttribute(ColAtt_hstmtTest2, 
					-1, 
					SQL_DESC_BASE_COLUMN_NAME, 
					&CharacterAttributePtrTest2, 
					BufferLengthTest2, 
					&StringLengthPtrTest2, 
					&NumericAttributePtrTest2);
      
      if (ColAtt_retTest2 == SQL_ERROR || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "Let TYPE is 'ITEM' in Table 20,ColumnNumber" 
		 << "is less than one" << endl;
	  ColAtt_DisplayErrorTest2(SQL_HANDLE_STMT, ColAtt_hstmtTest2);
	}
      
      //*********************************************************
      //** Let TYPE is 'ITEM' in Table 20, FieldIdentifer is zero 
      //*********************************************************
      ColAtt_retTest2 = SQLColAttribute(ColAtt_hstmtTest2, 
					1018, 
					0, 
					&CharacterAttributePtrTest2, 
					BufferLengthTest2, 
					&StringLengthPtrTest2, 
					&NumericAttributePtrTest2);
      
      if (ColAtt_retTest2 == SQL_ERROR || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "Let TYPE is 'ITEM' in Table 20, FieldIdentifer" 
		 << " is zero"  <<endl;
	  ColAtt_DisplayErrorTest2(SQL_HANDLE_STMT, ColAtt_hstmtTest2);
        }

      //**********************************************************
      //** Let TYPE is 'ITEM' in Table 20, ColumnNumber is greater 
      //** than TOP_LEVEL_COUNT(1044) 
      //*********************************************************
      ColAtt_retTest2 = SQLColAttribute(ColAtt_hstmtTest2, 
					1045, 
					SQL_DESC_BASE_COLUMN_NAME, 
					&CharacterAttributePtrTest2, 
					BufferLengthTest2, 
					&StringLengthPtrTest2, 
					&NumericAttributePtrTest2);
      
      if (ColAtt_retTest2 == SQL_ERROR || ColAtt_retTest2 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "Let TYPE is 'ITEM' in Table 20, ColumnNumber" << endl 
		 << "is greater than TOP_LEVEL_COUNT(1044)" << endl;
	  ColAtt_DisplayErrorTest2(SQL_HANDLE_STMT, ColAtt_hstmtTest2);
	}
      
    }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(ColAtt_hdbcTest2);
  SQLFreeHandle(SQL_HANDLE_STMT, ColAtt_hstmtTest2);
  SQLFreeHandle(SQL_HANDLE_DBC, ColAtt_hdbcTest2);
  SQLFreeHandle(SQL_HANDLE_ENV, ColAtt_henvTest2);

  return NDBT_OK;
}


void ColAtt_DisplayErrorTest2(SQLSMALLINT ColAttTest2_HandleType, 
			      SQLHSTMT ColAttTest2_InputHandle)
{
  SQLSMALLINT ColAtt_i = 1;
  SQLRETURN ColAtt_SQLSTATEs;
  SQLCHAR ColAtt_Sqlstate[5];
  SQLCHAR ColAtt_Msg[MAXIMUM_MESSAGE_LENGTH_Test2];
  SQLSMALLINT ColAtt_MsgLen;
  SQLINTEGER  ColAtt_NativeError;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((ColAtt_SQLSTATEs = SQLGetDiagRec(ColAttTest2_HandleType, 
					   ColAttTest2_InputHandle, 
					   ColAtt_i, 
					   ColAtt_Sqlstate, 
					   &ColAtt_NativeError, 
					   ColAtt_Msg, 
					   sizeof(ColAtt_Msg), 
					   &ColAtt_MsgLen)) 
	 != SQL_NO_DATA)                   
    {
      
      ndbout << "the HandleType is:" << ColAttTest2_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)ColAttTest2_InputHandle << endl;
      ndbout << "the ColAtt_Msg is: " << (char *) ColAtt_Msg << endl;
      ndbout << "the output state is:" << (char *)ColAtt_Sqlstate << endl; 
      
      ColAtt_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}
