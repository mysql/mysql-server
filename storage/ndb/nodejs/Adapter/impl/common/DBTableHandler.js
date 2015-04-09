/*
 Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights
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

"use strict";

var stats = {
	"constructor_calls"      : 0,
	"created"                : {},
	"default_mappings"       : 0,
	"explicit_mappings"      : 0,
	"return_null"            : 0,
	"result_objects_created" : 0,
	"DBIndexHandler_created" : 0
};

var assert          = require("assert"),
    TableMapping    = require(mynode.api.TableMapping).TableMapping,
    FieldMapping    = require(mynode.api.TableMapping).FieldMapping,
    stats_module    = require(mynode.api.stats),
    util            = require("util"),
    udebug          = unified_debug.getLogger("DBTableHandler.js");

// forward declaration of DBIndexHandler to avoid lint issue
var DBIndexHandler;

stats_module.register(stats,"spi","DBTableHandler");

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


/* DBTableHandler() constructor
   IMMEDIATE

   Create a DBTableHandler for a table and a mapping.

   The TableMetadata may not be null.

   If the TableMapping is null, default mapping behavior will be used.
   Default mapping behavior is to:
     select all columns when reading
     use default domainTypeConverters for all data types
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
      foreignKey,      // foreign key object from dbTable
      nMappedFields;

  stats.constructor_calls++;

  if(! ( dbtable && dbtable.columns)) {
    stats.return_null++;
    return null;
  }

	if(typeof stats.created[dbtable.name] === 'undefined') {
		stats.created[dbtable.name] = 1;
	} else { 
		stats.created[dbtable.name]++;
	}
  
  this.dbTable = dbtable;

  if(ctor) {
    this.newObjectConstructor = ctor;
  }

  if(tablemapping) {     
    stats.explicit_mappings++;
    this.mapping = tablemapping;
  }
  else {                                          // Create a default mapping
    stats.default_mappings++;
    this.mapping          = new TableMapping(this.dbTable.name);
    this.mapping.database = this.dbTable.database;
  }
  
  /* Default properties */
  this.resolvedMapping        = null;
  this.ValueObject            = null;
  this.errorMessages          = '\n';
  this.isValid                = true;
  this.autoIncFieldName       = null;
  this.autoIncColumnNumber    = null;
  this.numberOfLobColumns     = 0;
  this.numberOfNotPersistentFields = 0;
   
  /* New Arrays */
  this.columnNumberToFieldMap = [];  
  this.fieldNumberToColumnMap = [];
  this.fieldNumberToFieldMap  = [];
  this.fieldNameToFieldMap    = {};
  this.foreignKeyMap          = {};
  this.dbIndexHandlers        = [];
  this.relationshipFields     = [];

  /* Build the first draft of the columnNumberToFieldMap, using only the
     explicitly mapped fields. */
  if (typeof(this.mapping.fields) === 'undefined') {
    this.mapping.fields = [];
  }
  for(i = 0 ; i < this.mapping.fields.length ; i++) {
    f = this.mapping.fields[i];
    udebug.log_detail('DBTableHandler<ctor> field:', f, 'persistent', f.persistent, 'relationship', f.relationship);
    if(f && f.persistent) {
      if (!f.relationship) {
        c = getColumnByName(this.dbTable, f.columnName);
        if(c) {
          n = c.columnNumber;
          this.columnNumberToFieldMap[n] = f;
          f.columnNumber = n;
          f.defaultValue = c.defaultValue;
          f.databaseTypeConverter = c.databaseTypeConverter;
          // use converter or default domain type converter
          if (f.converter) {
            udebug.log_detail('domain type converter for ', f.columnName, ' is user-specified ', f.converter);
            f.domainTypeConverter = f.converter;
          } else {
            udebug.log_detail('domain type converter for ', f.columnName, ' is system-specified ', c.domainTypeConverter);
            f.domainTypeConverter = c.domainTypeConverter;
          }
        } else {
          this.appendErrorMessage(
              'for table ' + dbtable.name + ', field ' + f.fieldName + ': column ' + f.columnName + ' does not exist.');
        }
      } else {
        // relationship field
        this.relationshipFields.push(f);
      }
    } else {
      // increment not-persistent field count
      ++this.numberOfNotPersistentFields;
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
        f.databaseTypeConverter = c.databaseTypeConverter;
        // use converter or default domain type converter
        if (f.converter) {
          udebug.log_detail('domain type converter for ', f.columnName, ' is user-specified ', f.converter);
          f.domainTypeConverter = f.converter;
        } else {
          udebug.log_detail('domain type converter for ', f.columnName, ' is system-specified ', c.domainTypeConverter);
          f.domainTypeConverter = c.domainTypeConverter;
        }
      }
    }
  }

  /* Total number of mapped fields */
  nMappedFields = this.mapping.fields.length + stubFields.length - this.numberOfNotPersistentFields;
         
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
      this.autoIncFieldName = f.fieldName;
    }
    if(c.isLob) {
      this.numberOfLobColumns++;
    }    
    this.resolvedMapping.fields[i] = {};
    if(f) {
      f.fieldNumber = i;
      this.fieldNumberToColumnMap.push(c);
      this.fieldNumberToFieldMap.push(f);
      this.fieldNameToFieldMap[f.fieldName] = f;
      this.resolvedMapping.fields[i].columnName = f.columnName;
      this.resolvedMapping.fields[i].fieldName = f.fieldName;
      this.resolvedMapping.fields[i].persistent = true;
    }
  }
  var map = this.fieldNameToFieldMap;
  // add the relationship fields that are not mapped to columns
  this.relationshipFields.forEach(function(relationship) {
    map[relationship.fieldName] = relationship;
  });
  
  if (nMappedFields !== this.fieldNumberToColumnMap.length + this.relationshipFields.length) {
    this.appendErrorMessage('Mismatch between number of mapped fields and columns for ' + ctor.prototype.constructor.name);
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
  // build foreign key map
  for (i = 0; i < this.dbTable.foreignKeys.length; ++i) {
    foreignKey = this.dbTable.foreignKeys[i];
    this.foreignKeyMap[foreignKey.name] = foreignKey;
  }

  if (!this.isValid) {
    this.err = new Error(this.errorMessages);
  }
  
  if (ctor) {
    // cache this in ctor.prototype.mynode.dbTableHandler
    if (!ctor.prototype.mynode) {
      ctor.prototype.mynode = {};
    }
    if (!ctor.prototype.mynode.dbTableHandler) {
      ctor.prototype.mynode.dbTableHandler = this;
    }
  }
  udebug.log("new completed");
  udebug.log_detail("DBTableHandler<ctor>:\n", this);
}


