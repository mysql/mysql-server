 
/*
 Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights
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

var udebug       = unified_debug.getLogger("TableMapping.js"),
    path         = require("path"),
    util         = require("util"),
    doc          = require(path.join(mynode.fs.api_doc_dir, "TableMapping"));

/* file scope mapping id used to uniquely identify a mapped domain object */
var mappingId = 0;

/* Code to verify the validity of a TableMapping */

function isString(value) { 
  return (typeof value === 'string' && value !== null);
}

function isNonEmptyString(value) {
  return (isString(value) && value.length > 0);
}

function isBool(value) {
  return (value === true || value === false);
}

function isValidConverterObject(converter) {
  return ((converter === null) || 
            (typeof converter === 'object'
             && typeof converter.toDB === 'function' 
             && typeof converter.fromDB === 'function'));
}

function isValidConstructor(constructor) {
  return (constructor != null && typeof constructor === 'function');
}

function isMeta(value) {
  var i;
  if (Array.isArray(value)) {
    for (i=0; i > value.length; ++i) {
      if (!value[i].isMeta || ! value[i].isMeta()) {
        return false;
      }
    }
  } else {
    return (value.isMeta());
  }
  return true;
}

function Relationship() {
}
Relationship.prototype.relationship = true;
Relationship.prototype.persistent   = true;

function OneToOneMapping() {
}
OneToOneMapping.prototype = new Relationship();

function OneToManyMapping() {
}
OneToManyMapping.prototype = new Relationship();
OneToManyMapping.prototype.toMany = true;

function ManyToOneMapping() {
}
ManyToOneMapping.prototype = new Relationship();
ManyToOneMapping.prototype.manyTo = true;

function ManyToManyMapping() {
}
ManyToManyMapping.prototype = new Relationship();
ManyToManyMapping.prototype.toMany = true;
ManyToManyMapping.prototype.manyTo = true;

var fieldMappingProperties = {
  "fieldName"    : isNonEmptyString,
  "columnName"   : isString,
  "persistent"   : isBool,
  "converter"    : isValidConverterObject,
  "relationship" : isBool,
  "user"         : function() { return true; },
  "meta"         : isMeta
};

var manyToOneMappingProperties = {
  "type"           : "ManyToOne",
  "foreignKey"     : isNonEmptyString,
  "target"         : isValidConstructor,
  "targetField"    : isNonEmptyString,
  "fieldName"      : isNonEmptyString,
  "columnName"     : isString,
  "converter"      : isValidConverterObject,
  "user"           : function() { return true; },
  "ctor"           : ManyToOneMapping
};

var oneToManyMappingProperties = {
  "type"           : "OneToMany",
  "target"         : isValidConstructor,
  "targetField"    : isNonEmptyString,
  "fieldName"      : isNonEmptyString,
  "columnName"     : isString,
  "converter"      : isValidConverterObject,
  "user"           : function() { return true; },
  "ctor"           : OneToManyMapping
};

var manyToManyMappingProperties = {
  "type"           : "ManyToMany",
  "target"         : isValidConstructor,
  "targetField"    : isNonEmptyString,
  "fieldName"      : isNonEmptyString,
  "columnName"     : isString,
  "converter"      : isValidConverterObject,
  "joinTable"      : isNonEmptyString,
  "user"           : function() { return true; },
  "ctor"           : ManyToManyMapping
};

var oneToOneMappingProperties = {
  "type"           : "OneToOne",
  "foreignKey"     : isNonEmptyString,
  "target"         : isValidConstructor,
  "targetField"    : isNonEmptyString,
  "fieldName"      : isNonEmptyString,
  "columnName"     : isString,
  "converter"      : isValidConverterObject,
  "user"           : function() { return true; },
  "ctor"           : OneToOneMapping
};

// These functions return error message, or empty string if valid
function verifyProperty(property, value, verifiers) {
  udebug.log_detail('verifyProperty', property, value);
  var isValid = '', chk;
  if(verifiers[property]) {
    chk = verifiers[property](value);    
    if(chk !== true && chk.length) {
      isValid = 'property ' + property + ' invalid: ' + chk;
    }
    else if(chk === false) {
      isValid = 'property ' + property + ' invalid: ' + JSON.stringify(value);
    }
  }
  else if(typeof value !== 'function') {
    isValid = 'unknown property ' + property +'; ' ;
  }
  return isValid;
}

function isValidMapping(m, verifiers) {
  var property, isValid = '';
  for(property in m) {
    if(m.hasOwnProperty(property)) {
      isValid += verifyProperty(property, m[property], verifiers);
    }
  }
  return isValid;
}    

