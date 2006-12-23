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
 * @file SQLColAttributeTest.cpp
 */

#include <common.hpp>
using namespace std;

#define MAXIMUM_MESSAGE_LENGTH_Test 200
#define BufferLengthTest 156

SQLHSTMT    ColAtt_hstmt;
SQLHSTMT    ColAtt_hdbc;
SQLHENV     ColAtt_henv;
SQLHDESC    ColAtt_hdesc;

SQLCHAR        CharacterAttributePtr;
SQLINTEGER     NumericAttributePtr;
SQLSMALLINT    StringLengthPtr;

SQLRETURN ColAtt_ret;

void ColAtt_DisplayError(SQLSMALLINT ColAtt_HandleType, 
			 SQLHSTMT ColAtt_InputHandle);

/** 
 * Test returning descriptor information
 *
 * Tests:
 * -# Call SQLColAttribute, without preceeding SQLPrepare
 * -# ???
 * 
 * @return Zero, if test succeeded
 */
int SQLColAttributeTest()
{
  ndbout << endl << "Start SQLColAttribute Testing" << endl;

  SQLCHAR SQLStmt [120];

  /********************************************************************
   ** Test 1:                                                        **
   **                                                                **
   ** Checks to execute SQLColAttribute, when there is no            **
   ** prepared or executed statement associated with StatementHandle **
   **                                                                **
   ** Intended result:  SQL_ERROR ???                                **
   ********************************************************************/
  ColAtt_ret = SQLColAttribute(ColAtt_hstmt, 
			       1, 
			       SQL_DESC_AUTO_UNIQUE_VALUE, 
			       &CharacterAttributePtr, 
			       BufferLengthTest, 
			       &StringLengthPtr, 
			       &NumericAttributePtr);

  if (ColAtt_ret == SQL_ERROR)
    {
      ndbout << "ColAtt_ret = " << ColAtt_ret << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
    }
  else if (ColAtt_ret == SQL_SUCCESS_WITH_INFO)
    {
      ndbout << "ColAtt_ret = " << ColAtt_ret << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
    }
  else if (ColAtt_ret == SQL_SUCCESS)
    {
      ndbout << "ColAtt_ret = " << ColAtt_ret << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
    }
  else if (ColAtt_ret == -2)
    {
      ndbout << "ColAtt_ret = " << ColAtt_ret << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
    }
  else 
    {
      ndbout << "ColAtt_ret = " << ColAtt_ret << endl;
      ndbout << endl << "There is no prepared or executed" << endl 
	     << " statement associated with StatementHandle" << endl;
      ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
    }

  //*******************************************************************
  //** Test 2:                                                       **
  //**                                                               **
  //** hstmt                                                         **
  //** Execute a statement to retrieve rows from the Customers table **
  //** We can create the table and insert rows into Mysql            **
  //**                                                               **
  //** Intended result: ???                                          **
  //*******************************************************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  ColAtt_ret = SQLAllocHandle(SQL_HANDLE_ENV, 
			      SQL_NULL_HANDLE, 
			      &ColAtt_henv);
  
  if (ColAtt_ret == SQL_SUCCESS || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 2.x **
  //*********************************************
  ColAtt_ret = SQLSetEnvAttr(ColAtt_henv, 
			     SQL_ATTR_ODBC_VERSION, 
			     (SQLPOINTER) SQL_OV_ODBC2, 
			     SQL_IS_UINTEGER);
  
  if (ColAtt_ret == SQL_SUCCESS || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 2.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  ColAtt_ret = SQLAllocHandle(SQL_HANDLE_DBC, 
			      ColAtt_henv, 
			      &ColAtt_hdbc);

  if (ColAtt_ret == SQL_SUCCESS || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  ColAtt_ret = SQLConnect(ColAtt_hdbc, 
			  (SQLCHAR *) connectString(), 
			  SQL_NTS, 
			  (SQLCHAR *) "", 
			  SQL_NTS, 
			  (SQLCHAR *) "", 
			  SQL_NTS);
  
  if (ColAtt_ret == SQL_SUCCESS || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }
  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  ColAtt_ret = SQLAllocHandle(SQL_HANDLE_STMT, 
			      ColAtt_hdbc, 
			      &ColAtt_hstmt); 
  if(ColAtt_ret == SQL_SUCCESS || ColAtt_ret == SQL_SUCCESS_WITH_INFO) 
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
  ColAtt_ret = SQLPrepare(ColAtt_hstmt, 
			  SQLStmt, 
			  SQL_NTS);
  
  if (ColAtt_ret == SQL_SUCCESS || ColAtt_ret == SQL_SUCCESS_WITH_INFO) 
    {
      //**************************************************************
      //** FieldIdentifer is not one of the code valuess in Table 20, 
      //** "Codes used for descriptor fields" 
      //**************************************************************
      ColAtt_ret = SQLColAttribute(ColAtt_hstmt, 
				   2, 
				   9999, 
				   &CharacterAttributePtr, 
				   BufferLengthTest, 
				   &StringLengthPtr, 
				   &NumericAttributePtr);
      
      if (ColAtt_ret == SQL_ERROR || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << endl << "FieldIdentifer is not one of the" << endl;  
	  ndbout << "code valuess in Table 20, Codes used for" << endl;
	  ndbout << "descriptor fields" <<endl;
	  ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
	}

      //****************************************************************
      //** Let TYPE is 'ITEM' in Table 20, ColumnNumber is less than one
      //****************************************************************
      ColAtt_ret = SQLColAttribute(ColAtt_hstmt, 
				   -1, 
				   SQL_DESC_BASE_COLUMN_NAME, 
				   &CharacterAttributePtr, 
				   BufferLengthTest, 
				   &StringLengthPtr, 
				   &NumericAttributePtr);

      if (ColAtt_ret == SQL_ERROR || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "Let TYPE is 'ITEM' in Table 20,ColumnNumber" 
		 << "is less than one" << endl;
	  ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
	}
      
      //*********************************************************
      //** Let TYPE is 'ITEM' in Table 20, FieldIdentifer is zero 
      //*********************************************************
      ColAtt_ret = SQLColAttribute(ColAtt_hstmt, 
				   1018, 
				   0, 
				   &CharacterAttributePtr, 
				   BufferLengthTest,
				   &StringLengthPtr,
				   &NumericAttributePtr);
      
      if (ColAtt_ret == SQL_ERROR || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "Let TYPE is 'ITEM' in Table 20, FieldIdentifer" 
		 << " is zero"  <<endl;
	  ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
        }

      //**********************************************************
      //** Let TYPE is 'ITEM' in Table 20, ColumnNumber is greater 
      //** than TOP_LEVEL_COUNT(1044) 
      //*********************************************************
      ColAtt_ret = SQLColAttribute(ColAtt_hstmt, 
				   1045, 
				   SQL_DESC_BASE_COLUMN_NAME, 
				   &CharacterAttributePtr, 
				   BufferLengthTest, 
				   &StringLengthPtr, 
				   &NumericAttributePtr);
      
      if (ColAtt_ret == SQL_ERROR || ColAtt_ret == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "Let TYPE is 'ITEM' in Table 20, ColumnNumber" << endl 
		 << "is greater than TOP_LEVEL_COUNT(1044)" << endl;
	  ColAtt_DisplayError(SQL_HANDLE_STMT, ColAtt_hstmt);
	}
      
    }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(ColAtt_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, ColAtt_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, ColAtt_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, ColAtt_henv);

  return NDBT_OK;
}

void ColAtt_DisplayError(SQLSMALLINT ColAtt_HandleType, 
			 SQLHSTMT ColAtt_InputHandle)
{
  SQLSMALLINT ColAtt_i = 1;
  SQLRETURN ColAtt_SQLSTATEs;
  SQLCHAR ColAtt_Sqlstate[5];
  SQLCHAR ColAtt_Msg[MAXIMUM_MESSAGE_LENGTH_Test];
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



