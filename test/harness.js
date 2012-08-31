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
  
  var result = new harness.Result();            // create a Result
  result.listener = new harness.Listener();     // ... with a Listener

  var test1 = new harness.Test("test1");  
   
  // Here is the test body:
  test1.run = function() {
    ...
  }
*/

var exec = require("child_process").exec;

var re_matching_test_case = /Test\.js$/;


/* Test  
*/
function Test(name, phase) {
  this.filename = "";
  this.name = name;
  this.phase = (typeof(phase) == 'number') ? phase : 2;
  this.errorMessages = '';
  this.index = 0;
  this.failed = false;
  this.has_proxy = false;
  this.skipped = false;
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

function ConcurrentSubTest(name) {
  this.name = name;
  this.phase = 1;
  this.has_proxy = true;
};
ConcurrentSubTest.prototype = new Test();

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

  /* If a concurrent test has a proxy, then it is considered to be an async 
     test incorporated into some larger test, and it will pass or fail while 
     the larger test is running. */
  if(this.has_proxy) {
    return;
  }

  try {
    if (debug) console.log('test.run: ' + this.name);
    if (!this.run()) {
      if (debug) console.log(this.name + 'started.');
      // async test must call Test.pass or Test.fail when done
      return;
    }
    // fail if any error messages have been reported
    if(! this.skipped) {
      if (this.errorMessages === '') {
        if (debug) console.log(this.name + ' result.pass');
        result.pass(this);
      } else {
        this.failed = true;
        if (debug) console.log(this.name + ' result.fail');
        result.fail(this, this.errorMessages);
      }
    }
  }
  catch(e) {
    if (debug) console.log('result.fail');
    this.failed = true;
    result.fail(this, e);
  }

  this.teardown(); 
};

Test.prototype.pass = function() {
  this.result.pass(this);
  this.teardown();
};

Test.prototype.fail = function(message) {
  this.failed = true;
  if (message) {
    this.appendErrorMessage(message);
  }
  this.result.fail(this, { 'message' : this.errorMessages});
  this.teardown();
};

Test.prototype.appendErrorMessage = function(message) {
  this.errorMessages += message;
  this.errorMessages += '\n';
};

Test.prototype.failOnError = function() {
  if (this.errorMessages != '') {
    this.fail();
  } else {
    this.pass();
  }
}

Test.prototype.skip = function(message) {
  this.skipped = true;
  this.result.skip(this, message);
  return true;
}

Test.prototype.isTest = function() { return true; };
Test.prototype.setup = function() {};
Test.prototype.teardown = function() {};

Test.prototype.fullName = function() {
  var n = "";
  if(this.suite)    n = n + this.suite.name + " ";
  if(this.filename) n = n + path.relative(suites_dir, this.filename) + " ";
  return n + this.name;
}


Test.prototype.run = function() {
  throw {
    "name" : "unimplementedTest",
    "message" : "this test does not have a run() method"
  };
};

Test.prototype.errorIfNotEqual = function(message, o1, o2) {
	if (o1 != o2) {
		this.errorMessages += message;
		console.log(message);
	}
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
  this.suite = {};
  this.numberOfRunningConcurrentTests = 0;
  if (debug) console.log('creating Suite for ' + name + ' ' + path);
}

Suite.prototype.addTest = function(filename, test) {
  if (debug) console.log('Suite ' + this.name + ' adding test ' + test.name +
                         ' from file ' + filename);
  test.filename = filename;    /// shorten the filename here
  test.suite = this;
  this.tests.push(test);
  return test;
}

Suite.prototype.addTestsFromFile = function(f) {
  if(re_matching_test_case.test(f)) {
    var t = require(f);
    if(typeof(t.tests) === 'object' && t.tests instanceof Array) {
      for(j = 0 ; j < t.tests.length ; j++) {
        this.addTest(f, t.tests[j]);
      }
    }      
    else if(typeof(t.isTest) === 'function' && t.isTest()) {
      this.addTest(f, t);
    }
    else { 
      console.log("type : " + typeof(t.tests.length));
      console.dir(t);
      throw "Module " + f + " does not export a Test.";
    }
  }
}

