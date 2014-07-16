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
  // create the domain object 4070 with auto-generated pk
  var object = new global.autopk('Employee 4010', 4010, 4010);
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      var key = new global.autopk_magic_key(4010);
      session.find(global.autopk, key, fail_verify_autopk, 4010, testCase, true);
    });
  });
};

/***** Persist with constructor and domain object ***/
var t2 = new harness.SerialTest("testPersistConstructorAndObject");
t2.run = function() {
  var testCase = this;
  // create the domain object 4020 with auto-generated pk
  var object = new global.autopk('Employee 4020', 4020, 4020);
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4020}, fail_verify_autopk, 4020, testCase, true);
    }, session);
  });
};

/***** Persist with table name and domain object ***/
var t3 = new harness.SerialTest("testPersistTableNameAndObject");
t3.run = function() {
  var testCase = this;
  // create the domain object 4030 with auto-generated pk
  var object = new global.autopk('Employee 4030', 4030, 4030);
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4030}, fail_verify_autopk, 4030, testCase, true);
    }, session);
  });
};

/***** Persist with constructor and literal ***/
var t4 = new harness.SerialTest("testPersistConstructorAndLiteral");
t4.run = function() {
  var testCase = this;
  // create the literal 4040 with auto-generated pk
  var object = {name:'Employee 4040', age:4040, magic:4040};
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4040}, fail_verify_autopk, 4040, testCase, true);
    }, session);
  });
};

/***** Persist with table name and literal ***/
var t5 = new harness.SerialTest("testPersistTableNameAndLiteral");
t5.run = function() {
  var testCase = this;
  // create the literal 4050 with auto-generated pk
  var object = {name:'Employee 4050', age:4050, magic:4050};
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4050}, fail_verify_autopk, 4050, testCase, true);
    }, session);
  });
};

/***** Persist with domain object and explicit primary key ***/
var t6 = new harness.SerialTest("testPersistDomainObjectExplicitPK");
t6.run = function() {
  var testCase = this;
  // create the domain object 4060 with explicit pk
  var object = new global.autopk('Employee 4060', 4060, 4060);
  object.id = 4060;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      var key = new global.autopk_magic_key(4060);
      session.find(global.autopk, key, fail_verify_autopk, 4060, testCase, true, true);
    });
  });
};

/***** Persist with constructor and domain object and explicit primary key ***/
var t7 = new harness.SerialTest("testPersistConstructorAndObjectExplicitPK");
t7.run = function() {
  var testCase = this;
  // create the domain object 4070 with explicit pk
  var object = new global.autopk('Employee 4070', 4070, 4070);
  object.id = 4070;
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        console.log('testPersistConstructorAndObjectExplicitPK', err);
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4070}, fail_verify_autopk, 4070, testCase, true, true);
    }, session);
  });
};

/***** Persist with table name and domain object and explicit primary key ***/
var t8 = new harness.SerialTest("testPersistTableNameAndObjectExplicitPK");
t8.run = function() {
  var testCase = this;
  // create the domain object 4080 with explicit pk
  var object = new global.autopk('Employee 4080', 4080, 4080);
  object.id = 4080;
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4080}, fail_verify_autopk, 4080, testCase, true, true);
    }, session);
  });
};

/***** Persist with constructor and literal and explicit primary key ***/
var t9 = new harness.SerialTest("testPersistConstructorAndLiteralExplicitPK");
t9.run = function() {
  var testCase = this;
  // create the literal 4090 with explicit pk
  var object = {id: 4090, name:'Employee 4090', age:4090, magic:4090};
  fail_openSession(testCase, function(session) {
    session.persist(global.autopk, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4090}, fail_verify_autopk, 4090, testCase, true, true);
    }, session);
  });
};

/***** Persist with table name and literal and explicit primary key ***/
var t10 = new harness.SerialTest("testPersistTableNameAndLiteralExplicitPK");
t10.run = function() {
  var testCase = this;
  // create the literal 4100 with explicit pk
  var object = {id: 4100, name:'Employee 4100', age:4100, magic:4100};
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_autopk as extra parameters
      session2.find(global.autopk, {magic: 4100}, fail_verify_autopk, 4100, testCase, true, true);
    }, session);
  });
};


/***** Persist with table name and literal and explicit primary key ***/
var t11 = new harness.SerialTest("testPersistDuplicateExplicitPK");
t11.run = function() {
  var testCase = this;
  // create the literal 4130 with explicit pk
  var object = {id: 4130, name:'Employee 4130', age:4130, magic:4130};
  fail_openSession(testCase, function(session) {
    session.persist('autopk', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // try to persist explicit duplicate key; should fail
      object.magic += 1; // don't fail due to duplicate unique key
      session2.persist('autopk', object, function(err) {
        if (err) {
//          console.log('testPersistDuplicateExplicitPK', err);
          testCase.pass();
        } else {
          testCase.fail('failed to detect duplicate key.');
        }
      });
    }, session);
  });
};



/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11];
