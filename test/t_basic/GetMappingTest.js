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

var expectedMappingFor_t_basic = {
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
  session.getMapping(global.t_basic, function(err, result, testCase2) {
    if (err) {
      testCase2.fail(err);
      return;
    }
    verifyMapping(testCase2, expectedMappingFor_t_basic, result);
    testCase2.failOnError();
    }, testCase);
};

var t2 = new harness.SerialTest("getMappingForTableName");
t2.run = function() {
  fail_openSession(t2, t2.testGetMapping);
};

t2.testGetMapping = function(session, testCase) {
  session.getMapping('t_basic', function(err, result, testCase2) {
    if (err) {
      testCase2.fail(err);
      return;
    }
    verifyMapping(testCase2, expectedMappingFor_t_basic, result);
    testCase2.failOnError();
  }, testCase);
};

var t3 = new harness.SerialTest("getMappingForUnknownTableName");
t3.run = function() {
  fail_openSession(t3, t3.testGetMapping);
};

t3.testGetMapping = function(session, testCase) {
  session.getMapping('non_existent_table', function(err, result, testCase2) {
    if (err) {
      testCase2.pass();
      return;
    }
    testCase2.fail('getMapping for unknown table should fail');
  }, testCase);
};

var t4 = new harness.SerialTest("getMappingForUnMappedConstructor");
t4.run = function() {
  fail_openSession(t4, t4.testGetMapping);
};

t4.testGetMapping = function(session, testCase) {
  var unmappedDomainClass = function() {
  };
  session.getMapping(unmappedDomainClass, function(err, result, testCase2) {
    if (err) {
      testCase2.pass();
      return;
    }
    testCase2.fail('getMapping for unmapped domain object should fail');
  }, testCase);
};

var t5 = new harness.SerialTest("getMappingForNumber");
t5.run = function() {
  fail_openSession(t5, t5.testGetMapping);
};

t5.testGetMapping = function(session, testCase) {
  var unmappedDomainClass = function() {
  };
  try {
    session.getMapping(1, function(err, result, testCase2) {
      testCase2.fail('getMapping for 1 should fail');
    }, testCase);
  } catch (err) {
    testCase.pass();
  }
};

var t6 = new harness.SerialTest("getMappingForBoolean");
t6.run = function() {
  fail_openSession(t6, t6.testGetMapping);
};

t6.testGetMapping = function(session, testCase) {
  var unmappedDomainClass = function() {
  };
  try {
    session.getMapping(true, function(err, result, testCase2) {
      testCase2.fail('getMapping for true should fail');
    }, testCase);
  } catch (err) {
    testCase.pass();
  }
};

var t7 = new harness.SerialTest("getMappingForDomainObject");
t7.run = function() {
  fail_openSession(t7, t7.testGetMapping);
};

t7.testGetMapping = function(session, testCase) {
  session.getMapping(new global.t_basic(), function(err, result, testCase2) {
    if (err) {
      testCase2.fail(err);
      return;
    }
    verifyMapping(testCase2, expectedMappingFor_t_basic, result);
    testCase2.failOnError();
    }, testCase);
};

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7];
