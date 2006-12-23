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

SQLRETURN SQLHENVFREE_check, SQLHDBC_check;


// NDB_COMMAND(SQLTest1, ......., 65535)
int NDBT_ALLOCHANDLE_HDBC()
{

      SQLHENV     henv;
      SQLHDBC     hdbc;

      /*****************************HDBC Handle*****************************/
      SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

      SQLHDBC_check = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

      if (SQLHDBC_check == -1) {
      return NDBT_ProgramExit(NDBT_FAILED);
      }

      if (SQLHDBC_check == 0) {
      return 0;
      }

      SQLHENVFREE_check = SQLFreeHandle(SQL_HANDLE_ENV, henv);

      if (SQLHENVFREE_check == -1) {
	// Deallocate any allocated memory, if it exists
	return(-1);
	//return NDBT_ProgramExit(NDBT_FAILED);
      }

      if (SQLHENVFREE_check == 0) {
      return 0;
      }
}




