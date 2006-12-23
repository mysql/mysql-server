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
extern "C" {
#include <dba.h>
}

#include "common.hpp"

#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NdbMain.h>

static const
DBA_ColumnDesc_t ColDesc[] = {
  { "emp_no",     DBA_INT,  PCN_SIZE_OF( Employee, EmpNo ),      PCN_TRUE },
  { "first_name", DBA_CHAR, PCN_SIZE_OF( Employee, FirstName ),  PCN_FALSE },
  { "last_name",  DBA_CHAR, PCN_SIZE_OF( Employee, LastName ),   PCN_FALSE },
  { "street_name",DBA_CHAR, PCN_SIZE_OF( Address,  StreetName ), PCN_FALSE },
  { "street_no",  DBA_INT,  PCN_SIZE_OF( Address,  StreetNo ),   PCN_FALSE },
  { "city",       DBA_CHAR, PCN_SIZE_OF( Address,  City ),       PCN_FALSE }
};
static const int NbCol = 6;

static const
DBA_ColumnBinding_t AddBindings[] = {
  //DBA_BINDING( "emp_no",      DBA_INT,  Address, EmpNo ),
  DBA_BINDING( "street_name", DBA_CHAR, Address, StreetName ),
  DBA_BINDING( "street_no",   DBA_INT,  Address, StreetNo ),  
  DBA_BINDING( "city",        DBA_CHAR, Address, City )
};

static const 
int AddBindingRows = sizeof(AddBindings)/sizeof(DBA_ColumnBinding_t);

static const
DBA_ColumnBinding_t EmpBindings[] = {
  DBA_BINDING( "emp_no",      DBA_INT,  Employee, EmpNo ),
  DBA_BINDING( "last_name",   DBA_CHAR, Employee, LastName ),
  DBA_BINDING( "first_name",  DBA_CHAR, Employee, FirstName),
  DBA_BINDING_PTR(Employee, EmployeeAddress, AddBindings, AddBindingRows)
};
static const 
int EmpBindingRows = sizeof(EmpBindings)/sizeof(DBA_ColumnBinding_t);

static DBA_Binding_t * Bind;

static const int Rows = 6;

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

static
Employee_t EMP_TABLE_DATA[] = {
  { 1242, "Joe",     "Dalton" ,   &ADD_TABLE_DATA[0] },
  { 123,  "Lucky",   "Luke"   ,   &ADD_TABLE_DATA[1] },
  { 456,  "Averell", "Dalton" ,   &ADD_TABLE_DATA[2] },
  { 8976, "Gaston",  "Lagaffe",   &ADD_TABLE_DATA[3] },
  { 1122, "Jolly",   "Jumper" ,   &ADD_TABLE_DATA[4] },
  { 3211, "Leffe",   "Pagrotsky", &ADD_TABLE_DATA[5] },
};

static
Employee_t EMP_TABLE_DATA_READ[] = {
  { 1242, "", "", &ADD_TABLE_DATA_READ[0] },
  { 123,  "", "", &ADD_TABLE_DATA_READ[1] },
  { 456,  "", "", &ADD_TABLE_DATA_READ[2] },
  { 8976, "", "", &ADD_TABLE_DATA_READ[3] },
  { 1122, "", "", &ADD_TABLE_DATA_READ[4] },
  { 3211, "", "", &ADD_TABLE_DATA_READ[5] }
};

static const char TABLE[] = "employee_address";

static
void 
DbCreate(void){

  ndbout << "Opening database" << endl;
  require( DBA_Open() == DBA_NO_ERROR );
  
  ndbout << "Creating tables" << endl;
  require( DBA_CreateTable( TABLE, NbCol, ColDesc ) == DBA_NO_ERROR );
  
  ndbout << "Checking for table existance" << endl;
  require( DBA_TableExists( TABLE ) );
} 

static
void
CreateBindings(void){
  ndbout << "Creating bindings" << endl;

  Bind = DBA_CreateBinding(TABLE, 
			   EmpBindingRows,
			   EmpBindings,
			   sizeof(Employee_t) );
  
  require(Bind != 0);
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
  DBA_ArrayInsertRows(Bind, EMP_TABLE_DATA, Rows-2, insertCallback);
  NdbSleep_SecSleep(1);
  DBA_ArrayReadRows  (Bind, EMP_TABLE_DATA_READ, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows-2, ADD_TABLE_DATA_READ);
  
  // Basic update
  AlterRows  (EMP_TABLE_DATA, Rows-2);
  AlterRows  (ADD_TABLE_DATA, Rows-2);
  DBA_ArrayUpdateRows(Bind, EMP_TABLE_DATA, Rows-2, updateCallback);
  NdbSleep_SecSleep(1);
  DBA_ArrayReadRows  (Bind, EMP_TABLE_DATA_READ, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows-2, ADD_TABLE_DATA_READ);
  
  // Basic write
  AlterRows  (EMP_TABLE_DATA, Rows);
  AlterRows  (ADD_TABLE_DATA, Rows);
  DBA_ArrayWriteRows(Bind, EMP_TABLE_DATA, Rows, writeCallback);
  NdbSleep_SecSleep(1);
  DBA_ArrayReadRows (Bind, EMP_TABLE_DATA_READ, Rows, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows, ADD_TABLE_DATA_READ);
  
  // Basic delete
  DBA_ArrayDeleteRows(Bind, EMP_TABLE_DATA, Rows, deleteCallback);
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
  DBA_InsertRows(Bind, EMP_TABLE_DATA2, Rows-2, insertCallback);
  NdbSleep_SecSleep(1);
  DBA_ReadRows  (Bind, EMP_TABLE_DATA_READ2, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows-2, ADD_TABLE_DATA_READ);
    
  // Basic update
  AlterRows  (ADD_TABLE_DATA, Rows-2);
  AlterRows  (EMP_TABLE_DATA, Rows-2);
  DBA_UpdateRows(Bind, EMP_TABLE_DATA2, Rows-2, updateCallback);
  NdbSleep_SecSleep(1);
  DBA_ReadRows  (Bind, EMP_TABLE_DATA_READ2, Rows-2, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows-2, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows-2, ADD_TABLE_DATA_READ);

    // Basic write
  AlterRows  (ADD_TABLE_DATA, Rows);
  AlterRows  (EMP_TABLE_DATA, Rows);
  DBA_WriteRows(Bind, EMP_TABLE_DATA2, Rows, writeCallback);
  NdbSleep_SecSleep(1);
  DBA_ReadRows (Bind, EMP_TABLE_DATA_READ2, Rows, readCallback);
  NdbSleep_SecSleep(1);
  CompareRows(EMP_TABLE_DATA, Rows, EMP_TABLE_DATA_READ);
  CompareRows(ADD_TABLE_DATA, Rows, ADD_TABLE_DATA_READ);
  
    // Basic delete
  DBA_DeleteRows(Bind, EMP_TABLE_DATA2, Rows, deleteCallback);
  NdbSleep_SecSleep(1);
}

/*---------------------------------------------------------------------------*/
NDB_COMMAND(newton_pb, "newton_pb", 
	    "newton_pb", "newton_pb", 65535){

  DbCreate();
  CreateBindings();

  BasicArray();
  BasicPtr();
  
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

