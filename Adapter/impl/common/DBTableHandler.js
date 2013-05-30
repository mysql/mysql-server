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

/*global assert, unified_debug, path, api_dir, api_doc_dir */

"use strict";

var TableMapping    = require(path.join(api_dir, "TableMapping")).TableMapping,
    FieldMapping    = require(path.join(api_dir, "TableMapping")).FieldMapping,
    stats_module    = require(path.join(api_dir, "stats")),
    stats           = stats_module.getWriter(["spi","DBTableHandler"]),
    udebug          = unified_debug.getLogger("DBTableHandler.js");

// forward declaration of DBIndexHandler to avoid lint issue
var DBIndexHandler;

/* A DBTableHandler (DBT) combines dictionary metadata with user mappings.  
   It manages setting and getting of columns based on the fields of a 
   user's domain object.  It can also choose an index access path by 
   comapring user-supplied key fields of a domain object with a table's indexes.
   
   These are the structural parts of a DBT: 
     * mapping, an API TableMapping, either created explicitly or by default.
     * A TableMetadata object, obtained from the data dictionary.
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
  mapping                : {},    // TableMapping
  resolvedMapping        : null,
  newObjectConstructor   : null,  // Domain Object Constructor
  ValueObject            : null,  // Value Object Constructor

  fieldNameToFieldMap    : {},
  columnNumberToFieldMap : {},
  fieldNumberToColumnMap : {},
  fieldNumberToFieldMap  : {},
  errorMessages          : '\n',  // error messages during construction
  isValid                : true,
  autoIncColumnNumber    : null   
};

/* getColumnByName() is a utility function used in the building of maps.
*/
function getColumnByName(dbTable, colName) {
  udebug.log_detail("getColumnByName", colName);
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


/** Append an error message and mark this DBTableHandler as invalid.
 */
proto.appendErrorMessage = function(msg) {
  this.errorMessages += '\n' + msg;
  this.isValid = false;
};

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
  assert(arguments.length === 3);
  var i,               // an iterator
      f,               // a FieldMapping
      c,               // a ColumnMetadata
      n,               // a field or column number
      index,           // a DBIndex
      stubFields,      // fields created through default mapping
      nMappedFields;

  stats.incr("constructor_calls");

  if(! ( dbtable && dbtable.columns)) {
    stats.incr("return_null");
    return null;
  }

  stats.incr( [ "created", dbtable.database, dbtable.name ] );
  
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
    this.mapping          = new TableMapping(this.dbTable.name);
    this.mapping.database = this.dbTable.database;
  }
  
  /* New Arrays */
  this.columnNumberToFieldMap = [];  
  this.fieldNumberToColumnMap = [];
  this.fieldNumberToFieldMap  = [];
  this.fieldNameToFieldMap    = {};
  this.dbIndexHandlers        = [];

  /* Build the first draft of the columnNumberToFieldMap, using only the
     explicitly mapped fields. */
  if (typeof(this.mapping.fields) === 'undefined') {
    this.mapping.fields = [];
  }
  for(i = 0 ; i < this.mapping.fields.length ; i++) {
    f = this.mapping.fields[i];
    if(f && f.persistent) {
      c = getColumnByName(this.dbTable, f.columnName);
      if(c) {
        n = c.columnNumber;
        this.columnNumberToFieldMap[n] = f;
        f.columnNumber = n;
        f.defaultValue = c.defaultValue;
        f.typeConverter = c.typeConverter;
        udebug.log_detail('typeConverter for ', f.columnName, ' is ', f.typeConverter);
      } else {
        this.appendErrorMessage(
            'for table ' + dbtable.name + ', field ' + f.fieldName + ': column ' + f.columnName + ' does not exist.');
      }
    }
  }

  /* Now build the implicitly mapped fields and add them to the map */
  stubFields = [];
  if(this.mapping.mapAllColumns) {
    for(i = 0 ; i < this.dbTable.columns.length ; i++) {
      if(! this.columnNumberToFieldMap[i]) {
        c = this.dbTable.columns[i];
        f = new FieldMapping(c.name);
        stubFields.push(f);
        this.columnNumberToFieldMap[i] = f;
        f.columnNumber = i;
        f.defaultValue = c.defaultValue;
        f.typeConverter = c.typeConverter;
        udebug.log_detail('typeConverter for ', f.columnName, ' is ', f.typeConverter);
      }
    }
  }

  /* Total number of mapped fields */
  nMappedFields = this.mapping.fields.length + stubFields.length;
         
  /* Create the resolved mapping to be returned by getMapping() */
  this.resolvedMapping = {};
  this.resolvedMapping.database = this.dbTable.database;
  this.resolvedMapping.table = this.dbTable.name;
  this.resolvedMapping.fields = [];

  /* Build fieldNumberToColumnMap, establishing field order.
     Detect the autoincrement column.
     Also build the remaining fieldNameToFieldMap and fieldNumberToFieldMap. */
  for(i = 0 ; i < this.dbTable.columns.length ; i++) {
    c = this.dbTable.columns[i];
    f = this.columnNumberToFieldMap[i];
    if(c.isAutoincrement) { 
      this.autoIncColumnNumber = i;
    }      
    this.resolvedMapping.fields[i] = {};
    if(f) {
      this.fieldNumberToColumnMap.push(c);
      this.fieldNumberToFieldMap.push(f);
      this.fieldNameToFieldMap[f.fieldName] = f;
      this.resolvedMapping.fields[i].columnName = f.columnName;
      this.resolvedMapping.fields[i].fieldName = f.fieldName;
      this.resolvedMapping.fields[i].persistent = true;
    }
  }  
  if (nMappedFields !== this.fieldNumberToColumnMap.length) {
    this.appendErrorMessage();
  }

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

  if (!this.isValid) {
    this.err = new Error(this.errorMessages);
  }
  udebug.log("new completed");
  udebug.log_detail(this);
}