/** Append an error message and mark this DBTableHandler as invalid.
 */
DBTableHandler.prototype.appendErrorMessage = function(msg) {
  this.errorMessages += '\n' + msg;
  this.isValid = false;
};


/* DBTableHandler.newResultObject
   IMMEDIATE
   
   Create a new object using the constructor function (if set).
*/
DBTableHandler.prototype.newResultObject = function(values, adapter) {
  udebug.log("newResultObject");
  stats.result_objects_created++;
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


/* DBTableHandler.newResultObjectFromRow
 * IMMEDIATE

 * Create a new object using the constructor function (if set).
 * Values for the object's fields come from the row; first the key fields
 * and then the non-key fields. The row contains items named '0', '1', etc.
 * The value for the first key field is in row[offset]. Values obtained
 * from the row are first processed by the db converter and type converter
 * if present.
 */
DBTableHandler.prototype.newResultObjectFromRow = function(row, adapter,
    offset, keyFields, nonKeyFields) {
  var fieldIndex;
  var rowValue;
  var field;
  var newDomainObj;

  udebug.log("newResultObjectFromRow");
  stats.result_objects_created++;

  if(this.newObjectConstructor && this.newObjectConstructor.prototype) {
    newDomainObj = Object.create(this.newObjectConstructor.prototype);
  } else {
    newDomainObj = {};
  }

  if(this.newObjectConstructor) {
    udebug.log("newResultObject calling user constructor");
    this.newObjectConstructor.call(newDomainObj);
  }
  // set key field values from row using type converters

  for (fieldIndex = 0; fieldIndex < keyFields.length; ++fieldIndex) {
    rowValue = row[offset + fieldIndex];
    field = keyFields[fieldIndex];
    this.set(newDomainObj, field.fieldNumber, rowValue, adapter);
  }
  
  // set non-key field values from row using type converters
  offset += keyFields.length;
  for (fieldIndex = 0; fieldIndex < nonKeyFields.length; ++fieldIndex) {
    rowValue = row[offset + fieldIndex];
    field = nonKeyFields[fieldIndex];
    this.set(newDomainObj, field.fieldNumber, rowValue, adapter);
  }
  
  udebug.log("newResultObjectFromRow done", newDomainObj.constructor.name, newDomainObj);
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
  } else {
    this.applyFieldConverters(obj, adapter);
  }
  return obj;
};


/** applyFieldConverters(object) 
 *  IMMEDIATE
 *  Apply the field converters to an existing object
 */ 
DBTableHandler.prototype.applyFieldConverters = function(obj, adapter) {
  var i, f, value, convertedValue;

  for (i = 0; i < this.fieldNumberToFieldMap.length; i++) {
    f = this.fieldNumberToFieldMap[i];
    var databaseTypeConverter = f.databaseTypeConverter && f.databaseTypeConverter[adapter];
    if (databaseTypeConverter) {
      value = obj[f.fieldName];
      convertedValue = databaseTypeConverter.fromDB(value);
      obj[f.fieldName] = convertedValue;
    }
    if(f.domainTypeConverter) {
      value = obj[f.fieldName];
      convertedValue = f.domainTypeConverter.fromDB(value, obj, f);
      obj[f.fieldName] = convertedValue;
    }
  }
};


