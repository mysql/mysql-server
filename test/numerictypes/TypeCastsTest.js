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
 id int NOT NULL,
  tposint int unsigned,
  tfloat float NOT NULL,
  tposfloat float unsigned,
  tdouble double,
  tnumber decimal(11,3),
  tposnumber decimal(11,3) unsigned
*/
function makePass(testCase) {
  return function() { testCase.pass(); }
}

function makeFail(testCase, message) {
  return function() { testCase.fail(message); }
}

function shouldSucceed(test, promise) {
  promise.then(makePass(test), makeFail(test, "Should succeed"));
};

function shouldGetError(test, sqlstate, promise) {
  var message = "should get error " + sqlstate;
  promise.then(makeFail(test, message), function(error) {
    test.errorIfNotEqual("SQLState", sqlstate, error.sqlstate);
    test.failOnError();
  })
};


// Write numbers to all columns
var t1 = new harness.ConcurrentTest("writeNumericTypes");
t1.run = function() {
  fail_openSession(t1, function(session) {
    shouldSucceed(t1, 
      session.persist("numerictypes",
                      { id: 301, tposint: 301, tfloat: -301.1,
                        tposfloat: 301.1, tdouble: 301.10000000, 
                        tnumber: -301.101, tposnumber: 301.101 }));
  });
}

// Write a string to a float column; should succeed
var t2 = new harness.ConcurrentTest("writeStringToFloatCol");
t2.run = function() {
  fail_openSession(t2, function(session) {
    shouldSucceed(t2, 
      session.persist("numerictypes", { id: 302, tfloat: "302.15" }));
  });
}


// Write string to double column: OK
var t3 = new harness.ConcurrentTest("writeStringToDoubleCol");
t3.run = function() {
  fail_openSession(t3, function(session) {
    shouldSucceed(t3,
      session.persist("numerictypes", 
                      { id: 303, tfloat: 303, tdouble: "303.1000000" }));
  });
}

// Write string to decimal column: OK
var t4 = new harness.ConcurrentTest("writeStringToDecimalCol");
t4.run = function() {
  fail_openSession(t4, function(session) {
    shouldSucceed(t4,
      session.persist("numerictypes", 
                      { id: 304, tfloat: 304, tdecimal: "-12345678.123" }));
  });
}

// Write positive int to unsigned float column: OK
var t5 = new harness.ConcurrentTest("writeIntToUnsignedFloat");
t5.run = function() {
  fail_openSession(t5, function(session) {
    shouldSucceed(t5,
      session.persist("numerictypes", { id: 305, tfloat: 305, tposfloat: 305 }));
  });
}


// Write negative int to unsigned float column: should fail with 22003
var t6 = new harness.ConcurrentTest("writeNegativeToUnsignedFloat");
t6.run = function() {
  fail_openSession(t6, function(session) {
    shouldGetError(t6, "22003",
      session.persist("numerictypes", { id: 306, tfloat: 306, tposfloat: -306 }));
  });
}

// Write negative value to unsigned decimal column: 22003
var t7 = new harness.ConcurrentTest("writeNegativeToUnsignedDecimal");
t7.run = function() {
  fail_openSession(t7, function(session) {
    shouldGetError(t7, "22003",
      session.persist("numerictypes", { id: 307, tfloat: 307, tposnumber: -307 }));
  });
}

// Write string containing negative value to unsigned float: 22003
var t8 = new harness.ConcurrentTest("writeStringNegativeToUnsignedFloat");
t8.run = function() {
  fail_openSession(t7, function(session) {
    shouldGetError(t8, "22003",
      session.persist("numerictypes", { id: 308, tfloat: 308, tposfloat: "-308"}));
  });
}


// Write string containing negative value to unsigned decimal:  22003 
var t9 = new harness.ConcurrentTest("writeStringNegativeToUnsignedDecimal");
t9.run = function() {
  fail_openSession(t9, function(session) {
    shouldGetError(t9, "22003", 
      session.persist("numerictypes", { id: 309, tfloat: 309, tposnumber: "-309"}));
  });
}


module.exports.tests = [ t1,t2,t3,t4,t5,t6,t7,t8,t9 ] ;