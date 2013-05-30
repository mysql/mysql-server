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

// Domain Object Constructor
function TestData(id) {
  if(id) {
    this.id = id;
    this.cTimestamp = new Date();
  }
}

function ErrorVerifier(testCase, sqlState) {
  this.run = function onRead(err, rowRead) {
    var message = "Expected SQLState " + sqlState;
    if(testCase.errorIfUnset(message, err && err.cause && err.cause.sqlstate)) 
    {
      testCase.errorIfNotEqual("Expected sqlstate", sqlState, err.cause.sqlstate);
    }
    testCase.failOnError();
  };
}

function ValueVerifier(testCase, field, value) {
  this.run = function onRead(err, rowRead) {
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    /* Date objects can only be compared using Date.valueOf(), 
       so compare using x.valueOf(), but report using just x
    */
    try {
      if(value.valueOf() !== rowRead[field].valueOf()) {
        testCase.errorIfNotEqual(field, value, rowRead[field]);
      }
    }
    catch(e) {
      testCase.appendErrorMessage(e);
    }
    testCase.failOnError();
  };
}

function ReadFunction(testCase, session) { 
  return function onPersist(err) {
    if(testCase.insertErrorVerifier) {
      testCase.insertErrorVerifier.run(err);  
    }
    else if(err) {
      testCase.appendErrorMessage(err);
      testCase.failOnError();
    }
    else {
      session.find(TestData, testCase.data.id, testCase.verifier.run);
    }
  }
}

function InsertFunction(data) {
  return function onSession(session, testCase) {
    testCase.data = data;
    session.persist(data, ReadFunction(testCase, session));
  };
}

var mapping = new mynode.TableMapping("test.temporaltypes");
mapping.mapAllColumns = true;
mapping.applyToClass(TestData);

// cYear
var t1 = new harness.ConcurrentTest("VerifyYear");
t1.run = function() {
  var data = new TestData(1);
  data.cYear = 1989;
  this.verifier = new ValueVerifier(this, "cYear", 1989);
  fail_openSession(this, InsertFunction(data));
}

// cDatetimeDefault
var t2 = new harness.ConcurrentTest("VerifyDatetimeDefault");
t2.run = function() {
  var data = new TestData(2);
  var expect = new Date("Thu, 09 Nov 1989 17:00:00"); // the column default
  this.verifier = new ValueVerifier(this, "cDatetimeDefault", expect);
  fail_openSession(this, InsertFunction(data));
}

// cDatetime
var t3 = new harness.ConcurrentTest("VerifyDatetime");
t3.run = function() {
  var data = new TestData(3);
  var now = new Date();
  now.setMilliseconds(0);
  data.cDatetime = now;
  this.verifier = new ValueVerifier(this, "cDatetime", now);
  fail_openSession(this, InsertFunction(data));
}

// cTime
var t4 = new harness.ConcurrentTest("VerifyTime");
t4.run = function() {
  var data = new TestData(4);
  var now = Date.now();
  var plusTenMinutes = now + (10 * 60000);
  var diff = now - plusTenMinutes;   // A negative number 
  data.cTime = diff;
  this.verifier = new ValueVerifier(this, "cTime", diff);
  fail_openSession(this, InsertFunction(data));
}

// cNullableTimestamp Thu, 01 Jan 1970 00:00:00 GMT
var t5 = new harness.ConcurrentTest("TimestampZero");
t5.run = function() {
  var data = new TestData(5);
  var dateZero = new Date(0);
  data.cNullableTimestamp = dateZero;
  this.verifier = new ValueVerifier(this, "cNullableTimestamp", dateZero);
  fail_openSession(this, InsertFunction(data));
}

// cNullableTimestamp 1969.
// This should return 22008 INVALID DATETIME
var t6 = new harness.ConcurrentTest("Timestamp1969");
t6.run = function() {
  var data = new TestData(6);
  var date1969 = new Date(-10000);
  data.cNullableTimestamp = date1969;
  this.insertErrorVerifier = new ErrorVerifier(this, "22007");
  fail_openSession(this, InsertFunction(data));
}

// cNullableTimestamp 1970
var t7 = new harness.ConcurrentTest("Timestamp1970");
t7.run = function() {
  var data = new TestData(7);
  var date1970 = new Date(Date.UTC(1970, 0, 1, 3, 34, 30)); // 25 or 6 to 4
  data.cNullableTimestamp = date1970;
  this.verifier = new ValueVerifier(this, "cNullableTimestamp", date1970);
  fail_openSession(this, InsertFunction(data));
}

// cDate
var t8 = new harness.ConcurrentTest("Date");
t8.run = function() {
  var data = new TestData(8);
  var now = new Date(Date.UTC(1989, 10, 9));
  data.cDate = now;
  this.verifier = new ValueVerifier(this, "cDate", now);
  fail_openSession(this, InsertFunction(data));
}

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];
