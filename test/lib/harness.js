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

/*global unified_debug, fs, path, util, assert, suites_dir,
         test_conn_properties
*/

"use strict";

/* This test harness is documented in the README file.
*/

var udebug = unified_debug.getLogger("harness.js");
var exec = require("child_process").exec;
var re_matching_test_case = /Test\.js$/;
var SQL = {};

/* Test  
*/
function Test(name, phase) {
  this.filename = "";
  this.name = name;
  this.phase = (typeof(phase) === 'number') ? phase : 2;
  this.errorMessages = '';
  this.index = 0;
  this.failed = null;
  this.has_proxy = false;
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


function ConcurrentSubTest(name) {
  this.name = name;
  this.phase = 1;
  this.has_proxy = true;
}
ConcurrentSubTest.prototype = new Test();


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
  udebug.log('test starting:', this.suite.name, this.name);
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
    udebug.log_detail('test.run:', this.suite.name, this.name);
    if (!this.run()) {
      udebug.log_detail(this.name, 'started.');
      // async test must call Test.pass or Test.fail when done
      return;
    }
    // fail if any error messages have been reported
    if(! this.skipped) {
      if (this.errorMessages === '') {
        udebug.log(this.name, this.suite.name, 'result.pass');
        result.pass(this);
      } else {
        this.failed = true;
        udebug.log(this.name, this.suite.name, 'result.fail');
        result.fail(this, this.errorMessages);
      }
    }
  }
  catch(e) {
    udebug.log_detail('result.fail');
    this.failed = true;
    result.fail(this, e);
    this.teardown();
  }
};

Test.prototype.pass = function() {
  assert(this.failed === null); // must have not yet passed or failed
  this.failed = false;
  this.result.pass(this);
  this.teardown();
};

