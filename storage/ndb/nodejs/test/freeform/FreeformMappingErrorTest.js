/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

"use strict";
var util    = require("util");
var udebug  = unified_debug.getLogger("FreeformMappingErrorTest.js");

/** Error conditions tested:
 * t1 mapSparseFields with no column name
 * t2 mapSparseFields with numeric column name
 * t3 mapSparseFields with numeric field name
 * t4 mapSparseFields with numeric element in field name array
 * t5 mapSparseFields with object element in field name array
 * t6 mapSparseFields with non-converter object in field name parameter
 */

function checkErrorMessage(tc, tm, msg) {
  if (!tm.error) {
    tc.fail('Actual error was missing.');
  } else {
    if (tm.error.indexOf(msg) === -1) {
      tc.fail('Actual error did not contain \'' + msg + '\' in error message:\n' + tm.error);
    } else {
      tc.pass();
    }
  }
}

var t1 = new harness.ConcurrentTest('t1SparseMappingNoColumn');
t1.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('freeform');
  tableMapping.mapField('id');
  tableMapping.mapSparseFields();
  checkErrorMessage(testCase, tableMapping, 'valid arguments list with column name as the first argument');
};

var t2 = new harness.ConcurrentTest('t2SparseMappingNonStringColumnName');
t2.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('freeform');
  tableMapping.mapField('id');
  tableMapping.mapSparseFields(10);
  checkErrorMessage(testCase, tableMapping, 'valid arguments list with column name as the first argument');
};

var t3 = new harness.ConcurrentTest('t3SparseMappingNonStringFieldName');
t3.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('freeform');
  tableMapping.mapField('id');
  tableMapping.mapSparseFields('SPARSE', 1);
  checkErrorMessage(testCase, tableMapping, 'an array of field names, or a converter object');
};

var t4 = new harness.ConcurrentTest('t4SparseMappingNumberInArrayOfFieldNames');
t4.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('freeform');
  tableMapping.mapField('id');
  tableMapping.mapSparseFields('SPARSE', ['a', 'b', 1]);
  checkErrorMessage(testCase, tableMapping, 'element 2 is not a string');
};

var t5 = new harness.ConcurrentTest('t5SparseMappingObjectInArrayOfFieldNames');
t5.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('freeform');
  tableMapping.mapField('id');
  tableMapping.mapSparseFields('SPARSE', ['a', 'b', {f:9}]);
  checkErrorMessage(testCase, tableMapping, 'element 2 is not a string');
};

var t6 = new harness.ConcurrentTest('t6SparseMappingNonConverterObjectParameter');
t6.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('freeform');
  tableMapping.mapField('id');
  tableMapping.mapSparseFields('SPARSE', {f:9});
  checkErrorMessage(testCase, tableMapping, 'an array of field names, or a converter object');
};

exports.tests = [t1, t2, t3, t4, t5, t6];
