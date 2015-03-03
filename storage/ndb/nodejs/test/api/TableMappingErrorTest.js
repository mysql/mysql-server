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

var path = require("path");
var doc_parser  = require(path.join(mynode.fs.suites_dir, 'lib', 'doc_parser'));

/** Error conditions tested:
 * t1 missing table in TableMapping constructor using literal
 * t2 TableMapping constructor with bad table 'a.b.c'
 * t3 non-string in TableMapping constructor
 * t4 no parameter in TableMapping constructor
 * t5 TableMapping constructor with bad table 'a b'
 * t6 applyToClass with a non-domain object (constructor)
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

var t1 = new harness.ConcurrentTest('t1NewTableMappingFromLiteralMissingTable');
t1.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping(
    {
    'database' : 'test',
    'mapAllColumns' : false,
    'fields' : {
      'fieldName' : 'id',
      'columnName' : 'id',
      'persistent' : true
      }
    });
  checkErrorMessage(testCase, tableMapping, 'Required property \'table\' is missing');
};

var t2 = new harness.ConcurrentTest('t2NewTableMappingBadTable');
t2.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('a.b.c');
  checkErrorMessage(testCase, tableMapping, 'tableName must contain one or two parts: [database.]table');
};

var t3 = new harness.ConcurrentTest('t3NewTableMappingNonString');
t3.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping(99);
  checkErrorMessage(testCase, tableMapping, 'string tableName or literal tableMapping is a required parameter');
};

var t4 = new harness.ConcurrentTest('t4NewTableMappingNoParameter');
t4.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping();
  checkErrorMessage(testCase, tableMapping, 'string tableName or literal tableMapping is a required parameter');
};

var t5 = new harness.ConcurrentTest('t5NewTableMappingBadTableContainsBlanks');
t5.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('a b');
  checkErrorMessage(testCase, tableMapping, 'tableName must contain one or two parts: [database.]table');
};

var t6 = new harness.ConcurrentTest('t6NewTableMappingBadTable');
t6.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('a.b');
  var clazz = 'not a class';
  tableMapping.applyToClass(clazz);
  checkErrorMessage(testCase, tableMapping, 'parameter must be constructor');
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];