function isValidFieldMapping(fm, number) {
  var reason = isValidMapping(fm, fieldMappingProperties);
  number = number || '';
  if(reason.length) {
    return "field " + number + " is not a valid FieldMapping: " + reason;
  }
  return '';
}

function isValidFieldMappingArray(fieldMappings) {
  var i, isValid = '';
  if(fieldMappings !== null) {
    for(i = 0; i < fieldMappings.length ; i++) {
      isValid += isValidFieldMapping(fieldMappings[i], i+1);
    }
  }
  return isValid;
}

function isStringOrStringArray(arg) {
  var i;
  if (typeof arg === 'string') return true;
  if (!Array.isArray(arg)) return 'must be a string or string array';
  for (i = 0; i < arg.length; ++i) {
    if (typeof arg[i] !== 'string') return 'must be a string or string array';
  }
  return true;
}


var tableMappingProperties = {
  "error"         : isString,
  "table"         : isNonEmptyString,
  "database"      : isString, 
  "mapAllColumns" : isBool,
  "field"         : isValidFieldMapping,
  "fields"        : isValidFieldMappingArray,
  "user"          : function() { return true; },
  "excludedFieldNames": isStringOrStringArray,
  "mappedFieldNames" : isStringOrStringArray,
  "meta"          : isMeta
};

function isValidTableMapping(tm) {
  var err = isValidMapping(tm, tableMappingProperties);
  if (!err) {
    // make sure there is a valid table
    if (!tm.hasOwnProperty('table')) {
      return '\nRequired property \'table\' is missing.';
    }
  } else {
    return err;
  }
}

function buildMappingFromObject(mapping, literal, verifier) {
  var p, keys, key;
  keys = Object.keys(verifier);
  for(p in keys) {
    key = keys[p];
    if(typeof literal[key] !== 'undefined') {
      mapping[key] = literal[key];
    }
  }
}

/* A canonical TableMapping has a "fields" array,
   though a literal one may have a "field" or "fields" object or array
*/
function makeCanonical(tableMapping) {
  if(tableMapping.field) {            // rename field => fields
    tableMapping.fields = tableMapping.field;
    delete tableMapping.field;
  }

  if(! tableMapping.fields) {
    tableMapping.fields = [];        // create empty fields array if needed
  }                             
  else if(! Array.isArray(tableMapping.fields)) {
    tableMapping.fields = [ tableMapping.fields ];
  }
}


/* TableMapping constructor
   Takes tableName or tableMappingLiteral
*/
function TableMapping(tableNameOrLiteral) {
  var err;
  var i, arg;
  switch(typeof tableNameOrLiteral) {
    case 'object':
      buildMappingFromObject(this, tableNameOrLiteral, tableMappingProperties);
      makeCanonical(this);
      break;

    case 'string':
      var parts = tableNameOrLiteral.split(".");
      if (parts[2] || tableNameOrLiteral.indexOf(' ') !== -1) {
        this.error = 'MappingError: tableName must contain one or two parts: [database.]table';
        this.table = parts[0];
      } else if(parts[0] && parts[1]) {
        this.database = parts[0];
        this.table = parts[1];
      }
      else {
        this.table = parts[0];
      }
      this.fields = [];
      this.mappedFieldNames = [];
      if (arguments.length >1) {
        this.meta = [];
        // look for optional meta following the table name
        for (i = 1; i < arguments.length; i++) {
          arg = arguments[i];
          if (arg && arg.isMeta && arg.isMeta()) {
            this.meta.push(arg);
          } else {
            this.error += 'MappingError: valid arguments are meta; invalid argument ' + i + ': (' + typeof arg + ') ' + arg;
          }
        }
      }
      break;
    
    default: 
      this.error = "MappingError: string tableName or literal tableMapping is a required parameter.";
  }
  err = isValidTableMapping(this);
   if (err) {
    this.error += err;
  }
}
/* Get prototype from documentation
*/
TableMapping.prototype = doc.TableMapping;


/* FieldMapping constructor
 * This is exported & used by DBTableHandler, but not by the public.
 */
function FieldMapping(fieldName) {
  this.fieldName  = fieldName;
  this.columnName = fieldName; 
  this.relationship = false;
}
FieldMapping.prototype = doc.FieldMapping;


