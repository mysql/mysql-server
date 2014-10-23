/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

/* This test harness is documented in the README file.
*/

var path   = require("path"),
    fs     = require("fs"),
    assert = require("assert"),
    util   = require("util");

var udebug = unified_debug.getLogger("harness.js");
var re_matching_test_case = /Test\.js$/;
var disabledTests = {};
try {
  disabledTests = require("../disabled-tests.conf").disabledTests;
}
catch(e) {}

/* Test  
*/
function Test(name, phase) {
  this.filename = "";
  this.name = name;
  this.phase = (typeof(phase) === 'number') ? phase : 2;
  this.errorMessages = '';
  this.index = 0;
  this.failed = null;
  this.skipped = false;
}

function SmokeTest(name) {
  this.name = name;
  this.phase = 0;
}
SmokeTest.prototype = new Test();

function ConcurrentTest(name) {
  this.name = name;
  this.phase = 1;
}
ConcurrentTest.prototype = new Test();


function SerialTest(name) {
  this.name = name;
  this.phase = 2;
}
SerialTest.prototype = new Test();


function ClearSmokeTest(name) {
  this.name = name;
  this.phase = 3;
}
ClearSmokeTest.prototype = new Test();


Test.prototype.test = function(result) {
  udebug.log_detail('test starting:', this.suite.name, this.name);
  this.result = result;
  result.listener.startTest(this);
  var runReturnCode;

  udebug.log_detail('test.run:', this.suite.name, this.name);
  try {
    runReturnCode = this.run();
  }
  catch(e) {
    console.log(this.name, 'threw exception & failed\n', e.stack);
    this.failed = true;
    result.fail(this, e);
    return;
  }

  if(! runReturnCode) {
    // async test must call Test.pass or Test.fail when done
    udebug.log(this.name, 'started.');
    return;
  }

  // Test ran synchronously.  Fail if any error messages have been reported.
  if(! this.skipped) {
    if (this.errorMessages === '') {
      udebug.log_detail(this.name, 'passed');
      result.pass(this);
    } else {
      this.failed = true;
      udebug.log_detail(this.name, 'failed');
      result.fail(this, this.errorMessages);
    }
  }
};

Test.prototype.pass = function() {
  if (this.failed !== null) {
    console.log('Error: pass called with status already ' + (this.failed?'failed':'passed'));
    assert(this.failed === null);
  } else {
    if (this.session && !this.session.isClosed()) {
      // if session is open, close it
      if (this.session.currentTransaction().isActive()) {
        console.log('Test.pass found active transaction');
      }
      this.session.close();
    }
    this.failed = false;
    this.result.pass(this);
  }
};

Test.prototype.fail = function(message) {
  if (this.failed !== null) {
    console.log('Error: pass called with status already ' + (this.failed?'failed':'passed'));
    assert(this.failed === null);
  } else {
    if (this.session && !this.session.isClosed()) {
      // if session is open, close it
      this.session.close();
    }
    this.failed = true;
    if (message) {
      this.appendErrorMessage(message);
      this.stack = message.stack;
    }
    this.result.fail(this, { 'message' : this.errorMessages, 'stack': this.stack});
  }
};

Test.prototype.appendErrorMessage = function(message) {
  this.errorMessages += message;
  this.errorMessages += '\n';
};

Test.prototype.error = Test.prototype.appendErrorMessage;

Test.prototype.failOnError = function() {
  if (this.errorMessages !== '') {
    this.fail();
  } else {
    this.pass();
  }
};

Test.prototype.skip = function(message) {
  this.skipped = true;
  this.result.skip(this, message);
  return true;
};

Test.prototype.isTest = function() { return true; };

Test.prototype.fullName = function() {
  var n = "";
  if(this.suite)    { n = n + this.suite.name + " "; }
  if(this.filename) { n = n + path.basename(this.filename) + " "; }
  return n + this.name;
};

Test.prototype.run = function() {
  throw {
    "name" : "unimplementedTest",
    "message" : "this test does not have a run() method"
  };
};

function getType(obj) {
  var type = typeof(obj);
  if (type === 'object') return obj.constructor.name;
  return type;
}

function compare(o1, o2) {
  if (o1 == o2) return true;
  if (o1 == null && o2 == null) return true;
  if (typeof(o1) === 'undefined' && typeof(o2) === 'undefined') return true;
  if (typeof(o1) !== typeof(o2)) return false;
  if (o1.toString() === o2.toString()) return true;
  return false;
}

