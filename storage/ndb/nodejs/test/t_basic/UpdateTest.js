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
          if (!object3) {
            testCase.fail('t1 no result from find');
          } else {
            testCase.errorIfNotEqual('testSaveUpdate mismatch on magic', 5029, object3.magic);
            testCase.failOnError();
          }
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
    var i;
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
          if (!object3) {
            testCase.fail('t3 no result from find');
          } else {
            testCase.errorIfNotEqual('testPartialUpdate mismatch on name', 'Employee 5049', object3.name);
            testCase.failOnError();
          }
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
    var i;
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

/***** Update ***/
var t5 = new harness.ConcurrentTest("testUpdateTableName");
t5.run = function() {
  var testCase = this;
  // load the domain object 5220
  var object = new global.t_basic(5220, 'Employee 5220', 5220, 5220);
  var object2;
  fail_openSession(testCase, function(session) {
    // persist object 4020
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now update the object with the same primary key but different magic
      object2 = new global.t_basic(5220, 'Employee 5220', 5220, 5229);
      session2.update('t_basic', {id: 5220}, object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find('t_basic', 5220, function(err, object3) {
          // verify that object3 has updated magic field from object2
          if (!object3) {
            testCase.fail('t5 no result from find');
          } else {
            testCase.errorIfNotEqual('testSaveUpdate mismatch on magic', 5229, object3.magic);
            testCase.failOnError();
          }
        });
      }, session2);
    }, session);
  });
};

/***** Batch update ***/
var t6 = new harness.ConcurrentTest("testBatchUpdateTableName");
t6.run = function() {
  var testCase = this;
  var start_number = 5230;
  var number_of_objects = 10;
  // create objects
  
  fail_openSession(testCase, function(session) {
    var i;
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
        var object = new global.t_basic(i, 'Employee ' + i, i, i);
        batch2.update('t_basic', object, object, function(err) {
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
var t7 = new harness.ConcurrentTest("testPartialUpdateConstructor");
t7.run = function() {
  var testCase = this;
  // load the domain object 5240
  var object = new global.t_basic(5240, 'Employee 5240', 5240, 5240);
  var object2;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now update the object with the same primary key but different name
      object2 = new global.t_basic(5240, 'Employee 5249');
      delete object2.id;
      session2.update(global.t_basic, {id:5240}, object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find('t_basic', 5240, function(err, object3) {
          // verify that object3 has updated name field from object2
          testCase.errorIfNotEqual('testPartialUpdate mismatch on name', 'Employee 5249', object3.name);
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};

/***** Batch update ***/
var t8 = new harness.ConcurrentTest("testBatchPartialUpdateConstructor");
t8.run = function() {
  var testCase = this;
  var start_number = 5250;
  var number_of_objects = 10;
  var i;
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
        // update(constructor, key, value, callback)
        var key = new global.t_basic(i);
        var value = new global.t_basic(i, 'Employee ' + i);
        delete value.id;
        batch2.update(global.t_basic, key, value, function(err) {
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

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];
