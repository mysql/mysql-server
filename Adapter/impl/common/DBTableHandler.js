/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

/* Constructor and Prototype for TableMapping.
   Copied from the API Documentation for Annotations.
   FIXME Note that the prototypes (but not the constructors) could be moved
   into api/Annotations.js
*/

/* jslint --node --white --vars --plusplus */
/* global udebug, debug, module, exports */

"use strict";

function TableMapping(dbtable) { 
  this.name = dbtable.name;
  this.database = dbtable.database;
}

TableMapping.prototype = {
  name                   :  ""  ,  // Table Name
  database               :  ""  ,  // Database name
  autoIncrementBatchSize :  1   ,  // Auto-increment prefetch batch size
  isDefaultMapping       : true ,  // This mapping is a default mapping
  fields                 : []      // array of FieldMapping objects
};

/* Constructor and Prototype for FieldMapping.
   Copied from the API Documentation for Annotations
*/
function FieldMapping(dbcolumn) {
  this.columnName = dbcolumn.name;
  this.columnNumber = dbcolumn.columnNumber;
}

FieldMapping.prototype = {
  fieldName     :  ""     ,  // Name of the field in the domain object
  columnName    :  ""     ,  // Column name where this field is stored  
  columnNumber  :  0      ,  // Column number in table 
  actionOnNull  :  "NONE" ,  // One of NONE, ERROR, or DEFAULT
  notPersistent : false   ,  // Boolean TRUE if this field should *not* be stored
  converter     :  {}        // Converter class to use with this field  
};


function createDefaultMapping(dbtable) {
  var mapping = new TableMapping(dbtable);
  var i;
  for(i = 0 ; i < dbtable.columns.length ; i++) {
    mapping.fields[i] = new FieldMapping(dbtable.columns[i]);
    mapping.fields[i].fieldName = dbtable.columns[i].name;
  }
}


/* DBTableHandler() constructor
   IMMEDIATE

   Create a DBTableHandler for a table and a mapping.

   The DBTable may not be null.

   If the TableMapping is null, default mapping behavior will be used.
   Default mapping behavior is to:
     select all columns when reading
     use default converters for all data types
     perform no remapping between field names and column names
*/
exports.DBTableHandler = function (dbtable, tablemapping) {
  var i;
  var f;
  if(typeof tablemapping === 'null') {
    udebug.log("Creating default mapping for table " + dbtable.name);
    tablemapping = createDefaultMapping(dbtable);
  }
   
  this.dbTable = dbtable;
  this.mapping = tablemapping;

  /* Build Convenience Maps: 
       Only *persistent* fields are included in the maps
       Field Name to Field, Column Name to Field, Column Number To Field
  */
  for(i = 0 ; i < this.mapping.fields.length ; i++) {
    f = this.mapping.fields[i];
    if(! f.notPersistent) {
      this.fieldNameMap[f.fieldName] = f;  // Object
      this.columnNameToFieldMap[f.columnName] = f;  // Object 
      this.columnNumberToFieldMap[f.columnNumber] = f;   // Array
    }
  }
};

var proto = {
  "dbTable"             : {},
  "mapping"             : {},
  "newObjectPrototype"  : {},
  "fieldNameMap"        : {},
  "colNameToFieldMap"   : {},
  "colNumberToFieldMap" : []
};

/* DBTableHandler.setResultPrototype(Object proto_object)
   IMMEDIATE

   Declare that proto_object should be used as a prototype 
   when creating a results object for a row read from the database.
*/

proto.setResultPrototype = function(obj) {
  this.newObjectPrototype = obj;
};

/* registerFieldConverter(String fieldname, Converter converter)
  IMMEDIATE
  Register a converter for a field in a domain object 
*/
proto.registerFieldConverter = function(fieldName, converter) {
  var f = this.fieldNameMap[fieldName];
  if(f) {
    f.converter = converter;
  }  
};


/* registerColumnConverter(String columnName, Converter converter)
  IMMEDIATE
  Register a converter for a column in a table
*/
proto.registerColumnConverter = function(columnName, converter) {
  var f = this.colNameToFieldMap[columnName];
  if(f) {
    f.converter = converter;
  }
};

/* getMappedFieldCount()
   IMMEDIATE   
   Returns the number of fields mapped to columns in the table 
*/
proto.getMappedFieldCount = function() {
  return this.columnNumberToFieldMap.length;
};

/* allColumnsMapped()
   IMMEDIATE   
   Boolean: returns True if all columns are mapped
*/
proto.allColumnsMapped = function() {
  // Note that this.mapping.length cannot be used here
  return (this.dbTable.columns.length === this.columnNumberToFieldMap.length);
};

/* DBIndex chooseIndexForOperation(DBOperation op, Object keys)
   IMMEDIATE
   Given a DBOoperation and an object containing keys,
   return the index to use as an access path for the operation

From API Context.find():
   * The parameter "keys" may be of any type. Keys must uniquely identify
   * a single row in the database. If keys is a simple type
   * (number or string), then the parameter type must be the 
   * same type as or compatible with the primary key type of the mapped object.
   * Otherwise, properties are taken
   * from the parameter and matched against property names in the
   * mapping.

*/
proto.chooseIndex = function(keys) {
  var idxs = this.dbTable.indexes;
  var keyFields;
  var i, j, nmatches;
  
  if(typeof keys === 'number' || typeof keys === 'string') {
    if(idxs[0].length === 1) {
      return idxs[0];   // primary key
    }
  }
  else {
    /* Keys is an object */ 
     keyFields = Object.keys(keys);

    /* First look for a unique index.  All columns must match. */
    for(i = 0 ; i < idxs.length ; i++) {     
      if(idxs[i].isUnique && idxs[i].columnNumbers.length === keyFields.length) {
         nmatches = 0;
         for(j = 0 ; j < keyFields.length ; j++) {
          if(this.fieldNameMap[keys[keyFields[j]]]) {
            nmatches++;
          }
        }
        if(nmatches === keyFields.length) { 
          return idxs[i];   // bingo!
        }
      }    
    }

    /* Then look for an ordered index.  A prefix match is OK. */
    /* Return the first suitable index we find (which might not be the best) */
    for(i = 0 ; i < idxs.length ; i++) {
      if(idxs[i].isOrdered && idxs[i].columnNumbers.length >= keyFields.length) {
        if(this.fieldNameMap[keys[keyFields[0]]]) {
          return idxs[i];  // this is an ordered index scan
        }
      }
    }
  }

  return null;
};

  

exports.DBTableHandler.prototype = proto;
