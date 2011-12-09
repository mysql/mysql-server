/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/



/****** THIS LINE IS 80 CHARACTERS WIDE - DO *NOT* EXCEED 80 CHARACTERS! ****/
extern "C" {
#include <dba.h>
}

#include "common.hpp"

#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbMain.h>

static const
DBA_ColumnDesc_t EmpColDesc[] = {
  { "emp_no",     DBA_INT,  PCN_SIZE_OF( Employee, EmpNo ),     PCN_TRUE },
  { "first_name", DBA_CHAR, PCN_SIZE_OF( Employee, FirstName ), PCN_FALSE },
  { "last_name",  DBA_CHAR, PCN_SIZE_OF( Employee, LastName ),  PCN_FALSE }
};

static const
DBA_ColumnDesc_t AddColDesc[] = {
  { "emp_no",      DBA_INT,  PCN_SIZE_OF( Address, EmpNo ),      PCN_TRUE },
  { "street_name", DBA_CHAR, PCN_SIZE_OF( Address, StreetName ), PCN_FALSE},
  { "street_no",   DBA_INT,  PCN_SIZE_OF( Address, StreetNo ),   PCN_FALSE},
  { "city",        DBA_CHAR, PCN_SIZE_OF( Address, City ),       PCN_FALSE}
} ;

static const
DBA_ColumnBinding_t EmpBindings[] = {
  DBA_BINDING( "emp_no",     DBA_INT,  Employee, EmpNo ),
  DBA_BINDING( "last_name",  DBA_CHAR, Employee, LastName ),
  DBA_BINDING( "first_name", DBA_CHAR, Employee, FirstName)
};

static const
DBA_ColumnBinding_t AddBindings[] = {
  DBA_BINDING( "emp_no",      DBA_INT,  Address, EmpNo ),
  DBA_BINDING( "street_name", DBA_CHAR, Address, StreetName ),
  DBA_BINDING( "street_no",   DBA_INT,  Address, StreetNo ),  
  DBA_BINDING( "city",        DBA_CHAR, Address, City )
};

static DBA_Binding_t * EmpB;
static DBA_Binding_t * AddB;

static const int Rows = 6;

static
Employee_t EMP_TABLE_DATA[] = {
  { 1242, "Joe",     "Dalton" },
  { 123,  "Lucky",   "Luke" },
  { 456,  "Averell", "Dalton" },
  { 8976, "Gaston",  "Lagaffe" },
  { 1122, "Jolly",   "Jumper" },
  { 3211, "Leffe",   "Pagrotsky" }
};

static
Employee_t EMP_TABLE_DATA_READ[] = {
  { 1242, "", "" },
  { 123,  "", "" },
  { 456,  "", "" },
  { 8976, "", "" },
  { 1122, "", "" },
  { 3211, "", "" }
};

static
Address_t ADD_TABLE_DATA[] = {
  { 1242, "Lonesome Street", 12, "Crime Town" },
  { 123,  "Pistol Road",     13, "Fort Mount" },
  { 456,  "Banking Blv.",    43, "Las Vegas"  },
  { 8976, "ChancylleZee",    54, "Paris" },
  { 1122, "Lucky",          111, "Wild West"  },
  { 3211, "Parlament St.",   11, "Stockholm" }
};

static
Address_t ADD_TABLE_DATA_READ[] = {
  { 1242, "", 0, "" },
  { 123,  "", 0, "" },
  { 456,  "", 0, "" },
  { 8976, "", 0, "" },
  { 1122, "", 0, "" },
  { 3211, "", 0, "" }
};

static const char EMP_TABLE[] = "employees";
static const char ADD_TABLE[] = "addresses";

static const int EmpNbCol = 3;
static const int AddNbCol = 4;

