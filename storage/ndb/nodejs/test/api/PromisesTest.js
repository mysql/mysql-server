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
/*global fail_openSession */

var assert = require("assert");

var good_properties = global.test_conn_properties;

var bad_properties = {
  "implementation"    : global.test_conn_properties.implementation,
  "mysql_host"        : "localhost",
  "mysql_port"        : "3306",
  "mysql_user"        : "_BAD_USER_",
  "mysql_password"    : "_NOT_A_REAL_PASSWORD!_", 
  "ndb_connectstring" : "this_host_does_not_exist"
};

var tests = [];

/* 2.2.2 if onFulfilled is a function:
     2.2.2.1 it must be called after promise is fulfilled, with promise's value
             as its first argument.
     2.2.2.2 it must not be called before promise is fulfilled
     2.2.2.3 it must not be called more than once.
*/
var t_222 = new harness.ConcurrentTest("2.2.2 onFulfilled");
t_222.run = function() {
  var n, p, test;
  n = 0;
  test = this;

  function onSession(session) {
    n++;
    assert(n === 1);  // 2.2.2.3 ; could happen after test has already passed
    test.errorIfUnset("PromisesTest.t_222 onFulfilled gets a value", session);   // 2.2.2.1
    test.errorIfNotEqual("PromisesTest.t_222 Not a session", typeof session.find, "function");
    test.errorIfNotEqual("typeof this (2.2.5) " + this, "undefined", typeof this); 
    session.close(function() {
      test.failOnError();
      });
  }
  
  p = mynode.openSession(good_properties, null);
  if(typeof p === 'object' && typeof p.then === 'function') {
    p.then(onSession);
  } else {
    this.fail("PromisesTest.t_222 not thenable");
  }
};
tests.push(t_222);



/* 2.2.3 if onRejected is a function:
     2.2.3.1 it must be called after promise is rejected, with promise's reason
       as its first argument.
     2.2.3.2 it must not be called before promise is rejected
     2.2.3.3 it must not be called more than once. 
*/
var t_223 = new harness.ConcurrentTest("2.2.3 onRejected");
t_223.run = function() {
  var n, p, test;
  n = 0;
  test = this;
  
  function onSession(s) {
    assert("PromisesTest.t_223 openSession should fail" === 0);
    s.close();
  }
  
  function onError(e) {
    n++;
    assert(n === 1);   // 2.2.3.3 ; could happen after test has passed
    test.errorIfUnset("onRejected must get a reason", e);
    test.errorIfNotEqual("typeof this (2.2.5) " + this, "undefined", typeof this);
    test.failOnError();
  }
  
  p = mynode.openSession(bad_properties, null);
  if(typeof p === 'object' && typeof p.then === 'function') {
    p.then(onSession, onError);
  } else {
    this.fail("PromisesTest.t_223 not thenable");
  }
};
tests.push(t_223);


/*  2.2.6 then may be called multiple times on the same promise.
      2.2.6.1 If/when promise is fulfilled, all respective onFulfilled callbacks
              must execute in the order of their originating calls to then.
      2.2.6.2 If/when promise is rejected, all respective onRejected callbacks 
              must execute in the order of their originating calls to then.
*/
var t_2261 = new harness.ConcurrentTest("2.2.6.1 multiple onFulfilled called in order"); 
t_2261.run = function() {
  var n, p, test;
  n = 0;
  test = this;
  
  function onSession_1(s) {
    n++;
    test.errorIfNotEqual("PromisesTest.t_2261 wrong order; onSession_1 should be called first", 1, n);
  }

  function onSession_2(s) {
    n++;
    test.errorIfNotEqual("PromisesTest.t_2261 wrong order; onSession_2 should be called second", 2, n);
    s.close(function() { test.failOnError(); });
  }

  p = mynode.openSession(good_properties, null);
  if(typeof p === 'object' && typeof p.then === 'function') {
    p.then(onSession_1);
    p.then(onSession_2);
  } else {
    this.fail("PromisesTest.t_2261 not thenable");
  }  
};
tests.push(t_2261);