Test.prototype.fail = function(message) {
  assert(this.failed === null);  // must have not yet passed or failed
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
Test.prototype.setup = function() {};
Test.prototype.teardown = function() {};

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

Test.prototype.errorIfNotEqual = function(message, o1, o2) {
	if (o1 !== o2) {
	  message += ': expected ' + o1 + '; actual ' + o2 + '\n';
		this.errorMessages += message;
	}
};

Test.prototype.errorIfNull = function(message, val) {
  if(val === null) {
    this.errorMessages += message;
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
  this.suite = {};
  this.numberOfRunningConcurrentTests = 0;
  this.skipSmokeTest = false;
  this.skipClearSmokeTest = false;
  udebug.log('Creating Suite for', name, path);
}

Suite.prototype.addTest = function(filename, test) {
  this.filename = path.relative(suites_dir, filename);
  udebug.log_detail('Suite', this.name, 'adding test', test.name, 'from', this.filename);
  test.filename = filename;
  test.suite = this;
  this.tests.push(test);
  return test;
};

Suite.prototype.addTestsFromFile = function(f) {
  var t, j;
  if(re_matching_test_case.test(f)) {
    t = require(f);
    if(typeof(t.tests) === 'object' && t.tests instanceof Array) {
      for(j = 0 ; j < t.tests.length ; j++) {
        this.addTest(f, t.tests[j]);
      }
    }      
    else if(typeof(t.isTest) === 'function' && t.isTest()) {
      this.addTest(f, t);
    }
    else { 
      udebug.log_detail(t);
      throw "Module " + f + " does not export a Test.";
    }
  }
};

Suite.prototype.createTests = function() {  
  var stat = fs.statSync(this.path);
  var suite, i;
  
  if(stat.isFile()) {
    var testFile = this.path;
    this.path = path.dirname(testFile);
    this.addTestsFromFile(path.join(this.path, "SmokeTest.js"));
    this.addTestsFromFile(testFile);
    this.addTestsFromFile(path.join(this.path, "ClearSmokeTest.js"));
  }
  else if(stat.isDirectory()) {
    var files = fs.readdirSync(this.path);
    for(i = 0; i < files.length ; i++) {
      this.addTestsFromFile(path.join(this.path, files[i]));
    }
  }

  udebug.log('Suite', this.name, 'found', this.tests.length, 'tests.');

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
  udebug.log('numberOfConcurrentTests for', suite.name, 'is', suite.numberOfConcurrentTests);
  suite.numberOfSerialTests = suite.serialTests.length;
  udebug.log('numberOfSerialTests for', suite.name, 'is', suite.numberOfSerialTests);
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
  udebug.log('Suite.startSerialTests');
  if (this.firstSerialTestIndex !== -1) {
    this.startNextSerialTest(this.firstSerialTestIndex, result);
    return false;
  } 
  // else:
  return this.startClearSmokeTest(result);
};


Suite.prototype.startClearSmokeTest = function(result) {
  assert(result);
  udebug.log('Suite.startClearSmokeTest');
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
      udebug.log('Completed ', this.numberOfConcurrentTestsCompleted, 
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
      udebug.log('Suite.testCompleted there is no ClearSmokeTest so we are done with ' + testCase.suite.name);
      return true;

    case 3:   // the clear smoke test completed
      udebug.log('Suite.testCompleted completed ClearSmokeTest.');
      return true;
  }
};


/* Listener
*/
function Listener() {
  this.started = 0;
  this.ended   = 0;
  this.printStackTraces = false;
  this.running = [];
}

Listener.prototype.startTest = function(t) { 
  this.started++;
  this.running[t.index] = t.fullName();
};

Listener.prototype.pass = function(t) {
  this.ended++;
  delete this.running[t.index];
  console.log("[pass]", t.fullName() );
};

Listener.prototype.skip = function(t, message) {
  this.skipped++;
  delete this.running[t.index];
  console.log("[skipped]", t.fullName(), "\t", message);
};

Listener.prototype.fail = function(t, e) {
  var message = "";
  this.ended++;
  delete this.running[t.index];
  if(e) {
    message = e.toString();
    if (typeof(e.message) !== 'undefined') {
      message = e.message;
    } 
  }
  if ((this.printStackTraces) && typeof(e.stack) !== 'undefined') {
    message = e.stack;
  }
  
  console.log("[FAIL]", t.fullName(), "\t", message);
};

Listener.prototype.listRunningTests = function() {
  function listElement(e) {
    console.log("  " + e);
  }
  this.running.forEach(listElement);
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

  function childProcess(error, stdout, stderr) {
    udebug.log('harness runSQL process completed.');
    udebug.log(source + ' stdout: ' + stdout);
    udebug.log(source + ' stderr: ' + stderr);
    if (error !== null) {
      console.log(source + 'exec error: ' + error);
    } else {
      udebug.log(source + ' exec OK');
    }
    if(callback) {
      callback(error);  
    }
  }

  var p = test_conn_properties;
  var cmd = 'mysql';
  if(p) {
    if(p.mysql_socket)     { cmd += " --socket=" + p.mysql_socket; }
    else if(p.mysql_port)  { cmd += " --port=" + p.mysql_port; }
    if(p.mysql_host)     { cmd += " -h " + p.mysql_host; }
    if(p.mysql_user)     { cmd += " -u " + p.mysql_user; }
    if(p.mysql_password) { cmd += " --password=" + p.mysql_password; }
  }
  cmd += ' <' + sqlPath; 
  udebug.log('harness runSQL forking process...' + cmd);
  var child = exec(cmd, childProcess);
};

SQL.create =  function(suite, callback) {
  var sqlPath = path.join(suite.path, 'create.sql');
  udebug.log_detail("createSQL path: " + sqlPath);
  runSQL(sqlPath, 'createSQL', callback);
};

SQL.drop = function(suite, callback) {
  var sqlPath = path.join(suite.path, 'drop.sql');
  udebug.log_detail("dropSQL path: " + sqlPath);
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


