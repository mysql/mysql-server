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
 * @file SQLBindColTest.cpp
 */
#include <common.hpp>
using namespace std;

#define BindCol_NAME_LEN 10
#define BindCol_PHONE_LEN 10
#define BindCol_ADDRESS_LEN 10
#define BindCol_Price_LEN 10
#define BindCol_Weight_LEN 10
#define BindCol_Tax_LEN 10

#define BindCol_SQL_MAXIMUM_MESSAGE_LENGTH 200

//SQLHDBC     BindCol_hdbc;
//SQLHSTMT    BindCol_hstmt;
//SQLHENV     BindCol_henv;
//SQLHDESC    BindCol_hdesc;
//SQLRETURN   BCret;

//SQLCHAR     BindCol_Name[BindCol_NAME_LEN], BindCol_Phone[BindCol_PHONE_LEN];
//SQLCHAR     BindCol_Address[BindCol_ADDRESS_LEN];
//SQLINTEGER  NativeError;
//unsigned long BindCol_CustID;

void BindCol_DisplayError(SQLSMALLINT BindCol_HandleType, 
			  SQLHSTMT BindCol_InputHandle);

/** 
 * Test  setting column to bind
 * for a column in a result 
 *
 * -# Bind columns  1 
 * -# Bind columns  2 
 * -# Bind columns  3 
 * -# Bind columns  4 
 * -# Bind columns  5 
 * -# Bind columns  6 
 * -# Bind columns  7
 * @return Zero, if test succeeded
 */