Suite.prototype.createTests = function() {  
  var stat = fs.statSync(this.path);
  if(stat.isFile()) {
    this.addTestsFromFile(this.path);
  }
  else if(stat.isDirectory()) {
    var files = fs.readdirSync(this.path);
    for(var i = 0; i < files.length ; i++) {
      this.addTestsFromFile(path.join(this.path, files[i]));
    }
  }

  if (debug) console.log('Suite ' + this.name + ' found ' + this.tests.length + ' tests.');

  this.tests.sort(function(a,b) {
    if(a.phase < b.phase) return -1;
    if(a.phase == b.phase) return 0;
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
  if (debug) console.log('numberOfConcurrentTests for ' + suite.name + ' is ' + suite.numberOfConcurrentTests);
  suite.numberOfSerialTests = suite.serialTests.length;
  if (debug) console.log('numberOfSerialTests for ' + suite.name + ' is ' + suite.numberOfSerialTests);
};


Suite.prototype.runTests = function(result) {
  this.currentTest = 0;
  if (this.tests.length == 0) return false;
  tc = this.tests[this.currentTest];
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
       this.numberOfConcurrentTestsStarted++;
    });
    return false;    
  } else {
    return this.startSerialTests(result);
  }
};


Suite.prototype.startSerialTests = function(result) {
  assert(result);
  if (debug) console.log('Suite.startSerialTests');
  if (this.firstSerialTestIndex !== -1) {
    this.startNextSerialTest(this.firstSerialTestIndex, result);
    return false;
  } else {
    return this.startClearSmokeTest(result);
  }
};


Suite.prototype.startClearSmokeTest = function(result) {
  assert(result);
  if (debug) console.log('Suite.startClearSmokeTest');
  if (this.clearSmokeTest) {
    this.clearSmokeTest.test(result);
    return false;
  } else {
    return true;
  }
};


Suite.prototype.startNextSerialTest = function(index, result) {
  assert(result);
  var testCase = this.tests[index];
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
        return this.startConcurrentTests(result);
      }
      break;
    case 1:
      // one of the concurrent tests completed
      if (debug) console.log('Suite.testCompleted with concurrent tests already completed: ' + 
                             this.numberOfConcurrentTestsCompleted + ' out of ' + this.numberOfConcurrentTests);
      if (++this.numberOfConcurrentTestsCompleted == this.numberOfConcurrentTests) {
        // done with all concurrent tests; start the first serial test
        if (debug) console.log('Suite.testCompleted; all ' + this.numberOfConcurrentTests + ' concurrent tests completed');
        return this.startSerialTests(result);
      }
      break;
    case 2:
      // one of the serial tests completed
      var index = testCase.index + 1;
      if (index < this.tests.length) {
        // more tests to run; either another serial test or the clear smoke test
        tc = this.tests[index];
        if (tc.phase == 2) {
          // start another serial test
          tc.test(result);
          return false;
        } else if (tc.phase == 3) {
          // start the clear smoke test
          this.startClearSmokeTest(result);
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


/* Listener
*/
function Listener() {
  this.started = 0;
  this.ended   = 0;
  this.printStackTraces = false;
}

Listener.prototype.startTest = function(t) { 
  this.started++;
};

Listener.prototype.endTest = function(t) { 
};

Listener.prototype.pass = function(t) {
  this.ended++;
  console.log("[pass]", t.fullName() );
};

Listener.prototype.skip = function(t, message) {
  this.skipped++;
  console.log("[skipped]", t.fullName(), "\t", message);
};

Listener.prototype.fail = function(t, e) {
  var message = "";
  this.ended++;
  if(e) {
    message = e.toString();
    if (typeof(e.message) !== 'undefined') {
      message = e.message;
    } 
  }
  if ((this.printStackTraces || debug) && typeof(e.stack) !== 'undefined') {
    message = e.stack;
  }
  
  console.log("[FAIL]", t.fullName(), "\t", message);
};


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

/* SQL DDL Utilities
*/
var runSQL = function(sqlPath, source, callback) {
  var child = exec('mysql <' + sqlPath, function (error, stdout, stderr) {
    if (debug) console.log(source + ' stdout: ' + stdout);
    if (debug) console.log(source + ' stderr: ' + stderr);
    if (error !== null) {
      console.log(source + 'exec error: ' + error);
    } else {
      if (debug) console.log(source + ' exec OK');
    }
    callback(error);
  });
}

var SQL = {};
SQL.create =  function(suite, callback) {
  var sqlPath = path.join(suite.path, 'create.sql');
  if (debug) console.log("createSQL path: " + sqlPath);
  runSQL(sqlPath, 'createSQL', callback);
};

SQL.drop = function(suite, callback) {
  var sqlPath = path.join(suite.path, 'drop.sql');
  if (debug) console.log("dropSQL path: " + sqlPath);
  runSQL(sqlPath, 'dropSQL', callback);
};



/* Exports from this module */
exports.Test              = Test;
exports.Suite             = Suite;
exports.Listener          = Listener;
exports.Result            = Result;
exports.SmokeTest         = SmokeTest;
exports.ConcurrentTest    = ConcurrentTest;
exports.ConcurrentSubTest = ConcurrentSubTest;
exports.SerialTest        = SerialTest;
exports.ClearSmokeTest    = ClearSmokeTest;
exports.SQL               = SQL;