var t_2262 = new harness.ConcurrentTest("2.2.6.2 multiple onRejected called in order");
t_2262.run = function() {
  var n, p, test;
  n = 0;
  test = this;

  function onSession(s) {
    test.fail("PromisesTest.t_2262 openSession should fail");
    s.close();
  }

  function onErr_1(e) {
    n++;
    test.errorIfNotEqual("PromisesTest.t_2262 wrong order; onErr_1 should be called first", 1, n);
  }
  
  function onErr_2(e) {
    n++;
    test.errorIfNotEqual("PromisesTest.t_2262 wrong order; onErr_2 should be called second", 2, n);
    test.failOnError();
  }

  p = mynode.openSession(bad_properties, null);
  if(typeof p === 'object') {
    p.then(onSession, onErr_1);
    p.then(onSession, onErr_2);
  } else {
    this.fail("PromisesTest.t_2262 not thenable");
  }
};
tests.push(t_2262);


/* 2.2.7 "then" must return a promise
*/
var t_227 = new harness.ConcurrentTest("2.2.7 then returns a promise"); 
t_227.run = function() {
  var p1, p2, test;
  test = this;
  
  function onSession(s) {
    s.close(function() { test.failOnError(); });
  }

  p1 = mynode.openSession(good_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_227 p1 not thenable");
  } else {
    p2 = p1.then(onSession);
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_227 p2 not thenable");
    }
  }
};
tests.push(t_227);

/* 2.2.7.1 If onFulfilled returns a value x, run the Promise Resolution Procedure.
 */
var t_2271fv = new harness.ConcurrentTest("2.2.7.1 onFulfilled returns a value"); 
t_2271fv.run = function() {
  var p1, p2 ,p3, test, session;
  test = this;
  
  function onSession(s) {
    // onFulfilled returns a value; should fulfill p3 as well
    s.close();
    return 49;
  }

  function onP2Fulfilled(v) {
    test.errorIfNotEqual("t_2271fv onP2Fulfilled value", 49, v);
    test.failOnError();
  }

  p1 = mynode.openSession(good_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_227 p1 not thenable");
  } else {
    p2 = p1.then(onSession);
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_2271 p2 not thenable");
    }
    p3 = p2.then(onP2Fulfilled);
  }
};
tests.push(t_2271fv);

/* 2.2.7.1 If onRejected returns a value x, run the Promise Resolution Procedure.
 */
var t_2271rv = new harness.ConcurrentTest("2.2.7.1 onRejected returns a value"); 
t_2271rv.run = function() {
  var p1, p2 ,p3, test, session, onSession;
  test = this;
  
  function onError(err) {
    // onError returns a value; should fulfill p3 as well
    return 49;
  }

  function onP2Fulfilled(v) {
    test.errorIfNotEqual("t_2271rv onP2Rejected value", 49, v);
    test.failOnError();
  }

  function onP2Rejected(e) {
    test.fail('t_2271rv should not reject because onRejected returned a value.');
  }

  p1 = mynode.openSession(bad_properties, null);
  p1.name = 'p1';
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_227 p1 not thenable");
  } else {
    p2 = p1.then(onSession, onError);
    p2.name = 'p2';
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_2271 p2 not thenable");
    }
    // p3 tests for the result of p2
    p3 = p2.then(onP2Fulfilled, onP2Rejected);
    p3.name = 'p3';
  }
};
tests.push(t_2271rv);


/* 2.2.7.2 If onFulfilled throws an exception e, promise2 must be rejected with e as the reason. 
 */
var t_2272fe = new harness.ConcurrentTest("2.2.7.2 onFulfilled throws an error"); 
t_2272fe.run = function() {
  var p1, p2, p3, test, p2OnFulfilled;
  test = this;
  
  function onSession(s) {
    // onSession throws an exception; should reject p2
    s.close();
    throw new Error("PromisesTest.t_2272fe");
  }

  function p2OnRejected(e) {
    test.errorIfNotEqual('t_2272fe error value', 'PromisesTest.t_2272fe', e.message);
    test.failOnError();
  }

  p1 = mynode.openSession(good_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_2272fe p1 not thenable");
  } else {
    p2 = p1.then(onSession);
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_2271fe p2 not thenable");
    }
    p3 = p2.then(p2OnFulfilled, p2OnRejected);
  }
};
tests.push(t_2272fe);

/* 2.2.7.2 If onRejected throws an exception e, promise2 must be rejected with e as the reason. 
 */
var t_2272re = new harness.ConcurrentTest("2.2.7.2 onRejected throws an error"); 
t_2272re.run = function() {
  var p1, p2, p3, test, onSession, p2OnFulfilled;
  test = this;
  
  function onError(err) {
    // onError throws an exception; should reject p2
    throw new Error("PromisesTest.t_2272re");
  }

  function p2OnRejected(e) {
    test.errorIfNotEqual('t_2272re error value', 'PromisesTest.t_2272re', e.message);
    test.failOnError();
  }

  p1 = mynode.openSession(bad_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_2272re p1 not thenable");
  } else {
    p2 = p1.then(onSession, onError);
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_227 p2 not thenable");
    }
    p3 = p2.then(p2OnFulfilled, p2OnRejected);
  }
};
tests.push(t_2272re);