Test.prototype.errorIfNotEqual = function(message, o1, o2) {
	if (!compare(o1, o2)) {
	  var o1type = getType(o1);
	  var o2type = getType(o2);
    message += ': expected (' + o1type + ') ' + o1 + '; actual (' + o2type + ') ' + o2 + '\n';
		this.errorMessages += message;
	}
};

Test.prototype.errorIfNotStrictEqual = function(message, o1, o2) {
  if(o1 !== o2) {
    var o1type = getType(o1);
    var o2type = getType(o2);
    message += ': expected (' + o1type + ') ' + o1 + '; actual (' + o2type + ') ' + o2 + '\n';
		this.errorMessages += message;
	}
};

Test.prototype.errorIfTrue = function(message, o1) {
  if (o1) {
    message += ': expected not true; actual ' + o1 + '\n';
    this.errorMessages += message;
  }
};

Test.prototype.errorIfNotTrue = function(message, o1) {
  if (o1 !== true) {
    message += ': expected true; actual ' + o1 + '\n';
    this.errorMessages += message;
  }
};

Test.prototype.errorIfNotError = function(message, o1) {
  if (!o1) {
    message += ' did not occur.\n';
    this.errorMessages += message;
  }
};

Test.prototype.errorIfNull = function(message, val) {
  if(val === null) {
    this.errorMessages += message;
  }
};

Test.prototype.errorIfNotNull = function(message, val) {
  if(val !== null) {
    this.errorMessages += message;
  }
};

/* Use this with the error argument in a callback */
Test.prototype.errorIfError = function(val) {
  if(typeof val !== 'undefined' && val !== null) {
    this.errorMessages += util.inspect(val);
  }
};

/* Value must be defined and not-null 
   Function returns true if there was no error; false on error 
*/
Test.prototype.errorIfUnset = function(message, value) {
  var r = (typeof value === 'undefined' || value === null); 
  if(r) {
    this.errorMessages += message;
  }
  return ! r;
};

Test.prototype.hasNoErrors = function() {
  return this.errorMessages.length === 0;
};

/** Suite
  *  A suite consists of all tests in all test programs in a directory 
  *
  */
function Suite(name, path) {
  this.name = name;
  this.path = path;
  this.tests = [];
  this.currentTest = 0;
  this.smokeTest = {};
  this.concurrentTests = [];
  this.numberOfConcurrentTests = 0;
  this.numberOfConcurrentTestsCompleted = 0;
  this.numberOfConcurrentTestsStarted = 0;
  this.firstConcurrentTestIndex = -1;
  this.serialTests = [];
  this.numberOfSerialTests = 0;
  this.firstSerialTestIndex = -1;
  this.nextSerialTestIndex = -1;
  this.clearSmokeTest = {};
  this.testInFile = null;
  this.suite = {};
  this.numberOfRunningConcurrentTests = 0;
  this.skipSmokeTest = false;
  this.skipClearSmokeTest = false;
  udebug.log_detail('Creating Suite for', name, path);
}

Suite.prototype.addTest = function(filename, test) {
  this.filename = path.relative(mynode.fs.suites_dir, filename);
  udebug.log_detail('Suite', this.name, 'adding test', test.name, 'from', this.filename);
  test.filename = filename;
  test.suite = this;
  if(disabledTests && disabledTests[this.filename]) {
    udebug.log("Skipping ", this.filename, "[DISABLED]");
  }
  else {
    this.tests.push(test);
  }
  return test;
};

Suite.prototype.addTestsFromFile = function(f, onlyTests) {
  var t, i, j, k, testList, testHash;
  if(onlyTests) {
    onlyTests = String(onlyTests);
    testList = onlyTests.split(",");
    testHash = [];
    for(i = 0 ; i < testList.length ; i ++) {
      k = Number(testList[i]) - 1;
      testHash[k] = 1;
    }
  }
  if(re_matching_test_case.test(f)) {
    t = require(f);
    if(typeof(t.tests) === 'object' && t.tests instanceof Array) {
      for(j = 0 ; j < t.tests.length ; j++) {
        if(onlyTests === null || testHash[j] === 1) {
          this.addTest(f, t.tests[j]);
        }
      }
    }      
    else if(typeof(t.isTest) === 'function' && t.isTest()) {
      this.addTest(f, t);
    }
    else { 
      console.log("Warning: " + f + " does not export a Test.");
    }
  }
};

