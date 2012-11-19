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

/*global udebug_module */

var udebug = unified_debug.getLogger("t_basic/DifferentColumnName.js");

// Map a domain class with field names different from column names
var Different = function(id, name, age, magic) {
  this.getId = function() {return fid;};
  if (typeof(id) !== 'undefined') this.fid = id;
  if (typeof(name) !== 'undefined') this.fname = name;
  if (typeof(age) !== 'undefined') this.fage = age;
  if (typeof(magic) !== 'undefined') this.fmagic = magic;
};

/** Verify the instance or fail the test case */
fail_verify_Different = function(err, instance, id, testCase, domainObject) {
  if (err) {
    testCase.fail(err);
    return;
  }
  if (typeof(instance) !== 'object') {
    testCase.fail(new Error('Result for id ' + id + ' is not an object: ' + typeof(instance)));
  }
  if (instance === null) {
    testCase.fail(new Error('Result for id ' + id + ' is null.'));
    return;
  }
  if (domainObject) {
    if (typeof(instance.getId) !== 'function') {
      testCase.fail(new Error('Result for id ' + id + ' is not a Different domain object'));
      return;
    }
  }
  udebug.log_detail('instance:', instance);
  var message = '';
  if (instance.fid != id) {
    message += 'fail to verify id: expected: ' + id + ', actual: ' + instance.fid + '\n';
  }
  if (instance.fage != id) {
    message += 'fail to verify age: expected: ' + id + ', actual: ' + instance.fage + '\n';
  }
  if (instance.fmagic != id) {
    message += 'fail to verify magic: expected: ' + id + ', actual: ' + instance.fmagic + '\n';
  }
  if (instance.fname !== "Employee " + id) {
    message += 'fail to verify name: expected: ' + "Employee " + id + ', actual: ' + instance.fname + '\n';
  }
  if (message == '') {
    testCase.pass();
  } else {
    testCase.fail(message);
  }
};

var mapDifferent = function() {
  var annotations = new mynode.Annotations();
  annotations.mapClass(Different, {'table' : 't_basic',
    'fields': [
      {'fieldName' : 'fid', 'columnName' : 'id'},
      {'fieldName' : 'fname', 'columnName' : 'name'},
      {'fieldName' : 'fage', 'columnName' : 'age'},
      {'fieldName' : 'fmagic', 'columnName' : 'magic'}
    ]
  });

};

/***** Persist Different Find by number ***/
var t1 = new harness.ConcurrentTest("persistFindNumberDifferent");
t1.run = function() {
  var testCase = this;
  // create the domain object 6001
  var different = new Different(6001, 'Employee 6001', 6001, 6001);
  fail_openSession(testCase, function(session) {
    mapDifferent();
    // key and testCase are passed to fail_verify_Different as extra parameters
    session.persist(different, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(Different, 6001, fail_verify_Different, 6001, testCase, true);
    }, session);
  });
};

/***** Save Different Find by literal ***/
var t2 = new harness.ConcurrentTest("saveFindLiteralDifferent");
t2.run = function() {
  var testCase = this;
  // create the domain object 6002
  var different = new Different(6002, 'Employee 6002', 6002, 6002);
  fail_openSession(testCase, function(session) {
    mapDifferent();
    // key and testCase are passed to fail_verify_Different as extra parameters
    session.save(different, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(Different, {fid: 6002}, fail_verify_Different, 6002, testCase, true);
    }, session);
  });
};

/***** Save Different Find by object ***/
var t3 = new harness.ConcurrentTest("saveFindObjectDifferent");
t3.run = function() {
  var testCase = this;
  // create the domain object 6003
  var different = new Different(6003, 'Employee 6003', 6003, 6003);
  fail_openSession(testCase, function(session) {
    mapDifferent();
    // key and testCase are passed to fail_verify_Different as extra parameters
    session.save(different, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var different2 = new Different(6003);
      session2.find(Different, different2, fail_verify_Different, 6003, testCase, true);
    }, session);
  });
};

/***** Persist Save (Update) Load ***/
var t4 = new harness.ConcurrentTest("testPersistUpdateFindNumber");
t4.run = function() {
  var testCase = this;
  // save the domain object 6004
  var object = new Different(6004, 'Employee 6004', 6004, 6004);
  var object2;
  fail_openSession(testCase, function(session) {
    mapDifferent();
    // save object 6004
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now save an object with the same primary key but different magic
      object2 = new Different(6004, 'Employee 6004', 6004, 6009);
      session2.save(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        var object3 = new Different(6004);
        session3.load(object3, function(err, object4) {
          // verify that object3 has updated magic field from object2
          testCase.errorIfNotEqual('testSaveUpdate mismatch on magic', 6009, object4.fmagic);
          testCase.failOnError();
        }, object3);
      }, session2);
    }, session);
  });
};

/***** Persist Update Find by literal ***/
var t5 = new harness.ConcurrentTest("testPersistUpdateFindNumber");
t5.run = function() {
  var testCase = this;
  var object = new Different(6005, 'Employee 6005', 6005, 6005);
  var object2;
  fail_openSession(testCase, function(session) {
    mapDifferent();
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now save an object with the same primary key but different name
      object2 = new Different(6005, 'Employee 6009', 6005, 6005);
      session2.update(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find(Different, {fid: 6005}, function(err, object3) {
          // verify that object3 has updated name field from object2
          testCase.errorIfNotEqual('testSaveUpdate mismatch on fname', 'Employee 6009', object3.fname);
          testCase.failOnError();
        });
      }, session2);
    }, session);
  });
};


/***** Persist Remove Find by literal ***/
var t6 = new harness.ConcurrentTest("testPersistUpdateFindNumber");
t6.run = function() {
  var testCase = this;
  var object = new Different(6006, 'Employee 6006', 6006, 6006);
  var object2;
  fail_openSession(testCase, function(session) {
    mapDifferent();
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // now remove the object
      object2 = new Different(6006);
      session2.remove(object2, function(err, session3) {
        if (err) {
          testCase.fail(err);
          return;
        }
        session3.find(Different, {fid: 6006}, function(err, object3) {
          if (err) {
            testCase.fail(err);
          } else {
            if (object3) {
              testCase.fail(new Error('Find after remove should return null.'));
            } else {
              testCase.pass();
            }
          }
        });
      }, session2);
    }, session);
  });
};


module.exports.tests = [t1, t2, t3, t4, t5, t6];
