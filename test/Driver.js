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

var fs = require("fs");
var path = require("path");
var parent = path.dirname(__dirname);

/* Set global.testharnessmodule 
   and global.adapter
   for the use of test cases
*/
global.test_harness_module = path.join(__dirname, "harness.js");
global.adapter = path.join(parent, "Adapter");

global.api_module = path.join(parent, "Adapter", "api", "mynode.js");
global.spi_module = path.join(parent, "Adapter", "spi", "SPI.js");

global.debug = false;
global.exit = false;

var Test = require(global.test_harness_module);

var re_matching_test_case = /Test\.js$/;

global.createSQL = function(suite, callback) {
  var sqlPath = path.join(suite.path, 'create.sql');
  if (debug) console.log("createSQL path: " + sqlPath);
  var child = exec('mysql <' + sqlPath,
  function (error, stdout, stderr) {
  if (debug) console.log('stdout: ' + stdout);
  if (debug) console.log('stderr: ' + stderr);
  if (error !== null) {
    console.log('exec error: ' + error);
  }
  callback(error);
  });
};

global.dropSQL = function(suite, callback) {
  var sqlPath = path.join(suite.path, 'drop.sql');
  if (debug) console.log("dropSQL path: " + sqlPath);
  var child = exec('mysql <' + sqlPath,
  function (error, stdout, stderr) {
  if (debug) console.log('stdout: ' + stdout);
  if (debug) console.log('stderr: ' + stderr);
  if (error !== null) {
    console.log('exec error: ' + error);
  }
  callback(error);
  });
};

function Suite(name, path) {
	this.name = name;
	this.path = path;
	this.tests = [];
	this.testCases = [];
	this.currentTest = 0;
	this.smokeTest;
	this.concurrentTests = [];
	this.numberOfConcurrentTests = 0;
  this.firstConcurrentTestIndex = -1;
	this.serialTests = [];
	this.numberOfSerialTests = 0;
  this.firstSerialTestIndex = -1;
	this.nextSerialTestIndex = -1;
	this.clearSmokeTest;
	this.numberOfRunningConcurrentTests = 0;
//	this.phase = 0;
	if (debug) console.log('creating Suite for ' + name + ' ' + path);
}

Suite.prototype.createTests = function() {

  var files = fs.readdirSync(path.join(parent, 'test', this.name));
  for(var i = 0; i < files.length ; i++) {
    var f = files[i];
    var st = fs.statSync(path.join(parent, 'test', this.name, f));
    if(st.isFile() && re_matching_test_case.test(f)) {
      var t = require(path.join(parent, 'test', this.name, f));
      if(! t.isTest()) {
        throw { name : "NotATest" ,
          message : "Module " + f + " does not export a Test."
        };
      }
      if (debug) console.log('Suite ' + this.name + ' found test ' + f);
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

  this.testCases.sort(function(a,b) {
    if(a.phase < b.phase) return -1;
    if(a.phase == b.phase) return 0;
    return 1;
 });

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
  suite.numberOfSerialTests = suite.serialTests.length;
};

Suite.prototype.runTests = function() {
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
    this.startConcurrentTests();
    break;
  case 2:
    // serial test is the first test
    this.startSerialTests();
    break;
  case 3:
    // clear smoke test is the first test
    tc.test(result);
    break;
  }
  return true;
};

Suite.prototype.startConcurrentTests = function() {
  if (debug) console.log('Suite.startConcurrentTests');
  if (this.firstConcurrentTestIndex !== -1) {
    this.concurrentTests.forEach(function(testCase) {
      if (debug) console.log('Suite.startConcurrentTests starting ' + testCase.name);
      testCase.test(result);
    });
    return false;    
  } else {
    return this.startSerialTests();
  }
};

Suite.prototype.startSerialTests = function() {
  if (debug) console.log('Suite.startSerialTests');
  if (this.firstSerialTestIndex !== -1) {
    this.startNextSerialTest(this.firstSerialTestIndex);
    if (debug) console.log('Suite.startSerialTests set phase to ' + this.phase);
    return false;
  } else {
    return this.startClearSmokeTest();
  }
};

Suite.prototype.startClearSmokeTest = function() {
  if (debug) console.log('Suite.startClearSmokeTest');
  if (this.clearSmokeTest) {
    this.clearSmokeTest.test(result);
    return false;
  } else {
    return true;
  }
};

Suite.prototype.startNextSerialTest = function(index) {
  var testCase = this.testCases[index];
  testCase.test(result);
};

/* Notify the suite that a test has completed.
 */
Suite.prototype.testCompleted = function(testCase) {
  if (debug) console.log('Suite.testCompleted for testCase ' + testCase.name + ' phase ' + testCase.phase);
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
    if (debug) console.log('Suite.testCompleted; one concurrent test completed');
    if (--this.numberOfRunningConcurrentTests == 0) {
      // done with all concurrent tests; start the first serial test
      if (debug) console.log('Suite.testCompleted; all concurrent tests completed');
      return this.startSerialTests();
    }
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
        return false;
      }
    } else {
      // no more tests
      return true;
    }
  case 3:
    // the clear smoke test completed
    return true;
  }
};

