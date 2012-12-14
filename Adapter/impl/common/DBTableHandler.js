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

/*global unified_debug, path, api_dir, api_doc_dir */

"use strict";

var assert = require("assert"),
    TableMappingDoc = require(path.join(api_doc_dir, "TableMapping")),
    FieldMappingDoc = require(path.join(api_doc_dir, "FieldMapping")),
    stats_module    = require(path.join(api_dir, "stats")),
    stats           = stats_module.getWriter("spi","common","DBTableHandler"),
    udebug          = unified_debug.getLogger("DBTableHander.js");

// forward declaration of DBIndexHandler to avoid lint issue
var DBIndexHandler;

/* A DBTableHandler (DBT) combines dictionary metadata with user annotations.  
   It manages setting and getting of columns based on the fields of a 
   user's domain object.  It can also choose an index access path by 
   comapring user-supplied key fields of a domain object with a table's indexes.
   
   These are the structural parts of a DBT: 
     * mapping, an API TableMapping, either created explicitly or by default.
     * A TableMetadata object, obtained from the data dictionary.
     * The stubFields - a set of FieldMappings created implicitly by default rules. 
     * An internal set of maps between Fields and Columns
     
    The mapping and TableMetadata are supplied as arguments to the 
    constructor, which creates the maps.
    
    Some terms: 
      column number: column order in table as supplied by DataDictionary
      field number: an arbitrary ordering of only the mapped fields 
*/

/* DBT prototype */
var proto = {
  dbTable                : {},    // TableMetadata 
  mapping                : {},    // API TableMapping from mapClass()
  newObjectConstructor   : null,  // constructorFunction from mapClass()  
  stubFields             : null,  // FieldMappings constructed by default rules

  fieldNameToFieldMap    : {},
  columnNumberToFieldMap : {},
  fieldNumberToColumnMap : {},
  fieldNumberToFieldMap  : {}
};

/* getColumnByName() is a utility function used in the building of maps.
*/
function getColumnByName(dbTable, colName) {
  udebug.log("getColumnByName", colName);
  var i, col;
  
  for(i = 0 ; i < dbTable.columns.length ; i++) {
    col = dbTable.columns[i];
    if(col.name === colName) {
      return col;
    }
  }
  udebug.log("getColumnByName", colName, "NOT FOUND.");
  return null;
}


