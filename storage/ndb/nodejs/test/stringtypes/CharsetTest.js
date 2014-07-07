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

var ValueVerifier = require("./lib.js").ValueVerifier;
var ErrorVerifier = require("./lib.js").ErrorVerifier;
var BufferVerifier = require("./lib.js").BufferVerifier;

// Domain Object Constructor
var test_id = 1;

function TestData() {
  if(this.id === undefined) {
    this.id = test_id++;
  }
}

function ReadFunction(testCase, session) { 
  return function onPersist(err) {
    testCase.errorIfError(err);
    session.find(TestData, testCase.data.id, testCase.verifier.run);
  };
}

function InsertFunction(data) {
  return function onSession(session, testCase) {
    testCase.data = data;
    session.persist(data, ReadFunction(testCase, session));
  };
}

var mapping = new mynode.TableMapping("test.charset_test");
mapping.mapAllColumns = true;
mapping.applyToClass(TestData);


// 
var t1 = new harness.ConcurrentTest("1:str_fix_latin1_ASCII");
t1.run = function() {
  var data = new TestData();
  var value = "By early 1992 the";
  data.str_fix_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_fix_latin1", value);
  fail_openSession(this, InsertFunction(data));
};

var t2 = new harness.ConcurrentTest("2:str_var_latin1_ASCII");
t2.run = function() {
  var data = new TestData();
  var value = "search was on for a";
  data.str_var_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_var_latin1", value);
  fail_openSession(this, InsertFunction(data));
};

var t3 = new harness.ConcurrentTest("3:str_var_latin1");
t3.run = function() {
  var data = new TestData();
  var value = "gøød b¥te-stream";
  data.str_var_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_var_latin1", value);
  fail_openSession(this, InsertFunction(data));
};

var t4 = new harness.ConcurrentTest("4:str_fix_latin1");
t4.run = function() {
  var data = new TestData();
  var value = "éncØding of multi-";
  data.str_fix_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_fix_latin1", value);
  fail_openSession(this, InsertFunction(data));
};

var t5 = new harness.ConcurrentTest("5:str_fix_latin2");
t5.run = function() {
  var data = new TestData();
  var value = "byte Çhâracter sets.";
  data.str_fix_latin2 = value;
  this.verifier = new ValueVerifier(this, "str_fix_latin2", value);
  fail_openSession(this, InsertFunction(data));
};

var t6 = new harness.ConcurrentTest("6:str_var_latin2");
t6.run = function() {
  var data = new TestData();
  var value = "The dráft ÎSÖ 10646";
  data.str_var_latin2 = value;
  this.verifier = new ValueVerifier(this, "str_var_latin2", value);
  fail_openSession(this, InsertFunction(data));
};

var t7 = new harness.ConcurrentTest("7:str_fix_utf8");
t7.run = function() {
  var data = new TestData();
  var value = "standard contained";
  data.str_fix_utf8 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf8", value);
  fail_openSession(this, InsertFunction(data));
};

var t8 = new harness.ConcurrentTest("8:str_var_utf8");
t8.run = function() {
  var data = new TestData();
  var value = "a nÒn-rÉquired Ânnex";
  data.str_var_utf8 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf8", value);
  fail_openSession(this, InsertFunction(data));
};

var t9 = new harness.ConcurrentTest("9:str_fix_utf16");
t9.run = function() {
  var data = new TestData();
  var value = "called UTF-1 that";
  data.str_fix_utf16 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf16", value);
  fail_openSession(this, InsertFunction(data));
};

var t10 = new harness.ConcurrentTest("10:str_var_utf16");
t10.run = function() {
  var data = new TestData();
  var value = "provided a ☃ byte-";
  data.str_var_utf16 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf16", value);
  fail_openSession(this, InsertFunction(data));
};

var t11 = new harness.ConcurrentTest("11:str_fix_ascii");
t11.run = function() {
  var data = new TestData();
  var value = "stream encoding of";
  data.str_fix_ascii = value;
  this.verifier = new ValueVerifier(this, "str_fix_ascii", value);
  fail_openSession(this, InsertFunction(data));
};

var t12 = new harness.ConcurrentTest("12:str_var_ascii");
t12.run = function() {
  var data = new TestData();
  var value = "its 32-bit code";
  data.str_var_ascii = value;
  this.verifier = new ValueVerifier(this, "str_var_ascii", value);
  fail_openSession(this, InsertFunction(data));
};

var t13 = new harness.ConcurrentTest("13:str_fix_utf32");
t13.run = function() {
  var data = new TestData();
  var value = "points. This encod-";
  data.str_fix_utf32 = value;
  this.verifier = new ValueVerifier(this, "str_fix_utf32", value);
  fail_openSession(this, InsertFunction(data));
};

var t14 = new harness.ConcurrentTest("14:str_var_utf32");
t14.run = function() {
  var data = new TestData();
  var value = "ing was ☹ not satis-";
  data.str_var_utf32 = value;
  this.verifier = new ValueVerifier(this, "str_var_utf32", value);
  fail_openSession(this, InsertFunction(data));
};



/*
....+....+....+....+
factory on 
performance grounds, 
but did introduce 
the notion that 
bytes in the range 
of 0–127 continue 
representing the 
ASCII characters in 
UTF, thereby 
providing backward 
compatibility with 
ASCII.
*/

module.exports.tests = [t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14];
