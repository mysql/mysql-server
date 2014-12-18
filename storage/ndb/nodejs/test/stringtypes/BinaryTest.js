/*
 Copyright (c) 2014 , Oracle and/or its affiliates. All rights
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

var t5 = new harness.ConcurrentTest("t5:NonBufferInBinaryColumn");
t5.run = function() {
  var data = new TestData();
  data.bin_var_long = "Ceci n\'est pas un buffer";
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      if(err) {
        testCase.errorIfNotEqual("Expected Error", "22000", err.sqlstate);
      } else {
        testCase.appendErrorMessage("Expected error 22000 on insert");
      }
      testCase.failOnError();
    });
  });
};

// Insert a BLOB
var t6 = new harness.ConcurrentTest("t6:InsertBLOB");
t6.run = function() {
  var data = new TestData();
  data.bin_lob = new Buffer(20000);
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      testCase.errorIfError(err);
      testCase.failOnError();
    });
  });
};

// Attempt to insert a non-Buffer into a BLOB Column: 0F001
var t7 = new harness.ConcurrentTest("t7:InsertNonBufferAsBLOB");
t7.run = function() { 
  var data = new TestData();
  data.bin_lob = "aint no blob";
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
     if(err) {
        testCase.errorIfNotEqual("Expected Error", "0F001", err.sqlstate);
      } else {
        testCase.appendErrorMessage("Expected error 0F001 on insert");
      }
      testCase.failOnError();
    });
  });
};

// Insert and Read a blob
var t8 = new harness.ConcurrentTest("t8:WriteAndReadBlob");
t8.run = function() {
  var data, value, i;
  data = new TestData();
  value = new Buffer(20000);
  for(i = 0 ; i < 20000 ; i++) {
    value[i] = Math.ceil(Math.random() * 256);
  }
  data.bin_lob = value;
  this.verifier = new BufferVerifier(this, "bin_lob", value);
  fail_openSession(this, new InsertFunction(data));
};

var t9 = new harness.ConcurrentTest("t9:ReadModifyUpdate");
t9.run = function() {
  var data, value1, value2, i;
  data = new TestData();
  value1 = new Buffer(320);
  value2 = new Buffer(320);
  for(i = 0 ; i < 320 ; i ++) {
    value1[i] = 32 + (i % 90);
    value2[i] = 119 - (i % 80);
  }
  data.bin_lob = value1;
  data.bin_var_long = value1;
  fail_openSession(this, function(session, testCase) {
    session.persist(data, function(err) {
      session.find(TestData, data.id, function(err, obj) {
        obj.bin_var_long = value2;
        session.update(obj, function(err) {
          testCase.errorIfError(err);
          session.find(TestData, data.id, function(err, foundInstance) {
            testCase.errorIfError(err);
            testCase.errorIfNotEqual("Unexpected VARBINARY", value2, 
                                     foundInstance.bin_var_long);
            testCase.errorIfNotEqual("Unexpected BLOB", value1, 
                                     foundInstance.bin_lob);
            testCase.failOnError();          
          });
        });
      });
    });
  });
};



module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9];