/* DBTableHandler() constructor
   IMMEDIATE

   Create a DBTableHandler for a table and a mapping.

   The TableMetadata may not be null.

   If the TableMapping is null, default mapping behavior will be used.
   Default mapping behavior is to:
     select all columns when reading
     use default converters for all data types
     perform no remapping between field names and column names
*/
function DBTableHandler(dbtable, tablemapping, ctor) {
  udebug.log('DBTableHandler<ctor> ', dbtable.database, ':', dbtable.name);
  assert(arguments.length === 3);
  var i,               // an iterator
      f,               // a FieldMapping
      c,               // a ColumnMetadata
      n,               // a field or column number
      index,           // a DBIndex
      nMappedFields;

  stats.incr("constructor_calls");

  if(! ( dbtable && dbtable.columns)) {
    stats.incr("return_null");
    return null;
  }

  stats.incr("created", dbtable.database, dbtable.name);
  
  this.dbTable = dbtable;

  if(ctor) {
    this.newObjectConstructor = ctor;
  }

  if(tablemapping) {     
    stats.incr("explicit_mappings");
    this.mapping = tablemapping;
  }
  else {                                          // Create a default mapping
    stats.incr("default_mappings");
    this.mapping = Object.create(TableMappingDoc.TableMapping);
    this.mapping.name     = this.dbTable.name;
    this.mapping.database = this.dbTable.database;
    this.mapping.fields   = [];
  }
  
  /* New Arrays */
  this.stubFields             = [];
  this.columnNumberToFieldMap = [];  
  this.fieldNumberToColumnMap = [];
  this.fieldNumberToFieldMap  = [];
  this.fieldNameToFieldMap    = {};
  this.dbIndexHandlers = [];
  if (typeof(this.mapping.mapAllColumns === 'undefined')) {
    this.mapping.mapAllColumns = true;
  }

  /* Build the first draft of the columnNumberToFieldMap, using only the
     explicitly mapped fields. */
  if (typeof(this.mapping.fields) === 'undefined') {
    this.mapping.fields = [];
  }
  for(i = 0 ; i < this.mapping.fields.length ; i++) {
    f = this.mapping.fields[i];
    if(f && ! f.NotPersistent) {
      c = getColumnByName(this.dbTable, f.columnName);
      if(c) {
        n = c.columnNumber;
        this.columnNumberToFieldMap[n] = f;
        f.columnNumber = n;
      }
    }
  }

  /* Now build the implicitly mapped fields and add them to the map */
  if(this.mapping.mapAllColumns) {
    for(i = 0 ; i < this.dbTable.columns.length ; i++) {
      if(! this.columnNumberToFieldMap[i]) {
        c = this.dbTable.columns[i];
        f = Object.create(FieldMappingDoc.FieldMapping); // new FieldMapping
        f.fieldName = f.columnName = c.name;
        this.stubFields.push(f);
        this.columnNumberToFieldMap[i] = f;
        f.columnNumber = i;
      }
    }
  }

  /* Total number of mapped fields */
  nMappedFields = this.mapping.fields.length + this.stubFields.length;
         
  /* Create the resolved mapping to be returned by getMapping() */
  this.resolvedMapping = {};
  this.resolvedMapping.database = this.dbTable.database;
  this.resolvedMapping.table = this.dbTable.name;
  this.resolvedMapping.autoIncrementBatchSize = this.mapping.autoIncrementBatchSize || 1;
  this.resolvedMapping.fields = [];

  /* Build fieldNumberToColumnMap, establishing field order.
     Also build the remaining fieldNameToFieldMap and fieldNumberToFieldMap. */
  for(i = 0 ; i < this.dbTable.columns.length ; i++) {
    c = this.dbTable.columns[i];
    f = this.columnNumberToFieldMap[i];
    this.resolvedMapping.fields[i] = {};
    if(f) {
      this.fieldNumberToColumnMap.push(c);
      this.fieldNumberToFieldMap.push(f);
      this.fieldNameToFieldMap[f.fieldName] = f;
      this.resolvedMapping.fields[i].columnName = f.columnName;
      this.resolvedMapping.fields[i].fieldName = f.fieldName;
      this.resolvedMapping.fields[i].notPersistent = false;
      this.resolvedMapping.fields[i].actionOnNull = 'NONE';
    }
  }  
  assert.equal(nMappedFields, this.fieldNumberToColumnMap.length);

  // build dbIndexHandlers; one for each dbIndex, starting with primary key index 0
  for (i = 0; i < this.dbTable.indexes.length; ++i) {
    // a little fix-up for primary key unique index:
    index = this.dbTable.indexes[i];
    udebug.log_detail('DbTableHandler<ctor> creating DBIndexHandler for', index);
    if (typeof(index.name) === 'undefined') {
      index.name = 'PRIMARY';
    }
    this.dbIndexHandlers.push(new DBIndexHandler(this, index));
  }

  udebug.log("new completed");
  udebug.log_detail(this);
}

DBTableHandler.prototype = proto;     // Connect prototype to constructor


/* DBTableHandler.setResultConstructor(constructorFunction)
   IMMEDIATE

   Declare that constructorFunction and its prototype should be used
   when creating a results object for a row read from the database.
*/
DBTableHandler.prototype.setResultConstructor = function(constructorFunction) {
  this.newObjectConstructor = constructorFunction;
};


/* DBTableHandler.newResultObject
   IMMEDIATE
   
   Create a new object using the constructor function (if set).
*/
DBTableHandler.prototype.newResultObject = function(values) {
  udebug.log("newResultObject");
  stats.incr("result_objects_created");
  var newDomainObj;
  
  if(this.newObjectConstructor && this.newObjectConstructor.prototype) {
    newDomainObj = Object.create(this.newObjectConstructor.prototype);
  }
  else {
    newDomainObj = {};
  }
  
  if(this.newObjectConstructor) {
    udebug.log("newResultObject calling user constructor");
    this.newObjectConstructor.call(newDomainObj);
  }

  if (typeof(values) === 'object') {
    var x;
    // copy values into the new domain object
    for (x in values) {
      if (values.hasOwnProperty(x)) {
        newDomainObj[x] = values[x];
      }
    }
  }
  udebug.log("newResultObject done", newDomainObj);
  return newDomainObj;
};


