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

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5];