int SQLBindColTest()
{

  SQLHDBC     BindCol_hdbc;
  SQLHSTMT    BindCol_hstmt;
  SQLHENV     BindCol_henv;
  SQLHDESC    BindCol_hdesc;
  
  SQLCHAR SQLStmt1 [240];
  SQLCHAR SQLStmt2 [240];
  SQLCHAR SQLStmt3 [120];

  SQLRETURN   BCret;

  unsigned long  BindCol_CustID;
  SQLCHAR        BindCol_Name[BindCol_NAME_LEN];
  short          BindCol_Account;
  unsigned short BindCol_Phone;
  long           BindCol_Price;
  float          BindCol_Weight;
  double         BindCol_Tax;

  ndbout << endl << "Start SQLBindCol Testing" << endl;

  //*******************************************************************
  //** hstmt 
  //** Execute a statement to retrieve rows from the Customers table **
  //** We can create the table and insert rows into Customers        **
  //*******************************************************************

  //************************************
  //** Allocate An Environment Handle **
  //************************************
  BCret = SQLAllocHandle(SQL_HANDLE_ENV, 
			 SQL_NULL_HANDLE, 
			 &BindCol_henv);
  
if (BCret == SQL_SUCCESS || BCret == SQL_SUCCESS_WITH_INFO)
    ndbout << "Allocated an environment Handle!" << endl;
  
  //*********************************************
  //** Set the ODBC application Version to 3.x **
  //*********************************************
 BCret = SQLSetEnvAttr(BindCol_henv, 
		       SQL_ATTR_ODBC_VERSION, 
		       (SQLPOINTER) SQL_OV_ODBC3, 
		       SQL_IS_UINTEGER);
  
if (BCret == SQL_SUCCESS || BCret == SQL_SUCCESS_WITH_INFO)
  ndbout << "Set the ODBC application Version to 3.x!" << endl;

//**********************************
//** Allocate A Connection Handle **
//**********************************
 
 BCret = SQLAllocHandle(SQL_HANDLE_DBC, 
			BindCol_henv, 
			&BindCol_hdbc);
 
 if (BCret == SQL_SUCCESS || BCret == SQL_SUCCESS_WITH_INFO)
   ndbout << "Allocated a connection Handle!" << endl;
 
 // *******************
 // ** Connect to DB **
 // *******************
 BCret = SQLConnect(BindCol_hdbc, 
		    (SQLCHAR *) connectString(), 
		    SQL_NTS, 
		    (SQLCHAR *) "", 
		    SQL_NTS, 
		    (SQLCHAR *) "", 
		    SQL_NTS);
 
 if (BCret == SQL_SUCCESS || BCret == SQL_SUCCESS_WITH_INFO)
   ndbout << "Connected to DB : OK!" << endl;
 else 
   {  
     ndbout << "Failure to Connect DB!" << endl;
     return NDBT_FAILED;
   }

 //*******************************
 //** Allocate statement handle **
 //*******************************
 
 BCret = SQLAllocHandle(SQL_HANDLE_STMT, 
			BindCol_hdbc, 
			&BindCol_hstmt); 
 if(BCret == SQL_SUCCESS || BCret == SQL_SUCCESS_WITH_INFO) 
   ndbout << "Allocated a statement handle!" << endl;
 
 //************************
 //** Define a statement **
 //************************

 /* Primary key is Integer and Char */
 strcpy((char *) SQLStmt1, "CREATE TABLE Customer1(CustID Integer, Name Char(12), Account Char(12), Phone Char(12), Price Char(6), Weight Char(6), Tax Char(6), Primary Key(CustID, Name))");

 strcpy((char *) SQLStmt2, "INSERT INTO Customer1 (CustID, Name, Account, Phone, Price, Weight, Tax) VALUES(588, 'peter','6808','7190890', '5.68', '1.58', '0.88')");
   
 strcpy((char *) SQLStmt3, "SELECT * FROM Customer1");
   
 //************************************************
 //** Prepare and Execute CREATE TABLE statement **
 //************************************************
 ndbout << endl << "Prepare and Execute CREATE TABLE statement ......" << endl;
 ndbout << ">>>>" << (char*)SQLStmt1 << "<<<<" << endl;
 BCret = SQLExecDirect(BindCol_hstmt, 
		       SQLStmt1, 
		       SQL_NTS);
 if (BCret == SQL_SUCCESS)
   ndbout << "Prepare and Execute CREATE TABLE statement OK!" 
	  << endl<< endl;
  
 if (BCret == SQL_ERROR || BCret == SQL_SUCCESS_WITH_INFO) 
   BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
 
 if (BCret == -2)
   {
     ndbout << "BCret = SQLExexDirect()=" << BCret << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }

 //*******************************************************
 //** Prepare and Execute INSERT statement with prepare **
 //*******************************************************
 ndbout << "Prepare and Execute INSERT statement ......" << endl;
 ndbout << ">>>>" << (char*)SQLStmt2 << "<<<<" << endl;
 BCret = SQLExecDirect(BindCol_hstmt, 
		       SQLStmt2, 
		       SQL_NTS);
 
 if (BCret == SQL_SUCCESS)
   ndbout << "Prepare and Execute INSERT statement OK!" 
	  << endl << endl;
 
 if (BCret == SQL_ERROR || BCret == SQL_SUCCESS_WITH_INFO) 
   BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
  
 if (BCret == -2)
   {
     ndbout << "BCret = SQLExexDirect()=" << BCret << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }

 //******************************************
 //** Prepare and EXECUTE SELECT statement **
 //******************************************
 ndbout << "Prepare and Execute SELECT statement ......" << endl;
 ndbout << ">>>>" << (char*)SQLStmt3 << "<<<<" << endl;
 BCret = SQLExecDirect(BindCol_hstmt, 
		       SQLStmt3, 
		       SQL_NTS);
 
 if (BCret == SQL_SUCCESS)
   ndbout << "Prepare and Execute SELECT statement OK!" 
	  << endl << endl;
 
 if (BCret == SQL_ERROR || BCret == SQL_SUCCESS_WITH_INFO) 
   BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
  
 if (BCret == -2)
   {
     ndbout << "BCret = SQLExexDirect()=" << BCret << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }
 
 //*******************************
 //** Execute SELECT statement  **
 //******************************* 
 // BCret = SQLExecute(BindCol_hstmt);
 // if (BCret == SQL_ERROR || BCret == SQL_SUCCESS_WITH_INFO) 
 //   {  
 //     ndbout << "BCret = " << BCret << endl;
 //     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
 //   }
 // else
 //   {
     
 if (BCret == SQL_SUCCESS)
   ndbout << "Execute INSERT statement OK!" << endl;
     
 //*********************
 //** Test1           **
 //** Bind columns  1 **
 //*********************
 
 BCret =SQLBindCol(BindCol_hstmt, 
		   1, 
		   SQL_C_ULONG, 
		   &BindCol_CustID, 
		   sizeof(BindCol_CustID),
		   NULL);
 
 if (BCret == SQL_SUCCESS)
   {
     ndbout << endl << "Bind col 1 OK!" << endl;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 1 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }    
 else if (BCret == SQL_ERROR)
   { 
     ndbout << "Bind Col 1 Failed!" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else
   ndbout << endl;   
 
 //*********************
 //** Test2           **
 //** Bind columns  2 **
 //*********************
 
 BCret =SQLBindCol(BindCol_hstmt, 
		   2, 
		   SQL_C_CHAR, 
		   &BindCol_Name, 
		   BindCol_NAME_LEN,
		   NULL);
 
 if (BCret == SQL_SUCCESS)
   {
     ndbout << "Bind col 2 OK!" << endl;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 2 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }    
 else if (BCret == SQL_ERROR)
   { 
     ndbout << "Bind Col 2 Failed!" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else
   ndbout << endl;
 
 //*********************
 //** Test3           **
 //** Bind columns 3  **
 //*********************
 
 BCret = SQLBindCol(BindCol_hstmt, 
		    3,
		    SQL_C_USHORT, 
		    &BindCol_Account, 
		    sizeof(BindCol_Account),
		    NULL);
 
 if (BCret == SQL_ERROR)
   { 
     ndbout << "Bind Col 3 Failed!" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 3 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }
 else if (BCret == SQL_SUCCESS)
   {
     ndbout << "Bind col 3 OK!" << endl;
   }
 else
   ndbout << endl;
 
 //*********************
 //** Test4           **
 //** Bind columns 4  **
 //*********************
 
 BCret = SQLBindCol(BindCol_hstmt, 
		    4, 
		    SQL_C_USHORT, 
		    &BindCol_Phone, 
		    sizeof(BindCol_Phone),
		    NULL);
 
 if (BCret == SQL_ERROR)
   {
     ndbout << "Bind Col 4 Failed!" << endl; 
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 4 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }
 else if (BCret == SQL_SUCCESS) 
   {
     ndbout << "Bind col 4 OK!" << endl;
   }
 else 
   ndbout << endl;

 //*********************
 //** Test5           **
 //** Bind columns 5  **
 //*********************
 
 BCret = SQLBindCol(BindCol_hstmt, 
		    5, 
		    SQL_C_SLONG, 
		    &BindCol_Price, 
		    sizeof(BindCol_Price),
		    NULL);
 
 if (BCret == SQL_ERROR)
   {
     ndbout << "Bind Col 5 Failed!" << endl; 
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 5 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }
 else if (BCret == SQL_SUCCESS) 
   {
     ndbout << "Bind col 5 OK!" << endl;
   }
 else 
   ndbout << endl;

 //*********************
 //** Test6           **
 //** Bind columns 6  **
 //*********************
 
 BCret = SQLBindCol(BindCol_hstmt, 
		    6,
		    SQL_C_FLOAT, 
		    &BindCol_Weight, 
		    sizeof(BindCol_Weight),
		    NULL);
 
 if (BCret == SQL_ERROR)
   {
     ndbout << "Bind Col 6 Failed!" << endl; 
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 6 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }
 else if (BCret == SQL_SUCCESS) 
   {
     ndbout << "Bind col 6 OK!" << endl;
   }
 else 
   ndbout << endl;

 //*********************
 //** Test7           **
 //** Bind columns 7  **
 //*********************

 BCret = SQLBindCol(BindCol_hstmt, 
		    7,
		    SQL_C_DOUBLE,
		    &BindCol_Tax, 
		    sizeof(BindCol_Tax),
		    NULL);
 
 if (BCret == SQL_ERROR)
   {
     ndbout << "Bind Col 7 Failed!" << endl; 
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
     return NDBT_FAILED;
   }
 else if (BCret == SQL_SUCCESS_WITH_INFO)
   {
     ndbout << "Bind Col 7 OK but with INFO" << endl;
     BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
   }
 else if (BCret == SQL_SUCCESS) 
   {
     ndbout << "Bind col 7 OK!" << endl;
   }
 else 
   ndbout << endl;

 //}

//*****************************************
//* Fetch and print each row of data. On **
//* an error, display a message and exit **
//*****************************************

BCret = SQLFetch(BindCol_hstmt);

 ndbout << endl << "BCret = SQLFetch(BindCol_hstmt) = " 
	<< BCret << endl;

if (BCret == SQL_ERROR)
{ 
  BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
  return NDBT_FAILED;
}
else if (BCret == SQL_SUCCESS_WITH_INFO) 
{
  ndbout << "CustID = " << (int)BindCol_CustID << endl;
  ndbout << "Name = " << (char *)BindCol_Name << endl;
  ndbout << "Account = " << (int)BindCol_Account << endl;
  ndbout << "Phone = " << (int)BindCol_Phone << endl; 
  ndbout << "Price = " << (int)BindCol_Price << endl;
  ndbout << "Weight = " << (int)BindCol_Weight << endl;
  ndbout << "Tax = " << (int)BindCol_Tax << endl;
  BindCol_DisplayError(SQL_HANDLE_STMT, BindCol_hstmt);
}
else
{
  ndbout << "CustID = " << (int)BindCol_CustID << endl;
  ndbout << "Name = " << (char *)BindCol_Name << endl;
  ndbout << "Account = " << (int)BindCol_Account << endl;
  ndbout << "Phone = " << (int)BindCol_Phone << endl;     
  ndbout << "Price = " << (int)BindCol_Price << endl;
  ndbout << "Weight = " << (int)BindCol_Weight << endl;
  ndbout << "Tax = " << (int)BindCol_Tax << endl;
}

// *********************************
// ** Disconnect and Free Handles **
// *********************************  
SQLDisconnect(BindCol_hdbc);
SQLFreeHandle(SQL_HANDLE_STMT, BindCol_hstmt);
SQLFreeHandle(SQL_HANDLE_DBC, BindCol_hdbc);
SQLFreeHandle(SQL_HANDLE_ENV, BindCol_henv);

return NDBT_OK;

}

void BindCol_DisplayError(SQLSMALLINT BindCol_HandleType, 
			  SQLHSTMT BindCol_InputHandle)
{
  SQLSMALLINT BindCol_i = 1;
  SQLRETURN BindCol__SQLSTATEs;
  SQLCHAR BindCol_Sqlstate[5];
  SQLCHAR BindCol_Msg[BindCol_SQL_MAXIMUM_MESSAGE_LENGTH];
  SQLSMALLINT BindCol_MsgLen;
  SQLINTEGER  NativeError;
  
  ndbout << "-------------------------------------------------" << endl;
  ndbout << "Error diagnostics:" << endl;
  
  while ((BindCol__SQLSTATEs = SQLGetDiagRec(BindCol_HandleType, 
					     BindCol_InputHandle, 
					     BindCol_i, 
					     BindCol_Sqlstate, 
					     &NativeError, 
					     BindCol_Msg, 
					     sizeof(BindCol_Msg), 
					     &BindCol_MsgLen)
	  ) != SQL_NO_DATA)
    {
      
      ndbout << "the HandleType is:" << BindCol_HandleType << endl;
      ndbout << "the InputHandle is :" << (long)BindCol_InputHandle << endl;
      ndbout << "the BindCol_Msg is: " << (char *) BindCol_Msg << endl;
      ndbout << "the output state is:" << (char *)BindCol_Sqlstate << endl; 
      
      BindCol_i ++;
      break;
    }
  ndbout << "-------------------------------------------------" << endl;
}

