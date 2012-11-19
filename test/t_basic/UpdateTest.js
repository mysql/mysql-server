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

/***** Update ***/
var t1 = new harness.ConcurrentTest("testUpdate");
t1.run = function() {
  var testCase = this;
  // load the domain object 4020
  var object = new global.t_basic(5020, 'Employee 5020', 5020, 5020);
  var object2;
  fail_openSession(testCase, function(session) {
    // persist object 4020
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now update the object with the same primary key but different magic
      object2 = new global.t_basic(5020, 'Employee 5020', 5020, 5029);
      session2.update(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find('t_basic', 5020, function(err, object3) {
          // verify that object3 has updated magic field from object2
          testCase.errorIfNotEqual('testSaveUpdate mismatch on magic', 5029, object3.magic);
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};

/***** Batch update ***/
var t2 = new harness.ConcurrentTest("testBatchUpdate");
t2.run = function() {
  var testCase = this;
  var start_number = 5030;
  var number_of_objects = 10;
  // create objects
  
  fail_openSession(testCase, function(session) {
    var batch = session.createBatch();
    for (i = start_number; i < start_number + number_of_objects; ++i) {
      batch.persist(new global.t_basic(i, 'Employee ' + (i + 100), i + 100, i + 100), function(err) {
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
      // update all fields in the objects
      for (i = start_number; i < start_number + number_of_objects; ++i) {
        batch2.update(new global.t_basic(i, 'Employee ' + i, i, i), function(err) {
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

/***** Update ***/
var t3 = new harness.ConcurrentTest("testPartialUpdate");
t3.run = function() {
  var testCase = this;
  // load the domain object 5040
  var object = new global.t_basic(5040, 'Employee 5040', 5040, 5040);
  var object2;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now update the object with the same primary key but different name
      object2 = new global.t_basic(5040, 'Employee 5049');
      session2.update(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find('t_basic', 5040, function(err, object3) {
          // verify that object3 has updated name field from object2
          testCase.errorIfNotEqual('testPartialUpdate mismatch on name', 'Employee 5049', object3.name);
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};

/***** Batch update ***/
var t4 = new harness.ConcurrentTest("testBatchPartialUpdate");
t4.run = function() {
  var testCase = this;
  var start_number = 5050;
  var number_of_objects = 10;
  // create objects
  
  fail_openSession(testCase, function(session) {
    var batch = session.createBatch();
    for (i = start_number; i < start_number + number_of_objects; ++i) {
      batch.persist(new global.t_basic(i, 'Employee ' + (i + 100), i, i), function(err) {
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
      // update only the name
      for (i = start_number; i < start_number + number_of_objects; ++i) {
        batch2.update(new global.t_basic(i, 'Employee ' + i), function(err) {
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

/***** Update illegal argument ***/
var t5 = new harness.ConcurrentTest("testUpdateIllegalArgumentNumber");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.update(1, function(err, session2) {
      if (err) {
        testCase.pass();
        return;
      }
      testCase.fail(new Error('UpdateTest.t5 illegal argument should fail.'));
    });
  });
};

/***** Update illegal argument ***/
var t6 = new harness.ConcurrentTest("testUpdateIllegalArgumentString");
t6.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    session.update('t_basic', function(err, session2) {
      if (err) {
        testCase.pass();
        return;
      }
      testCase.fail(new Error('UpdateTest.t6 illegal argument should fail.'));
    });
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];
