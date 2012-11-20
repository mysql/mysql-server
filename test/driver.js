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

/*global path, fs, suites_dir, api_module, harness, unified_debug, mynode */

"use strict";

// Setup globals:
require("../Adapter/adapter_config.js");
global.suites_dir = __dirname;
global.harness    = require(path.join(suites_dir, "lib", "harness"));
global.mynode     = require(api_module);
global.adapter    = "ndb";

var tprops = require(path.join(suites_dir, "lib", "test_properties"));
var stream = require("stream");
var udebug = unified_debug.getLogger("Driver.js");

/** Driver 
*/
function Driver() {
  this.result = new harness.Result(this);
  this.result.listener = new harness.Listener();
  this.suites = [];
  this.fileToRun = "";
  this.suitesToRun = "";
  this.skipSmokeTest = false;
  this.skipClearSmokeTest = false;
}

Driver.prototype.findSuites = function(directory) {
  var files, f, i, st;
  
  if(this.fileToRun) {
    var suitename = path.dirname(this.fileToRun);
    var pathname = path.join(directory, this.fileToRun); 
    var suite = new harness.Suite(suitename, pathname);
    if(this.skipSmokeTest)      { suite.skipSmokeTest = true;      }
    if(this.skipClearSmokeTest) { suite.skipClearSmokeTest = true; }
    this.suites.push(suite);
  }
  else { 
    /* Read the test directory, building list of suites */
    files = fs.readdirSync(directory);
    for(i = 0; i < files.length ; i++) {
      f = files[i];
      st = fs.statSync(path.join(directory, f));
      if (st.isDirectory() && this.isSuiteToRun(f)) {
        udebug.log('Driver.findSuites found directory', f);
        var suite = new harness.Suite(f, path.join(directory, f));
        if(this.skipSmokeTest)      { suite.skipSmokeTest = true;      }
        if(this.skipClearSmokeTest) { suite.skipClearSmokeTest = true; }
        this.suites.push(suite);
      }
    }
  }
};

Driver.prototype.isSuiteToRun = function(directoryName) {
  if(directoryName === 'lib') {
    return false;
  }
  return (this.suitesToRun === "" || this.suitesToRun.indexOf(directoryName) > -1);
};

Driver.prototype.testCompleted = function(testCase) {
  var suite = testCase.suite;
  udebug.log('Driver.testCompleted Test:', testCase.name, 'Suite:', suite.name);
  if (suite.testCompleted(testCase)) {
    // this suite is done; remove it from the list of running suites
    if (--this.numberOfRunningSuites === 0) {
      // no more running suites; report and exit
      this.reportResultsAndExit();
    }
  } 
};

Driver.prototype.reportResultsAndExit = function() {
  // close all session factories
  var sessionFactories = mynode.getOpenSessionFactories();
  //sessionFactories.forEach(function(sessionFactory) {
  //  udebug.log('Driver.reportResultsAndExit closing', sessionFactory.key);
  //  sessionFactory.close();
  //});
  console.log("Started: ", this.result.listener.started);
  console.log("Passed:  ", this.result.passed.length);
  console.log("Failed:  ", this.result.failed.length);
  console.log("Skipped: ", this.result.skipped.length);
  unified_debug.close();
  process.exit(this.result.failed.length > 0);
};

//// END OF DRIVER


/*****************************************************************************
 ********************** main process *****************************************
 *****************************************************************************/

var driver = new Driver();
var exit = false;
var timeoutMillis = 8000;
var val, values, i, pair;

var usageMessage = 
  "Usage: node driver [options]\n" +
  "       -h or --help: print this message\n" +
  "      -d or --debug: set the debug flag\n" +
  "           --detail: set the detail debug flag\n" +
  "      -t or --trace: print stack trace from failing tests\n" +
  "    --suite=<suite>: only run the named suite(s)\n" +
  "   --suites=<suite>: only run the named suite(s)\n" +
  "      --file=<file>: only run the named test file\n" +
  "--adapter=<adapter>: only run on the named adapter (e.g. ndb or mysql)\n" +
  "       --timer=<ms>: set timeout (in msec); set to 0 to disable timer.\n" +
  "--set <var>=<value>: set a global variable\n" +
  "       --skip-smoke: do not run SmokeTest\n" +
  "       --skip-clear: do not run ClearSmokeTest\n"
  ;

// handle command line arguments
for(i = 2; i < process.argv.length ; i++) {
  val = process.argv[i];
  switch (val) {
  case '--debug':
  case '-d':
    unified_debug.on();
    unified_debug.level_debug();
    driver.result.listener.printStackTraces = true;
    break;
  case '--detail':
    unified_debug.on();
    unified_debug.level_detail();
    driver.result.listener.printStackTraces = true;
    break;
  case '--help':
  case '-h':
    exit = true;
    break;
  case '--trace':
  case '-t':
    driver.result.listener.printStackTraces = true;
    break;
  case '--skip-smoke':
    driver.skipSmokeTest = true;
    break;
  case '--skip-clear': 
    driver.skipClearSmokeTest = true;
    break;
  case '--set':
    i++;  // next argument
    pair = process.argv[i].split('=');
    if(pair.length === 2) {
      udebug.log("Setting global:", pair[0], "=", pair[1]);
      global[pair[0]] = pair[1];
    }
    else {
      console.log("Invalid --set option " + process.argv[i]);
      exit = true;
    }
    break;
  default:
    values = val.split('=');
    if (values.length === 2) {
      switch (values[0]) {
      case '--suite':
      case '--suites':
        driver.suitesToRun = values[1];
        break;
      case '--file':
        driver.fileToRun = values[1];
        break;
      case '--adapter':
        global.adapter = values[1];
        break;
      case '--timer':
        timeoutMillis = values[1];
        break;
      default:
        console.log('Invalid option ' + val);
        exit = true;
      }
    } else {
      console.log('Invalid option ' + val);
      exit = true;
   }
  }
}

if (exit) {
  console.log(usageMessage);
  process.exit(0);
}

// Now that global.adapter is set, get connection properties 
global.test_conn_properties = tprops.connectionProperties();

// Find suites
driver.findSuites(suites_dir);
udebug.log('suites found', driver.suites.length);

driver.suites.forEach(function(suite) {
  udebug.log('createTests for', suite.name);
  suite.createTests();
});

// now run tests
driver.numberOfRunningSuites = 0;
driver.suites.forEach(function(suite) {
  udebug.log('main running tests for', suite.name);
  if (suite.runTests(driver.result)) {
    driver.numberOfRunningSuites++;
  }
});

// if we did not start any suites, exit now
if (driver.numberOfRunningSuites === 0) {
  driver.reportResultsAndExit();
}

function onTimeout() { 
  var nwait = driver.result.listener.started - driver.result.listener.ended;
  var tests = (nwait === 1 ? " test:" : " tests:");
  console.log('TIMEOUT: still waiting for', nwait, tests);
  driver.result.listener.listRunningTests();
  driver.reportResultsAndExit();
}

// set a timeout to prevent process from waiting forever
if(timeoutMillis > 0) {
  udebug.log('Setting timeout of', timeoutMillis);
  setTimeout(onTimeout, timeoutMillis);
}
