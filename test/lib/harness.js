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

var re_matching_test_case = /Test\.js$/;


/* Test  
*/
function Test(name, phase) {
  this.name = name;
  this.phase = (typeof(phase) == 'number') ? phase : 2;
  this.errorMessages = '';
  this.index = 0;
  this.failed = false;
};

function SmokeTest(name) {
  this.name = name;
  this.phase = 0;
};

SmokeTest.prototype = new Test();

function ConcurrentTest(name) {
  this.name = name;
  this.phase = 1;
};

ConcurrentTest.prototype = new Test();

function SerialTest(name) {
  this.name = name;
  this.phase = 2;
};

SerialTest.prototype = new Test();

function ClearSmokeTest(name) {
  this.name = name;
  this.phase = 3;
};

ClearSmokeTest.prototype = new Test();

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
      if (!this.run()) {
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
  if (typeof(message.toString()) != 'undefined' ) {
    this.appendErrorMessage(message);
  }
  this.result.fail(this,
      { 'message' : this.errorMessages});
};

Test.prototype.appendErrorMessage = function(message) {
  this.errorMessages += message;
  this.errorMessages += '\n';
};

Test.prototype.failOnError = function() {
  if (errorMessages !== '') {
    this.fail();
  } else {
    this.pass();
  }
}
Test.prototype.getTestCases = function() { return [this];};
Test.prototype.isTest = function() { return true; };
Test.prototype.isFatal = function() { return false; };
Test.prototype.setFatal = function() { 
  this.isFatal = function() { return true; };
};
Test.prototype.setup = function() {};
Test.prototype.teardown = function() {};

Test.prototype.fullName = function() {
  var result = '';
  if (this.group) {
    result = this.group.fullName();
  }
  return result + ' ' + this.name;};

Test.prototype.run = function() {
  throw {
    "name" : "unimplementedTest",
    "message" : "this test does not have a run() method"
  };
};