DBTableHandler.prototype = proto;     // Connect prototype to constructor


/* DBTableHandler.newResultObject
   IMMEDIATE
   
   Create a new object using the constructor function (if set).
*/
DBTableHandler.prototype.newResultObject = function(values, adapter) {
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
    // copy values into the new domain object
    this.setFields(newDomainObj, values, adapter);
  }
  udebug.log("newResultObject done", newDomainObj);
  return newDomainObj;
};


/** applyMappingToResult(object)
 * IMMEDIATE
 * Apply the table mapping to the result object. The result object
 * has properties corresponding to field names whose values came
 * from the database. If a domain object is needed, a new domain
 * object is created and values are copied from the result object.
 * The result (either the original result object or a new domain
 * object) is returned.
 * @param obj the object to which to apply mapping
 * @return the object to return to the user
 */
DBTableHandler.prototype.applyMappingToResult = function(obj, adapter) {
  if (this.newObjectConstructor) {
    // create the domain object from the result
    obj = this.newResultObject(obj, adapter);
  }
  return obj;
};


/** applyFieldConverters(object) 
 *  IMMEDIATE
 *  Apply the field converters to an existing object
 */ 
DBTableHandler.prototype.applyFieldConverters = function(obj) {
  var i, f, value, convertedValue;

  for (i = 0; i < this.fieldNumberToFieldMap.length; i++) {
    f = this.fieldNumberToFieldMap[i];
    if(f.converter) {
      value = obj[f.fieldName];
      convertedValue = f.converter.fromDB(value);
      obj[f.fieldName] = convertedValue;
    }
  }
};


/* setAutoincrement(object, autoincrementValue) 
 * IMMEDIATE
 * Store autoincrement values into object
 */
DBTableHandler.prototype.setAutoincrement = function(object, autoincrementValue) {
  var autoIncField;
  if(typeof this.autoIncColumnNumber === 'number') {
    autoIncField = this.columnNumberToFieldMap[this.autoIncColumnNumber];
    object[autoIncField.fieldName] = autoincrementValue;
    udebug.log("setAutoincrement", autoIncField.fieldName, ":=", autoincrementValue);
  }
};


/* getMappedFieldCount()
   IMMEDIATE   
   Returns the number of fields mapped to columns in the table 
*/
DBTableHandler.prototype.getMappedFieldCount = function() {
  udebug.log_detail("getMappedFieldCount");
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


/** Return the property of obj corresponding to fieldNumber.
 * If resolveDefault is true, replace undefined with the default column value.
 * ResolveDefault is used only for persist, not for write or update.
 * If a column converter is defined, convert the value here.
 */
DBTableHandler.prototype.get = function(obj, fieldNumber, resolveDefault, adapter) { 
  udebug.log_detail("get", fieldNumber);
  if (typeof(obj) === 'string' || typeof(obj) === 'number') {
    return obj;
  }
  var f = this.fieldNumberToFieldMap[fieldNumber];
  var result;
  if (!f) {
    throw new Error('FatalInternalError: field number does not exist: ' + fieldNumber);
  }
  if(f.converter) {
    result = f.converter.toDB(obj[f.fieldName]);
  }
  else {
    result = obj[f.fieldName];
  }
  if ((result === undefined) && resolveDefault) {
    udebug.log_detail('using default value for', f.fieldName, ':', f.defaultValue);
    result = f.defaultValue;
  }
  var typeConverter = f.typeConverter && f.typeConverter[adapter];
  if (typeConverter && result !== undefined) {
    result = typeConverter.toDB(result);
  }
  return result;
};


/* Return an array of values in field order */
DBTableHandler.prototype.getFields = function(obj, resolveDefault, adapter) {
  var i, fields = [];
  for( i = 0 ; i < this.getMappedFieldCount() ; i ++) {
    fields[i] = this.get(obj, i, resolveDefault, adapter);
  }
  return fields;
};


/* Set field to value */
DBTableHandler.prototype.set = function(obj, fieldNumber, value, adapter) {
  udebug.log_detail("set", fieldNumber);
  var f = this.fieldNumberToFieldMap[fieldNumber];
  var userValue = value;
  var typeConverter;
  if(f) {
    typeConverter = f.typeConverter && f.typeConverter[adapter];
    if (typeConverter) {
      userValue = typeConverter.fromDB(value);
    }
    if(f.converter) {
      userValue = f.converter.fromDB(userValue);
    }
    obj[f.fieldName] = userValue;
    return true; 
  }
  return false;
};


/* Set all member values of object from a value object, which
 * has properties corresponding to field names. 
 * User-defined column conversion is handled in the set method.
*/
DBTableHandler.prototype.setFields = function(obj, values, adapter) {
  var i, f, value, columnName, fieldName;
  for (i = 0; i < this.fieldNumberToFieldMap.length; ++i) {
    f = this.fieldNumberToFieldMap[i];
    columnName = f.columnName;
    fieldName = f.fieldName;
    value = values[fieldName];
    if (value !== undefined) {
      this.set(obj, i, value, adapter);
    }
  }
};


/* DBIndexHandler constructor and prototype */
function DBIndexHandler(parent, dbIndex) {
  udebug.log("DBIndexHandler constructor");
  stats.incr( [ "DBIndexHandler","created" ] );
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
