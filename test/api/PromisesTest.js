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

var properties = global.test_conn_properties;

var bad_properties = {
  "implementation"    : "mysql",
  "mysql_host"        : "localhost",
  "mysql_port"        : "3306",
  "mysql_socket"      : null,
  "mysql_user"        : "_BAD_USER_",
  "mysql_password"    : "_NOT_A_REAL_PASSWORD!_", 
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
    test.errorIfUnset("onFulfilled gets a value", session);
    test.errorIfNotEqual("calls to onFulfilled", n, 1);
    test.errorIfNotEqual("Not a session", typeof session.find, "function");

    session.close(function() { test.failOnError() });
  }
  
  p = mynode.openSession(properties, null);
  if(typeof p === 'object') {
    p.then(onSession);
  } else {
    this.fail("not thenable");
  }
};
tests.push(t_222);



/* 2.2.3 if onRejected is a function:
     2.2.2.1 it must be called after promise is rejected, with promise's reason
       as its first argument.
     2.2.3.2 it must not be called before promise is rejected
     2.2.2.3 it must not be called more than once. 
*/
var t_223 = new harness.ConcurrentTest("2.2.3");
t_223.run = function() {
  var n, p, test;
  n = 0;
  test = this;
  
  function onSession(s) {
    test.fail("openSession should fail");
    s.close();
  }
  
  function onError(e) {
    n++;
    test.errorIfUnset("onRejected must get a reason", e);
    test.errorIfNotEqual("calls to onRejected", n, 1);
  }
  
  p = mynode.openSession(bad_properties, null);
  if(typeof p === 'object') {
    p.then(onSession, onError);
  } else {
    this.fail("not thenable");
  }
};
tests.push(t_223);



module.exports.tests = tests;