static
void 
DbCreate(void){

  ndbout << "Opening database" << endl;
  require( DBA_Open() == DBA_NO_ERROR );
  
  ndbout << "Creating tables" << endl;
  require( DBA_CreateTable( EMP_TABLE, EmpNbCol, EmpColDesc ) == DBA_NO_ERROR );
  require( DBA_CreateTable( ADD_TABLE, AddNbCol, AddColDesc ) == DBA_NO_ERROR );
  
  ndbout << "Checking for table existance" << endl;
  require( DBA_TableExists( EMP_TABLE ) );
  require( DBA_TableExists( ADD_TABLE ) );
} 

static
void
CreateBindings(void){
  ndbout << "Creating bindings" << endl;

  EmpB = DBA_CreateBinding(EMP_TABLE, 
			   EmpNbCol,
			   EmpBindings,
			   sizeof(Employee_t) );
  require(EmpB != 0);

  AddB = DBA_CreateBinding(ADD_TABLE, 
			   AddNbCol,
			   AddBindings,
			   sizeof(Address_t) );
  require(AddB != 0);
}

extern "C" {
  static void insertCallback( DBA_ReqId_t, DBA_Error_t, DBA_ErrorCode_t );
  static void deleteCallback( DBA_ReqId_t, DBA_Error_t, DBA_ErrorCode_t );
  static void updateCallback( DBA_ReqId_t, DBA_Error_t, DBA_ErrorCode_t );
  static void readCallback  ( DBA_ReqId_t, DBA_Error_t, DBA_ErrorCode_t );
  static void writeCallback ( DBA_ReqId_t, DBA_Error_t, DBA_ErrorCode_t );
}

