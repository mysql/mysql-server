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
var udebug  = unified_debug.getLogger("FreeformTest.js");

function Freeform(id, name, number, a) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.name = name;
    this.number = number;
    this.a = a;
  }
}

var t1 = new harness.SerialTest("ReadFreeformTest");
t1.run = function() {
  var testCase = this;
  var freeformMapping = new mynode.TableMapping('freeform');
  freeformMapping.mapField('id');
  freeformMapping.mapSparseFields('SPARSE_FIELDS');
  freeformMapping.applyToClass(Freeform);

  testCase.mappings = Freeform;
  fail_openSession(testCase, function(session) {
    testCase.session = session;
  })
  .then(function() {
    return testCase.session.find(Freeform, 1);
  })
  .then(function(found) {
    // verify found
    udebug.log("ReadFreeformTest found: " + util.inspect(found));
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify id", 1, found.id);
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify name", 'Name 1', found.name);
    testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify number", 1, found.number);
    if (Array.isArray(found.a)) {
      if (found.a.length === 2) {
        testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify a[0].a10", 'a10', found.a[0].a10);
        testCase.errorIfNotEqual('\n' + testCase.name + " failed to verify a[1].a11", 'a11', found.a[1].a11);
      } else {
        testCase.error('\n' + testCase.name + " array a has wrong length; expected: 2, actual: " + found.a.length);
      }
    } else {
      testCase.error('\n' + testCase.name + " failed to load property a.");
    }
  })
  // clean up and report errors
  .then(function() {
    return testCase.session.close();
  })
  .then(function() {testCase.failOnError();}, function(err) {testCase.fail(err);}
  );
};


var t2 = new harness.SerialTest("WriteFreeformTest");
t2.run = function() {
  var testCase = this;
  var freeformMapping = new mynode.TableMapping('freeform');
  freeformMapping.mapField('id');
  freeformMapping.mapSparseFields('SPARSE_FIELDS');
  freeformMapping.applyToClass(Freeform);

  testCase.mappings = Freeform;
  
  var freeform10 = new Freeform(10, 'Name 10', 10, [{a100: 'a100'}, {a101: 'a101'}]);
  fail_openSession(testCase, function(session) {
    testCase.session = session;
  })
  .then(function() {
    testCase.session.persist(freeform10);
  })
  .then(function() {
    return testCase.session.find(Freeform, 10);
  })
  .then(function(found) {
    // verify found
    udebug.log("FreeformTest found: " + util.inspect(found));
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
  })
  // clean up and report errors
  .then(function() {
    return testCase.session.close();
  })
  .then(function() {testCase.failOnError();}, function(err) {testCase.fail(err);}
  );
};



exports.tests = [t1, t2];
