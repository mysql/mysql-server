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

/**********************
  This is the standard TypeConverter class used with sparse JSON columns.
  Multiple fields are collected and treated as one string in the database.

  Writing from JavaScript to DB, this converter takes a JavaScript object and
  a sparse FieldMapping and returns a string formatted as JSON containing the
  fields specified by the FieldMapping.
  
  Reading from DB to JavaScript, this converter takes a JavaScript object, a sparse
  FieldMapping, and a string formatted as JSON. It modifies the object according
  to the FieldMapping using the JSON string to provide values.

************************/

var udebug = unified_debug.getLogger("JSONSparseConverter.js");

exports.toDB = function(value, jsObject, fieldMapping) {
  var candidateField;
  var mappedFieldNames = fieldMapping.tableMapping.mappedFieldNames;
  var excludedFieldNames = fieldMapping.tableMapping.excludedFieldNames || [];
  udebug.log_detail("JSONSparseConverter.toDB excludedFieldNames: ", excludedFieldNames);
  var dbValue = '{';
  var separator = '';
  function processField() {
    dbValue += separator;
    separator = ',';
    dbValue += '"';
    dbValue += candidateField;
    dbValue += '":';
    dbValue += JSON.stringify(jsObject[candidateField]);    
  }
  for (candidateField in jsObject) {
    if (jsObject.hasOwnProperty(candidateField)) {
      if (mappedFieldNames.indexOf(candidateField) === -1) {
        // don't handle mapped fields as sparse
        if (fieldMapping.sparseFieldNames) {
          // if sparse field names are explicit, only process fields that match
          if (fieldMapping.sparseFieldNames.indexOf(candidateField) !== -1) {
            processField();
          }
        } else if (excludedFieldNames.indexOf(candidateField) === -1) {
          // if sparse field names are not explicit, only process fields not excluded from the table
          processField();
        }
      }
    }
  }
  dbValue += '}';
  return dbValue;
};

exports.fromDB = function(dbValue, jsObject, fieldMapping) {
  udebug.log("JSONSparseConverter.fromDB value: ", dbValue);
  var dbValues = JSON.parse(dbValue);
  for (sparse in dbValues) {
    // do not need "if (dbValues.hasOwnProperty(sparse)) {" because JSON.parse returns a "naked" object
    jsObject[sparse] = dbValues[sparse];
  }
  return dbValues;
};

