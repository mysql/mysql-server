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

// This file is UTF-8 encoded, begins with a UTF-8 BOM, and contains
// unusual unicode characters.

"use strict";

// Domain Object Constructor
var test_id = 1;
function TestData() {
  if(! this.id) {
    this.id = test_id++;
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
    try {
      if(value !== rowRead[field]) {
        testCase.errorIfNotEqual("length", value.length, rowRead[field].length);
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
    if(err) {
      testCase.fail(err.message);
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

var mapping = new mynode.TableMapping("test.mysql56strings");
mapping.mapAllColumns = true;
mapping.applyToClass(TestData);


var t1 = new harness.ConcurrentTest("1:str_fix_utf16le:ascii");
t1.run = function() {
  var data = new TestData();
  var value = "By early 1992 the";
  data.str_fix_utf16le = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf16le", value);
  fail_openSession(this, InsertFunction(data));
}

var t2 = new harness.ConcurrentTest("2:str_fix_utf16le:nonascii");
t2.run = function() {
  var data = new TestData();
  var value = "search for ☕";
  data.str_fix_utf16le = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf16le", value);
  fail_openSession(this, InsertFunction(data));
}

var t3 = new harness.ConcurrentTest("3:str_var_utf16le:ascii");
t3.run = function() {
  var data = new TestData();
  var value = "good byte-stream";
  data.str_var_utf16le = value;
  this.verifier = new ValueVerifier(this, "str_var_utf16le", value);
  fail_openSession(this, InsertFunction(data));
}

var t4 = new harness.ConcurrentTest("4:str_var_utf16le:nonascii");
t4.run = function() {
  var data = new TestData();
  var value = "éncØding of multi-";
  data.str_var_utf16le = value;
  this.verifier = new ValueVerifier(this, "str_var_utf16le", value);
  fail_openSession(this, InsertFunction(data));
}

var t5 = new harness.ConcurrentTest("5:str_fix_utf8mb4:ascii");
t5.run = function() {
  var data = new TestData();
  var value = "byte character sets.";
  data.str_fix_utf8mb4 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf8mb4", value);
  fail_openSession(this, InsertFunction(data));
}

var t6 = new harness.ConcurrentTest("6:str_fix_utf8mb4:nonascii");
t6.run = function() {
  var data = new TestData();
  var value = "The dráft ÎSÖ 10646";
  data.str_fix_utf8mb4 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf8mb4", value);
  fail_openSession(this, InsertFunction(data));
}

var t7 = new harness.ConcurrentTest("7:str_var_utf8mb4:ascii");
t7.run = function() {
  var data = new TestData();
  var value = "standard contained";
  data.str_var_utf8mb4 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf8mb4", value);
  fail_openSession(this, InsertFunction(data));
}

var t8 = new harness.ConcurrentTest("8:str_var_utf8mb4:nonascii");
t8.run = function() {
  var data = new TestData();
  var value = "a nÒn-rÉquired Ânnex";
  data.str_var_utf8mb4 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf8mb4", value);
  fail_openSession(this, InsertFunction(data));
}

var t9 = new harness.ConcurrentTest("9:str_fix_utf8mb3:ascii");
t9.run = function() {
  var data = new TestData();
  var value = "called UTF-1 that";
  data.str_fix_utf8mb3 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf8mb3", value);
  fail_openSession(this, InsertFunction(data));
}

var t10 = new harness.ConcurrentTest("10:str_fix_utf8mb3:nonascii");
t10.run = function() {
  var data = new TestData();
  var value = "provided a ☃ byte-";
  data.str_fix_utf8mb3 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf8mb3", value);
  fail_openSession(this, InsertFunction(data));
}

var t11 = new harness.ConcurrentTest("11:str_var_utf8mb3:ascii");
t11.run = function() {
  var data = new TestData();
  var value = "stream encoding of";
  data.str_var_utf8mb3 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf8mb3", value);
  fail_openSession(this, InsertFunction(data));
}

var t12 = new harness.ConcurrentTest("12:str_var_utf8mb3:nonascii");
t12.run = function() {
  var data = new TestData();
  var value = "its ♬ 32-bit code";
  data.str_var_utf8mb3 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf8mb3", value);
  fail_openSession(this, InsertFunction(data));
}

var t13 = new harness.ConcurrentTest("13:str_fix_utf16le:non-bmp");
t13.run = function() {
  var data = new TestData();
  var value = "points. 𝍧";
  data.str_fix_utf16le = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf16le", value);
  fail_openSession(this, InsertFunction(data));
}

var t14 = new harness.ConcurrentTest("14:str_var_utf16le:non-bmp");
t14.run = function() {
  var data = new TestData();
  var value = "ing was 𝍧 not satis-";
  data.str_var_utf16le = value;
  this.verifier = new ValueVerifier(this, "str_var_utf16le", value);
  fail_openSession(this, InsertFunction(data));
}

var t15 = new harness.ConcurrentTest("15:str_fix_utf8mb4:non-bmp");
t15.run = function() {
  var data = new TestData();
  var value = "factory on 𝍧";
  data.str_fix_utf8mb4 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf8mb4", value);
  fail_openSession(this, InsertFunction(data));
}

var t16 = new harness.ConcurrentTest("16:str_var_utf8mb4:non-bmp");
t16.run = function() {
  var data = new TestData();
  var value = "performance 𝍧";
  data.str_var_utf8mb4 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf8mb4", value);
  fail_openSession(this, InsertFunction(data));
}


module.exports.tests = [t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16];
