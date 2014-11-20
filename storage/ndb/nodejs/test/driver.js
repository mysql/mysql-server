/*
 Copyright (c) 2012, 2013, 2014 Oracle and/or its affiliates. All rights
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

var path = require("path"),
    fs   = require("fs"),
    conf = require("../Adapter/adapter_config.js"),
    suites_dir = conf.suites_dir;

// Setup globals:
global.mynode     = require(conf.api_module);
global.adapter    = "ndb";
global.harness    = require(path.join(suites_dir, "lib", "harness"));


var udebug = unified_debug.getLogger("Driver.js");
var stats_module = require(mynode.api.stats);

/** Driver 
*/
function Driver() {
  this.result = new harness.Result(this);
  this.result.listener = new harness.Listener();
  this.suites = [];
  this.fileToRun = "";
  this.testInFile = null;
  this.suitesToRun = "";
  this.skipSmokeTest = false;
  this.skipClearSmokeTest = false;
  this.doStats = false;
  this.statsDomain = null;
}

Driver.prototype.findSuites = function(directory) {
  var files, f, i, st, suite, nsuites;
  nsuites = 0;

  if(this.fileToRun) {
    nsuites++;
    var suitename = path.dirname(this.fileToRun);
    var pathname = path.join(directory, this.fileToRun); 
    suite = new harness.Suite(suitename, pathname);
    if(this.skipSmokeTest)      { suite.skipSmokeTest = true;      }
    if(this.skipClearSmokeTest) { suite.skipClearSmokeTest = true; }
    if(this.testInFile)         { suite.testInFile = this.testInFile; }
    this.suites.push(suite);
  }
  else { 
    /* Read the test directory, building list of suites */
    files = fs.readdirSync(directory);
    for(i = 0; i < files.length ; i++) {
      f = files[i];
      st = fs.statSync(path.join(directory, f));
      if (st.isDirectory() && this.isSuiteToRun(f)) {
        nsuites++;
        udebug.log_detail('Driver.findSuites found directory', f);
        suite = new harness.Suite(f, path.join(directory, f));
        if(this.skipSmokeTest)      { suite.skipSmokeTest = true;      }
        if(this.skipClearSmokeTest) { suite.skipClearSmokeTest = true; }
        this.suites.push(suite);
      }
    }
  }
  return nsuites;
};

Driver.prototype.isSuiteToRun = function(directoryName) {
  if(directoryName === 'lib') {
    return false;
  }
  return (this.suitesToRun === "" || this.suitesToRun.indexOf(directoryName) > -1);
};

Driver.prototype.testCompleted = function(testCase) {
  var suite = testCase.suite;
  udebug.log_detail('Driver.testCompleted Test:', testCase.name, 'Suite:', suite.name);
  if (suite.testCompleted(testCase)) {
    udebug.log_detail('numberOfRunningSuites', this.numberOfRunningSuites);

    // this suite is done; remove it from the list of running suites
    if (--this.numberOfRunningSuites === 0) {
      // no more running suites; report and exit
      this.reportResultsAndExit();
    }
  } 
};

Driver.prototype.reportResultsAndExit = function() {
  var driver = this;

  console.log("Started: ", this.result.listener.started);
  console.log("Passed:  ", this.result.passed.length);
  console.log("Failed:  ", this.result.failed.length);
  console.log("Skipped: ", this.result.skipped.length);
  if(this.doStats) {
    stats_module.peek(this.statsDomain);
  }

  // close all open session factories and exit
  mynode.closeAllOpenSessionFactories(function() {
    unified_debug.close();
    process.exit(driver.result.failed.length > 0);
  });
};

Driver.prototype.runAllTests = function(test_suite_dir_array) {
  var i, dir, n;

  /* Find Suites */
  for(i = 0 ; i < test_suite_dir_array.length; i++) {
    dir = test_suite_dir_array[i];
    n = this.findSuites(dir);
    udebug.log_detail('found ', n, 'suite', (n == 1 ? '':'s'), ' in ', dir);
  }

  /* Create tests */
  for(i = 0; i < this.suites.length ; i++) {
    this.suites[i].createTests();
  }

  /* now run tests */
  this.numberOfRunningSuites = this.suites.length;
  for(i = 0; i < this.suites.length ; i++) {
    if (! this.suites[i].runTests(this.result)) {
      this.numberOfRunningSuites--;
    }
  }

  /* if we did not start any suites, exit now */
  if (this.numberOfRunningSuites === 0) {
    this.reportResultsAndExit();
  }
};



