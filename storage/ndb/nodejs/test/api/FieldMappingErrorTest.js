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

'use strict';

var path = require("path"),
    doc_parser  = require(path.join(mynode.fs.suites_dir, 'lib', 'doc_parser'));

/** Error conditions tested:
 * t1 mapField with no parameters
 * t2 mapField with empty string
 * t3 mapField with numeric field name
 * t4 mapField with numeric parameter
 * t5 mapOneToOne with empty string
 * t6 mapOneToOne with missing field name
 * t7 mapOneToOne with missing targetField
 * t8 mapOneToOne with missing target 
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

var domainClass = function(id, name, age, magic) {
  this.id = id;
  this.name = name;
  this.age = age;
  this.magic = magic;
};

var t1 = new harness.ConcurrentTest('t1FieldMappingNoParameters');
t1.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapField();
  checkErrorMessage(testCase, tableMapping, 'literal FieldMapping or valid arguments list');
};

var t2 = new harness.ConcurrentTest('t2FieldMappingInvalidBlankFieldName');
t2.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapField('');
  checkErrorMessage(testCase, tableMapping, 'property fieldName invalid');
};

var t3 = new harness.ConcurrentTest('t3FieldMappingInvalidNumericFieldName');
t3.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapField(3);
  checkErrorMessage(testCase, tableMapping, 'literal FieldMapping or valid arguments list');
};

var t4 = new harness.ConcurrentTest('t4FieldMappingInvalidNumericParameter');
t4.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapField('id', 3);
  checkErrorMessage(testCase, tableMapping, 'Invalid argument 3');
};

var t5 = new harness.ConcurrentTest('t5FieldMappingInvalidEmptyStringParameter');
t5.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapOneToOne('');
  checkErrorMessage(testCase, tableMapping, 'mapOneToOne supports only literal field mapping');
};

var t6 = new harness.ConcurrentTest('t6FieldMappingInvalidMissingFieldName');
t6.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapOneToOne({});
  checkErrorMessage(testCase, tableMapping, 'fieldName is a required field for relationship mapping');
};

var t7 = new harness.ConcurrentTest('t7FieldMappingInvalidMissingTargetField');
t7.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapOneToOne({
    fieldName: 'r1'
  });
  checkErrorMessage(testCase, tableMapping, 'targetField, foreignKey, or joinTable is a required field');
};

var t8 = new harness.ConcurrentTest('t8FieldMappingInvalidMissingTarget');
t8.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('t_basic');
  tableMapping.mapOneToOne({
    fieldName: 'r1',
    targetField: 't1'
  });
  checkErrorMessage(testCase, tableMapping, 'target is a required field for relationship mapping');
};



module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];