function Driver() {
  this.suites = [];
};

Driver.prototype.findSuites = function(directory) {
  /* Read the test directory, building list of suites */
  var files = fs.readdirSync(directory);
  for(var i = 0; i < files.length ; i++) {
    var f = files[i];
    var st = fs.statSync(path.join(directory, f));
    if (st.isDirectory()) { 
      if (debug) console.log('Driver.findSuites found directory ' + f);
      this.suites.push(new Suite(f, path.join(directory, f)));
    }
  }
};

Driver.prototype.testCompleted = function(testCase) {
  if (debug) console.log('Driver.testCompleted: ' + testCase.name);
  var suite = testCase.suite;
  if (debug) console.log('Driver.testCompleted suite for ' + suite.name);
  if (suite.testCompleted(testCase)) {
    // this suite is done; remove it from the list of running suites
    if (--this.numberOfRunningSuites == 0) {
      // no more running suites; report and exit
      this.reportResultsAndExit();
    }
  } 
};

Driver.prototype.reportResultsAndExit = function() {
  console.log("Passed: ", result.passed.length);
  console.log("Failed: ", result.failed.length);
  process.exit(result.failed.length > 0);  
};

var util = require('util');
var exec = require('child_process').exec;

/*****************************************************************************
 ********************** main process *****************************************
 *****************************************************************************/

driver = new Driver();

var usageMessage = 
  "Usage: node driver [options]\n" +
  "              -h: print this message\n" +
  "          --help: print this message\n" +
  "              -d: set the debug flag\n" +
  "         --debug: set the debug flag]\n";

// handle command line arguments
process.argv.forEach(function (val, index, array) {
  switch (val) {
  case '--debug':
  case '-d':
    console.log('Setting debug to true');
    debug = true;
    break;
  case '--help':
  case '-h':
    console.log(usageMessage);
    exit = true;
  }
});
if (exit) process.exit(0);

var result = new Test.Result(driver);
result.listener = new Test.Listener();

driver.findSuites(__dirname);
if (debug) console.log('suites found ' + driver.suites.length);

driver.suites.forEach(function(suite) {
  if (debug) console.log('createTests for ' + suite.name);
  suite.createTests();
});

// now run tests
driver.numberOfRunningSuites = 0;
driver.suites.forEach(function(suite) {
  if (debug) console.log('main running tests for ' + suite.name);
  if (suite.runTests()) {
    driver.numberOfRunningSuites++;
  }
});

// if we did not start any suites, exit now
if (driver.numberOfRunningSuites == 0) {
  driver.reportResultsAndExit();
}

var timeoutMillis = 10000;
// set a timeout to prevent process from waiting forever
if (debug) console.log('Setting timeout of ' + timeoutMillis);
setTimeout(function() {
  console.log('TIMEOUT');
}, timeoutMillis);