Test.prototype.makeTestGroup = function() {
  var tests = [];  // private!
  for (var i = 0; i < arguments.length; ++i) {
    var testCase = arguments[i];
    testCase.group = this;
    tests.push(testCase);
	}
  this.getTestCases = function() {
    var result = [];
    if (debug) console.log('harness.makeTestGroup.getTestCases for ' + this.name + ' has ' + tests.length + ' tests.');
    for (var i = 0; i < tests.length; ++i) {
      var testCases = tests[i].getTestCases();
      if (debug) console.log('harness.makeTestGroup.getTestCases has ' + testCases.length + ' test cases.');
      for (var j = 0; j < testCases.length; ++j) {
        result.push(testCases[j]);
      }
    }
    if (debug) console.log('harness.makeTestGroup function getTestCases returns ' + result.length);
    return result;
  };
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


/** A Suite consists of all tests in all test programs in a directory 
*/
function Suite(name, path) {
  this.name = name;
  this.path = path;
  this.tests = [];
  this.testCases = [];
  this.currentTest = 0;
  this.smokeTest;
  this.concurrentTests = [];
  this.numberOfConcurrentTests = 0;
  this.numberOfConcurrentTestsCompleted = 0;
  this.numberOfConcurrentTestsStarted = 0;
  this.firstConcurrentTestIndex = -1;
  this.serialTests = [];
  this.numberOfSerialTests = 0;
  this.firstSerialTestIndex = -1;
  this.nextSerialTestIndex = -1;
  this.clearSmokeTest;
  this.numberOfRunningConcurrentTests = 0;
  this.group = null;
  if (debug) console.log('creating Suite for ' + name + ' ' + path);
}


Suite.prototype.createTests = function() {  
  var files = fs.readdirSync(path.join(driver_dir, this.name));
  for(var i = 0; i < files.length ; i++) {
    var f = files[i];
    var st = fs.statSync(path.join(driver_dir, this.name, f));
    if(st.isFile() && re_matching_test_case.test(f)) {
      var t = require(path.join(driver_dir, this.name, f));
      if(! t.isTest()) {
        throw { name : "NotATest" ,
          message : "Module " + f + " does not export a Test."
        };
      }
      if (debug) console.log('Suite ' + this.name + ' found test ' + f);
      t.name = f; // the name of this group is the test program name
      t.group = this;
      this.tests.push(t);
    }
  }
  if (debug) console.log('Suite ' + this.name + ' found ' + this.tests.length +  ' tests.');
  
  for (var i = 0; i < this.tests.length; ++i) {
    var t = this.tests[i].getTestCases();
    for (var j = 0; j < t.length; ++j) {
      this.testCases.push(t[j]);
    }
  }
  
  suite = this;
  this.testCases.forEach(function(t, index) {
    t.index = index;
    t.suite = suite;
    switch(t.phase) {
      case 0:
        suite.smokeTest = t;
        break;
      case 1:
        suite.concurrentTests.push(t);
        if (suite.firstConcurrentTestIndex == -1) {
          suite.firstConcurrentTestIndex = t.index;
          if (debug) console.log('Suite.createTests firstConcurrentTestIndex: ' + suite.firstConcurrentTestIndex);
        }
        break;
      case 2:
        suite.serialTests.push(t);
        if (suite.firstSerialTestIndex == -1) {
          suite.firstSerialTestIndex = t.index;
          if (debug) console.log('Suite.createTests firstSerialTestIndex: ' + suite.firstSerialTestIndex);
        }
        break;
      case 3:
        suite.clearSmokeTest = t;
        break;
    }
    if (debug) console.log('createTests sorted test case ' + ' ' + t.name + ' ' + t.phase + ' ' + t.index);
    });
  suite.numberOfConcurrentTests = suite.concurrentTests.length;
  if (debug) console.log('numberOfConcurrentTests for ' + suite.fullName() + ' is ' + suite.numberOfConcurrentTests);
  suite.numberOfSerialTests = suite.serialTests.length;
  if (debug) console.log('numberOfSerialTests for ' + suite.fullName() + ' is ' + suite.numberOfSerialTests);
};


Suite.prototype.runTests = function(result) {
  this.currentTest = 0;
  if (this.testCases.length == 0) return false;
  tc = this.testCases[this.currentTest];
  switch (tc.phase) {
    case 0:
      // smoke test
      // start the smoke test
      tc.test(result);
      break;
    case 1:
      // concurrent test is the first test
      // start all concurrent tests
      this.startConcurrentTests(result);
      break;
    case 2:
      // serial test is the first test
      this.startSerialTests(result);
      break;
    case 3:
      // clear smoke test is the first test
      tc.test(result);
      break;
  }
  return true;
};


Suite.prototype.startConcurrentTests = function(result) {
  if (debug) console.log('Suite.startConcurrentTests');
  if (this.firstConcurrentTestIndex !== -1) {
    this.concurrentTests.forEach(function(testCase) {
      if (debug) console.log('Suite.startConcurrentTests starting ' + testCase.name);
      testCase.test(result);
       ++this.numberOfConcurrentTestsStarted;
    });
    return false;    
  } else {
    return this.startSerialTests();
  }
};


Suite.prototype.startSerialTests = function(result) {
  if (debug) console.log('Suite.startSerialTests');
  if (this.firstSerialTestIndex !== -1) {
    this.startNextSerialTest(this.firstSerialTestIndex, result);
    return false;
  } else {
    return this.startClearSmokeTest(result);
  }
};


Suite.prototype.startClearSmokeTest = function(result) {
  if (debug) console.log('Suite.startClearSmokeTest');
  if (this.clearSmokeTest) {
    this.clearSmokeTest.test(result);
    return false;
  } else {
    return true;
  }
};


Suite.prototype.startNextSerialTest = function(index, result) {
  var testCase = this.testCases[index];
  testCase.test(result);
};


/* Notify the suite that a test has completed.
 */
Suite.prototype.testCompleted = function(testCase) {
  if (debug) console.log('Suite.testCompleted for testCase ' + testCase.name + ' phase ' + testCase.phase);
  var result = testCase.result;
  switch (testCase.phase) {
    case 0:
      // the smoke test completed
      if (testCase.failed) {
        // if the smoke test failed, we are done
        return true;
      } else {
        if (debug) console.log('Suite.testCompleted; starting concurrent tests');
        return this.startConcurrentTests();
      }
      break;
    case 1:
      // one of the concurrent tests completed
      if (debug) console.log('Suite.testCompleted with concurrent tests already completed: ' + 
                             this.numberOfConcurrentTestsCompleted + ' out of ' + this.numberOfConcurrentTests);
      if (++this.numberOfConcurrentTestsCompleted == this.numberOfConcurrentTests) {
        // done with all concurrent tests; start the first serial test
        if (debug) console.log('Suite.testCompleted; all ' + this.numberOfConcurrentTests + ' concurrent tests completed');
        return this.startSerialTests();
      }
      break;
    case 2:
      // one of the serial tests completed
      var index = testCase.index + 1;
      if (index < this.testCases.length) {
        // more tests to run; either another serial test or the clear smoke test
        tc = this.testCases[index];
        if (tc.phase == 2) {
          // start another serial test
          tc.test(result);
          return false;
        } else if (tc.phase == 3) {
          // start the clear smoke test
          this.startClearSmokeTest();
          if (debug) console.log('Suite.testCompleted started ClearSmokeTest.');
          return false;
        }
      } else {
        // no more tests
        if (debug) console.log('Suite.testCompleted there is no ClearSmokeTest so we are done.');
        return true;
      }
    case 3:
      // the clear smoke test completed
      if (debug) console.log('Suite.testCompleted completed ClearSmokeTest.');
      return true;
  }
};

Suite.prototype.fullName = function() {
  var result = '';
  if (this.group) {
    result = this.group.fullName();
  }
  return result + ' ' + this.name;
};


/* Listener
*/
function Listener() {
}

Listener.prototype.startTest = function(t) {};
Listener.prototype.endTest = function(t) {};
Listener.prototype.pass = function(t) {
  console.log("[pass]", t.group.fullName() + ' ' + t.name );
};
Listener.prototype.fail = function(t, e) {
  var message = e.toString();
  if (typeof(e.stack) !== 'undefined') {
    message = e.stack;
  } else if (typeof(e.message) !== 'undefined') {
    message = e.message;
  }
//  console.log("[FAIL]", t.group.fullName() + ' ' + t.name, "\t", message);
  console.log("[FAIL]", t.fullName(), "\t", message);
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


/* SQL DDL Utilities
*/
var runSQL = function(sqlPath, callback) {
  var child = exec('mysql <' + sqlPath, function (error, stdout, stderr) {
    if (debug) console.log('createSQL stdout: ' + stdout);
    if (debug) console.log('createSQL stderr: ' + stderr);
    if (error !== null) {
      console.log('createSQL exec error: ' + error);
    } else {
      if (debug) console.log('createSQL exec OK');
    }
    callback(error);
  });
}

var SQL = {};
SQL.create =  function(suite, callback) {
  var sqlPath = path.join(suite.path, 'create.sql');
  if (debug) console.log("createSQL path: " + sqlPath);
  runSQL(sqlPath, callback);
};

SQL.drop = function(suite, callback) {
  var sqlPath = path.join(suite.path, 'drop.sql');
  if (debug) console.log("dropSQL path: " + sqlPath);
  runSQL(sqlPath, callback);
};



/* Exports from this module */
exports.Test           = Test;
exports.Suite          = Suite;
exports.Listener       = Listener;
exports.Result         = Result;
exports.SmokeTest      = SmokeTest;
exports.ConcurrentTest = ConcurrentTest;
exports.SerialTest     = SerialTest;
exports.ClearSmokeTest = ClearSmokeTest;
exports.SQL            = SQL;
