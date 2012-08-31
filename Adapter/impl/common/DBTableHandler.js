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
/*global udebug, debug, module, exports */

"use strict";

var assert = require("assert");


function TableMapping(dbtable) { 
  udebug.log("DBTableHandler TableMapping constructor " + dbtable.name);
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

TableMapping.prototype.merge = function(apiMapping) {
  udebug.log("DBTableHandler TableMapping merge");
  var x;
  
  for(x in apiMapping) {
    if(apiMapping.hasOwnProperty(x)) {
      switch(x) {
        case "autoIncrementBatchSize":
          this[x] = apiMapping[x];
          break;
        default:
          break;
      }
    }
  }
};

/* Constructor and Prototype for FieldMapping.
   Copied from the API Documentation for Annotations
*/
function FieldMapping(dbconnpool, dbcolumn) {
  udebug.log("DBTableHandler FieldMapping constructor: " + dbcolumn.name);
  this.columnName = dbcolumn.name;
  this.columnNumber = dbcolumn.columnNumber;
  this.converter = dbconnpool.getConverter(dbcolumn);
}

FieldMapping.prototype = {
  fieldName     :  ""     ,  // Name of the field in the domain object
  columnName    :  ""     ,  // Column name where this field is stored  
  columnNumber  :  0      ,  // Column number in table 
  actionOnNull  :  "NONE" ,  // One of NONE, ERROR, or DEFAULT
  notPersistent : false   ,  // Boolean TRUE if this field should *not* be stored
  converter     :  {}        // Converter class to use with this field  
};

FieldMapping.prototype.merge = function(mappedField) {
  udebug.log("DBTableHandler FieldMapping merge");
  var x;

  for(x in mappedField) {
    if(mappedField.hasOwnProperty(x)) {
      switch(x) {
        case "actionOnNull":
        case "converter":
          this[x] = mappedField[x];
          break;
        default:
          break;
      }
    }
  }
};


function getColumnByName(dbTable, colName) {
  udebug.log("DBTableHandler getColumnByName " + colName);
  var i, col;
  
  for(i = 0 ; i < dbTable.columns.length ; i++) {
    col = dbTable.columns[i];
    if(col.name === colName) {
      return col;
    }
  }
  udebug.log("DBTableHandler getColumnByName " + colName + " NOT FOUND.");
  return null;
}


function createDefaultMapping(dbconnpool, dbtable) {
  udebug.log("DBTableHandler createDefaultMapping for table " + dbtable.name);
  var mapping = new TableMapping(dbtable);
  var i;
  for(i = 0 ; i < dbtable.columns.length ; i++) {
    mapping.fields[i] = new FieldMapping(dbconnpool, dbtable.columns[i]);
    mapping.fields[i].fieldName = dbtable.columns[i].name;
  }
  return mapping;
}


// TODO: Figure out error handling if the API Mapping is invalid

function resolveApiMapping(connPool, dbTable, apiMapping) {
  udebug.log("DBTableHandler resolveApiMapping for table " + dbTable.name);
  var mapping, field, apiField, col, i;

  mapping = new TableMapping(dbTable);
  mapping.isDefaultMapping = false;
  mapping.merge(apiMapping);
    
  for(i = 0 ; i < apiMapping.fields.length ; i++) {
    apiField = apiMapping.fields[i]; 
    if(! apiField.notPersistent) {
      col = getColumnByName(dbTable, field.columnName);
      /* TODO: check for null */
      field = new FieldMapping(connPool, col);
      field.merge(apiField);
      mapping.fields.push(field);      
    }
  }
  
  return mapping;
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
function DBTableHandler(dbconnpool, dbtable, tablemapping) {
  udebug.log("DBTableHandler constructor");
  assert(arguments.length === 3);
  var i;
  var f;
  if(tablemapping === null) {     // Create a default mapping
    this.mapping = createDefaultMapping(dbconnpool, dbtable);
  }
  else {                                   // Resolve the API Mapping
    this.mapping = resolveApiMapping(dbconnpool, dbtable, tablemapping);
  }
   
  this.connectionPool = dbconnpool;
  this.dbTable = dbtable;

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
  udebug.log("DBTableHandler new completed");
}

var proto = {
  connectionPool         : {},
  dbTable                : {},
  mapping                : {},
  newObjectPrototype     : {},
  fieldNameMap           : {},
  columnNameToFieldMap   : {},
  columnNumberToFieldMap : []
};


DBTableHandler.prototype = proto;     // Connect prototype to constructor


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


/* DBIndex chooseIndex(dbTableHandler, keys) 
 Returns the index to use as an access path.
 From API Context.find():
   * The parameter "keys" may be of any type. Keys must uniquely identify
   * a single row in the database. If keys is a simple type
   * (number or string), then the parameter type must be the 
   * same type as or compatible with the primary key type of the mapped object.
   * Otherwise, properties are taken
   * from the parameter and matched against property names in the
   * mapping.
*/
function chooseIndex(self, keys) {
debugger;
  var idxs = self.dbTable.indexes;
  var keyFields;
  var i, j, f, nmatches;
  
  if(typeof keys === 'number' || typeof keys === 'string') {
    if(idxs[0].columnNumbers.length === 1) {
      return idxs[0];   // primary key
    }
  }
  else {
    /* Keys is an object */ 
     keyFields = Object.keys(keys);

    /* First look for a unique index.  All columns must match. */
    for(i = 0 ; i < idxs.length ; i++) {
debugger;
      if(idxs[i].isUnique && idxs[i].columnNumbers.length === keyFields.length) {
         // Each key field resolves to a column, which must be in the index
         nmatches = 0;
         for(j = 0 ; j < keyFields.length ; j++) {
          f = self.fieldNameMap[keyFields[j]];
          if(idxs[i].columnNumbers.indexOf(f.columnNumber) != -1) {
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
        // FIXME: This code may be incorrect
        if(self.fieldNameMap[keys[keyFields[0]]]) {
          return idxs[i];  // this is an ordered index scan
        }
      }
    }
  }

  return null;
}

/* Return the property of obj corresponding to fieldNumber */
proto.get = function(obj, fieldNumber) { 
  var f = this.mapping.fields[fieldNumber];
  udebug.log("DBTableHandler get " + fieldNumber +" "+ f.fieldName);
  return obj[f.fieldName];
};

/* Set field to value */
proto.set = function(obj, fieldNumber, value) {
  var f = this.mapping.fields[fieldNumber];
  obj[f.fieldName] = value;
};

proto.writeFieldToBuffer = function(obj, fieldNumber, buffer, offset) {
  var f = this.mapping.fields[fieldNumber];
  udebug.log("DBTableHandler writeFieldToBuffer @"+ offset);
  return f.converter.writeToBuffer(obj[f.fieldName], buffer, offset);
};

proto.writeFieldToString = function(obj, fieldNumber, string) {
  var f = this.mapping.fields[fieldNumber];
  return f.converter.writeToString(obj[f.fieldName], string);
};

/* Sets field in obj */
proto.readBufferToField = function(obj, fieldNumber, buffer, offset) {
  var f = this.mapping.fields[fieldNumber];
  obj[f.fieldName] = f.converter.readFromBuffer(buffer, offset);
};

/* Sets field in obj */
proto.readStringToField = function(obj, fieldNumber, string) {
  var f = this.mapping.fields[fieldNumber];
  obj[f.fieldName] = f.converter.readFromString(string);
};


/* DBIndexHandler constructor */
function DBIndexHandler(dbTableHandler, dbIndex) {
  var i;

  this.tableHandler = dbTableHandler;
  this.dbIndex = dbIndex;
  
  for(i = 0 ; i < dbIndex.columnNumbers.length ; i++) {
    this.mapping.fields[i] = 
      dbTableHandler.columnNumberToFieldMap[dbIndex.columnNumbers[i]];
  }  
}

DBIndexHandler.prototype = {
  tableHandler        : null,
  dbIndex             : null,
  mapping             : { fields : [] },
  getMappedFieldCount : function() { return this.dbIndex.columnNumbers.length;},
  get                 : proto.get,                    // inherited
  writeFieldToBuffer  : proto.writeFieldToBuffer      // inherited
};


/* DBIndexHandler getIndexHandler(Object keys)
   IMMEDIATE

   Given an object containing keys as defined in API Context.find(),
   choose an index to use as an access path for the operation,
   and return a DBIndexHandler for that index.
*/
proto.getIndexHandler = function(keys) {
  var idx = chooseIndex(this, keys);
  var handler = null;
  if(idx) {
    handler = new DBIndexHandler(this, idx);
  }
  return handler;
};

exports.DBTableHandler = DBTableHandler;
