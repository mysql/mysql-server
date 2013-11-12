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

var good_properties = global.test_conn_properties;

var bad_properties = {
  "implementation"    : global.test_conn_properties.implementation,
  "mysql_host"        : "localhost",
  "mysql_port"        : "3306",
  "mysql_user"        : "_BAD_USER_",
  "mysql_password"    : "_NOT_A_REAL_PASSWORD!_", 
  "ndb_connectstring" : "this_host_does_not_exist"
}

var tests = [];

/* 2.2.2 if onFulfilled is a function:
     2.2.2.1 it must be called after promise is fulfilled, with promise's value
             as its first argument.
     2.2.2.2 it must not be called before promise is fulfilled
     2.2.2.3 it must not be called more than once.
*/
var t_222 = new harness.ConcurrentTest("2.2.2");
t_222.run = function() {
  var n, p, test;
  n = 0;
  test = this;

  function onSession(session) {
    n++;
    assert(n === 1);  // 2.2.2.3 ; could happen after test has already passed
    test.errorIfUnset("onFulfilled gets a value", session);   // 2.2.2.1
    test.errorIfNotEqual("Not a session", typeof session.find, "function");
    test.errorIfNotEqual("typeof this (2.2.5)", typeof this, "undefined");

    session.close(function() { test.failOnError(); });
  }
  
  p = mynode.openSession(good_properties, null);
  if(typeof p === 'object') {
    p.then(onSession);
  } else {
    this.fail("not thenable");
  }
};
tests.push(t_222);



/* 2.2.3 if onRejected is a function:
     2.2.3.1 it must be called after promise is rejected, with promise's reason
       as its first argument.
     2.2.3.2 it must not be called before promise is rejected
     2.2.3.3 it must not be called more than once. 
*/
var t_223 = new harness.ConcurrentTest("2.2.3");
t_223.run = function() {
  var n, p, test;
  n = 0;
  test = this;
  
  function onSession(s) {
    assert("openSession should fail" === 0);
    s.close();
  }
  
  function onError(e) {
    n++;
    assert(n === 1);   // 2.2.3.3 ; could happen after test has passed
    test.errorIfUnset("onRejected must get a reason", e);
    test.errorIfNotEqual("typeof this (2.2.5)", typeof this, "undefined");
    test.failOnError();
  }
  
  p = mynode.openSession(bad_properties, null);
  if(typeof p === 'object') {
    p.then(onSession, onError);
  } else {
    this.fail("not thenable");
  }
};
tests.push(t_223);


/*  2.2.6 then may be called multiple times on the same promise.
      2.2.6.1 If/when promise is fulfilled, all respective onFulfilled callbacks
              must execute in the order of their originating calls to then.
      2.2.6.2 If/when promise is rejected, all respective onRejected callbacks 
              must execute in the order of their originating calls to then.
*/
var t_2261 = new harness.ConcurrentTest("2.2.6.1"); 
t_2261.run = function() {
  var n, p, test;
  n = 0;
  test = this;
  
  function onSession_1(s) {
    n++;
    test.errorIfNotEqual("Wrong order", n, 1);
  }

  function onSession_2(s) {
    n++;
    test.errorIfNotEqual("Wrong order", n, 2);
    s.close(function() { test.failOnError(); });
  }

  p = mynode.openSession(good_properties, null);
  if(typeof p === 'object') {
    p.then(onSession_1);
    p.then(onSession_2);
  } else {
    this.fail("not thenable");
  }  
};
tests.push(t_2261);

var t_2262 = new harness.ConcurrentTest("2.2.6.2");
t_2262.run = function() {
  var n, p, test;
  n = 0;
  test = this;

  function onSession(s) {
    test.fail("openSession should fail");
    s.close();
  }

  function onErr_1(e) {
    n++;
    test.errorIfNotEqual("Wrong order", n, 1);
  }
  
  function onErr_2(e) {
    n++;
    test.errorIfNotEqual("Wrong order", n, 2);
    test.failOnError();
  }

  p = mynode.openSession(bad_properties, null);
  if(typeof p === 'object') {
    p.then(onSession, onErr_1);
    p.then(onSession, onErr_2);
  } else {
    this.fail("not thenable");
  }
};
tests.push(t_2262);


/* 2.2.7 "then" must return a promise
*/
var t_227 = new harness.ConcurrentTest("2.2.7"); 
t_227.run = function() {
  var p1, p2, test;
  test = this;
  
  function onSession(s) {
    s.close(function() { test.failOnError(); });
  }

  p1 = mynode.openSession(good_properties, null);
  if(typeof p1 !== 'object' || typeof p1.then !== 'function') {
    this.fail("p1 not thenable");
  } else {
    p2 = p1.then(onSession);
    if(typeof p2 !== 'object' || typeof p2.then !== 'function') {
      this.appendErrorMessage("p2 not thenable");
    }
  }
};
tests.push(t_227);


module.exports.tests = tests;