Suite.prototype.createTests = function() {  
  var stat = fs.statSync(this.path);
  var suite, i;
  
  if(stat.isFile()) {
    var testFile = this.path;
    this.path = path.dirname(testFile);
    try {
      this.addTestsFromFile(path.join(this.path, "SmokeTest.js"), null);
    } catch(e1) {}
    this.addTestsFromFile(testFile, this.testInFile);
    try {
      this.addTestsFromFile(path.join(this.path, "ClearSmokeTest.js"), null);
    } catch(e2) {}
  }
  else if(stat.isDirectory()) {
    var files = fs.readdirSync(this.path);
    for(i = 0; i < files.length ; i++) {
      this.addTestsFromFile(path.join(this.path, files[i]), null);
    }
  }

  udebug.log_detail('Suite', this.name, 'found', this.tests.length, 'tests.');

  this.tests.forEach(function(t, index) {
    t.original = index;
  });

  this.tests.sort(function(a,b) {
    // sort the tests by phase, preserving the original order within each phase
    if(a.phase < b.phase)  { return -1; }
    if(a.phase === b.phase) { return (a.original < b.original)?-1:1;  }
    return 1;
  });

  suite = this;
  this.tests.forEach(function(t, index) {
    t.index = index;
    t.suite = suite;
    switch(t.phase) {
      case 0:
        suite.smokeTest = t;
        break;
      case 1:
        suite.concurrentTests.push(t);
        if (suite.firstConcurrentTestIndex === -1) {
          suite.firstConcurrentTestIndex = t.index;
          udebug.log_detail('Suite.createTests firstConcurrentTestIndex:', suite.firstConcurrentTestIndex);
        }
        break;
      case 2:
        suite.serialTests.push(t);
        if (suite.firstSerialTestIndex === -1) {
          suite.firstSerialTestIndex = t.index;
          udebug.log_detail('Suite.createTests firstSerialTestIndex:', suite.firstSerialTestIndex);
        }
        break;
      case 3:
        suite.clearSmokeTest = t;
        break;
    }
    udebug.log_detail('createTests sorted test case', t.name, ' ', t.phase, ' ', t.index);
    });
  suite.numberOfConcurrentTests = suite.concurrentTests.length;
  udebug.log_detail('numberOfConcurrentTests for', suite.name, 'is', suite.numberOfConcurrentTests);
  suite.numberOfSerialTests = suite.serialTests.length;
  udebug.log_detail('numberOfSerialTests for', suite.name, 'is', suite.numberOfSerialTests);
};


Suite.prototype.runTests = function(result) {
  var tc;
  if (this.tests.length === 0) { 
    return false;
  }
  this.currentTest = 0;
  tc = this.tests[this.currentTest];
  switch (tc.phase) {
    case 0:
      // smoke test
      // start the smoke test
      if(this.skipSmokeTest) {
        tc.result = result;
        tc.skip("skipping SmokeTest");
      }
      else {
        tc.test(result);
      }
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
      if(this.skipClearSmokeTest) {
       tc.result = result;
       tc.skip("skipping ClearSmokeTest");
      }
      else {
        tc.test(result);
      }
      break;
  }
  return true;
};


Suite.prototype.startConcurrentTests = function(result) {
  var self = this;
  udebug.log_detail('Suite.startConcurrentTests');
  if (this.firstConcurrentTestIndex !== -1) {
    this.concurrentTests.forEach(function(testCase) {
      udebug.log_detail('Suite.startConcurrentTests starting ', self.name, testCase.name);
      testCase.test(result);
      self.numberOfConcurrentTestsStarted++;
    });
    return false;    
  } 
  // else:
  return this.startSerialTests(result);
};


Suite.prototype.startSerialTests = function(result) {
  assert(result);
  udebug.log_detail('Suite.startSerialTests');
  if (this.firstSerialTestIndex !== -1) {
    this.startNextSerialTest(this.firstSerialTestIndex, result);
    return false;
  } 
  // else:
  return this.startClearSmokeTest(result);
};


Suite.prototype.startClearSmokeTest = function(result) {
  assert(result);
  udebug.log_detail('Suite.startClearSmokeTest');
  if (this.skipClearSmokeTest) {
    this.clearSmokeTest.result = result;
    this.clearSmokeTest.skip("skipping ClearSmokeTest");
  }
  else if (this.clearSmokeTest && this.clearSmokeTest.test) {
    this.clearSmokeTest.test(result);
    return false;
  } 
  return true;
};


Suite.prototype.startNextSerialTest = function(index, result) {
  assert(result);
  var testCase = this.tests[index];
  testCase.test(result);
};


