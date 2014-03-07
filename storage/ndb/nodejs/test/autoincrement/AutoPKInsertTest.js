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
/*global fail_verify_autopk*/

/***** Persist with domain object ***/
var t1 = new harness.SerialTest("testPersistDomainObject");
t1.run = function() {
  var testCase = this;
  // create the domain object 4070
  var object = new global.autopk('Employee 4070', 4070, 4070);
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      var key = new global.autopk_magic_key(4070);
      session.find(global.autopk, key, fail_verify_autopk, 4070, testCase, true);
    });
  });
};

/***** Persist with constructor and domain object ***/
var t2 = new harness.SerialTest("testPersistConstructorAndObject");
t2.run = function() {
  var testCase = this;
  // create the domain object 4071
  var object = new global.autopk('Employee 4071', 4071, 4071);
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4071}, fail_verify_autopk, 4071, testCase, true);
    }, session);
  });
};

/***** Persist with table name and domain object ***/
var t3 = new harness.SerialTest("testPersistTableNameAndObject");
t3.run = function() {
  var testCase = this;
  // create the domain object 4072
  var object = new global.autopk('Employee 4072', 4072, 4072);
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4072}, fail_verify_autopk, 4072, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and literal ***/
var t4 = new harness.SerialTest("testPersistConstructorAndLiteral");
t4.run = function() {
  var testCase = this;
  // create the literal 4073
  var object = {name:'Employee 4073', age:4073, magic:4073};
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4073}, fail_verify_autopk, 4073, testCase, true);
    }, session);
  });
};

/***** Persist with table name and literal ***/
var t5 = new harness.SerialTest("testPersistTableNameAndLiteral");
t5.run = function() {
  var testCase = this;
  // create the literal 4074
  var object = {name:'Employee 4074', age:4074, magic:4074};
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4074}, fail_verify_autopk, 4074, testCase, true);
    }, session);
  });
};

/***** Persist with domain object and explicit primary key ***/
var t6 = new harness.ConcurrentTest("testPersistDomainObjectExplicitPK");
t6.run = function() {
  var testCase = this;
  // create the domain object 4075
  var object = new global.autopk('Employee 4075', 4075, 4075);
  object.id = 4075;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      var key = new global.autopk_magic_key(4075);
      session.find(global.autopk, key, fail_verify_autopk, 4075, testCase, true);
    });
  });
};

/***** Persist with constructor and domain object and explicit primary key ***/
var t7 = new harness.ConcurrentTest("testPersistConstructorAndObjectExplicitPK");
t7.run = function() {
  var testCase = this;
  // create the domain object 4076
  var object = new global.autopk('Employee 4076', 4076, 4076);
  object.id = 4076;
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4076}, fail_verify_autopk, 4076, testCase, true);
    }, session);
  });
};

/***** Persist with table name and domain object and explicit primary key ***/
var t8 = new harness.ConcurrentTest("testPersistTableNameAndObjectExplicitPK");
t8.run = function() {
  var testCase = this;
  // create the domain object 4077
  var object = new global.autopk('Employee 4077', 4077, 4077);
  object.id = 4077;
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4077}, fail_verify_autopk, 4077, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and literal and explicit primary key ***/
var t9 = new harness.ConcurrentTest("testPersistConstructorAndLiteralExplicitPK");
t9.run = function() {
  var testCase = this;
  // create the literal 4078
  var object = {id: 4078, name:'Employee 4078', age:4078, magic:4078};
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4078}, fail_verify_autopk, 4078, testCase, true);
    }, session);
  });
};

/***** Persist with table name and literal and explicit primary key ***/
var t10 = new harness.ConcurrentTest("testPersistTableNameAndLiteralExplicitPK");
t10.run = function() {
  var testCase = this;
  // create the literal 4079
  var object = {id: 4079, name:'Employee 4079', age:4079, magic:4079};
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4079}, fail_verify_autopk, 4079, testCase, true);
    }, session);
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10];
