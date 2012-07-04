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
  this.phase = 2;
  this.errorMessages = '';
  this.index = 0;
  this.failed = false;
  this.result;
}

Test.prototype.test = function(result) {
  if (debug) console.log('test starting: ' + this.name);
  this.result = result;
  result.listener.startTest(this);
  this.setup();

  if(this.isFatal()) {  // Let suite fail with uncaught exception
    this.run();
    result.pass(this);
  }
  else {
    try {
      if (debug) console.log('test.run: ' + this.name);
      if (this.run()) {
        if (debug) console.log('test returning from async call without calling pass or fail for test ' + this.name);
        // async test must call Test.pass or Test.fail when done
        return;
      }
      // fail if any error messages have been reported
      if (this.errorMessages == '') {
        if (debug) console.log(this.name + ' result.pass');
        result.pass(this);
      } else {
        this.failed = true;
        if (debug) console.log(this.name + 'result.fail');
    	  result.fail(this);
      }
    }
    catch(e) {
      if (debug) console.log('result.fail');
      this.failed = true;
      result.fail(this, e);
    }
  }
  this.teardown(); 
  result.listener.endTest(this);
};

Test.prototype.pass = function() {
  this.result.pass(this);
};

Test.prototype.fail = function(message) {
  this.failed = true;
  this.appendMessage(message);
  this.result.fail(this,
      { 'message' : this.errorMessage});
};

Test.prototype.appendMessage = function(message) {
  this.errorMessages += message;
  this.errorMessages += '\n';
};

Test.prototype.getTestCases = function() { return [this];};
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

Test.prototype.makeTestGroup = function() {
  var tests = [];  // private!
  for (var i = 0; i < arguments.length; ++i) {
		tests.push(arguments[i]);
	}
  this.getTestCases = function() {
	  var result = [];
	  if (debug) console.log('harness.makeTestGroup.getTestCases has ' + tests.length);
	  for (var i = 0; i < tests.length; ++i) {
		  var testCases = tests[i].getTestCases();
		  if (debug) console.log('harness.makeTestGroup.getTestCases has ' + testCases.length);
		  for (var j = 0; j < testCases.length; ++j) {
		      result.push(testCases[j]);
		  }
	  }
	  if (debug) console.log('harness.makeTestGroup function getTestCases returns ' + result.length);
	  return result;
  }
  this.addTest = function(t) { 
    tests.push(t);
    t.name = this.name + "." + t.name;
  };
  this.test = function(result) {
  	if (debug) console.log("Running group " + this.name);
    for(var i = 0; i < tests.length ; i++) {
      tests[i].test(result);
    }
  };
  return this;
};

Test.prototype.errorIfNotEqual = function(message, o1, o2) {
	if (o1 != o2) {
		this.errorMessages += message;
		console.log(message);
	}
};

/* Listener
*/
function Listener() {
}

Listener.prototype.startTest = function(t) {};
Listener.prototype.endTest = function(t) {};
Listener.prototype.pass = function(t) {
  console.log("[pass]", t.suite.name + ' ' + t.name );
};
Listener.prototype.fail = function(t, e) {
  console.log("[FAIL]", t.name, "\t", e.stack);
};



/* Result 
*/
function Result(driver) {
  this.driver = driver;
  this.passed = [];
  this.failed = [];
}

Result.prototype.pass = function(t) {
  this.passed.push(t.name);
  this.listener.pass(t);
  this.driver.testCompleted(t);
};

Result.prototype.fail = function(t, e) {
  this.failed.push(t.name);
  this.listener.fail(t, e);
  this.driver.testCompleted(t);
};


/* Exports from this module */
exports.Test = Test;
exports.Listener = Listener;
exports.Result = Result;
