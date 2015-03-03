/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

var path = require("path");
var fs = require("fs");
var util = require("util");
var skipTests = false;
var jslint, linter, lintName;
var haveJsLint = false;
var ignoredErrors = {};

try { jslint = require("jslint/lib/linter").lint; haveJsLint = true; }
catch(e) {}

 if(haveJsLint) { 
  linter=jslint; 
  lintName = "jslint"; 
}
else { 
  skipTests = true;
}

var jslintOptions = {
  "vars"      : true,     // allow multiple var declarations
  "plusplus"  : true,     // allow ++ and -- operators
  "white"     : true,     // misc. white space
  "stupid"    : true,     // sync methods
  "node"      : true,     // node.js globals
  "nomen"     : true,     // allow dangling underscore
  "eqeq"      : true,     // allow ==
  "bitwise"   : true,
  "predef"    :  
    [ /* globals from test/driver.js */
      "unified_debug", "harness", "mynode", "adapter",
      /* globals commonly defined in test suites: */
      "fail_openSession", "sqlCreate", "sqlDrop"      
    ]
};

var lintOptions = jslintOptions;

var ignoreAlways = "Expected \'{\' and instead saw";

function ignore(file, pos, msg, count) { 
  var i;
  var list = ignoredErrors[file];
  if(! list) {
    list = []; ignoredErrors[file] = list;
  }
  if(typeof count === 'undefined') {
    list.push({ 'pos': pos, 'msg': msg});
  }
  else {
    for(i = 0 ; i < count ; i++) {
      list.push({ 'pos': pos, 'msg': msg});
    }
  }
}


function isIgnored(file, pos, msg) {
  var list;
  list = ignoredErrors[file];
  if(list && list[0] && (list[0].pos === pos) && (list[0].msg == msg)) {
    list.shift();
    return true;
  }
  if(msg.indexOf(ignoreAlways) == 0) {
    return true;
  }
  return false;
}

function lintTest(basePath, sourceFile) {
  var name = path.basename(basePath) + "/" + path.basename(sourceFile);
  var t = new harness.SerialTest(name);
  t.sourceFileName = path.basename(sourceFile);
  t.sourceFile = path.join(basePath, sourceFile);

  t.run = function runLintTest() {
    if(skipTests) { return this.skip("jslint not avaliable"); }

    var e, i, n=0, line;
    var data = fs.readFileSync(this.sourceFile, "utf8");  
    var result = linter(data, lintOptions);
    var ok, errors, msg = "";

    /* Adapt to differing APIs of jslint and jshint */
    if(typeof result === 'boolean') {
      /* We are using jshint */
      ok = result;
      errors = linter.errors;
    }
    else {
      /* jslint */
      ok = result.ok;
      errors = result.errors;
    }

    if(! ok) {
      for (i = 0; i < errors.length; i += 1) {
        e = errors[i];
        if(e && ! isIgnored(this.sourceFileName, e.character, e.reason)) {
          n += 1;
          msg += util.format('\n * Line %d[%d]: %s', e.line, e.character, e.reason);
        }
      }
      msg = util.format("%d %s error%s", n, lintName, n===1 ? '':'s') + msg;
      if (n > 0) {
        this.appendErrorMessage(msg);
      }
    }
    return true;
  };
  
  return t;
}

var skipFilePatterns = [
  /^\./,        // skip files starting with .
  /~[1-9]~$/,   // bzr leaves these around
];

function checkDirectory(base, dir) {
  var dirname, files, file, i, useFile;
  dirname = path.join(base, dir);
  files = fs.readdirSync(dirname);
  while(file = files.pop()) {
    useFile = false;
    for(i = 0 ; i < skipFilePatterns.length ; i++) {
      if( (file.match(/\.js$/) 
         && (! file.match(skipFilePatterns[i]))))
      {
        useFile = true;
      }
    }
    if(useFile) {
      exports.tests.push(lintTest(dirname, file));
    }
  }
}

function checkFile(base, mid, file) {
  var dirname = path.join(base, mid);
  exports.tests.push(lintTest(dirname, file));
}


/// ******** SMOKE TEST FOR THIS SUITE ******* ///
exports.tests = [];

var smokeTest = new harness.SmokeTest("jslint smoke test");
smokeTest.run = function runLintSmokeTest() {
  if(skipTests) {
    this.fail("jslint is not available");
  } else if (typeof linter !== 'function') {
    this.fail("incompatible jslint");
  }
  else {
    this.pass();
  }
};
exports.tests.push(smokeTest);


// ****** SOURCES FILES TO CHECK ********** //

