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

// TODO save with some but not all fields included

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
      batch.save(object, function(err) {
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
    session.save(object, function(err, session2) {
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
    var batch = session.createBatch();
    for (i = start_number; i < start_number + number_of_objects; i += 2) {
      batch.save(new global.t_basic(i, 'Employee ' + (i + 100), i + 100, i + 100), function(err) {
        if (err) {
          testCase.appendError(err);
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

/***** Save illegal parameter ***/
var t5 = new harness.ConcurrentTest("testSaveNoUpdate");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.save(1, function(err, session2) {
      if (err) {
        testCase.pass();
        return;
      }
      testCase.fail(new Error('SaveTest.t5 illegal argument should fail.'));
    });
  });
};

/***** Save illegal parameter ***/
var t6 = new harness.ConcurrentTest("testSaveNoUpdate");
t6.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.save('t_basic', function(err, session2) {
      if (err) {
        testCase.pass();
        return;
      }
      testCase.fail(new Error('SaveTest.t6 illegal argument should fail.'));
    });
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];
