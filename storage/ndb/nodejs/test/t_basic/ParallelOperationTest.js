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

/** Dummy test */
t0 = new harness.SerialTest('dummy');
t0.run = function() {
  this.pass();
};

udebug      = unified_debug.getLogger("ParallelOperationTest.js");

function findParallel(start_value, number, session, testCase) {
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
};

/***** Parallel Find Autocommit ***/
t1 = new harness.ConcurrentTest('testParallelFindAutocommit');
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
t2 = new harness.ConcurrentTest('testParallelFindBeginCommit');
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
