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

udebug      = unified_debug.getLogger("BatchTest.js");

function createBatch(session, start_value, number, testCase) {
  udebug.log("createBatch");
  var batch = session.createBatch();
  var i;
  var object;
  for (i = start_value; i < start_value + number; ++i) {
    object = new global.t_basic(i, 'Employee ' + i, i, i);
    batch.persist(object, function(err, callback_session, callback_i, callback_testCase) {
      if (err && !testCase.expectError) {
        callback_testCase.appendErrorMessage('Error inserting ' + callback_i + JSON.stringify(err));
      }
    }, session, i, testCase);
  }
  return batch;
};

function onExecuteBatch(err, session, start_value, number, testCase) {
  udebug.log("onExecuteBatch");
  if (err) {
    testCase.fail(err);
    return;
  }
  var tx = session.currentTransaction();
  if (tx.isActive()) {
    tx.commit(verifyInsert, session, start_value, number, testCase);
  }
};

function verifyInsert(err, session, start_value, number, testCase) {
  udebug.log('verifyInsert');
  if (err) {
    testCase.fail(err);
    return;
  }
  // after the batch insert, verify that the insert occurred
  var j;
  var completed = 0;
  for (j = start_value; j < start_value + number; ++j) {
    session.find(global.t_basic, j, function(err, found, testCase, callback_j, session) {
      var tx = session.currentTransaction();
      if (err && !testCase.expectError) {
        testCase.appendErrorMessage(
            'BatchTest.verifyInsert find callback for ', j,' failed: ', JSON.stringify(err));
        return;
      }
      found_j = 'undefined';
      if (found !== null) {
        found_j = found.id;
        if (found_j !== callback_j) {
          testCase.errorIfNotEqual(
              'BatchTest.verifyInsert find callback failed', found_j, found.id);
        }
      }
      udebug.log('BatchTest.verifyInsert find callback for ', found_j);
      if (++completed === number) {
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
    }, testCase, j, session);
  }
};

/***** Insert Autocommit ***/
var t1 = new harness.SerialTest('testBatchInsertAutocommit');
t1.run = function() {
  var testCase = this;
  this.number_to_insert = 10;
  this.start_value = 10000;
  fail_openSession(testCase, function(session) {
    var batch = createBatch(session, testCase.start_value, testCase.number_to_insert, testCase);
    batch.execute(verifyInsert, session, testCase.start_value, testCase.number_to_insert, testCase);
  });
};

/***** Insert begin commit ***/
var t2 = new harness.SerialTest('testBatchInsertBeginCommit');
t2.run = function() {
  var testCase = this;
  this.number_to_insert = 10;
  this.start_value = 20000;
  fail_openSession(testCase, function(session) {
    var batch = createBatch(session, testCase.start_value, testCase.number_to_insert, testCase);
    session.currentTransaction().begin();
    batch.execute(onExecuteBatch, session, testCase.start_value, testCase.number_to_insert, testCase);
  });
};

/***** getSession ***/
var t3 = new harness.ConcurrentTest('getSession');
t3.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var batch = session.createBatch();
    if (batch.getSession() !== session) {
      testCase.fail(new Error('getSession failed to return session'));
    } else {
      testCase.pass();
    }
  });
};

/***** clear ***/
var t4 = new harness.ConcurrentTest('clear');
t4.expectError = true;
t4.run = function() {
  var testCase = this;
  this.number_to_insert = 10;
  // these values conflict with t1 but these values are cleared, not committed
  this.start_value = 10000;
  fail_openSession(testCase, function(session) {
    var batch = createBatch(session, testCase.start_value, testCase.number_to_insert, testCase);
    batch.clear();
    batch.execute(function(err) {
      if (err) {
        testCase.appendError(err);
      }
    });
    testCase.failOnError();
  });
};


/*************** EXPORT THE TOP-LEVEL GROUP ********/
// module.exports.tests = [t1, t2, t3, t4];
module.exports.tests = [t1];
