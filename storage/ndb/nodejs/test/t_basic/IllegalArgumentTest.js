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

var t1 = new harness.SerialTest("testPersistIllegalArgument");
t1.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.persist(1, null, function(err) {
        if (err) {
          testCase.pass();
        } else {
          testCase.fail('t1 persist with illegal argument must fail.');
        }
      });
    } catch(err) {
      testCase.pass();
    }
  });
};

var t2 = new harness.SerialTest("testRemoveIllegalArgument");
t2.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.remove(1, null, function(err) {
        if (err) {
          testCase.pass();
        } else {
          testCase.fail('t2 remove with illegal argument must fail.');
        }
      });
    } catch(err) {
      testCase.pass();
    }
  });
};

var t3 = new harness.SerialTest("testSaveIllegalArgument");
t3.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.save(1, null, function(err) {
        if (err) {
          testCase.pass();
        } else {
          testCase.fail('t3 save with illegal argument must fail.');
        }
      });
    } catch(err) {
      testCase.pass();
    }
  });
};

var t4 = new harness.SerialTest("testUpdateIllegalArgument");
t4.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.update(1, null, null, function(err) {
        if (err) {
          testCase.pass();
        } else {
          testCase.fail('t4 update with illegal argument must fail.');
        }
      });
    } catch(err) {
      testCase.pass();
    }
  });
};

var t5 = new harness.SerialTest("testFindIllegalArgument");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    try {
      session.find(1, null, function(err) {
        if (err) {
          testCase.pass();
        } else {
          testCase.fail('t5 find with illegal argument must fail.');
        }
      });
    } catch(err) {
      testCase.pass();
    }
  });
};

var t11 = new harness.SerialTest("testPersistNoArgumentNoCallback");
t11.run = function() {
  var testCase = this;
  var promise;
  function test_api(session) {
    return session.persist();
  }
  function fail_to_openSession(err) {
    // fail_openSession has already failed this test so we need to fail it
    testCase.fail('t11 openSession failed:' + err);
  }
  function test_api_fulfill() {
    testCase.fail('t11 persist with no arguments must fail.');
  }
  function test_api_reject(err) {
    testCase.errorIfNull('t11 error message does not contain \'User error\': ' + err.message, 
        err.message.match('User error'));
    testCase.failOnError();
  }
  promise = fail_openSession(testCase);
  promise.then(test_api, fail_to_openSession).
    then(test_api_fulfill, test_api_reject);
};

var t12 = new harness.SerialTest("testRemoveNoArgumentNoCallback");
t12.run = function() {
  var testCase = this;
  var promise;
  function test_api(session) {
    return session.remove();
  }
  function test_api_fulfill() {
    testCase.fail('t12 remove with no arguments must fail.');
  }
  function fail_to_openSession(err) {
    // fail_openSession has already failed this test so we need to fail it
    testCase.fail('t12 openSession failed:' + err);
  }
  function test_api_reject(err) {
    testCase.errorIfNull('t12 error message does not contain \'User error\': ' + err.message, 
        err.message.match('User error'));
    testCase.failOnError();
  }
  promise = fail_openSession(testCase);
  promise.then(test_api, fail_to_openSession).
    then(test_api_fulfill, test_api_reject);
};

var t13 = new harness.SerialTest("testSaveNoArgumentNoCallback");
t13.run = function() {
  var testCase = this;
  var promise;
  function test_api(session) {
    return session.save();
  }
  function fail_to_openSession(err) {
    // fail_openSession has already failed this test so we need to fail it
    testCase.fail('t13 openSession failed:' + err);
  }
  function test_api_fulfill() {
    testCase.fail('t12 save with no arguments must fail.');
  }
  function test_api_reject(err) {
    testCase.errorIfNull('t13 error message does not contain \'User error\': ' + err.message, 
        err.message.match('User error'));
    testCase.failOnError();
  }
  promise = fail_openSession(testCase);
  promise.then(test_api, fail_to_openSession).
    then(test_api_fulfill, test_api_reject);
};

var t14 = new harness.SerialTest("testUpdateNoArgumentNoCallback");
t14.run = function() {
  var testCase = this;
  var promise;
  function test_api(session) {
    return session.update();
  }
  function fail_to_openSession(err) {
    // fail_openSession has already failed this test so we need to fail it
    testCase.fail('t14 openSession failed:' + err);
  }
  function test_api_fulfill() {
    testCase.fail('t14 update with no arguments must fail.');
  }
  function test_api_reject(err) {
    testCase.errorIfNull('t14 error message does not contain \'User error\': ' + err.message, 
        err.message.match('User error'));
    testCase.failOnError();
  }
  promise = fail_openSession(testCase);
  promise.then(test_api, fail_to_openSession).
    then(test_api_fulfill, test_api_reject);
};

var t15 = new harness.SerialTest("testFindNoArgumentNoCallback");
t15.run = function() {
  var testCase = this;
  var promise;
  function test_api(session) {
    return session.find();
  }
  function fail_to_openSession(err) {
    // fail_openSession has already failed this test so we need to fail it
    testCase.fail('t15 openSession failed:' + err);
  }
  function test_api_fulfill() {
    testCase.fail('t15 find with no arguments must fail.');
  }
  function test_api_reject(err) {
    testCase.errorIfNull('t15 error message does not contain \'User error\': ' + err.message, 
        err.message.match('User error'));
    testCase.failOnError();
  }
  promise = fail_openSession(testCase);
  promise.then(test_api, fail_to_openSession).
    then(test_api_fulfill, test_api_reject);
};

var t16 = new harness.SerialTest("testFindUndefinedSecondArgumentNoCallback");
t16.run = function() {
  var testCase = this;
  var promise;
  function test_api(session) {
    return session.find('t_basic');
  }
  function fail_to_openSession(err) {
    // fail_openSession has already failed this test so we need to fail it
    testCase.fail('t16 openSession failed:' + err);
  }
  function test_api_fulfill() {
    testCase.fail('t16 find with no second argument must fail.');
  }
  function test_api_reject(err) {
    testCase.errorIfNull('t16 error message does not contain \'User error\': ' + err.message, 
        err.message.match('User error'));
    testCase.failOnError();
  }
  promise = fail_openSession(testCase);
  promise.then(test_api, fail_to_openSession).
    then(test_api_fulfill, test_api_reject);
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5, t11, t12, t13, t14, t15, t16];