/* 2.2.7.3 If onFulfilled is not a function and promise1 is fulfilled, promise2 must be fulfilled with the same value.
 */
var t_2273 = new harness.ConcurrentTest("2.2.7.3 missing onFulfilled"); 
t_2273.run = function() {
  var p1, p2, p3, test;
  test = this;
  
  function p2OnFulfilled(v) {
    v.close(function(){test.failOnError();});
  }

  p1 = mynode.openSession(good_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_2273 p1 not thenable");
  } else {
    p2 = p1.then();
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_2273 p2 not thenable");
    }
    p3 = p2.then(p2OnFulfilled);
  }
};
tests.push(t_2273);

/* 2.2.7.4 If onRejected is not a function and promise1 is rejected, promise2 must be rejected with the same reason.
 */
var t_2274 = new harness.ConcurrentTest("2.2.7.4 missing onRejected"); 
t_2274.run = function() {
  var p1, p2, p3, test, p2OnFulfilled;
  test = this;

  function p2OnRejected(err) {
    test.failOnError();
  }

  p1 = mynode.openSession(bad_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("PromisesTest.t_227 p1 not thenable");
  } else {
    p2 = p1.then();
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("PromisesTest.t_227 p2 not thenable");
    }
    p3 = p2.then(p2OnFulfilled, p2OnRejected);
  }
};
tests.push(t_2274);

/* 2.3 If x is a thenable, it attempts to make promise adopt the state of x, under the assumption that 
 * x behaves at least somewhat like a promise. 
 * Otherwise, it fulfills promise with the value x.
 */
var t_23f = new harness.ConcurrentTest("2.3 The Promise Resolution Procedure: fulfilled");
t_23f.run = function() {
  var test = this;
  var session;
  var p1, p2, p3, p4;
  
  function onSession(s) {
    session = s;
    p4 = s.close();
    return p4;
  }
  
  function reportSuccess(result) {
    if (result) {
    test.appendErrorMessage('t_23f result of session.close should be null or undefined but is ' + result, result);
    }
    test.failOnError();
  }
  
  function reportFailure(err) {
    test.fail('t_23f failed', err);
  }
  
  // t_23 begins here
  p1 = mynode.openSession(good_properties);
  p2 = p1.then(onSession);
  p3 = p2.then(reportSuccess, reportFailure);
};
tests.push(t_23f);

/* 2.3 If x is a thenable, it attempts to make promise adopt the state of x, under the assumption that 
 * x behaves at least somewhat like a promise. 
 * Otherwise, it fulfills promise with the value x.
 */
var t_23r = new harness.ConcurrentTest("2.3 The Promise Resolution Procedure: rejected");
t_23r.run = function() {
  var test = this;
  var session;
  var p1, p2, p3, p4;
  
  function onSession(s) {
    session = s;
    p4 = s.close();
    return p4;
  }
  
  function reportSuccess(result) {
    test.errorIfNotNull('t_23r result of session.close should be null', result);
    test.fail('t_23r should fail.');
  }
  
  function reportFailure(err) {
    // make sure err is what we expect it to be
    if (typeof(err.message) !== 'string') {
      test.appendErrorMessage('t_23r err.message must be a string.');
    }
    if (typeof(err.sqlstate) === 'string') {
      test.errorIfNull('t_23r err.sqlstate', err.sqlstate.match('0800'));
    } else {
      test.appendErrorMessage('t_23r err.sqlstate must be a string.');
    }
    test.failOnError();
  }
  
  // t_23 begins here
  p1 = mynode.openSession(bad_properties);
  p2 = p1.then(onSession);
  p3 = p2.then(reportSuccess, reportFailure);
};
tests.push(t_23r);

/** Transaction state tests.
 * Helpers is a closure of testCase, session, errorSqlStateMatch, and errorMessageMatch.
 * Methods of Helpers are called with no 'this' and they use the original parameters passed.
 */
