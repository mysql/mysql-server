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
var test_id = 1;

function TestData() {
  if(this.id === undefined) {
    this.id = test_id++;
  }
}

function BufferVerifier(testCase, field, expectedValue) {
  this.run = function onRead(err, rowRead) {
    var buffer, len, i;
    var mismatch = false;
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    try {  
      buffer = rowRead[field];
      len = expectedValue.length;
      testCase.errorIfNotEqual("length", len, buffer.length);
      for(i = 0; i < len ; i++) {
        if(buffer[i] !== expectedValue[i]) {
          mismatch = i;
        }
      }
      testCase.errorIfNotEqual("mismatch at position " + mismatch, false, mismatch);
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
  };
}

function InsertFunction(data) {
  return function onSession(session, testCase) {
    testCase.data = data;
    session.persist(data, new ReadFunction(testCase, session));
  };
}

var mapping = new mynode.TableMapping("test.binary_test");
mapping.mapAllColumns = true;
mapping.applyToClass(TestData);

var t1 = new harness.ConcurrentTest("t1:binary_full_length");
t1.run = function() {
  var data, value, i;
  data = new TestData();
  value = new Buffer(20);
  for(i = 0 ; i < 20 ; i ++) {
    value[i] = i;
  }
  
  data.bin_fix = value;
  this.verifier = new BufferVerifier(this, "bin_fix", value);
  fail_openSession(this, new InsertFunction(data));
};

var t2 = new harness.ConcurrentTest("t2:binary_zero_padded");
t2.run = function() {
  var data, value, expected;
  data = new TestData();
  value     = new Buffer([1,2,3,4,5,6,7,8,9,10]);
  expected  = new Buffer([1,2,3,4,5,6,7,8,9,10,0,0,0,0,0,0,0,0,0,0]);

  data.bin_fix = value;
  this.verifier = new BufferVerifier(this, "bin_fix", expected);
  fail_openSession(this, new InsertFunction(data));
};

var t3 = new harness.ConcurrentTest("t3:varbinary");
t3.run = function() {
  var data, value, i;
  data = new TestData();
  value = new Buffer(120);
  for(i = 0 ; i < 120 ; i ++) {
    value[i] = i;
  }
  
  data.bin_var = value;
  this.verifier = new BufferVerifier(this, "bin_var", value);
  fail_openSession(this, new InsertFunction(data));
};

var t4 = new harness.ConcurrentTest("t4:longvarbinary");
t4.run = function() {
  var data, value, i;
  data = new TestData();
  value = new Buffer(320);
  for(i = 0 ; i < 320 ; i ++) {
    value[i] = 32 + (i % 90);
  }
  
  data.bin_var_long = value;
  this.verifier = new BufferVerifier(this, "bin_var_long", value);
  fail_openSession(this, new InsertFunction(data));
};

module.exports.tests = [t1, t2, t3, t4];
