/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

/***** Failed find should not set rollback only ***/
var t1 = new harness.ConcurrentTest("testFindRollbackOnly");
t1.run = function() {
  var testCase = this;
  // use the domain object and primitive to find an instance
  var from = global.t_basic;
  var key = 999;
  fail_openSession(testCase, function(session) {
    testCase.errorIfTrue('t1 transaction before begin should not be active', session.currentTransaction().isActive());
    session.currentTransaction().begin();
    testCase.errorIfNotTrue('t1 transaction after begin should be active', session.currentTransaction().isActive());
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, function(err, object) {
      testCase.errorIfNotTrue('t1 transaction after failed find should be active', session.currentTransaction().isActive());
      testCase.errorIfTrue('t1 rollback only after failed find should be false', session.currentTransaction().getRollbackOnly());
      testCase.errorIfNotNull('t1 object should be null', object);
      session.currentTransaction().commit(function(err) {
        // should succeed
        if (err) {
          testCase.fail('t1 commit after find failure should succeed.');
        } else {
          testCase.failOnError();
        }
      });
    });
  });
};

/***** Failed persist should set rollback only ***/
var t2 = new harness.ConcurrentTest("testPersistRollbackOnly");
t2.run = function() {
  var testCase = this;
  // use the domain object to persist an instance
  var object = new global.t_basic(0, 'Employee 0', 0, 0);
  fail_openSession(testCase, function(session) {
    testCase.errorIfTrue('t2 transaction before begin should not be active', session.currentTransaction().isActive());
    session.currentTransaction().begin();
    testCase.errorIfNotTrue('t2 transaction after begin should be active', session.currentTransaction().isActive());
    session.persist(object, function(err) {
      testCase.errorIfNotTrue('t2 transaction after failed persist should be active', session.currentTransaction().isActive());
      testCase.errorIfNotTrue('t2 rollback only after failed persist should be true', session.currentTransaction().getRollbackOnly());
      session.currentTransaction().commit(function(err) {
        testCase.errorIfNotTrue('t2 transaction after failed persist commit should be active', session.currentTransaction().isActive());
        testCase.errorIfNotTrue('t2 rollback only after failed persist commit should be true', session.currentTransaction().getRollbackOnly());
        // should fail
        if (err) {
          session.currentTransaction().rollback(function(err) {
            // should succeed
            testCase.errorIfTrue('t2 transaction after failed persist commit rollback should not be active', session.currentTransaction().isActive());
            testCase.errorIfTrue('t2 rollback only after failed persist commit rollback should be false', session.currentTransaction().getRollbackOnly());
            if (err) {
              testCase.appendErrorMessage('t2 rollback after failed persist commit should succeed. ' + err);
              testCase.failOnError();
            } else {
              testCase.pass();
            }
          });
        } else {
          testCase.appendErrorMessage('t2 commit after persist failure should fail.');
          testCase.failOnError();
        }
      });
    });
  });
};


/***** Failed remove should set rollback only ***/
var t3 = new harness.ConcurrentTest("testRemoveRollbackOnly");
t3.run = function() {
  var testCase = this;
  // use the domain object and primitive to delete a non-existent instance
  var from = global.t_basic;
  var key = 999;
  fail_openSession(testCase, function(session) {
    testCase.errorIfTrue('t3 transaction before begin should not be active', session.currentTransaction().isActive());
    session.currentTransaction().begin();
    testCase.errorIfNotTrue('t3 transaction after begin should be active', session.currentTransaction().isActive());
    session.remove(from, key, function(err) {
      testCase.errorIfNotTrue('t3 transaction after failed remove should be active', session.currentTransaction().isActive());
      testCase.errorIfNotTrue('t3 rollback only after failed remove should be true', session.currentTransaction().getRollbackOnly());
      session.currentTransaction().commit(function(err) {
        testCase.errorIfNotTrue('t3 transaction after failed remove commit should be active', session.currentTransaction().isActive());
        testCase.errorIfNotTrue('t3 rollback only after failed remove commit should be true', session.currentTransaction().getRollbackOnly());
        // should fail
        if (err) {
          session.currentTransaction().rollback(function(err) {
            // should succeed
            testCase.errorIfTrue('t3 transaction after failed remove commit rollback should not be active', session.currentTransaction().isActive());
            testCase.errorIfTrue('t3 rollback only after failed remove commit rollback should be false', session.currentTransaction().getRollbackOnly());
            if (err) {
              testCase.appendErrorMessage('t3 rollback after failed remove commit should succeed. ' + err);
              testCase.failOnError();
            } else {
              testCase.pass();
            }
          });
        } else {
          testCase.appendErrorMessage('t3 commit after remove failure should fail.');
          testCase.failOnError();
        }
      });
    });
  });
};


/***** Failed update should set rollback only ***/
var t4 = new harness.ConcurrentTest("testUpdateRollbackOnly");
t4.run = function() {
  var testCase = this;
  // use the domain object and primitive to delete a non-existent instance
  var object = new global.t_basic(999, 'Employee 0', 0, 0);
  fail_openSession(testCase, function(session) {
    testCase.errorIfTrue('t4 transaction before begin should not be active', session.currentTransaction().isActive());
    session.currentTransaction().begin();
    testCase.errorIfNotTrue('t4 transaction after begin should be active', session.currentTransaction().isActive());
    session.update(object, function(err) {
      testCase.errorIfNotTrue('t4 transaction after failed update should be active', session.currentTransaction().isActive());
      testCase.errorIfNotTrue('t4 rollback only after failed update should be true', session.currentTransaction().getRollbackOnly());
      session.currentTransaction().commit(function(err) {
        testCase.errorIfNotTrue('t4 transaction after failed update commit should be active', session.currentTransaction().isActive());
        testCase.errorIfNotTrue('t4 rollback only after failed update commit should be true', session.currentTransaction().getRollbackOnly());
        // should fail
        if (err) {
          session.currentTransaction().rollback(function(err) {
            // should succeed
            testCase.errorIfTrue('t4 transaction after failed update commit rollback should not be active', session.currentTransaction().isActive());
            testCase.errorIfTrue('t4 rollback only after failed update commit rollback should be false', session.currentTransaction().getRollbackOnly());
            if (err) {
              testCase.appendErrorMessage('t4 rollback after failed update commit should succeed. ' + err);
              testCase.failOnError();
            } else {
              testCase.pass();
            }
          });
        } else {
          testCase.appendErrorMessage('t4 commit after update failure should fail.');
          testCase.failOnError();
        }
      });
    });
  });
};


module.exports.tests = [t1, t2, t3, t4];