function createHelpers(testCase, session, errorSqlStateMatch, errorMessageMatch) {
  return {
    begin : function() {
      var result = session.currentTransaction().begin();
      return result;
    },
    commit : function() {
      return session.currentTransaction().commit();
    },
    activeBeginFails : function(err) {
      // check for the right err
      testCase.errorIfNull('activeBeginFails: ' + err.message,
          err.message.match('Active cannot begin'));
      throw err;
    },
    rollback : function() {
      return session.currentTransaction().rollback();
    },
    setRollbackOnly : function() {
      session.currentTransaction().setRollbackOnly();
    },
    errorReportSuccess : function() {
      testCase.fail('must return an error but did not.');
    },
    checkReportFailure : function(err) {
      if (errorSqlStateMatch) {
        if (typeof(err.sqlstate) === 'string') {
          testCase.errorIfNull('wrong sqlstate: ' + err.sqlstate + '; should match ' + errorSqlStateMatch,
              err.sqlstate.match(errorSqlStateMatch));
        } else {
          testCase.appendErrorMessage('err.sqlstate must be a string.');
        }
      }
      if (errorMessageMatch) {
        if (typeof(err.message) === 'string') {
          testCase.errorIfNull('wrong err.message ' + err.message + '; should match ' + errorMessageMatch,
              err.message.match(errorMessageMatch));
        } else {
          testCase.appendErrorMessage('err.message must be a string.');
        }
      }
      if (session.currentTransaction().isActive()) {
        session.currentTransaction().rollback(function(err) {
          if (err) {
            err.message += '\n failure occurred on rollback after test in checkReportFailure';
            testCase.fail(err);
          } else {
            testCase.failOnError();
          }
        });
      } else {
        testCase.failOnError();
      }
    }
  };
}

var testIdleCommit = new harness.ConcurrentTest('Idle commit must fail');
testIdleCommit.run = function() {
  var testCase = this;
  mynode.openSession(good_properties, null, function(err, session) {
    testCase.session = session;
    if (err) throw err;
    var helpers = createHelpers(testCase, session, '2500', 'Idle cannot commit');
    helpers.commit().
      then(helpers.errorReportSuccess, helpers.checkReportFailure);
  });
};
tests.push(testIdleCommit);

var testIdleRollback = new harness.ConcurrentTest('Idle rollback must fail');
testIdleRollback.run = function() {
  var testCase = this;
  mynode.openSession(good_properties, null, function(err, session) {
    testCase.session = session;
    if (err) throw err;
    var helpers = createHelpers(testCase, session, '2500', 'Idle cannot rollback');
    helpers.rollback().
      then(helpers.errorReportSuccess, helpers.checkReportFailure);
  });
};
tests.push(testIdleRollback);

var testActiveBegin = new harness.ConcurrentTest('Active begin must fail');
testActiveBegin.run = function() {
  var testCase = this;
  mynode.openSession(good_properties, null, function(err, session) {
    if (err) throw err;
    testCase.session = session;
    var helpers = createHelpers(testCase, session, '2500', 'Active cannot begin');
    var p1 = helpers.begin();
      var p2 = p1.then(helpers.begin);
      p2.name = 'p2';
      var p3 = p2.then(helpers.errorReportSuccess, helpers.checkReportFailure);
      p3.name = 'p3';
  });
};
tests.push(testActiveBegin);

var testRollbackOnlyBegin = new harness.ConcurrentTest('RollbackOnly begin must fail');
testRollbackOnlyBegin.run = function() {
  var testCase = this;
  mynode.openSession(good_properties, null, function(err, session) {
    testCase.session = session;
    if (err) throw err;
    var helpers = createHelpers(testCase, session, '2500', 'RollbackOnly cannot begin');
    helpers.begin().
      then(helpers.setRollbackOnly).
      then(helpers.begin).
      then(helpers.errorReportSuccess, helpers.checkReportFailure);
  });
};
tests.push(testRollbackOnlyBegin);

var testRollbackOnlyCommit = new harness.ConcurrentTest('RollbackOnly commit must fail');
testRollbackOnlyCommit.run = function() {
  var testCase = this;
  mynode.openSession(good_properties, null, function(err, session) {
    testCase.session = session;
    if (err) throw err;
    var helpers = createHelpers(testCase, session, '2500', 'RollbackOnly cannot commit');
    helpers.begin().
      then(helpers.setRollbackOnly).
      then(helpers.commit).
      then(helpers.errorReportSuccess, helpers.checkReportFailure);
  });
};
tests.push(testRollbackOnlyCommit);

// need to test "then" when the promise has already been fulfilled or rejected

module.exports.tests = tests;