static
void BasicArray(){
  ndbout << "Testing basic array operations" << endl;
  
  // Basic insert
  DBA_ArrayInsertRows(EmpB, EMP_TABLE_DATA, Rows-2, insertCallback);
  NdbSleep_SecSleep(1);
  DBA_ArrayReadRows(EmpB, EMP_TABLE_DATA_READ, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  
  // Basic update
  AlterRows(EMP_TABLE_DATA, Rows-2);
  DBA_ArrayUpdateRows(EmpB, EMP_TABLE_DATA, Rows-2, updateCallback);
  NdbSleep_SecSleep(1);
  DBA_ArrayReadRows(EmpB, EMP_TABLE_DATA_READ, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  
  // Basic write
  AlterRows(EMP_TABLE_DATA, Rows);
  DBA_ArrayWriteRows(EmpB, EMP_TABLE_DATA, Rows, writeCallback);
  NdbSleep_SecSleep(1);
  DBA_ArrayReadRows(EmpB, EMP_TABLE_DATA_READ, Rows, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows, EMP_TABLE_DATA_READ);
  
  // Basic delete
  DBA_ArrayDeleteRows(EmpB, EMP_TABLE_DATA, Rows, deleteCallback);
  NdbSleep_SecSleep(1);
}

static
void Multi(){
  ndbout << "Testing multi operations" << endl;
  
  const int R2 = Rows + Rows;

  DBA_Binding_t * Bindings[R2];
  void * DATA[R2];
  void * DATA_READ[R2];
  for(int i = 0; i<Rows; i++){
    Bindings[2*i]   = EmpB;
    Bindings[2*i+1] = AddB;
    
    DATA[2*i]   = &EMP_TABLE_DATA[i];
    DATA[2*i+1] = &ADD_TABLE_DATA[i];

    DATA_READ[2*i]   = &EMP_TABLE_DATA_READ[i];
    DATA_READ[2*i+1] = &ADD_TABLE_DATA_READ[i];
  }
  
  // Basic insert
  DBA_MultiInsertRow(Bindings, DATA, R2-4, insertCallback);
  NdbSleep_SecSleep(1);
  DBA_MultiReadRow  (Bindings, DATA_READ, R2-4, readCallback);
  NdbSleep_SecSleep(1);

  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows-2, ADD_TABLE_DATA_READ);
  
  // Basic update
  AlterRows(EMP_TABLE_DATA, Rows-2);
  AlterRows(ADD_TABLE_DATA, Rows-2);
  DBA_MultiUpdateRow(Bindings, DATA, R2-4, updateCallback);
  NdbSleep_SecSleep(1);
  DBA_MultiReadRow  (Bindings, DATA_READ, R2-4, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows-2, ADD_TABLE_DATA_READ);
  
  // Basic write
  AlterRows(EMP_TABLE_DATA, Rows);
  AlterRows(ADD_TABLE_DATA, Rows);
  DBA_MultiWriteRow(Bindings, DATA, R2, writeCallback);
  NdbSleep_SecSleep(1);
  DBA_MultiReadRow (Bindings, DATA_READ, R2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows, ADD_TABLE_DATA_READ);
  
  // Basic delete
  DBA_MultiDeleteRow(Bindings, DATA, R2, deleteCallback);
  NdbSleep_SecSleep(1);
}

static
void BasicPtr(){
  ndbout << "Testing array of pointer operations" << endl;
  Employee_t * EmpData[Rows];
  Employee_t * EmpDataRead[Rows];
  for(int i = 0; i<Rows; i++){
    EmpData[i]     = &EMP_TABLE_DATA[i];
    EmpDataRead[i] = &EMP_TABLE_DATA_READ[i];
  }
    
  void * const * EMP_TABLE_DATA2      = (void * const *)EmpData;
  void * const * EMP_TABLE_DATA_READ2 = (void * const *)EmpDataRead;
    
  // Basic insert
  DBA_InsertRows(EmpB, EMP_TABLE_DATA2, Rows-2, insertCallback);
  NdbSleep_SecSleep(1);
  DBA_ReadRows  (EmpB, EMP_TABLE_DATA_READ2, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
    
  // Basic update
  AlterRows(EMP_TABLE_DATA, Rows-2);
  DBA_UpdateRows(EmpB, EMP_TABLE_DATA2, Rows-2, updateCallback);
  NdbSleep_SecSleep(1);
  DBA_ReadRows  (EmpB, EMP_TABLE_DATA_READ2, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
    
    // Basic write
  AlterRows  (EMP_TABLE_DATA, Rows);
  DBA_WriteRows(EmpB, EMP_TABLE_DATA2, Rows, writeCallback);
  NdbSleep_SecSleep(1);
  DBA_ReadRows (EmpB, EMP_TABLE_DATA_READ2, Rows, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows, EMP_TABLE_DATA_READ);
    
    // Basic delete
  DBA_DeleteRows(EmpB, EMP_TABLE_DATA2, Rows, deleteCallback);
  NdbSleep_SecSleep(1);
}

/*---------------------------------------------------------------------------*/
NDB_COMMAND(newton_basic, "newton_basic", 
	    "newton_basic", "newton_basic", 65535){

  DbCreate();
  CreateBindings();

  BasicArray();
  BasicPtr();
  Multi();
  
  DBA_Close();

  return 0;
}



/**
 *  callbackStatusCheck  checks whether or not the operation succeeded
 */
void
callbackStatusCheck( DBA_Error_t status, const char* operation) {
  ndbout_c("%s: %d", operation, status);
}

void insertCallback( DBA_ReqId_t, DBA_Error_t s, DBA_ErrorCode_t ){
  callbackStatusCheck(s, "insert");
}
void deleteCallback( DBA_ReqId_t, DBA_Error_t s, DBA_ErrorCode_t ){
  callbackStatusCheck(s, "delete");
}
void updateCallback( DBA_ReqId_t, DBA_Error_t s, DBA_ErrorCode_t ){
  callbackStatusCheck(s, "update");
}
void readCallback  ( DBA_ReqId_t, DBA_Error_t s, DBA_ErrorCode_t ){
  callbackStatusCheck(s, "read");
}
void writeCallback  ( DBA_ReqId_t, DBA_Error_t s, DBA_ErrorCode_t ){
  callbackStatusCheck(s, "write");
}

