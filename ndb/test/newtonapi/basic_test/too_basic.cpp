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



/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/

#include <ndb_global.h>
#include <NdbOut.hpp>

//#include <cfg/cfg_db.h>
//#include <init/init_start_restart.h>
//#include "pcn_types.h"
//#include <testing/testing.h>

extern "C" {
#include <cfg_db.h>
}

typedef struct Employee {

    UInt32_t   EmpNo;
    char       FirstName[22];
    char       LastName[22];

} Employee_t;
#define CHECK_DB_CALL( Call ) \
   CheckDbCall( Call, #Call, __FILE__, __LINE__ )



/* --- Exported functions --- */

/*---------------------------------------------------------------------------*/
int main() {


  char EMP_TABLE_NAME[] = "employees";
  
  Employee_t t;
  
  CFG_DbColumnDesc_t   EmpColDesc[] = {
    { "first_name", CFG_DB_CHAR, PCN_SIZE_OF( Employee, FirstName ), 
      PCN_FALSE },
    { "emp_no", CFG_DB_INT, PCN_SIZE_OF( Employee, EmpNo ), PCN_TRUE },
    { "last_name", CFG_DB_CHAR, PCN_SIZE_OF( Employee, LastName ), 
      PCN_FALSE },
  };
  
  int EmpNbCol = 3;
  
  
  
  CFG_DbColumnBinding_t ColBindings[] = {
    CFG_DB_BINDING( "last_name", CFG_DB_CHAR, Employee, LastName ),
    CFG_DB_BINDING( "emp_no", CFG_DB_INT, Employee, EmpNo ),
    CFG_DB_BINDING( "first_name", CFG_DB_CHAR, Employee, FirstName)
  };
  
  
  Employee_t EMP_TABLE_DATA[] = {
    { 1242, "Joe", "Dalton" },
    { 123, "Lucky", "Luke" },
    { 456, "Averell", "Dalton" },
    { 8976, "Gaston", "Lagaffe" }
  };
  
  
  char* DbName;
  
  DbName = NULL;
  

    // On Linux: will destroy the table to start from a clean slate. 
     
    CFG_DbDestroy();
    CFG_DbOpen( &DbName ) ;
    CFG_DbCreateTable( EMP_TABLE_NAME,
		       EmpNbCol, EmpColDesc );
    
    CFG_DbTableExists( EMP_TABLE_NAME );

    //#ifndef CELLO_PLATFORM
    //CHECK_DB_CALL( CFG_DbDumpSchema( stdout ) );
    //#endif 

    CFG_DbClose();
    //    INIT_StopSystem();

} 

 
