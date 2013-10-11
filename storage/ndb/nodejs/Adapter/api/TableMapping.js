
/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

/*global assert, path, api_doc_dir, unified_debug */

"use strict";

var udebug       = unified_debug.getLogger("TableMapping.js"),
    doc          = require(path.join(api_doc_dir, "TableMapping"));


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

var fieldMappingProperties = {
  "fieldName"    : isNonEmptyString,
  "columnName"   : isString,
  "persistent"   : isBool,
  "converter"    : isValidConverterObject,
  "user"         : function() { return true; }
};

// These functions return error message, or empty string if valid
function verifyProperty(property, value, verifiers) {
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

var tableMappingProperties = {
  "table"         : isNonEmptyString,
  "database"      : isString, 
  "mapAllColumns" : isBool,
  "field"         : isValidFieldMapping,
  "fields"        : isValidFieldMappingArray,
  "user"          : function() { return true; }
};

function isValidTableMapping(tm) {
  return isValidMapping(tm, tableMappingProperties);
}

function buildMappingFromObject(mapping, literal, verifier) {
  var p;
  for(p in Object.keys(verifier)) {
    if(typeof literal[p] !== 'undefined') {
      mapping[p] = literal[p];
    }
  }
}

/* A canoncial TableMapping has a "fields" array,
   though a literal one may have a "field" or "fields" object or array
*/
function makeCanonical(tableMapping) {
  if(tableMapping.fileds === null) {
    tableMapping.fields = [];
  }
  else if(typeof tableMapping.fields === 'object') {
    tableMapping.fields = [ tableMapping.fields ];
  }
  if(typeof tableMapping.field !== 'undefined') { 
    tableMapping.fileds.concat(tableMapping.field);
    delete tableMapping.field;
  }
}


/* TableMapping constructor
   Takes tableName or tableMappingLiteral
*/
function TableMapping(tableNameOrLiteral) {
  var err;
  switch(typeof tableNameOrLiteral) {
    case 'object':
      buildMappingFromObject(this, tableNameOrLiteral, tableMappingProperties);
      makeCanonical(this);
      break;

    case 'string':
      var parts = tableNameOrLiteral.split(".");
      if(parts[0] && parts[1]) {
        this.database = parts[0];
        this.table = parts[1];
      }
      else {
        this.table = parts[0];
      }
      this.fields = [];
      break;
    
    default: 
      throw new Error("TableMapping(): tableName or tableMapping required.");
  }
  
  err = isValidTableMapping(this);
  if(err.length) {
    throw new Error(err);
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
}
FieldMapping.prototype = doc.FieldMapping;


/* mapField(fieldName, [columnName], [converter], [persistent])
   mapField(literalFieldMapping)
   IMMEDIATE
   
   Create or replace FieldMapping for fieldName
*/
TableMapping.prototype.mapField = function() {
  var i, args, fieldName, fieldMapping, err;
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
  if(typeof args[0] === 'string') {
    fieldName = args[0];
    fieldMapping = getFieldMapping(this, fieldName);
    for(i = 1; i < args.length ; i++) {
      switch(typeof args[i]) {
        case 'string':
          fieldMapping.columnName = args[i];
          break;
        case 'boolean':
          fieldMapping.persistent = args[i];
          break;
        case 'object':
          fieldMapping.converter = args[i];
          break;
        default:
          throw new Error("Invalid argument " + args[i]);
      }
    }
  }
  else if(typeof args[0] === 'object') {
    fieldName = args[0].fieldName;
    fieldMapping = getFieldMapping(this, fieldName);
    buildMappingFromObject(fieldMapping, args[0], fieldMappingProperties);
  }
  else {
    throw new Error("mapField() expects a literal FieldMapping or valid arguments list");
  }

  /* Validate the candidate mapping */
  err = isValidFieldMapping(fieldMapping);
  if(err.length) {
    throw new Error(err);
  }

  return this;
};


/* applyToClass(constructor) 
   IMMEDIATE
*/
TableMapping.prototype.applyToClass = function(ctor) {
  udebug.log("applyToClass", this);
  if (typeof ctor === 'function') {
    ctor.prototype.mynode = {};
    ctor.prototype.mynode.mapping = this;
    ctor.prototype.mynode.constructor = ctor;
  }
  else {
    throw new Error("applyToClass() parameter must be constructor");
  }
  
  return ctor;
};


/* Public exports of this module: */
exports.TableMapping = TableMapping;
exports.FieldMapping = FieldMapping;
exports.isValidConverterObject = isValidConverterObject;
