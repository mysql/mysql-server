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
    if(testCase.errorIfUnset(message, err && err.sqlstate)) 
    {
      testCase.errorIfNotEqual("Expected sqlstate", sqlState, err.sqlstate);
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
      testCase.appendErrorMessage('ValueVerifier caught unexpected e: ' + util.inspect(e));
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
      testCase.appendErrorMessage('ReadFunction err: ' + util.inspect(err));
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
  var data = new TestData(11);
  data.cYear = 1989;
  this.verifier = new ValueVerifier(this, "cYear", 1989);
  fail_openSession(this, InsertFunction(data));
}

// cDatetimeDefault
var t2 = new harness.ConcurrentTest("VerifyDatetimeDefault");
t2.run = function() {
  var data = new TestData(12);
  var expect = new Date("Thu, 09 Nov 1989 17:00:00"); // the column default
  this.verifier = new ValueVerifier(this, "cDatetimeDefault", expect);
  fail_openSession(this, InsertFunction(data));
}

// cDatetime
var t3 = new harness.ConcurrentTest("VerifyDatetime");
t3.run = function() {
  var data = new TestData(13);
  var now = new Date();
  now.setMilliseconds(0);
  data.cDatetime = now;
  this.verifier = new ValueVerifier(this, "cDatetime", now);
  fail_openSession(this, InsertFunction(data));
}

// cTime
var t4 = new harness.ConcurrentTest("VerifyTime");
t4.run = function() {
  var data = new TestData(14);
  data.cTime = "-00:10:00";
  this.verifier = new ValueVerifier(this, "cTime", "-00:10:00");
  fail_openSession(this, InsertFunction(data));
}

// cNullableTimestamp Thu, 01 Jan 1970 00:00:00 GMT
var t5 = new harness.ConcurrentTest("TimestampZero");
t5.run = function() {
  var data = new TestData(15);
  var dateZero = new Date(60*60*1000);
  data.cNullableTimestamp = dateZero;
  this.verifier = new ValueVerifier(this, "cNullableTimestamp", dateZero);
  fail_openSession(this, InsertFunction(data));
}

// cNullableTimestamp 1969.
// This should return 22007 INVALID DATETIME
var t6 = new harness.ConcurrentTest("Timestamp1969");
t6.run = function() {
  var data = new TestData(16);
  var date1969 = new Date(-10000);
  data.cNullableTimestamp = date1969;
  this.insertErrorVerifier = new ErrorVerifier(this, "22007");
  fail_openSession(this, InsertFunction(data));
}

// cNullableTimestamp 1970
var t7 = new harness.ConcurrentTest("Timestamp1970");
t7.run = function() {
  var data = new TestData(17);
  var date1970 = new Date(Date.UTC(1970, 0, 1, 3, 34, 30)); // 25 or 6 to 4
  data.cNullableTimestamp = date1970;
  this.verifier = new ValueVerifier(this, "cNullableTimestamp", date1970);
  fail_openSession(this, InsertFunction(data));
}

// cDate
var t8 = new harness.ConcurrentTest("Date");
t8.run = function() {
  var data = new TestData(18);
  var test_date = "1989-11-09";
  data.cDate = test_date;
  this.verifier = new ValueVerifier(this, "cDate", test_date);
  fail_openSession(this, InsertFunction(data));
}

// cTime: Special case 
// "nn:nn" must be treated as "HH:MM"
var t9 = new harness.ConcurrentTest("VerifyTime_HH:MM");
t9.run = function() {
  var data = new TestData(19);
  data.cTime = "13:22";
  this.verifier = new ValueVerifier(this, "cTime", "13:22:00");
  fail_openSession(this, InsertFunction(data));
}

// cTime: 
// String "nnnn" must be treated as "MM:SS"
var t10 = new harness.ConcurrentTest("VerifyTime_MMSS");
t10.run = function() {
  var data = new TestData(20);
  data.cTime = "1322";
  this.verifier = new ValueVerifier(this, "cTime", "00:13:22");
  fail_openSession(this, InsertFunction(data));
}

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10];
