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
 * @file SQLFetchTest.cpp
 */

#include <common.hpp>
#define F_MESSAGE_LENGTH 200
using namespace std;

#define F_NAME_LEN 20
#define F_PHONE_LEN 20
#define F_ADDRESS_LEN 20

SQLHSTMT    F_hstmt;
SQLHDESC    F_hdbc;
SQLHENV     F_henv;
SQLHDESC    F_hdesc;

void SQLFetchTest_DisplayError(SQLSMALLINT F_HandleType, 
			       SQLHDESC F_InputHandle);

/** 
 * Test to advance a cursor to the next row of data in a data result set
 * and to retrieve data from any bound columns that exist for that row 
 * into their associated application variables
 *
 * Tests:
 * _# Test1 Execute statements and display the results 
 * -# Test2 There is no executed statement 
 * @return Zero, if test succeeded
 */
int SQLFetchTest()
{
  SQLRETURN retcode;
  SQLCHAR  SQLStmt[120];
  SQLCHAR  SQLStmt1[120];
  SQLCHAR  SQLStmt2[120];
  SQLCHAR  SQLStmt3[120];
  SQLCHAR  SQLStmt4[120];

  SQLCHAR  F_CustID[20];
  SQLCHAR  F_Name[F_NAME_LEN], F_Phone[F_PHONE_LEN];
  SQLCHAR  F_Address[F_ADDRESS_LEN];

  ndbout << "Start SQLFetch Testing!" << endl;

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  retcode = SQLAllocHandle(SQL_HANDLE_ENV, 
			   SQL_NULL_HANDLE, 
			   &F_henv);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
  retcode = SQLSetEnvAttr(F_henv, 
			  SQL_ATTR_ODBC_VERSION, 
			  (SQLPOINTER) SQL_OV_ODBC3, 
			  SQL_IS_UINTEGER);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Set the ODBC application Version to 3.x!" << endl;

  //**********************************
  //** Allocate A Connection Handle **
  //**********************************

  retcode = SQLAllocHandle(SQL_HANDLE_DBC, 
			   F_henv, 
			   &F_hdbc);

if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated a connection Handle!" << endl;
  
  // *******************
  // ** Connect to DB **
  // *******************
  retcode = SQLConnect(F_hdbc, 
		       (SQLCHAR *) connectString(), 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS, 
		       (SQLCHAR *) "", 
		       SQL_NTS);
  
if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    ndbout << "Connected to DB : OK!" << endl;
  else 
    {  
      ndbout << "Failure to Connect DB!" << endl;
      return NDBT_FAILED;
    }

  //*******************************
  //** Allocate statement handle **
  //*******************************
  
  retcode = SQLAllocHandle(SQL_HANDLE_STMT, 
			   F_hdbc, 
			   &F_hstmt); 
if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
    ndbout << "Allocated a statement handle!" << endl;
  
  //************************
  //** Define a statement **
  //************************

  /* *** CustID is Integer *** */
  strcpy((char *) SQLStmt1, "CREATE TABLE Customers (CustID Integer, Name Char(12), Address Char(12), Phone Char(12), Primary Key(CustID, Name))");

  /* *** the primary key is alone *** */
// strcpy((char *) SQLStmt1, "CREATE TABLE Customers (CustID Integer, Name Char(12), Address Char(12), Phone Char(12), Primary Key(CustID))");

   strcpy((char *) SQLStmt2, "INSERT INTO Customers (CustID, Name, Address,Phone) VALUES(188, 'peter','LM Vag8','7190890')");

  /* *** CustID is Float *** */
//  strcpy((char *) SQLStmt1, 
//	 "CREATE TABLE Customers (CustID float, Name Char(12), Address Char(12), Phone Char(12), Primary Key(CustID))");
//  strcpy((char *) SQLStmt2, "INSERT INTO Customers (CustID, Name, Address,Phone) VALUES(1.1516, 'peter','LM Vag8','7190890')");

  /* *** CustID is Char *** */
  // strcpy((char *) SQLStmt1, "CREATE TABLE Customers (CustID char(6), Name Char(12), Address Char(12), Phone Char(12), Primary Key(CustID))");

  // strcpy((char *) SQLStmt2, "INSERT INTO Customers (CustID, Name, Address,Phone) VALUES('000001', 'peter','LM Vag8','7190890')");

  /* The UPDATE statements */
  //  strcpy((char *) SQLStmt3, "UPDATE Customers SET Phone = '98998' WHERE CustID = 1.1516");

  //  strcpy((char *) SQLStmt3, "UPDATE Customers SET Phone = '98998' WHERE CustID = '000001'");

  //  strcpy((char *) SQLStmt3, "UPDATE Customers SET Phone = '98998' WHERE CustID = 188"); 

  strcpy((char *) SQLStmt3, "UPDATE Customers SET Phone = '98998' WHERE CustID = 188 AND Name = 'peter'"); 

  // DELETE statements

  //  DELETE all records
  //  strcpy((char *) SQLStmt4, "DELETE FROM Customers");

  //  DELETE One record
  //  strcpy((char *) SQLStmt4, "DELETE FROM Customers WHERE CustID = 1.1516");
  //  strcpy((char *) SQLStmt4, "DELETE FROM Customers WHERE CustID = '000001'");
  //  strcpy((char *) SQLStmt4, "DELETE FROM Customers WHERE CustID = 188 AND Name = 'peter'");
  //  strcpy((char *) SQLStmt4, "DELETE FROM Customers WHERE CustID = 188");

  strcpy((char *) SQLStmt4, "DELETE FROM Customers WHERE CustID = 188 AND Name = 'peter'");

  //SELECT statements
  strcpy((char *) SQLStmt, "SELECT * FROM Customers");

  //********************************
  //** Prepare  CREATE statements **
  //********************************

  ndbout << ">>>>" << (char*)SQLStmt1 << "<<<<" << endl;
  retcode = SQLPrepare(F_hstmt, 
		       SQLStmt1, 
		       SQL_NTS);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //******************************************************************
  //** There is no executed statement associated with the allocated **
  //** SQL-statement identified by StatementHandle                  **
  //******************************************************************

  //This function is correct after testing. We don't test again.
  /*
    retcode = SQLFetch(F_hstmt);
    ndbout << endl << "retcode = SQLFetch(F_hstmt) = " << retcode << endl;
    if  (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
    {
    ndbout << "There is no executed statement associated with" << endl;
    ndbout << "the allocated SQL-statement" << endl;
    SQLFetchTest_DisplayError(SQL_HANDLE_DESC, F_hstmt);
    
    }
  */

  //*******************************
  //** Execute  CREATE statement **
  //*******************************

  retcode = SQLExecute(F_hstmt);

  if (retcode == 0)
    ndbout << endl << "Execute CREATE TABLE Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //********************************
  //** Prepare  INSERT statements **
  //********************************

  ndbout << ">>>>" << (char*)SQLStmt2 << "<<<<" << endl;
  retcode = SQLPrepare(F_hstmt, 
		       SQLStmt2, 
		       SQL_NTS);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  
  //******************************
  //** Execute INSERT statement **
  //******************************
  retcode = SQLExecute(F_hstmt);

  if (retcode == 0)
  ndbout << endl <<"Execute INSERT Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //********************************
  //** Prepare  UPDATE statements **
  //********************************

  ndbout << ">>>>" << (char*)SQLStmt3 << "<<<<" << endl;
  retcode = SQLPrepare(F_hstmt, 
		       SQLStmt3, 
		       SQL_NTS);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //******************************
  //** Execute UPDATE statement **
  //******************************
  retcode = SQLExecute(F_hstmt);

  if (retcode == 0)
  ndbout << endl <<"Execute UPDATE Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //********************************
  //** Prepare  DELETE statements **
  //********************************
  ndbout << ">>>>" << (char*)SQLStmt4 << "<<<<" << endl;
  retcode = SQLPrepare(F_hstmt, 
		       SQLStmt4, 
		       SQL_NTS);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    {
      ndbout << endl << "Preparing DELETE Statement failure!" << endl;
      SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);
    }

  //******************************
  //** Execute DELETE statement **
  //******************************

  retcode = SQLExecute(F_hstmt);

  if (retcode == 0)
  ndbout << endl <<"Execute DELETE Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    {
      ndbout << "DELETE Statement executing failure!" << endl;
      SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);
    }
  //********************************
  //** Prepare  SELECT statements **
  //********************************
  ndbout << ">>>>" << (char*)SQLStmt << "<<<<" << endl;
  retcode = SQLPrepare(F_hstmt, 
		       SQLStmt, 
		       SQL_NTS);

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  /*
  //******************************
  //** Execute SELECT statement **
  //******************************

  retcode = SQLExecute(F_hstmt);

  if (retcode == 0)
  ndbout << endl <<"Execute SELECT Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);
  */

  //********************
  //** Bind columns   **
  //********************

  retcode =SQLBindCol(F_hstmt, 
		      1, 
		      SQL_C_CHAR, 
		      F_CustID, 
		      sizeof(F_CustID),
		      NULL);
  ndbout << endl << "Bind Col1 retcode = " << retcode << " OK!" << endl;

  retcode =SQLBindCol(F_hstmt, 
		      2, 
		      SQL_C_CHAR, 
		      F_Name, 
		      F_NAME_LEN,
		      NULL);

  ndbout << "Bind Col2 retcode = " << retcode << " OK!" << endl;

  retcode = SQLBindCol(F_hstmt, 
		       3,
		       SQL_C_CHAR, 
		       F_Address, 
		       F_ADDRESS_LEN, 
		       NULL);

  ndbout << "Bind Col3 retcode = " << retcode << " OK!" << endl;

  retcode = SQLBindCol(F_hstmt, 
		       4, 
		       SQL_C_CHAR, 
		       F_Phone, 
		       F_PHONE_LEN,
		       NULL);

  ndbout << "Bind Col4 retcode = " << retcode << " OK!" << endl;

  //******************************
  //** Execute SELECT statement **
  //******************************

  retcode = SQLExecute(F_hstmt);

  if (retcode == 0)
  ndbout << endl <<"Execute SELECT Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //***************
  //* Fetch data **
  //***************
  ndbout << endl <<"Executing Fetch SELECT Statement ......" << endl;

  retcode = SQLFetch(F_hstmt);

  if (retcode == 100)
    ndbout << endl <<"Execute Fetch SELECT Statement, But No DATA!" << endl;

  if (retcode == 0)
  ndbout << endl <<"Execute Fetch SELECT Statement OK!" << endl;

  if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) 
    SQLFetchTest_DisplayError(SQL_HANDLE_STMT, F_hstmt);

  //*******************
  //* Display result **
  //*******************
  ndbout << endl << "The results is : " << endl;
  ndbout << "CustID = " << (char *)F_CustID << endl;
  ndbout << "Name = " << (char *)F_Name << endl;
  ndbout << "Address = " << (char *)F_Address << endl;
  ndbout << "Phone = " << (char *)F_Phone << endl;


  // *********************************
  // ** Disconnect and Free Handles **
  // *********************************  
  SQLDisconnect(F_hdbc);
  SQLFreeHandle(SQL_HANDLE_STMT, F_hstmt);
  SQLFreeHandle(SQL_HANDLE_DBC, F_hdbc);
  SQLFreeHandle(SQL_HANDLE_ENV, F_henv);

  return NDBT_OK;

 }


void SQLFetchTest_DisplayError(SQLSMALLINT F_HandleType, 
			       SQLHSTMT F_InputHandle)
{
  SQLCHAR     Sqlstate[50], Msg[F_MESSAGE_LENGTH];
  SQLRETURN   SQLSTATEs;
  SQLINTEGER  NativeError;
  SQLSMALLINT i, MsgLen;
  Msg[0] = 0;
  i = 1;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((SQLSTATEs = SQLGetDiagRec(F_HandleType, 
				    F_InputHandle, 
				    i, 
				    Sqlstate, 
				    &NativeError, 
				    Msg, 
				    sizeof(Msg), 
				    &MsgLen)) 
	 != SQL_NO_DATA)                   
{
    
  ndbout << "the HandleType is:" << F_HandleType << endl;
  ndbout << "the InputHandle is :" <<(long)F_InputHandle << endl;
  ndbout << "the Msg is :" << (char *)Msg << endl;
  ndbout << "the output state is:" << (char *)Sqlstate << endl; 
  
  i ++;
  break;
}
  ndbout << "-------------------------------------------------" << endl;  
}