//// END OF DRIVER


/*****************************************************************************
 ********************** main process *****************************************
 *****************************************************************************/

var driver = new Driver();
var exit = false;
var timeoutMillis = 0;
var val, values, i, pair;
var storageEngine = null;

var usageMessage = 
  "Usage: node driver [options]\n" +
  "       -h or --help: print this message\n" +
  "      -d or --debug: set the debug flag\n" +
  "           --detail: set the detail debug flag\n" +
  "         -df=<file>: enable debug output from source file <file> \n" +
  "      -t or --trace: print stack trace from failing tests\n" +
  "                 -q: (quiet) do not print individual test results \n" +
  "           --failed: suppress passed tests, print failures only\n" +
  "    --suite=<suite>: only run the named suite(s)\n" +
  "   --suites=<suite>: only run the named suite(s)\n" +
  "      --file=<file>: only run the named test file\n" +
  "   --test=<n,m,...>: only run tests numbered n, m, etc. in <file>\n " +
  "--adapter=<adapter>: only run on the named adapter/engine \n" +
  "                     optionally add engine (e.g. mysql/ndb or mysql/innodb\n" +
  "     --timeout=<ms>: set timeout in msec.\n" +
  "--set <var>=<value>: set a global variable\n" +
  "       --skip-smoke: do not run SmokeTest\n" +
  "       --skip-clear: do not run ClearSmokeTest\n" +
  "    --stats=<query>: show server statistics after test run\n"
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
      udebug.log_detail("Setting global:", pair[0], "=", pair[1]);
      global[pair[0]] = pair[1];
    }
    else {
      console.log("Invalid --set option " + process.argv[i]);
      exit = true;
    }
    break;
  case '--failed':
    driver.result.listener = new harness.FailOnlyListener();
    // --failed and -q both imply 10 sec. timeout:
    timeoutMillis = 10000;
    break;
  case '-q':
    driver.result.listener = new harness.QuietListener();
    timeoutMillis = 10000;
    break;
  default:
    values = val.split('=');
    if (values.length === 2) {
      switch (values[0]) {
      case '--stats':
        driver.doStats = true;
        driver.statsDomain = values[1];
        break;
      case '--suite':
      case '--suites':
        driver.suitesToRun = values[1];
        break;
      case '--file':
        driver.fileToRun = values[1];
        break;
      case '--test':
        driver.testInFile = values[1];
        break;
      case '--adapter':
        // adapter is adapter/engine
        var adapterSplit = values[1].split('/');
        switch (adapterSplit.length) {
        case 1:
          global.adapter = values[1];
          break;
        case 2:
          global.adapter = adapterSplit[0];
          storageEngine = adapterSplit[1];
          break;
        default:
          console.log('Invalid adapter parameter');
        exit = true;
        break;
        }
        break;
      case '--timeout':
        timeoutMillis = values[1];
        break;
      case '-df':
        unified_debug.on();
        unified_debug.set_file_level(values[1], 5);
        break;
      default:
        console.log('Invalid option ' + val);
        exit = true;
      }
    } else if(values[0] == '--stats') {  // --stats argument is optional
      driver.doStats = true;
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

/* global.adapter is now set.  Read in the utilities library for the test suite; 
   it may set some additional globals.
*/
require(path.join(suites_dir, "utilities"));

/* Set storage engine from command-line options */
if(storageEngine && global.test_conn_properties) {
  global.test_conn_properties.mysql_storage_engine = storageEngine;
}

/* Find and run all tests */
driver.runAllTests( [ suites_dir ] );

function onTimeout() { 
  var nwait = driver.result.listener.started - driver.result.listener.ended;
  var tests = (nwait === 1 ? " test:" : " tests:");
  console.log('TIMEOUT: still waiting for', nwait, tests);
  driver.result.listener.listRunningTests();
  driver.reportResultsAndExit();
}

// set a timeout to prevent process from waiting forever
if(timeoutMillis > 0) {
  udebug.log_detail('Setting timeout of', timeoutMillis);
  setTimeout(onTimeout, timeoutMillis);
}

