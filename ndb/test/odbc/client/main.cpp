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
 * @file main.cpp
 */

#include <common.hpp>

/**
 * main ODBC Tests
 *
 * Tests main ODBC functions.
 */

#include <common.hpp>

int check = NDBT_OK;
static char* myConnectString;

char* connectString()
{
  return myConnectString;
}

int main(int argc, char** argv)
{

  if (argc != 3) {
    return NDBT_ProgramExit(NDBT_WRONGARGS);
  }
  myConnectString = argv[1];

  if ( strcmp(argv[2], "help") == 0 )
    {
      ndbout << "Number of Testing Program   " << "  Name of Testing Program" << endl;
      ndbout << "      1                       SQLGetDataTest()" << endl;
      ndbout << "      2                       SQLTablesTest()" << endl;
      ndbout << "      3                       SQLGetFunctionsTest()" << endl;
      ndbout << "      4                       SQLGetInfoTest()" << endl;
      ndbout << "      5                       SQLGetTypeInfoTest()" << endl;
      ndbout << "      6                       SQLDisconnectTest()" << endl;
      ndbout << "      7                       SQLFetchTest()" << endl;
      ndbout << "      8                       SQLRowCountTest()" << endl;
      ndbout << "      9                       SQLGetCursorNameTest()" << endl;
      ndbout << "      10                      SQLCancelTest()" << endl;
      ndbout << "      11                      SQLTransactTest()" << endl;
      ndbout << "      12                      SQLSetCursorNameTest()" << endl;
      ndbout << "      13                      SQLNumResultColsTest()" << endl;
      ndbout << "      14                      SQLDescribeColTest()" << endl;
      ndbout << "      15                      SQLExecDirectTest()" << endl;
      ndbout << "      16                      SQLColAttributeTest3()" << endl;
      ndbout << "      17                      SQLColAttributeTest2()" << endl;
      ndbout << "      18                      SQLColAttributeTest1()" << endl;
      ndbout << "      19                      SQLColAttributeTest()" << endl;
      ndbout << "      20                      SQLBindColTest()" << endl;
      ndbout << "      21                      SQLGetDiagRecSimpleTest()" << endl;
      ndbout << "      22                      SQLConnectTest()" << endl;
      ndbout << "      23                      SQLPrepareTest()" << endl;
    }
  else
    {

      ndbout << endl << "Executing Files Name = " << argv[0] << endl;
      ndbout << "The Number of testing program = " << argv[2] << endl;

      int i = atoi(argv[2]);
      switch (i) {
      case 1:
	if (check == NDBT_OK) check = SQLGetDataTest();
	break;
      case 2:
	if (check == NDBT_OK) check = SQLTablesTest();
	break;
      case 3:
	if (check == NDBT_OK) check = SQLGetFunctionsTest();
	break;
      case 4:
	if (check == NDBT_OK) check = SQLGetInfoTest();
	break;
      case 5:
	if (check == NDBT_OK) check = SQLGetTypeInfoTest();
	break;
      case 6:
	if (check == NDBT_OK) check = SQLDisconnectTest();
	break;
      case 7:
	if (check == NDBT_OK) check = SQLFetchTest();
	break;
      case 8:
	if (check == NDBT_OK) check = SQLRowCountTest();
	break;
      case 9:
	if (check == NDBT_OK) check = SQLGetCursorNameTest();
	break;
      case 10:
	if (check == NDBT_OK) check = SQLCancelTest();
	break;
      case 11:
	if (check == NDBT_OK) check = SQLTransactTest();
	break;
      case 12:
	if (check == NDBT_OK) check = SQLSetCursorNameTest();
	break;
      case 13:
	if (check == NDBT_OK) check = SQLNumResultColsTest();
	break;
      case 14:
	if (check == NDBT_OK) check = SQLDescribeColTest();
	break;
      case 15:
	if (check == NDBT_OK) check = SQLExecDirectTest();
	break;
      case 16:
	if (check == NDBT_OK) check = SQLColAttributeTest3();
	break;
      case 17:
	if (check == NDBT_OK) check = SQLColAttributeTest2();
	break;
      case 18:
	if (check == NDBT_OK) check = SQLColAttributeTest1();
	break;
      case 19:
	if (check == NDBT_OK) check = SQLColAttributeTest();
	break;
      case 20:
	if (check == NDBT_OK) check = SQLBindColTest();
	break;
      case 21:
	if (check == NDBT_OK) check = SQLGetDiagRecSimpleTest();
	break;
      case 22:
	if (check == NDBT_OK) check = SQLConnectTest();
	break;
      case 23:
	if (check == NDBT_OK) check = SQLPrepareTest();
	break;
      }
    }

  return NDBT_ProgramExit(check);
}



