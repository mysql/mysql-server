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
var util    = require("util");
var udebug  = unified_debug.getLogger("ExcludeFieldsTest.js");

function Semistruct(id, name, number, a, c) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.name = name;
    this.number = number;
    this.a = a;
    this.c = c;
  }
}

var t1 = new harness.SerialTest("WriteSemistructExcludeTest");
t1.run = function() {
  var testCase = this;
  var semistructMapping = new mynode.TableMapping('semistruct');
  semistructMapping.mapField('id');
  semistructMapping.mapField('name');
  semistructMapping.mapField('number');
  semistructMapping.mapSparseFields('SPARSE_FIELDS');
  semistructMapping.excludeFields('c');
  semistructMapping.applyToClass(Semistruct);

  testCase.mappings = Semistruct;
  
  var semistruct10 = new Semistruct(10, 'Name 10', 10, [{a100: 'a100'}, {a101: 'a101'}], 'ignored because c is excluded');
  fail_openSession(testCase, function(session) {
    testCase.session = session;
  })
  .then(function() {
    testCase.session.persist(semistruct10);
  })
  .then(function() {
    return testCase.session.find(Semistruct, 10);
  })
  .then(function(found) {
    // verify found
    udebug.log("WriteSemistructTest found: " + util.inspect(found));
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify id", 10, found.id);
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify name", 'Name 10', found.name);
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify number", 10, found.number);
    if (Array.isArray(found.a)) {
      if (found.a.length === 2) {
        testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify a[0].a100", 'a100', found.a[0].a100);
        testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify a[1].a101", 'a101', found.a[1].a101);
      } else {
        testCase.error('\n' + testCase.name + " array a has wrong length; expected: 2, actual: " + found.a.length);
      }
    } else {
      testCase.error('\n' + testCase.name + " failed to load property a.");
    }
    if (found.c) {
      testCase.error('\n' + testCase.name + ' incorrectly loaded property c.');
    }
  })
  // clean up and report errors
  .then(function() {
    return testCase.session.close();
  })
  .then(function() {testCase.failOnError();}, function(err) {testCase.fail(err);}
  );
};

var t2 = new harness.SerialTest("WriteSemistructExcludeNotPersistentTest");
t2.run = function() {
  var testCase = this;
  var semistructMapping = new mynode.TableMapping('semistruct');
  semistructMapping.mapField('id');
  semistructMapping.mapField('name');
  semistructMapping.mapField('number');
  semistructMapping.mapSparseFields('SPARSE_FIELDS');
  semistructMapping.mapField('c', false); // c is not persistent
  semistructMapping.applyToClass(Semistruct);

  testCase.mappings = Semistruct;
  
  var semistruct11 = new Semistruct(11, 'Name 11', 11, [{a110: 'a110'}, {a111: 'a111'}], 'ignored because c is not persistent');
  fail_openSession(testCase, function(session) {
    testCase.session = session;
  })
  .then(function() {
    testCase.session.persist(semistruct11);
  })
  .then(function() {
    return testCase.session.find(Semistruct, 11);
  })
  .then(function(found) {
    // verify found
    udebug.log("WriteSemistructTest found: " + util.inspect(found));
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify id", 11, found.id);
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify name", 'Name 11', found.name);
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify number", 11, found.number);
    if (Array.isArray(found.a)) {
      if (found.a.length === 2) {
        testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify a[0].a110", 'a110', found.a[0].a110);
        testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify a[1].a111", 'a111', found.a[1].a111);
      } else {
        testCase.error('\n' + testCase.name + " array a has wrong length; expected: 2, actual: " + found.a.length);
      }
    } else {
      testCase.error('\n' + testCase.name + " failed to load property a.");
    }
    if (found.c) {
      testCase.error('\n' + testCase.name + ' incorrectly loaded property c.');
    }
  })
  // clean up and report errors
  .then(function() {
    return testCase.session.close();
  })
  .then(function() {testCase.failOnError();}, function(err) {testCase.fail(err);}
  );
};



exports.tests = [t1, t2];
