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

static bool matchType(NdbDictionary::Column::Type, DBA_DataTypes_t);
static bool matchSize(NdbDictionary::Column::Type, unsigned, Size_t);
static int  computeChecksum(const DBA_Binding_t * bindings);

struct DBA__Array {
  int count;
  int data[1];

  bool exists(int value) const {
    for(int i = 0; i<count; i++)
      if(data[i] == value)
	return true;
    return false;
  }

  void insert(int value){
    data[count] = value;
    count++;
  }
};

/**
 * createBindings
 */
static 
DBA_Binding_t *
createBinding(const char* TableName, 
	      int NbCol,
	      const DBA_ColumnBinding_t ColsBindings[],
	      Size_t StructSz,
	      const NdbDictionary::Table * theTable, 
	      struct DBA__Array * keys,
	      struct DBA__Array * columns);

extern "C"
DBA_Binding_t *
DBA_CreateBinding( const char* TableName, 
		   int NbCol, 
		   const DBA_ColumnBinding_t ColsBindings[], 
		   Size_t StructSz ){

  NdbDictionary::Dictionary * dict = DBA__TheNdb->getDictionary();
  if(dict == 0){
    DBA__SetLatestError(DBA_NDB_ERROR, 0, 
			"Internal NDB error: No dictionary");
    return 0;
  }
  
  const NdbDictionary::Table * table = dict->getTable(TableName);
  if(table == 0){
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, 
			"No such table: %s", TableName);
    return 0;
  }
  
  /**
   * Keys/Columns in table
   */
  const int tabColumns = table->getNoOfColumns();
  const int tabKeys    = table->getNoOfPrimaryKeys();
  
  /**
   * Ok, ok... I alloc four bytes extra so what...
   */
  struct DBA__Array * keys    = (struct DBA__Array *)malloc
    (sizeof(struct DBA__Array)+tabKeys*sizeof(int));

  if(keys == 0){
    DBA__SetLatestError(DBA_ERROR, 0,
			"malloc(%d) failed", 
			sizeof(struct DBA__Array)+tabKeys*sizeof(int));
    return 0;
  }
  
  struct DBA__Array * columns = (struct DBA__Array *)malloc
    (sizeof(struct DBA__Array)+tabColumns*sizeof(int));
  
  if(columns == 0){
    DBA__SetLatestError(DBA_ERROR, 0,
			"malloc(%d) failed", 
			sizeof(struct DBA__Array)+tabColumns*sizeof(int));
    free(keys);
    return 0;
  }
  
  columns->count = 0;
  keys->count = 0;
  
  DBA_Binding_t * bindings = createBinding(TableName,
					   NbCol,
					   ColsBindings,
					   StructSz,
					   table, 
					   keys,
					   columns);
  
  for(int i = 0; i<tabColumns; i++){
    const NdbDictionary::Column * col = table->getColumn(i);
    if(col->getPrimaryKey()){
      if(!keys->exists(i)){
	DBA__SetLatestError(DBA_APPLICATION_ERROR, 0,
			    "Key column: %s not specified in binding",
			    col->getName());
	
	free(keys); free(columns);
	DBA_DestroyBinding(bindings);
	return 0;
      }
    }
  }
  
  free(keys); free(columns);
  
  DBA__ValidBinding(bindings);

  return bindings;
}

