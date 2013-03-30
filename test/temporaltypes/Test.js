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
  if(id) this.id = id;
  this.cTimestamp = new Date();
}

function VerifyFunction(testCase) {
  return function onRead(err, rowRead) {
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    testCase.errorIfNotEqual(testCase.fieldToVerify,
                             testCase.valueToVerify.valueOf(),
                             rowRead[testCase.fieldToVerify].valueOf());
    testCase.failOnError();
  };
}

function ReadFunction(testCase, session) { 
  return function onPersist(err) {
    testCase.errorIfError(err);
    session.find(TestData, testCase.data.id, VerifyFunction(testCase));
  };
}

function InsertFunction(data, fieldToVerify, valueToVerify) {
  return function onSession(session, testCase) {
    testCase.data = data;
    testCase.fieldToVerify = fieldToVerify;
    testCase.valueToVerify = valueToVerify;
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
  fail_openSession(this, InsertFunction(data, "cYear", 1989));
}

// cDatetimeDefault
var t2 = new harness.ConcurrentTest("VerifyDatetimeDefault");
t2.run = function() {
  var data = new TestData(2);
  var expect = new Date("November 9 1989 17:00:00 UTC"); // the column default
  fail_openSession(this, InsertFunction(data, "cDatetimeDefault", expect));
}

// cDatetime
var t3 = new harness.ConcurrentTest("VerifyDatetime");
t3.run = function() {
  var data = new TestData(3);
  var now = new Date();
  now.setMilliseconds(0);
  data.cDatetime = now;
  fail_openSession(this, InsertFunction(data, "cDatetime", now));
}

// cTime
var t4 = new harness.ConcurrentTest("VerifyTime");
t4.run = function() {
  var data = new TestData(4);
  var now = Date.now();
  var plusTenMinutes = now + (10 * 60000);
  var diff = now - plusTenMinutes;   // A negative number 
  data.cTime = diff;
  fail_openSession(this, InsertFunction(data, "cTime", diff));
}

// cNullableTimestamp Thu, 01 Jan 1970 00:00:00 GMT
var t5 = new harness.ConcurrentTest("TimestampZero");
t5.run = function() {
  var data = new TestData(5);
  var dateZero = new Date(0);
  data.cNullableTimestamp = dateZero;
  fail_openSession(this, InsertFunction(data, "cNullableTimestamp", dateZero));
}

// cNullableTimestamp 1969.
var t6 = new harness.ConcurrentTest("Timestamp1969");
t6.run = function() {
  var data = new TestData(6);
  var date1969 = new Date(-100);
  data.cNullableTimestamp = date1969;
  fail_openSession(this, InsertFunction(data, "cNullableTimestamp", date1969));
}

// cNullableTimestamp 1970
var t7 = new harness.ConcurrentTest("Timestamp1970");
t7.run = function() {
  var data = new TestData(7);
  var date1970 = new Date(Date.UTC(1970, 0, 1, 3, 34, 30)); // 25 or 6 to 4
  data.cNullableTimestamp = date1970;
  fail_openSession(this, InsertFunction(data, "cNullableTimestamp", date1970));
}


module.exports.tests = [t1, t2, t3, t4, t5, t6, t7];
