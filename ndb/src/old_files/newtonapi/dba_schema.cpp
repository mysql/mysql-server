/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "dba_internal.hpp"
#include "NdbSchemaCon.hpp"

static bool getNdbAttr(DBA_DataTypes_t,
		       Size_t,
		       int * attrSize,
		       int * arraySize,
		       AttrType * attrType);

extern "C"
DBA_Error_t 
DBA_CreateTable(const char* TableName, 
		int NbColumns, 
		const DBA_ColumnDesc_t Columns[] ){
  
  if(DBA_TableExists(TableName))
    return DBA_NO_ERROR;
  
  NdbSchemaCon * schemaCon = NdbSchemaCon::startSchemaTrans(DBA__TheNdb);
  if(schemaCon == 0){
    DBA__SetLatestError(DBA_NDB_ERROR, 0,
			"Internal NDB error: No schema transaction");
    return DBA_NDB_ERROR;
  }
  
  NdbSchemaOp * schemaOp   = schemaCon->getNdbSchemaOp();	
  if(schemaOp == 0){    
    NdbSchemaCon::closeSchemaTrans(schemaCon);    
    DBA__SetLatestError(DBA_NDB_ERROR, 0,
			"Internal NDB error: No schema op");
    return DBA_NDB_ERROR;
  }

  if(schemaOp->createTable( TableName,
			    8, // Data Size
			    TupleKey,
			    2, // Index size
			    All,
			    6,
			    78,
			    80,
			    1,
			    false) == -1){
    NdbSchemaCon::closeSchemaTrans(schemaCon);    
    DBA__SetLatestError(DBA_NDB_ERROR, 0,
			"Internal NDB error: Create table failed");
    return DBA_NDB_ERROR;
  }
  
  for (int i = 0; i < NbColumns; i++){
    int attrSize;
    int arraySize;
    AttrType attrType;
    
    if(!getNdbAttr(Columns[i].DataType, Columns[i].Size,
		   &attrSize,
		   &arraySize,
		   &attrType)){
      NdbSchemaCon::closeSchemaTrans(schemaCon);    
      DBA__SetLatestError(DBA_APPLICATION_ERROR, 0,
			  "Invalid datatype/size combination");
      return DBA_APPLICATION_ERROR;
    }
    
    if(schemaOp->createAttribute( Columns[i].Name,
				  Columns[i].IsKey ? TupleKey : NoKey,
				  attrSize,
				  arraySize,
				  attrType) == -1){
      NdbSchemaCon::closeSchemaTrans(schemaCon);    
      DBA__SetLatestError(DBA_NDB_ERROR, 0,
			  "Internal NDB error: Create attribute failed");
      return DBA_NDB_ERROR;
    }
  }
  
  if(schemaCon->execute() == -1){
    NdbSchemaCon::closeSchemaTrans(schemaCon);    
    DBA__SetLatestError(DBA_NDB_ERROR, 0,
			"Internal NDB error: Execute schema failed");
    return DBA_NDB_ERROR;
  }
  
  NdbSchemaCon::closeSchemaTrans(schemaCon);    
    
  return DBA_NO_ERROR;
}

DBA_Error_t 
DBA_DropTable( char* TableName ){
  return DBA_NOT_IMPLEMENTED;
}

Boolean_t 
DBA_TableExists( const char* TableName ){
  NdbDictionary::Dictionary * dict = DBA__TheNdb->getDictionary();
  if(dict == 0){
    return 0;
  }
  
  const NdbDictionary::Table * tab = dict->getTable(TableName);
  if(tab == 0){
    return 0;
  }
  return 1;
}

static
bool 
getNdbAttr(DBA_DataTypes_t type,
	   Size_t size,
	   int * attrSize,
	   int * arraySize,
	   AttrType * attrType) {
  
  if(type == DBA_CHAR){
    * attrType = String;
    * attrSize  = 8;
    * arraySize = size;
    return true;
  }
  
  * attrType = Signed;
  if((size % 4) == 0){
    * attrSize  = 32;
    * arraySize = size / 4;
    return true;
  }
  
  * attrSize  = 8;
  * arraySize = size;
  
  return true;
}