DBA_Binding_t *
createBinding(const char* TableName, 
	      int NbCol,
	      const DBA_ColumnBinding_t ColsBindings[],
	      Size_t StructSz,
	      const NdbDictionary::Table * table, 
	      struct DBA__Array * keys,
	      struct DBA__Array * columns){
  /**
   * Counters for this part of binding
   */
  int noOfKeys        = 0;
  int noOfColumns     = 0;
  int noOfSubBindings = 0;

  /**
   * Check names and types and sizes
   */
  for(int i = 0; i<NbCol; i++){
    if(ColsBindings[i].Ptr){
      /**
       * Pointer binding
       */
      noOfSubBindings ++;

      DBA_Binding_t * tmp = createBinding(TableName,
					  ColsBindings[i].Size,
					  ColsBindings[i].SubBinding,
					  StructSz,
					  table,
					  keys,
					  columns);
      DBA__ValidBinding(tmp);
      
      if(tmp == 0){
	// createBindings have already set latestError
	return 0;
      }
      
      DBA_DestroyBinding(tmp);
    } else {
      const NdbDictionary::Column * col = 
	table->getColumn(ColsBindings[i].Name);
      const Uint32 attrId = col->getColumnNo();

      if(col == 0){
	DBA__SetLatestError(DBA_APPLICATION_ERROR, 0,
			    "Unknown column: %s", ColsBindings[i].Name);
	return 0;
      }
      const NdbDictionary::Column::Type type = col->getType();
      if(!matchType(type, ColsBindings[i].DataType)){
	DBA_DEBUG("Incorrect type for: " << ColsBindings[i].Name);
	DBA_DEBUG("type: " << type);
	DBA_DEBUG("ColsBindings[i].DataType: " << ColsBindings[i].DataType);
	
	DBA__SetLatestError(DBA_APPLICATION_ERROR, 0,
			    "Incorrect type for column: %s", 
			    ColsBindings[i].Name);
	
	return 0;
      }

      if(!matchSize(type, col->getLength(), ColsBindings[i].Size)){
	DBA_DEBUG("Incorrect size for: " << ColsBindings[i].Name);
	DBA_DEBUG("type: " << type);
	DBA_DEBUG("length: " << col->getLength());
	DBA_DEBUG("ColsBindings[i].Size" << (Uint64)ColsBindings[i].Size);
	
	DBA__SetLatestError(DBA_APPLICATION_ERROR, 0,
			    "Incorrect size for column: %s", 
			    ColsBindings[i].Name);
	return 0;
      }
      
      if(col->getPrimaryKey()){
	noOfKeys++;
      } else {
	noOfColumns++;
      }
      
      /**
       * Check only in "validate" phase
       */
      if(columns != 0 && keys != 0){
	if(columns->exists(attrId) || keys->exists(attrId)){
	  DBA_DEBUG("Column bound multiple times: " << ColsBindings[i].Name);
	  
	  DBA__SetLatestError(DBA_APPLICATION_ERROR, 0,
			      "Column bound multiple times: %s",
			      ColsBindings[i].Name);
	  return 0;
	}
	
	if(col->getPrimaryKey()){
	  keys->insert(attrId);
	} else {
	  columns->insert(attrId);
	}
      }
    }
  }
  
  /**
   * Validation is all set
   */
  
  /**
   * Allocate memory
   */
  const int szOfStruct = 
    sizeof(DBA_Binding_t)
    + strlen(TableName) + 4
    + (2 * sizeof(int) * noOfKeys)
    + (2 * sizeof(int) * noOfColumns)
    + ((sizeof(struct DBA_Binding *) + sizeof(int)) * noOfSubBindings)
    - 4;
  
  DBA_Binding * ret = (DBA_Binding *)malloc(szOfStruct);
  if(ret == 0){
    DBA__SetLatestError(DBA_ERROR, 0,
			"malloc(%d) failed", szOfStruct);
    return 0;
  }
  
  for(int i = 0; i<DBA__MagicLength; i++)
    ret->magic[i] = DBA__TheMagic[i];
  
  ret->noOfKeys          = noOfKeys;
  ret->noOfColumns       = noOfColumns;
  ret->noOfSubBindings   = noOfSubBindings;

  ret->keyIds            = (int *)&(ret->data[0]);
  ret->keyOffsets        = ret->keyIds + noOfKeys;

  ret->columnIds         = ret->keyOffsets + noOfKeys;
  ret->columnOffsets     = ret->columnIds + noOfColumns;

  ret->subBindingOffsets = ret->columnOffsets + noOfColumns;
  ret->subBindings       = (DBA_Binding **)
    (ret->subBindingOffsets + noOfSubBindings);
  
  ret->tableName         = (char *)(ret->subBindings + noOfSubBindings);
  ret->structSz          = StructSz;
  ret->checkSum          = computeChecksum(ret);

  /**
   * Populate arrays
   */
  strcpy(ret->tableName, TableName);

  int k = 0;
  int c = 0;
  int p = 0;

  for(int i = 0; i<NbCol; i++){
    if(ColsBindings[i].Ptr){
      ret->subBindings[p] = createBinding(TableName,
					  ColsBindings[i].Size,
					  ColsBindings[i].SubBinding,
					  StructSz,
					  table, 
					  0, 
					  0);
      
      DBA__ValidBinding(ret->subBindings[p]);

      ret->subBindingOffsets[p] = ColsBindings[i].Offset;
      p++;
    } else {
      const NdbDictionary::Column * col = 
	table->getColumn(ColsBindings[i].Name);

      if(col->getPrimaryKey()){
	ret->keyIds[k]     = col->getColumnNo();
	ret->keyOffsets[k] = ColsBindings[i].Offset;
	k++;
      } else {
	ret->columnIds[c]     = col->getColumnNo();
	ret->columnOffsets[c] = ColsBindings[i].Offset;
	c++;
      }
    }  
  }
  
  return ret;
}


