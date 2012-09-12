
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

/*global path, api_doc_dir */

"use strict";

var doc          = require(path.join(api_doc_dir, "TableMapping")),
    fieldmapping = require("./FieldMapping.js");


/* TableMapping constructor
*/
function TableMapping(tableName) {
  if(! tableName) {
    throw new Error("TableMapping(): tableName required.");
  }  
  this.name   = tableName;
  this.fields = [];
}
TableMapping.prototype = doc.TableMapping;


/* verify() 
   Returns true on OK 
   or string error message on error
*/
function verify(property, value, strict) {
  var i;
  
  function valErr() {
    return "unlawful value " + value + "for property " + property;
  }

  if(typeof value === 'undefined' || value === null) { return valErr(); }
 
  switch(property) {
    case "name":
    case "database":
      if(typeof value !== 'string')             { return valErr(); }
      break;
    case "autoIncrementBatchSize":
      if(! isFinite(value))                     { return valErr(); }
      break;
    case "mapAllColumns": 
      if(! (value === true || value === false)) { return valErr(); }
      break;
    case "fields": 
      // must be an array of FieldMappings
      if(typeof value !== 'object')             { return valErr(); }
      for(i = 0 ; i < value.length ; i++) {
        if(! fieldmapping.isValidFieldMapping(value[i], strict)) {
          return "element " + i + "is not a FieldMapping.";
        }
      }
      break;
    default:
      return "unknown property " + property;
  }
  return true;
}


/* Check TableMapping for Annotations strict mode.
   returns true on OK, or string error message
*/
function isStrictlyValidTableMapping(m) {
  var property, validity;
  for(property in m) {
    if(m.hasOwnProperty(property)) {
      validity = verify(property, m[property], true);
      if(validity !== true) {
        return validity;
      }
    }
  }
  return true;
}


/* set(property, value [, property, value, ... ])
   IMMEDIATE

   Set a property (or several properties) of a TableMapping, 
   with error checking.
*/
TableMapping.prototype.set = function() {
  var i, property, value, isValid;

  for(i = 0 ; i < arguments.length ; i += 2) {
    property = arguments[i];
    value    = arguments[i+1];
    isValid = verify(property, value); 
    if(isValid === true) {
      this[property] = value;
    }
    else {
      throw new Error("TableMapping.set() " + isValid);
    }
  }
};


/* addFieldMapping(fieldMapping) 
   IMMEDIATE
   
   Add a FieldMapping to the list of mapped fields, with error checking.
   fieldMapping must be an object originally obtained from 
   Annotations.newFieldMapping(). 
   Throws an exception on error.
*/
TableMapping.prototype.addFieldMapping = function(m) {
  if(fieldmapping.isValidFieldMapping(m)) {
    this.fields.push(m);
  }
  else {
    throw new Error("TableMapping.addFieldMapping(): invalid FieldMapping.");
  }
};


exports.TableMapping = TableMapping;
exports.isStrictlyValidTableMapping = isStrictlyValidTableMapping;

