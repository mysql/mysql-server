/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

/* Using this test harness

  var harness = require("harness.js");
  
  var result = new harness.Result();            // create a Result
  result.listener = new harness.Listener();     // ... with a Listener

  var test1 = new harness.Test("test1");  
  
  // Cause the whole suite to fail immediately if test1 fails.
  test1.setFatal();
  
  // Here is the test body:
  test1.run = function() {
    ...
  }
  
  // Now you can run test1 by itself
  test1.test(result)
  

  // Or create another test, and put two tests together into a suite:
  var test2 = new harness.Test("test2");
  test2.run = function() {
    ...
  }
  
  var suite = new harness.Test("my_suite").makeTestSuite();
  suite.addTest(test1);
  suite.addTest(test2);
  
  // Run the test cases 
  
  suite.test(result);
*/



/* Test  
*/
function Test(name) {
  this.name = name;
  this.order = 3;
}

Test.prototype.test = function(result) {
  result.listener.startTest(this);
  this.setup();

  if(this.isFatal()) {  // Let suite fail with uncaught exception
    this.run();
    result.pass(this);
  }
  else {
    try {
      this.run();
      result.pass(this);
    }
    catch(e) {
      result.fail(this, e);
    }
  }
  this.teardown(); 
  result.listener.endTest(this);
};

Test.prototype.isTest = function() { return true; };
Test.prototype.isFatal = function() { return false; };
Test.prototype.setFatal = function() { 
  this.isFatal = function() { return true; };
};
Test.prototype.setup = function() {};
Test.prototype.teardown = function() {};

Test.prototype.run = function() {
  throw {
    "name" : "unimplementedTest",
    "message" : "this test does not have a run() method"
  };
};

Test.prototype.makeTestSuite = function() {
  var tests = [];  // private!
  this.addTest = function(t) { 
    tests.push(t);
    t.name = this.name + "." + t.name;
  };
  this.test = function(result) {
    for(var i = 0; i < tests.length ; i++) {
      tests[i].test(result);
    }
  };
  return this;
};


/* Listener
*/
function Listener() {
}

Listener.prototype.startTest = function(t) {};
Listener.prototype.endTest = function(t) {};
Listener.prototype.pass = function(t) {
  console.log("[pass]", t.name );
};
Listener.prototype.fail = function(t, e) {
  console.log("[FAIL]", t.name, "\t", e.message);
};



/* Result 
*/
function Result() {
  this.passed = [];
  this.failed = [];
}

Result.prototype.pass = function(t) {
  this.passed.push(t.name);
  this.listener.pass(t);
};

Result.prototype.fail = function(t, e) {
  this.failed.push(t.name);
  this.listener.fail(t, e);
};


/* Exports from this module */
exports.Test = Test;
exports.Listener = Listener;
exports.Result = Result;