checkDirectory(mynode.fs.adapter_dir, "impl/common");
checkDirectory(mynode.fs.adapter_dir, "impl/mysql");
checkDirectory(mynode.fs.adapter_dir, "impl/ndb");
checkDirectory(mynode.fs.adapter_dir, "api");

checkFile(mynode.fs.suites_dir, "lint", "LintTest.js");
checkFile(mynode.fs.suites_dir, "", "driver.js");
checkFile(mynode.fs.suites_dir, "lib", "harness.js");
checkFile(mynode.fs.suites_dir, "", "utilities.js");

checkDirectory(mynode.fs.suites_dir, "spi");
checkDirectory(mynode.fs.suites_dir, "numerictypes");
checkDirectory(mynode.fs.suites_dir, "stringtypes");
checkDirectory(mynode.fs.suites_dir, "autoincrement");
checkDirectory(mynode.fs.suites_dir, "multidb");
checkDirectory(mynode.fs.suites_dir, "t_basic");
checkDirectory(mynode.fs.suites_dir, "composition");
checkDirectory(mynode.fs.suites_dir, "freeform");

checkDirectory(mynode.fs.samples_dir, "loader", "lib");
checkDirectory(mynode.fs.samples_dir, "tweet");

/**** ERRORS TO IGNORE:
   ignore(filename, startpos, message) 
   Ignores error in <filename> starting at character <startpos> of any line
   and matching <message>.
   If multiple errors are declared for one file, they must match in the order declared.
***/

// Adapter/impl/common
ignore("IndexBounds.js", 11, "Expected a conditional expression and instead saw an assignment.");
ignore("IndexBounds.js", 13, "Expected a conditional expression and instead saw an assignment.");

// Adapter/impl/ndb
ignore("NdbOperation.js", 22, "Use the array literal notation [].");  // 374
ignore("NdbOperation.js",27,"\'gather\' was used before it was defined."); //550

ignore("NdbConnectionPool.js",15,"Expected a conditional expression and instead saw an assignment.");
ignore("NdbConnectionPool.js",17,"Expected a conditional expression and instead saw an assignment.");

ignore("LintTest.js",14,"Expected a conditional expression and instead saw an assignment.");
ignore("TableMapping.js",3,"The body of a for in should be wrapped in an if statement to filter unwanted properties from the prototype.");
ignore("stats.js",13,"Expected '{' and instead saw 'r'.");
ignore("MySQLDictionary.js",7,"Missing 'break' after 'case'.");

ignore("UserContext.js", 33, "Unexpected \'\\.\'.");
ignore("UserContext.js", 7, "Confusing use of \'!\'.");

ignore("NdbTransactionHandler.js", 32, "Expected \'{\' and instead saw \'scans\'.");
ignore("NdbScanFilter.js", 34, "Expected \'{\' and instead saw \'return\'.");

// spi
ignore("BasicVarcharTest.js", 19, "Expected \'{\' and instead saw \'onSession\'.");
ignore("BasicVarcharTest.js", 10, "Expected \'{\' and instead saw \'connection\'.");
ignore("SmokeTest.js", 13, "Expected \'{\' and instead saw \'test\'.");
ignore("SmokeTest.js", 10, "Expected \'{\' and instead saw \'test\'.");

//stringtypes
ignore("CharsetTest.js", 27, "Missing \'new\'.");
ignore("CharsetTest.js", 26, "Missing \'new\'.", 14);

//numerictypes
ignore("QueryKeywordTest.js", 95, "Expected \'String\' and instead saw \'\'\'\'.");
ignore("lib.js", 95, "Expected \'String\' and instead saw \'\'\'\'.");

// t_basic
ignore("BatchTest.js", 6, "Don't make functions within a loop.");
ignore("ParallelOperationTest.js", 6, "Don't make functions within a loop.");
ignore("SaveTest.js", 8, "Don't make functions within a loop.");
ignore("SaveTest.js", 8, "Don't make functions within a loop.");
ignore("SaveTest.js", 10, "Don't make functions within a loop.");
ignore("UpdateTest.js", 8, "Don't make functions within a loop.");
ignore("UpdateTest.js", 10, "Don't make functions within a loop.");
ignore("UpdateTest.js", 8, "Don't make functions within a loop.");
ignore("UpdateTest.js", 10, "Don't make functions within a loop.");
ignore("UpdateTest.js", 8, "Don't make functions within a loop.");
ignore("UpdateTest.js", 10, "Don't make functions within a loop.");
ignore("UpdateTest.js", 8, "Don't make functions within a loop.");
ignore("UpdateTest.js", 10, "Don't make functions within a loop.");

// multidb
ignore("ConnectTest.js", 42,  "Unexpected \'\\.\'.");
ignore("ConnectTest.js", 42,  "Unexpected \'\\.\'.");
