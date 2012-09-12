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

/*global path, udebug, api_doc_dir */
"use strict";

var doc = require(path.join(api_doc_dir, "FieldMapping"));


function FieldMapping(fieldName, columnName) {
  if(! fieldName) {
    throw new Error("FieldMapping(): fieldName required.");
  }
  columnName = columnName || fieldName;
  
  this.fieldName  = fieldName;
  this.columnName = columnName; 
}
FieldMapping.prototype = doc.FieldMapping;


/** Validity ***/
var validActions = ["NONE", "ERROR", "DEFAULT"];

/* verify():
    0 = mapping is valid
    1 = bad value for valid key
    2 = unknown key
*/
function verify(property, value) {
  switch(property) {
    case "fieldName":
    case "columnName":      
      if(value === null || typeof value !== 'string')       { return 1; }
      break;
    case "actionOnNull":
      if(validActions.indexOf(value) < 0)                   { return 1; }
      break;
    case "notPersistent": 
      if(! (value === true || value === false))             { return 1; }
      break;
    case "converter": 
      if(typeof value !== 'object')                         { return 1; }
      break;
    default:
      return 2;
  }
  return 0; 
}


function isValidFieldMapping(m, strict) {
  var property;
  for(property in m) {
    if(m.hasOwnProperty(property)) {
      switch(verify(property, m[property])) {
        case 0:
          break;
        case 1:
          return false;
        case 2:
          if(strict === true) { return false; }
      }
    }
  }
  return true;
}


/* set(property, value [, property, value, ... ])
   IMMEDIATE

   Set a property (or several properties) of a FieldMapping, 
   with error checking.
*/

FieldMapping.prototype.set = function() {
  var i, property, value;

  for(i = 0 ; i < arguments.length ; i += 2) {
    property = arguments[i];
    value    = arguments[i+1];
    switch(verify(property, value)) {
      case 0:
        this[property] = value;
        break;
      case 1:
        throw new Error("FieldMapping.set() unlawful value " + value + 
                        "for property " + property);
      case 2:
        throw new Error("FieldMapping.set() unknown property " + property);
    }
  }
};

exports.FieldMapping = FieldMapping;
exports.isValidFieldMapping = isValidFieldMapping;

