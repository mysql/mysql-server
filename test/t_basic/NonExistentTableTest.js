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

var t1 = new harness.SerialTest("testPersistNonExistentTable");
t1.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.persist('non_existent_table', null, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

var t2 = new harness.SerialTest("testRemoveNonExistentTable");
t2.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.remove('non_existent_table', null, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

var t3 = new harness.SerialTest("testSaveNonExistentTable");
t3.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.save('non_existent_table', null, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

var t4 = new harness.SerialTest("testUpdateNonExistentTable");
t4.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.update('non_existent_table', null, null, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

var t5 = new harness.SerialTest("testFindNonExistentTable");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.find('non_existent_table', null, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5];
