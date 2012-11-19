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

global.util            = require("util");

var udebug      = unified_debug.getLogger("GetMappingTest.js");

expectedMappingFor_t_basic = {
    "table" : "t_basic",
    "database" : "test",
    "autoIncrementBatchSize" : 1,
    "fields" : [{
      "fieldName" : "id",
      "actionOnNull" : "NONE",
      "columnName" : "id",
      "notPersistent" : false
    }]
};

var verifyMapping = function(testCase, expected, result) {
  udebug.log('GetMappingTest result: ', util.inspect(result));
  testCase.errorIfNotEqual('Mapping.table mismatch', expected.table, result.table);
  testCase.errorIfNotEqual('Mapping.database mismatch', expected.database, result.database);
  testCase.errorIfNotEqual('Mapping.autoIncrementBatchSize mismatch', expected.autoIncrementBatchSize, result.autoIncrementBatchSize);
  testCase.errorIfNotEqual('Mapping.fields.fieldName mismatch', expected.fields[0].fieldName, result.fields[0].fieldName);
  testCase.errorIfNotEqual('Mapping.fields.actionOnNull mismatch', expected.fields[0].actionOnNull, result.fields[0].actionOnNull);
  testCase.errorIfNotEqual('Mapping.fields.columnName mismatch', expected.fields[0].columnName, result.fields[0].columnName);
  testCase.errorIfNotEqual('Mapping.fields.notPersistent mismatch', expected.fields[0].notPersistent, result.fields[0].notPersistent);
};

var t1 = new harness.SerialTest("getMappingForConstructor");
t1.run = function() {
  fail_openSession(t1, t1.testGetMapping);
};

t1.testGetMapping = function(session, testCase) {
  // verify the mapping
  var result = session.getMapping(global.t_basic);
  var expected = expectedMappingFor_t_basic;
  verifyMapping(testCase, expected, result);
  testCase.failOnError();
};

var t2 = new harness.SerialTest("getMappingForTableName");
t2.run = function() {
  fail_openSession(t2, t2.testGetMapping);
};

t2.testGetMapping = function(session, testCase) {
  // verify the mapping
  var result = session.getMapping('t_basic');
  var expected = expectedMappingFor_t_basic;
  verifyMapping(testCase, expected, result);
  testCase.failOnError();
};

var t3 = new harness.SerialTest("getMappingForUnknownTableName");
t3.run = function() {
  fail_openSession(t3, t3.testGetMapping);
};

t3.testGetMapping = function(session, testCase) {
  // verify the mapping
  var result = session.getMapping('unknown');
  testCase.errorIfNotEqual('getMapping for unknown table should return null', null, result);
  testCase.failOnError();
};

var t4 = new harness.SerialTest("getMappingForUnMappedConstructor");
t4.run = function() {
  fail_openSession(t4, t4.testGetMapping);
};

t4.testGetMapping = function(session, testCase) {
  // verify the mapping
  var unmappedDomainClass = function(id, name, age, magic) {
    this.id = id;
    this.name = name;
    this.age = age;
    this.magic = magic;
  };

  var result = session.getMapping(unmappedDomainClass);
  testCase.errorIfNotEqual('getMapping for unmapped constructor should return null', null, result);
  testCase.failOnError();
};

module.exports.tests = [t1, t2, t3, t4];