/* mapField(fieldName, [columnName], [converter], [persistent])
   mapField(literalFieldMapping)
   IMMEDIATE

   Create or replace FieldMapping for fieldName
*/
TableMapping.prototype.mapField = function() {
  var i, args, arg, fieldName, fieldMapping;
  args = arguments;  

  function getFieldMapping(tableMapping, fieldName) {
    var fm, i;
    for(i = 0 ; i < tableMapping.fields.length ; i++) {
      fm = tableMapping.fields[i];
      if(fm.fieldName === fieldName) {
        return fm;
      }
    }
    fm = new FieldMapping(fieldName);
    tableMapping.fields.push(fm);
    return fm;
  }

  /* mapField() starts here */
  arg = args[0];
  if(typeof arg === 'string') {
    fieldName = arg;
    fieldMapping = getFieldMapping(this, fieldName);
    for(i = 1; i < args.length ; i++) {
      arg = args[i];
      switch(typeof arg) {
        case 'string':
          fieldMapping.columnName = arg;
          break;
        case 'boolean':
          fieldMapping.persistent = arg;
          break;
        case 'object':
          // argument is a meta or converter
          if (arg && arg.isMeta && arg.isMeta()) {
            fieldMapping.meta = arg;
          } else {
            fieldMapping.converter = arg;
          }
          break;
        default:
          this.error += "mapField(): Invalid argument " + arg;
      }
    }
  }
  else if(typeof args[0] === 'object') {
    fieldName = args[0].fieldName;
    fieldMapping = getFieldMapping(this, fieldName);
    buildMappingFromObject(fieldMapping, args[0], fieldMappingProperties);
  }
  else {
    this.error +="\nmapField() expects a literal FieldMapping or valid arguments list";
  }

  /* Validate the candidate mapping */
  this.error  += isValidFieldMapping(fieldMapping);
  this.mappedFieldNames.push(fieldName);
  return this;
};

function createRelationshipFieldFromLiteral(relationshipProperties, tableMapping, literal) {
  var relationship = new relationshipProperties.ctor();
  relationship.error = '';
  var fieldValidator, value, valid;
  var errorMessage = "";
  // iterate the literal and set properties
  var literalField;
  for (literalField in literal) {
    if (literal.hasOwnProperty(literalField)) {
      // validate each field in the literal
      udebug.log_detail('createRelationshipFieldFromLiteral validating', relationshipProperties.type, literalField, 
          literal[literalField]);
      fieldValidator = relationshipProperties[literalField];
      if (!fieldValidator) {
        errorMessage += "\nMappingError: invalid literal field: " + literalField + "\n";
      } else {
        value = literal[literalField];
        valid = fieldValidator(value);
        udebug.log_detail('createRelationshipFieldFromLiteral fieldValidator for', literalField, "is", valid);
        if (valid) {
          relationship[literalField] = value;
        } else {
          errorMessage += "\nMappingError: invalid value for literal field: " + literalField + "\n";
        }
      }
    }
  }
  if (!relationship.fieldName) {
    errorMessage += "\nMappingError: fieldName is a required field for relationship mapping";
  }
  if (!relationship.targetField && !relationship.foreignKey && !relationship.joinTable) {
    errorMessage += "\nMappingError: targetField, foreignKey, or joinTable is a required field for relationship mapping";
  }
  if (!relationship.target) {
    errorMessage += '\nMappingError: target is a required field for relationship mapping';
  }
  if (errorMessage) {
    tableMapping.error += errorMessage;
  }
  return relationship;
}

/* mapOneToOne(literalFieldMapping)
 * IMMEDIATE
 */
TableMapping.prototype.mapOneToOne = function(literalMapping) {
  var mapping;
  if (typeof literalMapping === 'object') {
    mapping = createRelationshipFieldFromLiteral(oneToOneMappingProperties, this, literalMapping);
    this.fields.push(mapping);
  } else {
    this.error += '\nMappingError: mapOneToOne supports only literal field mapping';
  }
  return this;
};

/* mapManyToOne(literalFieldMapping)
 * IMMEDIATE
 */
TableMapping.prototype.mapManyToOne = function(literalMapping) {
  var mapping;
  if (typeof literalMapping === 'object') {
    mapping = createRelationshipFieldFromLiteral(manyToOneMappingProperties, this, literalMapping);
    this.fields.push(mapping);
  } else {
    this.error += '\nMappingError: mapManyToOne supports only literal field mapping';
  }
  return this;
};

/* mapOneToMany(literalFieldMapping)
 * IMMEDIATE
 */
TableMapping.prototype.mapOneToMany = function(literalMapping) {
  var mapping;
  if (typeof literalMapping === 'object') {
    mapping = createRelationshipFieldFromLiteral(oneToManyMappingProperties, this, literalMapping);
    this.fields.push(mapping);
  } else {
    this.error += '\nMappingError: mapManyToOne supports only literal field mapping';
  }
  return this;
};

