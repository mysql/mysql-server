/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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


function getTypeAdapter(type) {
  switch(type.toLocaleUpperCase()) {
    case "CHAR":
    case "VARCHAR":
    case "TEXT":
      return function(str) { return str };

    case "TINYINT":
    case "SMALLINT":
    case "MEDIUMINT":
    case "INT":
    case "BIGINT":
    case "FLOAT":
    case "DOUBLE":
    case "DECIMAL":
    case "YEAR":    
    case "TIMESTAMP":
      return Number;

    case "BINARY":
    case "VARBINARY":
    case "BLOB":
      return function(str) { return new Buffer(str, 'utf8') };

    case "DATE":
    case "DATETIME":
      return function(str) { return new Date(str); };

    case "BIT":
    case "TIME":
    default:
      throw("UNSUPPORTED COLUMN TYPE " + type);
  }
}

// a string to value adapter can take in an array of strings
// and produce an object with proper JS types for a particular table

function JsValueAdapter(tableHandler, isJSON) {
  var i, col;

  this.tableHandler   = tableHandler;
  this.columnMetadata = tableHandler.getColumnMetadata();
  this.isJSON         = isJSON;
  this.fieldNames     = [];
  this.fieldAdapters  = [];

  for(i = 0 ; i < this.columnMetadata.length ; i++) {
    col = this.columnMetadata[i];
    this.fieldNames.push(col.name);
    this.fieldAdapters.push(getTypeAdapter(col.columnType));
  }
};

JsValueAdapter.prototype.useFieldSubset = function(fieldArray) {
  var i, col, type;

  this.fieldNames   = fieldArray;
  this.typeAdapters = [];

  for(i = 0; i < fieldArray.length ; i++) {
    // FIXME: Private access into tablehandler
    col = this.tableHandler.fieldNameToFieldMap[fieldArray[i]];
    if(! col) {
      throw new Error("Column " + fieldArray[i] + " does not exist in table");
    }
    type = this.columnMetadata[col.columnNumber].columnType;
    this.fieldAdapters[i] = getTypeAdapter(type);
  }
};

JsValueAdapter.prototype.adapt = function(value) {
  return this.isJSON ? this.adaptObject(value) : this.adaptArray(value);
};

JsValueAdapter.prototype.adaptArray = function(array) {
  var i, result;
  result = {};
  for(i = 0 ; i < array.length && i < this.fieldNames.length ; i++) {
    result[this.fieldNames[i]] = this.fieldAdapters[i](array[i]);
  }
  return result;
};

JsValueAdapter.prototype.adaptObject = function(obj) {
  var i;
  for(i = 0 ; i < this.fieldNames.length ; i++) {
    obj[this.fieldNames[i]] = this.fieldAdapters[i](obj[this.fieldNames[i]]);
  }
  return obj;
};


exports.JsValueAdapter = JsValueAdapter;

