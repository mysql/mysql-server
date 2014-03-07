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
/*global fail_verify_autouk */

/***** Persist with domain object ***/
var t1 = new harness.SerialTest("testPersistDomainObject");
t1.run = function() {
  var testCase = this;
  var key = 4070;
  // create the domain object 4070
  var object = new global.autouk(4070, 'Employee 4070', 4070);
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session.find(global.autouk, key, fail_verify_autouk, 4070, testCase, true);
    });
  });
};

/***** Persist with constructor and domain object ***/
var t2 = new harness.SerialTest("testPersistConstructorAndObject");
t2.run = function() {
  var testCase = this;
  // create the domain object 4071
  var object = new global.autouk(4071, 'Employee 4071', 4071);
  fail_openSession(testCase, function(session) {
    session.persist(global.autouk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4071, fail_verify_autouk, 4071, testCase, true);
    }, session);
  });
};

/***** Persist with table name and domain object ***/
var t3 = new harness.SerialTest("testPersistTableNameAndObject");
t3.run = function() {
  var testCase = this;
  // create the domain object 4072
  var object = new global.autouk(4072, 'Employee 4072', 4072);
  fail_openSession(testCase, function(session) {
    session.persist('autouk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4072, fail_verify_autouk, 4072, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and literal ***/
var t4 = new harness.SerialTest("testPersistConstructorAndLiteral");
t4.run = function() {
  var testCase = this;
  // create the literal 4073
  var object = {id: 4073, name:'Employee 4073', age:4073};
  fail_openSession(testCase, function(session) {
    session.persist(global.autouk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4073, fail_verify_autouk, 4073, testCase, true);
    }, session);
  });
};

/***** Persist with table name and literal ***/
var t5 = new harness.SerialTest("testPersistTableNameAndLiteral");
t5.run = function() {
  var testCase = this;
  // create the literal 4074
  var object = {id: 4074, name:'Employee 4074', age:4074};
  fail_openSession(testCase, function(session) {
    session.persist('autouk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4074, fail_verify_autouk, 4074, testCase, true);
    }, session);
  });
};

/***** Persist with domain object and explicit unique key ***/
var t6 = new harness.ConcurrentTest("testPersistDomainObjectExplicitUK");
t6.run = function() {
  var testCase = this;
  var key = 4075;
  // create the domain object 4075
  var object = new global.autouk(4075, 'Employee 4075', 4075);
  object.magic = 4075;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session.find(global.autouk, key, fail_verify_autouk, 4075, testCase, true);
    });
  });
};

/***** Persist with constructor and domain object and explicit unique key ***/
var t7 = new harness.ConcurrentTest("testPersistConstructorAndObjectExplicitUK");
t7.run = function() {
  var testCase = this;
  // create the domain object 4076
  var object = new global.autouk(4076, 'Employee 4076', 4076);
  object.magic = 4076;
  fail_openSession(testCase, function(session) {
    session.persist(global.autouk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4076, fail_verify_autouk, 4076, testCase, true);
    }, session);
  });
};

/***** Persist with table name and domain object and explicit unique key ***/
var t8 = new harness.ConcurrentTest("testPersistTableNameAndObjectExplicitUK");
t8.run = function() {
  var testCase = this;
  // create the domain object 4077
  var object = new global.autouk(4077, 'Employee 4077', 4077);
  object.magic = 4077;
  fail_openSession(testCase, function(session) {
    session.persist('autouk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4077, fail_verify_autouk, 4077, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and literal and explicit unique key ***/
var t9 = new harness.ConcurrentTest("testPersistConstructorAndLiteralExplicitUK");
t9.run = function() {
  var testCase = this;
  // create the literal 4078
  var object = {id: 4078, name:'Employee 4078', age:4078, magic: 4078};
  fail_openSession(testCase, function(session) {
    session.persist(global.autouk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4078, fail_verify_autouk, 4078, testCase, true);
    }, session);
  });
};

/***** Persist with table name and literal and explicit unique key ***/
var t10 = new harness.ConcurrentTest("testPersistTableNameAndLiteralExplicitUK");
t10.run = function() {
  var testCase = this;
  // create the literal 4079
  var object = {id: 4079, name:'Employee 4079', age:4079, magic: 4079};
  fail_openSession(testCase, function(session) {
    session.persist('autouk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autouk as extra parameters
      session2.find(global.autouk, 4079, fail_verify_autouk, 4079, testCase, true);
    }, session);
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10];
