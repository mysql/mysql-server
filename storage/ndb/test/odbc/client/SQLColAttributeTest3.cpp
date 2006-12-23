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
 * @file SQLColAttributeTest3.cpp
 */

#include <common.hpp>
using namespace std;

#define MAXIMUM_MESSAGE_LENGTH_Test3 200
#define BufferLengthTest3 156

SQLHSTMT    ColAtt_hstmtTest3;
SQLHSTMT    ColAtt_hdbcTest3;
SQLHENV     ColAtt_henvTest3;
SQLHDESC    ColAtt_hdescTest3;

SQLCHAR     TypeName[18];
SQLSMALLINT TypeNameLen;

SQLRETURN ColAtt_retTest3;

void ColAtt_DisplayErrorTest3(SQLSMALLINT ColAttTest3_HandleType, 
			      SQLHSTMT ColAttTest3_InputHandle);

/** 
 * Test returning descriptor information
 *
 * Test:
 * -# Print out column name without executing statement
 * 
 * @return Zero, if test succeeded
 */

int SQLColAttributeTest3()
{
  ndbout << endl << "Start SQLColAttribute Testing3" << endl;

  SQLCHAR SQLStmt [120];

  //********************************************************************
  //** Test 3:                                                        **
  //**                                                                **
  //** Prepare a statement without executing the statement            **
  //** We want to print out the Type Name of each column in the table **
  //** Customers                                                      **
  //**                                                                **
  //** Intended result: Only display column name, but there is no new **
  //**          row in table Customers                                **
  //********************************************************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  ColAtt_retTest3 = SQLAllocHandle(SQL_HANDLE_ENV, 
				   SQL_NULL_HANDLE, 
				   &ColAtt_henvTest3);
  
  if (ColAtt_retTest3 == SQL_SUCCESS || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;

  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  ColAtt_retTest3 = SQLSetEnvAttr(ColAtt_henvTest3, 
				  SQL_ATTR_ODBC_VERSION, 
				  (SQLPOINTER) SQL_OV_ODBC3, 
				  SQL_IS_UINTEGER);
  
  if (ColAtt_retTest3 == SQL_SUCCESS || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  ColAtt_retTest3 = SQLAllocHandle(SQL_HANDLE_DBC, 
				   ColAtt_henvTest3, 
				   &ColAtt_hdbcTest3);

  if (ColAtt_retTest3 == SQL_SUCCESS || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;

  // *******************
  // ** Connect to DB **
  // *******************
  ColAtt_retTest3 = SQLConnect(ColAtt_hdbcTest3, 
			       (SQLCHAR *) connectString(), 
			       SQL_NTS, 
			       (SQLCHAR *) "", 
			       SQL_NTS, 
			       (SQLCHAR *) "", 
			       SQL_NTS);
  
  if (ColAtt_retTest3 == SQL_SUCCESS || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }

  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  ColAtt_retTest3 = SQLAllocHandle(SQL_HANDLE_STMT, 
				   ColAtt_hdbcTest3, 
				   &ColAtt_hstmtTest3); 
  if(ColAtt_retTest3 == SQL_SUCCESS || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO) 
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

  //*****************************
  //** Prepare  SQL statement  **
  //*****************************
  ColAtt_retTest3 = SQLPrepare(ColAtt_hstmtTest3, 
			  SQLStmt, 
			  SQL_NTS);
  
  if (ColAtt_retTest3 == SQL_SUCCESS || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO) 
    {
      //************************************
      //** Display the name of column one **
      //************************************
      ColAtt_retTest3 = SQLColAttribute(ColAtt_hstmtTest3, 
					1, 
					SQL_COLUMN_TYPE_NAME, 
					TypeName, 
					sizeof(TypeName), 
					&TypeNameLen, 
					NULL);
   
      if (ColAtt_retTest3 == SQL_ERROR || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << endl << "ColAtt_retTest3 = " << ColAtt_retTest3 << endl; 
	  ndbout << endl << "Name of column 1 is:" 
		 << (char *)TypeName <<endl;  
	  ColAtt_DisplayErrorTest3(SQL_HANDLE_STMT, ColAtt_hstmtTest3);
	}

      //************************************
      //** Display the name of column two **
      //************************************
      ColAtt_retTest3 = SQLColAttribute(ColAtt_hstmtTest3, 
					2, 
					SQL_DESC_BASE_COLUMN_NAME, 
					TypeName, 
					sizeof(TypeName), 
					&TypeNameLen,
					NULL);

      if (ColAtt_retTest3 == SQL_ERROR || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << endl << "ColAtt_retTest3 = " << ColAtt_retTest3 << endl;
	  ndbout << endl << "Name of column 2 is:" 
		 << (char *)TypeName <<endl; 
	  ColAtt_DisplayErrorTest3(SQL_HANDLE_STMT, ColAtt_hstmtTest3);
	}
      
      //***************************************
      //**  Display the name of column three **
      //***************************************
      ColAtt_retTest3 = SQLColAttribute(ColAtt_hstmtTest3, 
					3, 
					SQL_DESC_BASE_COLUMN_NAME, 
					TypeName, 
					sizeof(TypeName), 
					&TypeNameLen, 
					NULL);

      if (ColAtt_retTest3 == SQL_ERROR || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "ColAtt_retTest3 = " << ColAtt_retTest3 << endl;
	  ndbout << endl << "Name of column 3 is:" 
		 << (char *)TypeName <<endl; 
	  ColAtt_DisplayErrorTest3(SQL_HANDLE_STMT, ColAtt_hstmtTest3);
        }

      //**************************************
      //**  Display the name of column four **
      //**************************************
      ColAtt_retTest3 = SQLColAttribute(ColAtt_hstmtTest3, 
					4, 
					SQL_DESC_BASE_COLUMN_NAME, 
					TypeName, 
					sizeof(TypeName), 
					&TypeNameLen, 
					NULL);

      if (ColAtt_retTest3 == SQL_ERROR || ColAtt_retTest3 == SQL_SUCCESS_WITH_INFO)
	{
	  ndbout << "ColAtt_retTest3 = " << ColAtt_retTest3 << endl;
	  ndbout << endl << "Name of column 4 is:" 
		 << (char *)TypeName <<endl; 
	  ColAtt_DisplayErrorTest3(SQL_HANDLE_STMT, ColAtt_hstmtTest3);
	}
      
    }

  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(ColAtt_hdbcTest3);
  SQLFreeHandle(SQL_HANDLE_STMT, ColAtt_hstmtTest3);
  SQLFreeHandle(SQL_HANDLE_DBC, ColAtt_hdbcTest3);
  SQLFreeHandle(SQL_HANDLE_ENV, ColAtt_henvTest3);

  return NDBT_OK;
}

void ColAtt_DisplayErrorTest3(SQLSMALLINT ColAttTest3_HandleType, 
			      SQLHSTMT ColAttTest3_InputHandle)
{
  SQLSMALLINT ColAtt_i = 1;
  SQLRETURN ColAtt_SQLSTATEs;
  SQLCHAR ColAtt_Sqlstate[5];
  SQLCHAR ColAtt_Msg[MAXIMUM_MESSAGE_LENGTH_Test3];
  SQLSMALLINT ColAtt_MsgLen;
  SQLINTEGER  ColAtt_NativeError;

  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((ColAtt_SQLSTATEs = SQLGetDiagRec(ColAttTest3_HandleType, 
					   ColAttTest3_InputHandle, 
					   ColAtt_i, 
					   ColAtt_Sqlstate, 
					   &ColAtt_NativeError, 
					   ColAtt_Msg, 
					   sizeof(ColAtt_Msg), 
					   &ColAtt_MsgLen)) 
	 != SQL_NO_DATA)                   
    {
      
      ndbout << "the HandleType is:" << ColAttTest3_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)ColAttTest3_InputHandle << endl;
      ndbout << "the ColAtt_Msg is: " << (char *) ColAtt_Msg << endl;
      ndbout << "the output state is:" << (char *)ColAtt_Sqlstate << endl; 
      
      ColAtt_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}