/* registerFieldConverter(String fieldname, Converter converter)
  IMMEDIATE
  Register a converter for a field in a domain object 
*/
DBTableHandler.prototype.registerFieldConverter = function(fieldName, converter) {
  stats.incr("field_converters_registered");
  var f = this.fieldNameToFieldMap[fieldName];
  if(f) {
    f.converter = converter;
  }  
};


/* getMappedFieldCount()
   IMMEDIATE   
   Returns the number of fields mapped to columns in the table 
*/
DBTableHandler.prototype.getMappedFieldCount = function() {
  udebug.log("getMappedFieldCount");
  return this.fieldNumberToColumnMap.length;
};


/* allColumnsMapped()
   IMMEDIATE   
   Boolean: returns True if all columns are mapped
*/
DBTableHandler.prototype.allColumnsMapped = function() {
  return (this.dbTable.columns.length === this.fieldNumberToColumnMap.length);
};

/** allFieldsIncluded(values)
 *  IMMEDIATE
 *  returns array of indexes of fields included in values
 */
DBTableHandler.prototype.allFieldsIncluded = function(values) {
  // return a list of fields indexes that are found
  // the caller can easily construct the appropriate database statement
  var i, f, result = [];
  for (i = 0; i < this.fieldNumberToFieldMap.length; ++i) {
    f = this.fieldNumberToFieldMap[i];
    if (typeof(values[i]) !== 'undefined') {
      result.push(i);
    }
  }
  return result;
};

/* getColumnMetadata() 
   IMMEDIATE 
   
   Returns an array containing ColumnMetadata objects in field order
*/   
DBTableHandler.prototype.getColumnMetadata = function() {
  return this.fieldNumberToColumnMap;
};


/* IndexMetadata chooseIndex(dbTableHandler, keys) 
 Returns the index number to use as an access path.
 From API Context.find():
   * The parameter "keys" may be of any type. Keys must uniquely identify
   * a single row in the database. If keys is a simple type
   * (number or string), then the parameter type must be the 
   * same type as or compatible with the primary key type of the mapped object.
   * Otherwise, properties are taken
   * from the parameter and matched against property names in the
   * mapping.
*/
function chooseIndex(self, keys, uniqueOnly) {
  udebug.log("chooseIndex");
  var idxs = self.dbTable.indexes;
  var keyFieldNames, firstIdxFieldName;
  var i, j, f, n, index, nmatches, x;
  
  udebug.log_detail("chooseIndex for:", JSON.stringify(keys));
  
  if(typeof keys === 'number' || typeof keys === 'string') {
    if(idxs[0].columnNumbers.length === 1) {
      return 0;
    }
  }
  else {
    /* Keys is an object */ 
     keyFieldNames = [];
     for (x in keys) {
       // only include properties of the keys itself that are defined and not null
       if (keys.hasOwnProperty(x) && keys[x]) {
         keyFieldNames.push(x);
       }
     }

    /* First look for a unique index.  All columns must match. */
    for(i = 0 ; i < idxs.length ; i++) {
      index = idxs[i];
      if(index.isUnique) {
        udebug.log_detail("Considering:", (index.name || "primary key ") + " for " + JSON.stringify(keys));
        // Each key field resolves to a column, which must be in the index
        nmatches = 0;
        for(j = 0 ; j < index.columnNumbers.length ; j++) {
          n = index.columnNumbers[j];
          f = self.columnNumberToFieldMap[n]; 
          udebug.log_detail("index part", j, "is column", n, ":", f.fieldName);
          if(typeof keys[f.fieldName] !== 'undefined') {
            nmatches++;
            udebug.log_detail("match! ", nmatches);
          }
        }
        if(nmatches === index.columnNumbers.length) {
          udebug.log("chooseIndex picked unique index", i);
          return i; // all columns are found in the key object
        }
      }    
    }

    // if unique only, return failure
    if (uniqueOnly) {
      udebug.log("chooseIndex for unique index FAILED");
      return -1;
    }

    /* Then look for an ordered index.  A prefix match is OK. */
    /* Return the first suitable index we find (which might not be the best) */
    /* TODO: A better algorithm might be to return the one with the longest train of matches */
    for(i = 0 ; i < idxs.length ; i++) {
      index = idxs[i];
      if(index.isOrdered) {
        // f is the field corresponding to the first column in the index
        f = self.columnNumberToFieldMap[index.columnNumbers[0]];
        if(keyFieldNames.indexOf(f.fieldName) >= 0) {
         udebug.log("chooseIndex picked ordered index", i);
         return i; // this is an ordered index scan
        }
      }
    }
  }

  udebug.log("chooseIndex FAILED");
  return -1; // didn't find a suitable index
}


