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
  }
}

var mapping = new mynode.TableMapping("mysql56times");
mapping.mapField("id");
mapping.mapField("Time1", "a");
mapping.mapField("Datetime2", "b");
mapping.mapField("Timestamp3", "c");
mapping.mapField("Time4", "d");
mapping.mapField("Datetime5", "e", null); // No Converter
mapping.mapField("Timestamp6", "f");
mapping.applyToClass(TestData);


function ValueVerifier(testCase, field, value) {
  this.run = function onRead(err, rowRead) {
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    /* Date objects can only be compared using Date.valueOf() */
    try {
      if(value.valueOf() !== rowRead[field].valueOf()) {
        testCase.errorIfNotEqual(field, value.valueOf(), rowRead[field].valueOf());
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
    testCase.errorIfError(err);
    session.find(TestData, testCase.data.id, testCase.verifier.run);
  }
}

function InsertFunction(data) {
  return function onSession(session, testCase) {
    testCase.data = data;
    session.persist(data, ReadFunction(testCase, session));
  };
}

// Time1. Tenths of a second.
var t1 = new harness.ConcurrentTest("t1_time_odd");
t1.run = function() {
  var data = new TestData(1);
  data.Time1 = "1.622";   // Milliseconds; will be rounded.
  this.verifier = new ValueVerifier(this, "Time1", "00:00:01.6");
  fail_openSession(this, InsertFunction(data));
}

// Datetime2.  Hundredths of a second.
// Mapped to a JavaScript date.
var t2 = new harness.ConcurrentTest("t2_datetime_even");
t2.run = function() {
  var data = new TestData(2);
  data.Datetime2 = new Date("Thu, 09 Nov 1989 17:00:00.1622");
  var expect = new Date("Thu, 09 Nov 1989 17:00:00.16"); // Round down
  this.verifier = new ValueVerifier(this, "Datetime2", expect);
  fail_openSession(this, InsertFunction(data));
}

// Timestamp3.  Thousandths of a second (Milliseconds).
// Mapped to a JavaScript date.
var t3 = new harness.ConcurrentTest("t3_timestamp_odd");
t3.run = function() {
  var data = new TestData(3);
  data.Timestamp3 = new Date("Thu, 09 Nov 1989 17:00:00.116");
  this.verifier = new ValueVerifier(this, "Timestamp3", data.Timestamp3);
  fail_openSession(this, InsertFunction(data));
}

// Time4.  S.xxxxZZZ
var t4 = new harness.ConcurrentTest("t4_time_even");
t4.run = function() {
  var data = new TestData(4);
  data.Time4 = "1.1116222";
  this.verifier = new ValueVerifier(this, "Time4", "00:00:01.1116");
  fail_openSession(this, InsertFunction(data));
}

/* TODO: TEST 5 IS WAITING FOR DECISIONS ABOUT THE CONVERTER APIS */

// Datetime5 without a Column Converter.
// 
var t5 = new harness.ConcurrentTest("t5_datetime_odd");
t5.run = function() {
  this.skip("Test requires API changes");
  var data = new TestData(5);
  //data.Datetime5 = new Date("
  // this.verifier = new ValueVerifier(this, "Datetime5", ... );
  //fail_openSession(this, InsertFunction(data));
}

// Timestamp6.  Precision will be lost.
var t6 = new harness.ConcurrentTest("t6_timestamp_even");
t6.run = function() {
  var data = new TestData(6);
  data.Timestamp6 = new Date("Thu, 09 Nov 1989 17:00:00.111116");
  var expect = new Date("Thu, 09 Nov 1989 17:00:00.111000");  // lost precision
  this.verifier = new ValueVerifier(this, "Timestamp6", expect);
  fail_openSession(this, InsertFunction(data));
}

// Time1. Tenths of a second.  Negative
var t7 = new harness.ConcurrentTest("t7_time_negative");
t7.run = function() {
  var data = new TestData(7);
  data.Time1 = "-21";
  this.verifier = new ValueVerifier(this, "Time1", "-00:00:21.0");
  fail_openSession(this, InsertFunction(data));
}

// Time4.  Negative.
var t8 = new harness.ConcurrentTest("t8_time_fractional_negative");
t8.run = function() {
  var data = new TestData(8);
  data.Time4 = "-21.0150";
  this.verifier = new ValueVerifier(this, "Time4", "-00:00:21.0150");
  fail_openSession(this, InsertFunction(data));
}


module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];
