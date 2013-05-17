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
/*jslint newcap: true */
/*global t_basic, verify_t_basic, fail_verify_t_basic */


/***** Persist with domain object ***/
var t1 = new harness.SerialTest("testPersistDomainObjectAutocommit");
t1.run = function() {
  var testCase = this;
  // create the domain object 4080
  var object = new global.t_basic(4080, 'Employee 4080', 4080, 4080);
  fail_openSession(testCase, function(session) {
    // persist with no callback
    session.persist(object);
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(global.t_basic, 4080, fail_verify_t_basic, 4080, testCase, true);
  });
};

/***** Persist with constructor and domain object ***/
var t2 = new harness.SerialTest("testPersistConstructorAndObjectAutocommit");
t2.run = function() {
  var testCase = this;
  // create the domain object 4081
  var object = new global.t_basic(4081, 'Employee 4081', 4081, 4081);
  fail_openSession(testCase, function(session) {
    // persist with no callback
    session.persist(global.t_basic, object);
    // remove with no callback);
    session.remove(global.t_basic, 4081); 
    session.find(global.t_basic, object, function(err, found) {
      if (err) {
        testCase.appendErrorMessage('t2 returned error' + err);
      } else {
        testCase.errorIfNotEqual('t2 should have removed object 4081', null, found);
      }
      testCase.failOnError();
    });
  });
};

/***** Persist with domain object ***/
var t3 = new harness.SerialTest("testPersistDomainObjectBeginCommit");
t3.run = function() {
  var testCase = this;
  // create the domain object 4082
  var object = new global.t_basic(4082, 'Employee 4082', 4082, 4082);
  fail_openSession(testCase, function(session) {
    // make sure DBTableHandler for t_basic is loaded
    session.find(global.t_basic, 0, function(err) {
      // begin transaction
      session.currentTransaction().begin();
      testCase.errorIfNotTrue('t3 after begin transaction.isActive()', session.currentTransaction().isActive());
      // persist with no callback
      session.persist(object);
      testCase.errorIfNotTrue('t3 after persist transaction.isActive()', session.currentTransaction().isActive());
      // find should find the object just inserted
      session.find(global.t_basic, 4082, function(err, found) {
        testCase.errorIfNotTrue('t3 after find transaction.isActive()', session.currentTransaction().isActive());
        if (err) {
          testCase.appendErrorMessage('t3 find of inserted object returned error' + err);
        } else if (found === null) {
          testCase.appendErrorMessage('t3 find of inserted object returned null.');
        } else {
          testCase.errorIfNotEqual('t3 find of inserted object has wrong magic', 4082, found.magic);
        }
        session.currentTransaction().commit(function(err) {
          if (err) {
            testCase.appendErrorMessage('t3 commit failed.' + err);
          }
          testCase.failOnError();
        });
      });      
    });
  });
};

/***** Persist with constructor and domain object ***/
var t4 = new harness.SerialTest("testPersistConstructorAndObjectBeginCommit");
t4.run = function() {
  var testCase = this;
  // create the domain object 4083
  var object = new global.t_basic(4083, 'Employee 4083', 4083, 4083);
  fail_openSession(testCase, function(session) {
    // make sure DBTableHandler for t_basic is loaded
    session.find(global.t_basic, 0, function(err) {
      session.currentTransaction().begin();
      testCase.errorIfNotTrue('t4 after begin transaction.isActive()', session.currentTransaction().isActive());
      session.persist(global.t_basic, object);
      testCase.errorIfNotTrue('t4 after persist transaction.isActive()', session.currentTransaction().isActive());
      session.remove(global.t_basic, 4083);
      testCase.errorIfNotTrue('t4 after remove transaction.isActive()', session.currentTransaction().isActive());
      session.find(global.t_basic, object, function(err, found) {
        testCase.errorIfNotTrue('t4 after find transaction.isActive()', session.currentTransaction().isActive());
        if (err) {
          testCase.appendErrorMessage('t4 find of inserted and deleted object returned error' + err);
        } else {
          testCase.errorIfNotNull('t4 should have removed object 4083', found);
        }
        session.currentTransaction().commit(function(err) {
          if (err) {
            testCase.appendErrorMessage('t4 returned error ' + err);
          }
          testCase.failOnError();
        });
      });
    });
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4];