/* Return the property of obj corresponding to fieldNumber */
DBTableHandler.prototype.get = function(obj, fieldNumber) { 
  udebug.log("get", fieldNumber);
  if (typeof(obj) === 'string' || typeof(obj) === 'number') {
    return obj;
  }
  var f = this.fieldNumberToFieldMap[fieldNumber];
  return f ? obj[f.fieldName] : null;
};


/* Return an array of values in field order */
DBTableHandler.prototype.getFields = function(obj) {
  var i, fields = [];
  for( i = 0 ; i < this.getMappedFieldCount() ; i ++) {
    fields[i] = this.get(obj, i);
  }
  return fields;
};


/* Set field to value */
DBTableHandler.prototype.set = function(obj, fieldNumber, value) {
  udebug.log("set", fieldNumber);
  var f = this.fieldNumberToFieldMap[fieldNumber];
  if(f) {
    obj[f.fieldName] = value;
    return true; 
  }
  return false;
};


/* Set all member values of object according to an ordered array of fields 
*/
DBTableHandler.prototype.setFields = function(obj, values) {
//  var i;
//  for(i = 0; i < this.getMappedFieldCount() ; i ++) {
//    if(values[i]) {
//      this.set(obj, i, values[i]);
//    }
//  }
  var x;
  // copy values into the domain object
  for (x in values) {
    if (values.hasOwnProperty(x)) {
      obj[x] = values[x];
    }
  }
};


/* DBIndexHandler constructor and prototype */
function DBIndexHandler(parent, dbIndex) {
  udebug.log("DBIndexHandler constructor");
  stats.incr("DBIndexHandler","created");
  var i, colNo;

  this.tableHandler = parent;
  this.dbIndex = dbIndex;
  this.fieldNumberToColumnMap = [];
  this.fieldNumberToFieldMap  = [];
  
  for(i = 0 ; i < dbIndex.columnNumbers.length ; i++) {
    colNo = dbIndex.columnNumbers[i];
    this.fieldNumberToFieldMap[i]  = parent.columnNumberToFieldMap[colNo];
    this.fieldNumberToColumnMap[i] = parent.dbTable.columns[colNo];
  }
}

DBIndexHandler.prototype = {
  tableHandler           : null,
  dbIndex                : null,
  fieldNumberToColumnMap : null,
  fieldNumberToFieldMap  : null,
  getMappedFieldCount    : proto.getMappedFieldCount,    // inherited
  get                    : proto.get,                    // inherited
  getFields              : proto.getFields,              // inherited
  getColumnMetadata      : proto.getColumnMetadata       // inherited
};


/* DBIndexHandler getIndexHandler(Object keys)
   IMMEDIATE

   Given an object containing keys as defined in API Context.find(),
   choose an index to use as an access path for the operation,
   and return a DBIndexHandler for that index.
*/
DBTableHandler.prototype.getIndexHandler = function(keys, uniqueOnly) {
  udebug.log("getIndexHandler");
  var idx = chooseIndex(this, keys, uniqueOnly);
  var handler = null;
  if (idx !== -1) {
    handler = this.dbIndexHandlers[idx];
  }
  return handler;
};


exports.DBTableHandler = DBTableHandler;