extern "C"
DBA_Error_t 
DBA_DestroyBinding( DBA_Binding_t* Binding ){

  for(int i = 0; i<Binding->noOfSubBindings; i++)
    DBA_DestroyBinding(Binding->subBindings[i]);
  
  free(Binding);
  
  return DBA_NO_ERROR;
}

static
bool
matchType(NdbDictionary::Column::Type t1, DBA_DataTypes_t t2){
  for(int i = 0; i<DBA__NoOfMappings; i++)
    if(DBA__DataTypesMappings[i].newtonType == t2 &&
       DBA__DataTypesMappings[i].ndbType    == t1)
      return true;
  return false;
}

static
bool
matchSize(NdbDictionary::Column::Type t, unsigned b, Size_t s) {
  switch(t){
  case NdbDictionary::Column::Int:
  case NdbDictionary::Column::Unsigned:
  case NdbDictionary::Column::Float:
    return (4 * b) == s;
  case NdbDictionary::Column::Bigint:
  case NdbDictionary::Column::Bigunsigned:
  case NdbDictionary::Column::Double:
    return (8 * b) == s;
  case NdbDictionary::Column::Decimal:
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Binary:
    return (1 * b) == s;
  case NdbDictionary::Column::Varchar:
  case NdbDictionary::Column::Varbinary:
  case NdbDictionary::Column::Datetime:
  case NdbDictionary::Column::Timespec:
  case NdbDictionary::Column::Blob:
  case NdbDictionary::Column::Tinyint:
  case NdbDictionary::Column::Tinyunsigned:
  case NdbDictionary::Column::Smallint:
  case NdbDictionary::Column::Smallunsigned:
  case NdbDictionary::Column::Mediumint:
  case NdbDictionary::Column::Mediumunsigned:
  case NdbDictionary::Column::Undefined:
    return false;
  }
  return false;
}

bool
DBA__ValidBinding(const DBA_Binding_t * bindings){
  if(bindings == 0){
    DBA_DEBUG("Null pointer passed to validBinding");
    return false;
  }
  
  for(int i = 0; i<DBA__MagicLength; i++)
    if(bindings->magic[i] != DBA__TheMagic[i]){
      DBA_DEBUG("Invalid magic in validBinding");
      return false;
    }
  
  const int cs = computeChecksum(bindings);
  if(cs != bindings->checkSum){
    DBA_DEBUG("Invalid checksum in validBinding");
    DBA_DEBUG("cs = " << cs << " b->cs= " << bindings->checkSum);
    return false;
  }

  return true;
}

bool
DBA__ValidBindings(const DBA_Binding_t * const * pBindings, int n){
  for(int i = 0; i<n; i++)
    if(!DBA__ValidBinding(pBindings[i]))
      return false;
  return true;
}

/**
 * Note: currently only checksum "static" part of struct
 */
static
int
computeChecksum(const DBA_Binding_t * bindings){
  int sum = 0;
  int pos = 0;
  const char * ptr = ((const char *)bindings)+DBA__MagicLength+sizeof(int);
  const int sz = sizeof(DBA_Binding_t) - DBA__MagicLength - sizeof(int) - 4;

  for(int i = 0; i<sz; i++){
    sum += ((int)ptr[i]) << pos;
    pos += 8;
    if(pos == 32)
      pos = 0;
  }
  
  return sum;
}

int
DBA__GetStructSize(const DBA_Binding_t * bind){
  if(!DBA__ValidBinding(bind))
    return 0;
  return bind->structSz;
}