/* mapManyToMany(literalFieldMapping)
 * IMMEDIATE
 */
TableMapping.prototype.mapManyToMany = function(literalMapping) {
  var mapping;
  if (typeof literalMapping === 'object') {
    mapping = createRelationshipFieldFromLiteral(manyToManyMappingProperties, this, literalMapping);
    this.fields.push(mapping);
  } else {
    this.error += '\nMappingError: mapManyToOne supports only literal field mapping';
  }
  return this;
};

/** excludeFields(fieldNames)
 * Exclude the named field(s) from being persisted as part of sparse field handling.
 */
TableMapping.prototype.excludeFields = function() {
  var i, j, fieldName;
  if (!this.excludedFieldNames) this.excludedFieldNames = [];
  for (i = 0; i < arguments.length; ++i) {
    var fieldNames = arguments[i];
    if (typeof fieldNames === 'string') {
      this.excludedFieldNames.push(fieldNames);
    } else if (Array.isArray(fieldNames)) {
      for (j = 0; j < fieldNames.length; ++j) {
        fieldName = fieldNames[j];
        if (typeof fieldName === 'string') {
          this.excludedFieldNames.push(fieldName);
        } else {
          this.error += '\nMappingError: excludeFields argument must be a field name or an array or list of field names: \"' +
              fieldName + '\"';
        }
      }
    } else {
      this.error += '\nMappingError: excludeFields argument must be a field name or an array or list of field names: \"' +
          fieldNames + '\"';
    }
  }
};


/* mapSparseFields(columnName, fieldNames, converter)
 * columnName: required
 * fieldNames: optional string or array of strings
 * converter: optional converter function default Converters/JSONSparseFieldsConverter
 */
TableMapping.prototype.mapSparseFields = function() {
  var i, j, args, arg, columnName, fieldMapping, sparseFieldNames = [];
  args = arguments;  
    
  if(typeof args[0] === 'string') {
    columnName = args[0];
    fieldMapping = new FieldMapping(columnName);
    fieldMapping.tableMapping = this;
    for(i = 1; i < args.length ; i++) {
      arg = args[i];
      switch(typeof arg) {
        case 'string':
          sparseFieldNames.push(arg);
          break;
        case 'object':
          if (Array.isArray(arg)) {
            // verify array of field names
            for (j = 0; j < arg.length; ++j) {
              if (typeof arg[j] !== 'string') {
                this.error += "\nmapSparseFields Illegal argument; element " + j + 
                    " is not a string: \"" + util.inspect(arg[j]) + "\"";
              } else {
                sparseFieldNames.push(arg[j]);
              }
            }
          } else {
            // argument is a meta or converter
            if (arg && arg.isMeta && arg.isMeta()) {
              fieldMapping.meta = arg;
            } else {
              // validate converter
              if (isValidConverterObject(arg)) {
                fieldMapping.converter = arg;
              } else {
                this.error += "\nmapSparseFields Argument is an object " +
                    "that is not a meta, an array of field names, or a converter object: \"" + util.inspect(arg) + "\"";
              }
            }
          }
          break;
        default:
          this.error += "\nmapSparseFields: Argument must be a field name, a meta, an array of field names, or a converter object: \"" + 
            util.inspect(arg) + "\"";
      }
    }
    if (!fieldMapping.converter) {
      // default sparse fields converter
      fieldMapping.converter = mynode.converters.JSONSparseConverter;
    }
    if (sparseFieldNames.length !== 0) {
      fieldMapping.sparseFieldNames = sparseFieldNames;
    }
    fieldMapping.sparseFieldMapping = true;
    this.fields.push(fieldMapping);
  }
  else {
    this.error +="\nmapSparseFields() requires a valid arguments list with column name as the first argument";
  }
  return this;

};


/* applyToClass(constructor) 
   IMMEDIATE
*/
TableMapping.prototype.applyToClass = function(ctor) {
  if (typeof ctor === 'function') {
    ctor.prototype.mynode = {};
    ctor.prototype.mynode.mapping = this;
    ctor.prototype.mynode.constructor = ctor;
    ctor.prototype.mynode.mappingId = ++mappingId;
  } else {
    this.error += '\nMappingError: applyToClass() parameter must be constructor';
  }
  return ctor;
};


/* Public exports of this module: */
exports.TableMapping = TableMapping;
exports.FieldMapping = FieldMapping;
exports.isValidConverterObject = isValidConverterObject;
