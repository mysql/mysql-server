/*
Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights
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

var harness = require("jones-test");
var config = require("jones-ndb").config;
var tests = [ new harness.LintSmokeTest() ];

function more(more_tests) {
  if(harness.linterAvailable) {
    Array.prototype.push.apply(tests, more_tests);
  }
}

more(harness.getLintTestsForDirectory(config.impl_js_dir));
more(harness.getLintTestsForDirectory(config.converters_dir));
more(harness.getLintTestsForDirectory(config.root_dir));

harness.ignoreLint("NdbOperation.js", 22, "Use the array literal notation");
harness.ignoreLint("NdbOperation.js", 27, "'gather' was used before it was defined.");

harness.ignoreLint("NdbConnectionPool.js",15,"Expected a conditional expression and instead saw an assignment.");
harness.ignoreLint("NdbConnectionPool.js",17,"Expected a conditional expression and instead saw an assignment.");

module.exports.tests = tests;
