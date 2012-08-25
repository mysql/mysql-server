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
   Copied from the API Documentation for Annotations
*/
var TableMapping = function(dbtable) { 
  this.name = dbtable.name;
  this.database = dbtable.database;
};

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
var FieldMapping = function(dbcolumn) {
  this.columnName = dbcolumn.name;
  this.columnNumber = dbcolumn.columnNumber;
};

FieldMapping.prototype = {
  fieldName     :  ""     ,  // Name of the field in the domain object
  columnName    :  ""     ,  // Column name where this field is stored  
  columnNumber  :  0      ,  // Column number in table 
  actionOnNull  :  "NONE" ,  // One of NONE, ERROR, or DEFAULT
  notPersistent : false   ,  // Boolean TRUE if this field should *not* be stored
  converter     :  {}        // Converter class to use with this field  
};


function createDefaultMapping(dbtable) {
  var mapping = TableMapping(dbtable);
  var i;
  for(i = 0 ; i < dbtable.columns.length ; i++) {
    mapping.fields[i] = new FieldMapping(dbtable.columns[i]);
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
  if(typeof tablemapping == 'null') {
    tablemapping = createDefaultMapping(dbtable);
  }
  
  this.dbTable = dbtable;
  this.mapping = tablemapping;
};

proto = {
  "dbTable"            : {},
  "mapping"            : {},
  "newObjectPrototype" : {}
};


exports.DBTableHandler.prototype = proto;
