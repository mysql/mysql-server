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

/*global path, unified_debug, api_doc_dir */
"use strict";

var udebug = unified_debug.getLogger("FieldMapping.js"),
    doc    = require(path.join(api_doc_dir, "FieldMapping"));


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

/* verify(property, value, strict):
    true = mapping is valid
    string = invalid mapping error message
*/
function verify(property, value, strict) {
  switch(property) {
    case "fieldName":
    case "columnName":      
      if(value === null || typeof value !== 'string')       {
        return strict?'property ' + property + ' has invalid value ' + JSON.stringify(value):0;
      }
      break;
    case "notPersistent": 
      if(! (value === true || value === false))             {
        return strict?'property ' + property + ' has invalid value ' + JSON.stringify(value):0;
      }
      break;
    case "converter": 
      if(typeof value !== 'object')                         {
        return strict?'property ' + property + ' has invalid value ' + JSON.stringify(value):0;
      }
      break;
    default:
      return strict?'property ' + property + ' is unknown.':0;
  }
  return true; 
}


function isValidFieldMapping(m, strict) {
  var property, verifyField, verifyFields = '';
  for(property in m) {
    if(m.hasOwnProperty(property)) {
      verifyField = verify(property, m[property], strict);
      if (verifyField !== true) {
        verifyFields += verifyField + ';';
      }
    }
  }
  if (verifyFields !== '') {
    return verifyFields;
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

