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

/***** Save no update***/
var t1 = new harness.ConcurrentTest("testSaveNoUpdate");
t1.run = function() {
  var testCase = this;
  // create the domain object 4000
  var object = new global.t_basic(4000, 'Employee 4000', 4000, 4000);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.save(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4000, fail_verify_t_basic, 4000, testCase, true);
    }, session);
  });
};

/***** Batch save no update ***/
var t2 = new harness.ConcurrentTest("testBatchSaveNoUpdate");
t2.run = function() {
  var testCase = this;
  var start_number = 4010;
  var number_of_objects = 10;
  var i, object;
  
  fail_openSession(testCase, function(session) {
    var batch = session.createBatch();
    // create objects
    for (i = start_number; i < start_number + number_of_objects; ++i) {
      object = new global.t_basic(i, 'Employee ' + i, i, i);
      // use variant save(constructor, object, callback)
      batch.save(global.t_basic, object, function(err) {
        if (err) {
          testCase.appendErrorMessage(err);
        }
      });
    }
    batch.execute(function(err, session2) {
      if (err) {
        testCase.appendErrorMessage(err);
      }
      var batch2 = session2.createBatch();
      for (i = start_number; i < start_number + number_of_objects; ++i) {
        batch2.find(global.t_basic, i, verify_t_basic, i, testCase, true);
      }
      batch2.execute(function(err) {
        if (err) {
          testCase.appendErrorMessage(err);
        }
        testCase.failOnError();
      });
    }, session);
  });
};

/***** Save update***/
var t3 = new harness.ConcurrentTest("testSaveUpdate");
t3.run = function() {
  var testCase = this;
  // load the domain object 4020
  var object = new global.t_basic(4020, 'Employee 4020', 4020, 4020);
  var object2;
  fail_openSession(testCase, function(session) {
    // save object 4020
    // use variant save(tableName, object, callback)
    session.save('t_basic', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now save an object with the same primary key but different magic
      object2 = new global.t_basic(4020, 'Employee 4020', 4020, 4029);
      session2.save(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find('t_basic', 4020, function(err, object3) {
          // verify that object3 has updated magic field from object2
          testCase.errorIfNotEqual('testSaveUpdate mismatch on magic', 4029, object3.magic);
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};

/***** Batch save update ***/
var t4 = new harness.ConcurrentTest("testBatchSaveUpdate");
t4.run = function() {
  var testCase = this;
  var start_number = 4030;
  var number_of_objects = 10;
  // create objects
  
  fail_openSession(testCase, function(session) {
    var i;
    var batch = session.createBatch();
    for (i = start_number; i < start_number + number_of_objects; i += 2) {
      batch.save(new global.t_basic(i, 'Employee ' + (i + 100), i + 100, i + 100), function(err) {
        if (err) {
          testCase.appendErrorMessage(err);
        }
      });
    }
    batch.execute(function(err, session2) {
      if (err) {
        testCase.appendErrorMessage(err);
      }
      var batch2 = session2.createBatch();
      // save objects (on top of original objects)
      for (i = start_number; i < start_number + number_of_objects; ++i) {
        batch2.save(new global.t_basic(i, 'Employee ' + i, i, i), function(err) {
          if (err) {
            testCase.appendErrorMessage(err);
          }
        });
      }
      batch2.execute(function(err, session3) {
        if (err) {
         testCase.appendErrorMessage(err); 
        }
        var batch3 = session3.createBatch();
        // find all objects and verify they have the proper state
        for (i = start_number; i < start_number + number_of_objects; ++i) {
          batch3.find(global.t_basic, i, global.verify_t_basic, i, testCase, true);
        }
        batch3.execute(function(err) {
          if (err) {
            testCase.appendErrorMessage(err);
          }
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};

/***** Save partial ***/
var t7 = new harness.ConcurrentTest("testSavePartial");
t7.run = function() {
  var testCase = this;
  var object = new global.t_basic(4040, 'Employee 4040', 4040, 4040);
  var object2;
  fail_openSession(testCase, function(session) {
    session.save(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now save an object with the same primary key but different name and magic
      object2 = new global.t_basic(4040, 'Employee 4050', 4050, 4050);
      // remove age from update object --> age will not be changed
      delete object2.age;
      session2.save(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find('t_basic', 4040, function(err, object3) {
          // verify that object3 has updated name field from object2
          testCase.errorIfNotEqual('testSaveUpdate mismatch on name', 'Employee 4050', object3.name);
          testCase.errorIfNotEqual('testSaveUpdate mismatch on magic', 4050, object3.magic);
          // age should be unchanged
          testCase.errorIfNotEqual('testSaveUpdate mismatch on age', 4040, object3.age);
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};

/***** Save partial no update***/
var t8 = new harness.ConcurrentTest("testSavePartialNoUpdate");
t8.run = function() {
  var testCase = this;
  var object = new global.t_basic(4060, 'Employee 4060', 4060, 4060);
  // do not persist age --> age is null
  delete object.age;
  fail_openSession(testCase, function(session) {
    // save the object with just id, name, and magic
    session.save(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4060, function(err, object2) {
        if (err) {
          testCase.fail(err);
          return;
        }
        testCase.errorIfNotEqual('mismatch on id', 4060, object2.id);
        testCase.errorIfNotEqual('mismatch on name', 'Employee 4060', object2.name);
        testCase.errorIfNotEqual('mismatch on age', null, object2.age);
        testCase.errorIfNotEqual('mismatch on magic', 4060, object2.magic);
        testCase.failOnError();
      });
    }, session);
  });
};

module.exports.tests = [t1, t2, t3, t4, t7, t8];