/* Notify the suite that a test has completed.
   Returns false if there are more tests to be run,
   true if suite is complete.
 */
Suite.prototype.testCompleted = function(testCase) {
  var tc, index;

  udebug.log_detail('Suite.testCompleted for', this.name, testCase.name, 'phase', 
                    testCase.phase);
  var result = testCase.result;
  switch (testCase.phase) {
    case 0:     // the smoke test completed
      if (testCase.failed) {        // if the smoke test failed, we are done
        return true;          
      } 
      udebug.log_detail('Suite.testCompleted; starting concurrent tests');
      return this.startConcurrentTests(result);

    case 1:     // one of the concurrent tests completed
      udebug.log_detail('Completed ', this.numberOfConcurrentTestsCompleted, 
                 ' out of ', this.numberOfConcurrentTests);
      if (++this.numberOfConcurrentTestsCompleted === this.numberOfConcurrentTests) {
        return this.startSerialTests(result);   // go on to the serial tests
      }
      return false;

    case 2:     // one of the serial tests completed
      index = testCase.index + 1;
      if (index < this.tests.length) {
        tc = this.tests[index];
        if (tc.phase === 2) {     // start another serial test
          tc.test(result);
        } 
        else if (tc.phase === 3) {     // start the clear smoke test
          this.startClearSmokeTest(result);
        }
        return false;
      }
      /* Done */
      udebug.log_detail('Suite.testCompleted there is no ClearSmokeTest so we are done with ' + testCase.suite.name);
      return true;

    case 3:   // the clear smoke test completed
      udebug.log_detail('Suite.testCompleted completed ClearSmokeTest.');
      return true;
  }
};


/* Listener
*/
function Listener() {
  this.started = 0;
  this.ended   = 0;
  this.printStackTraces = false;
  this.runningTests = {};
}

Listener.prototype.startTest = function(t) { 
  this.started++;
  this.runningTests[t.fullName()] = 1;
};

Listener.prototype.pass = function(t) {
  this.ended++;
  delete this.runningTests[t.fullName()];
  console.log("[pass]", t.fullName() );
};

Listener.prototype.skip = function(t, message) {
  this.skipped++;
  delete this.runningTests[t.fullName()];
  console.log("[skipped]", t.fullName(), "\t", message);
};

Listener.prototype.fail = function(t, e) {
  var message = "";
  this.ended++;
  delete this.runningTests[t.fullName()];
  if (e) {
    if (typeof(e.stack) !== 'undefined') {
      t.stack = e.stack;
    }
    if (typeof(e.message) !== 'undefined') {
      message = e.message;
    } else {
      message = e.toString();
    }
  }
  if ((this.printStackTraces) && typeof(t.stack) !== 'undefined') {
    message = t.stack;
  }

  if(t.phase === 0) {
    console.log("[FailSmokeTest]", t.fullName(), "\t", message);
  }
  else {
    console.log("[FAIL]", t.fullName(), "\t", message);
  }
};

Listener.prototype.listRunningTests = function() {
  console.log(this.runningTests);
};


/* QuietListener */
function QuietListener() {
  this.started = 0;
  this.ended   = 0;
  this.runningTests = {};
}

QuietListener.prototype.startTest = Listener.prototype.startTest;

QuietListener.prototype.pass = function(t) {
  this.ended++;
  delete this.runningTests[t.fullName()];
};

QuietListener.prototype.skip = QuietListener.prototype.pass;
QuietListener.prototype.fail = QuietListener.prototype.pass;

QuietListener.prototype.listRunningTests = Listener.prototype.listRunningTests;

/* FailOnlyListener */
function FailOnlyListener() {
  this.fail = Listener.prototype.fail;
}

FailOnlyListener.prototype = new QuietListener();

  
/* Result 
*/
function Result(driver) {
  this.driver = driver;
  this.passed = [];
  this.failed = [];
  this.skipped = [];
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

Result.prototype.skip = function(t, reason) {
  this.skipped.push(t.name);
  this.listener.skip(t, reason);
  this.driver.testCompleted(t);
};


/* Exports from this module */
exports.Test              = Test;
exports.Suite             = Suite;
exports.Listener          = Listener;
exports.QuietListener     = QuietListener;
exports.FailOnlyListener  = FailOnlyListener;
exports.Result            = Result;
exports.SmokeTest         = SmokeTest;
exports.ConcurrentTest    = ConcurrentTest;
exports.SerialTest        = SerialTest;
exports.ClearSmokeTest    = ClearSmokeTest;
