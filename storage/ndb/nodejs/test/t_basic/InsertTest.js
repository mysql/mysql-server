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
"use strict";
/*global t_basic, verify_t_basic, fail_verify_t_basic */

/***** Persist with domain object ***/
var t1 = new harness.SerialTest("testPersistDomainObject");
t1.run = function() {
  var testCase = this;
  // create the domain object 4070
  var object = new global.t_basic(4070, 'Employee 4070', 4070, 4070);
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_t_basic as extra parameters
      session2.find(global.t_basic, 4070, fail_verify_t_basic, 4070, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and domain object ***/
var t2 = new harness.SerialTest("testPersistConstructorAndObject");
t2.run = function() {
  var testCase = this;
  // create the domain object 4071
  var object = new global.t_basic(4071, 'Employee 4071', 4071, 4071);
  fail_openSession(testCase, function(session) {
    session.persist(global.t_basic, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_t_basic as extra parameters
      session2.find(global.t_basic, 4071, fail_verify_t_basic, 4071, testCase, true);
    }, session);
  });
};

/***** Persist with table name and domain object ***/
var t3 = new harness.SerialTest("testPersistTableNameAndObject");
t3.run = function() {
  var testCase = this;
  // create the domain object 4072
  var object = new global.t_basic(4072, 'Employee 4072', 4072, 4072);
  fail_openSession(testCase, function(session) {
    session.persist('t_basic', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_t_basic as extra parameters
      session2.find(global.t_basic, 4072, fail_verify_t_basic, 4072, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and literal ***/
var t4 = new harness.SerialTest("testPersistConstructorAndLiteral");
t4.run = function() {
  var testCase = this;
  // create the literal 4073
  var object = {id:4073, name:'Employee 4073', age:4073, magic:4073};
  fail_openSession(testCase, function(session) {
    session.persist(global.t_basic, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_t_basic as extra parameters
      session2.find(global.t_basic, 4073, fail_verify_t_basic, 4073, testCase, true);
    }, session);
  });
};

/***** Persist with table name and literal ***/
var t5 = new harness.SerialTest("testPersistTableNameAndLiteral");
t5.run = function() {
  var testCase = this;
  // create the literal 4074
  var object = {id:4074, name:'Employee 4074', age:4074, magic:4074};
  fail_openSession(testCase, function(session) {
    session.persist('t_basic', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_t_basic as extra parameters
      session2.find(global.t_basic, 4074, fail_verify_t_basic, 4074, testCase, true);
    }, session);
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5];
