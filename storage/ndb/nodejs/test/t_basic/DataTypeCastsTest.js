/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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


/* These tests use primary keys 300-399 */

/*  
  id int not null,
  name varchar(32),
  age int,
  magic int not null,
*/
function makePass(testCase) {
  return function() { testCase.pass(); };
}

function makeFail(testCase, message) {
  return function() { testCase.fail(message); };
}

function shouldSucceed(test, promise) {
  promise.then(makePass(test), makeFail(test, "Should succeed"));
}

function shouldGetError(test, sqlstate, promise) {
  var message = "should get error " + sqlstate;
  promise.then(makeFail(test, message), function(error) {
    test.errorIfNotEqual("SQLState", sqlstate, error.sqlstate);
    test.failOnError();
  });
}

/* Writing a string to an int column should succeed if the string 
   can be cast to a legal int value for the column.
*/
var t1 = new harness.ConcurrentTest("WriteStringToIntCol");
t1.run = function() {
  fail_openSession(t1, function(session) {
    shouldSucceed(t1, 
      session.persist("t_basic", { id: 301, age: 1, magic: 301}));
  });
};

/* Writing a string to an int column should fail with 22003 
   if the string cannot be cast to an int.
*/
var t2 = new harness.ConcurrentTest("WriteAlphaStringToIntCol");
t2.run = function() {
  fail_openSession(t2, function(session) {
    shouldGetError(t2, "HY000",
      session.persist("t_basic", { id: 302, age: "young", magic: 302}));
  });
};

/* Writing a string to an int column should fail with error 22003
   if the string can be cast to an int but the int is not valid for the column.
*/
var t3 = new harness.ConcurrentTest("WriteStringToIntCol:3");
t3.run = function() {
  fail_openSession(t3, function(session) {
    shouldGetError(t3, "22003",
      session.persist("t_basic", { id: 303, age: "-8589934592", magic: 303}));
  });
};

/* If you write a decimal number to an int column, MySQL rounds 
   it to the nearest int 
*/
var t4 = new harness.ConcurrentTest("WriteFloatToIntCol");
t4.run = function() {
  fail_openSession(t4, function(session) {
    shouldSucceed(t4,
      session.persist("t_basic", { id:304, age: 14.6, magic: 304}));
  });
};

/* If you write a string containing a decimal number to an int column,
   MySQL converts it to a number and then rounds the number to an int.
*/
var t5 = new harness.ConcurrentTest("WriteStringToIntCol:4");
t5.run = function() {
  fail_openSession(t5, function(session) {
    shouldSucceed(t5,
      session.persist("t_basic", { id:305, age: "15.6", magic: 305}));
  });
};

/* Writing a positive int to a string column should succeed. 
*/
var t6 = new harness.ConcurrentTest("WriteIntToStringCol");
t6.run = function() {
  fail_openSession(t6, function(session) {
    shouldSucceed(t6, 
      session.persist("t_basic", { id:306, name: 306, magic: 306}));
  });
};

/* Writing a negative number to a string column should succeed.
*/
var t7 = new harness.ConcurrentTest("WriteNumberToStringCol");
t7.run = function() {
  fail_openSession(t7, function(session) {
    shouldSucceed(t7,
      session.persist("t_basic", { id: 307, name: -307.14, magic: 307 }));
  });
};


/* Writing a string that's too long should give error SQLState 22001
*/
var t8 = new harness.ConcurrentTest("WriteTooLongString");
t8.run = function() {
  fail_openSession(t8, function(session) {
    shouldGetError(t8, "22001", 
      session.persist("t_basic", { id:308, magic:308, 
                                   name:"abcdefghijklmnopqrstuv ABCDEFGHIJK"}));
  });
};

module.exports.tests = [ t1 , t2 , t3 , t4 , t5 , t6 , t7 , t8 ];
