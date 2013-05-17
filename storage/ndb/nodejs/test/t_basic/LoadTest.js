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
/*jslint newcap: true */
/*global t_basic, verify_t_basic, fail_verify_t_basic */

/***** Load by primary key ***/
var t1 = new harness.ConcurrentTest("testLoad");
t1.run = function() {
  var testCase = this;
  // load the domain object 1
  var object = new global.t_basic(1);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.load(object, fail_verify_t_basic, object, 1, testCase, true);
  });
};

/***** Batch Load by primary key ***/
var t2 = new harness.ConcurrentTest("testBatchLoad");
t2.run = function() {
  var testCase = this;
  var number_of_objects = 10;
  // create objects
  
  fail_openSession(testCase, function(session) {
    var i, object;
    var batch = session.createBatch();
    for (i = 0; i < number_of_objects; ++i) {
      object = new global.t_basic(i);
      // key and testCase are passed to verify_t_basic as extra parameters
      batch.load(object, verify_t_basic, object, i, testCase, true);
    }
    batch.execute(function(err) {
      if (err) {
        testCase.appendErrorMessage(err);
      }
      testCase.failOnError();
    });
  });
};

/***** Load by unique key ***/
var t3 = new harness.ConcurrentTest("testLoadByUniqueKey");
t3.run = function() {
  var testCase = this;
  // load the domain object 1
  var object = new global.t_basic();
  object.magic = 3;
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.load(object, fail_verify_t_basic, object, 3, testCase, true);
  });
};

/***** Load by non-unique key must fail ***/
var t4 = new harness.ConcurrentTest("testLoadByNonUniqueKey");
t4.run = function() {
  var testCase = this;
  // load the domain object 1
  var object = new global.t_basic();
  object.age = 3;
  fail_openSession(testCase, function(session) {
    session.load(object, function(err) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail('load with non-unique key must fail.');
      }
    });
  });
};

/***** Load by string must fail ***/
var t5 = new harness.ConcurrentTest("testLoadByString");
t5.run = function() {
  var testCase = this;
  var object = 't_basic';
  fail_openSession(testCase, function(session) {
    session.load(object, function(err) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail('load with string must fail.');
      }
    });
  });
};

/***** Load by number must fail ***/
var t6 = new harness.ConcurrentTest("testLoadByNumber");
t6.run = function() {
  var testCase = this;
  // load the domain object 1
  var object = 1;
  fail_openSession(testCase, function(session) {
    session.load(object, function(err) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail('load with number must fail.');
      }
    });
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];
