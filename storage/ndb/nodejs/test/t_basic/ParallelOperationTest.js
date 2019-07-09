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

var udebug = unified_debug.getLogger("ParallelOperationTest.js");

/** Dummy test */
var t0 = new harness.SerialTest('dummy');
t0.run = function() {
  this.pass();
};

function findParallel(start_value, number, session, testCase) {
  var i, found_i;
  for (i = start_value; i < start_value + number; ++i) {
    session.find(global.t_basic, i, function(err, found, callback_i, session, testCase) {
      udebug.log('ParallelOperationTest.testParallelFind find callback for ', i);
      var tx = session.currentTransaction();
      if (err) {
        testCase.appendErrorMessage(
            'ParallelOperationTest.testParallelFind find callback for ', i,' failed: ', JSON.stringify(err));
      }
      found_i = 'undefined';
      if (found !== null) {
        found_i = found.id;
        testCase.errorIfNotEqual(
            'ParallelOperationTest.testParallelFind find callback mismatch id value:', found_i, found.id);
      }
      if (++testCase.completed === number) {
        // end of test case; all callbacks completed
        if (tx.isActive()) {
          tx.commit(function(err, session, testCase) {
            if (err) {
              testCase.fail(err);
              return;
            }
            testCase.failOnError();
          }, session, testCase);
        } else {
          testCase.failOnError();
        }
      }
    }, i, session, testCase);
  }
}

/***** Parallel Find Autocommit ***/
var t1 = new harness.ConcurrentTest('testParallelFindAutocommit');
t1.run = function() {
  var testCase = this;
  var start_value = 0;
  var number = 10;
  testCase.completed = 0;
  fail_openSession(testCase, function(session) {
    findParallel(start_value, number, session, testCase);
  });
};


/***** Parallel Find Begin Commit ***/
var t2 = new harness.ConcurrentTest('testParallelFindBeginCommit');
t2.run = function() {
  var testCase = this;
  var start_value = 0;
  var number = 10;
  testCase.completed = 0;
  fail_openSession(testCase, function(session) {
    session.currentTransaction().begin();
    findParallel(start_value, number, session, testCase);
  });
};



/*************** EXPORT THE TOP-LEVEL GROUP ********/
// module.exports.tests = [t1, t2];
module.exports.tests = [];
