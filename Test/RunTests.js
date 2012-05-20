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

global.api_module = path.join(parent, "Adapter", "api", "ClusterJ.js");
global.spi_module = path.join(parent, "Adapter", "spi", "SPI.js");

var Test = require(global.test_harness_module);

var result = new Test.Result();
result.listener = new Test.Listener();

var re_matching_test_case = /\.test\.js$/;

function runTestsInDir(d) {
  var tests = [];
  var subdirs = [];

  /* Read the directory, building lists of tests and subdirectories
  */
  var files = fs.readdirSync(d);
  for(var i = 0; i < files.length ; i++) {
    var f = files[i];
    var st = fs.statSync(path.join(d, f));

    if(st.isFile() && re_matching_test_case.test(f)) {
      var t = require(path.join(d, f));
      if(! t.isTest()) {
          throw { name : "NotATest" ,
                  message : "Module " + f + " does not export a Test."
                };
      }
      tests.push(t);
    }
    else if(st.isDirectory()) { 
      subdirs.push(f);
    }
  }


  /* Sort the tests */
  tests.sort(function(a,b) {
     if(a.order < b.order) return -1;
     if(a.order == b.order) return 0;
     return 1;
  });

  /* Run each test */
  for(var i = 0; i < tests.length ; i++) {
    tests[i].test(result);
  }
  
  /* Then descend into subdirectories */
  for(var i = 0 ; i < subdirs.length ; i++) {
    runTestsInDir(path.join(d, subdirs[i]));
  }
}


/* Start in the current directory 
*/
runTestsInDir(__dirname);

console.log("Passed: ", result.passed.length);
console.log("Failed: ", result.failed.length);
process.exit(result.failed.length > 0);

