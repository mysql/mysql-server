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

var t1 = new harness.SerialTest("testPersistIllegalArgument");
t1.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.persist(1, null, function(err, testCase2) {
        testCase2.fail('Persist with illegal argument must fail.');
      }, testCase);
    } catch(err) {
      testCase.pass();
    }
  });
};

var t2 = new harness.SerialTest("testRemoveIllegalArgument");
t2.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.remove(1, null, function(err, testCase2) {
        testCase2.fail('Remove with illegal argument must fail.');
      }, testCase);
    } catch(err) {
      testCase.pass();
    }
  });
};

var t3 = new harness.SerialTest("testSaveIllegalArgument");
t3.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.save(1, null, function(err, testCase2) {
        testCase2.fail('Save with illegal argument must fail.');
      }, testCase);
    } catch(err) {
      testCase.pass();
    }
  });
};

var t4 = new harness.SerialTest("testUpdateIllegalArgument");
t4.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.update(1, null, null, function(err, testCase2) {
        testCase2.fail('Update with illegal argument must fail.');
      }, testCase);
    } catch(err) {
      testCase.pass();
    }
  });
};

var t5 = new harness.SerialTest("testFindIllegalArgument");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.find(1, null, function(err, testCase2) {
        testCase2.fail('Find with illegal argument must fail.');
      }, testCase);
    } catch(err) {
      testCase.pass();
    }
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5];