/* setAutoincrement(object, autoincrementValue) 
 * IMMEDIATE
 * Store autoincrement value into object
 */
DBTableHandler.prototype.setAutoincrement = function(object, autoincrementValue) {
  if(typeof this.autoIncColumnNumber === 'number') {
    object[this.autoIncFieldName] = autoincrementValue;
    udebug.log("setAutoincrement", this.autoIncFieldName, ":=", autoincrementValue);
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
 * If a domain type converter and/or database type converter is defined, convert the value here.
 * If a fieldValueDefinedListener is passed, notify it via setDefined or setUndefined for each column.
 * Call setDefined if a column value is defined in the object and setUndefined if not.
 */
DBTableHandler.prototype.get = function(obj, fieldNumber, adapter, fieldValueDefinedListener) { 
  udebug.log_detail("get", fieldNumber);
  if (typeof(obj) === 'string' || typeof(obj) === 'number') {
    if (fieldValueDefinedListener) {
      fieldValueDefinedListener.setDefined(fieldNumber);
    }
    return obj;
  }
  var f = this.fieldNumberToFieldMap[fieldNumber];
  var result;
  if (!f) {
    throw new Error('FatalInternalError: field number does not exist: ' + fieldNumber);
  }
  if(f.domainTypeConverter) {
    result = f.domainTypeConverter.toDB(obj[f.fieldName], obj, f);
  }
  else {
    result = obj[f.fieldName];
  }
  var databaseTypeConverter = f.databaseTypeConverter && f.databaseTypeConverter[adapter];
  if (databaseTypeConverter && result !== undefined) {
    result = databaseTypeConverter.toDB(result);
  }
  if (fieldValueDefinedListener) {
    if (typeof(result) === 'undefined') {
      fieldValueDefinedListener.setUndefined(fieldNumber);
    } else {
      if (this.fieldNumberToColumnMap[fieldNumber].isBinary && result.constructor && result.constructor.name !== 'Buffer') {
        var err = new Error('Binary field with non-Buffer data for field ' + f.fieldName);
        err.sqlstate = '22000';
        fieldValueDefinedListener.err = err;
      }
      fieldValueDefinedListener.setDefined(fieldNumber);
    }
  }
  return result;
};


/** Return the property of obj corresponding to fieldNumber.
*/
DBTableHandler.prototype.getFieldsSimple = function(obj, fieldNumber) {
  var f;
  f = this.fieldNumberToFieldMap[fieldNumber];
  if(f.domainTypeConverter) {
    return f.domainTypeConverter.toDB(obj[f.fieldName], obj, f);
  }
  return obj[f.fieldName];
};
  
  
/* Return an array of values in field order */
DBTableHandler.prototype.getFields = function(obj) {
  var i, n, fields;
  fields = [];
  n = this.getMappedFieldCount();
  switch(typeof obj) {
    case 'number':
    case 'string':
      fields.push(obj);
      break;
    default: 
      for(i = 0 ; i < n ; i++) { fields.push(this.getFieldsSimple(obj, i)); }
  }
  return fields;
};


/* Return an array of values in field order */
DBTableHandler.prototype.getFieldsWithListener = function(obj, adapter, fieldValueDefinedListener) {
  var i, fields = [];
  for( i = 0 ; i < this.getMappedFieldCount() ; i ++) {
    fields[i] = this.get(obj, i, adapter, fieldValueDefinedListener);
  }
  return fields;
};


/* Set field to value */
DBTableHandler.prototype.set = function(obj, fieldNumber, value, adapter) {
  udebug.log_detail("set", fieldNumber);
  var f = this.fieldNumberToFieldMap[fieldNumber];
  var userValue = value;
  var databaseTypeConverter;
  if(f) {
    databaseTypeConverter = f.databaseTypeConverter && f.databaseTypeConverter[adapter];
    if (databaseTypeConverter) {
      userValue = databaseTypeConverter.fromDB(value);
    }
    if(f.domainTypeConverter) {
      userValue = f.domainTypeConverter.fromDB(userValue, obj, f);
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
DBIndexHandler = function (parent, dbIndex) {
  udebug.log("DBIndexHandler constructor");
  stats.DBIndexHandler_created++;
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
  
  if(i === 1) {    // One-column index
    this.singleColumn = this.fieldNumberToColumnMap[0];
  } else {
    this.singleColumn = null;
  }
};

/* DBIndexHandler inherits some methods from DBTableHandler */
DBIndexHandler.prototype = {
  getMappedFieldCount    : DBTableHandler.prototype.getMappedFieldCount,   
  get                    : DBTableHandler.prototype.get,   
  getFieldsSimple        : DBTableHandler.prototype.getFieldsSimple,
  getFields              : DBTableHandler.prototype.getFields,
  getColumnMetadata      : DBTableHandler.prototype.getColumnMetadata
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

DBTableHandler.prototype.getForeignKey = function(foreignKeyName) {
  return this.foreignKeyMap[foreignKeyName];
};


exports.DBTableHandler = DBTableHandler;
