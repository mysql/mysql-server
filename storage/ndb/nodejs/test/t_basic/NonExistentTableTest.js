/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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
