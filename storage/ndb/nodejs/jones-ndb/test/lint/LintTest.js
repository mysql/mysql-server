/*
Copyright (c) 2015, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

"use strict";

var harness = require("jones-test");
var config = require("jones-ndb").config;
var tests = [new harness.LintSmokeTest()];

function more(more_tests) {
  if (harness.linterAvailable) {
    Array.prototype.push.apply(tests, more_tests);
  }
}

more(harness.getLintTestsForDirectory(config.impl_js_dir));
more(harness.getLintTestsForDirectory(config.converters_dir));
more(harness.getLintTestsForDirectory(config.root_dir));

harness.ignoreLint("NdbOperation.js", 22, "Use the array literal notation");
harness.ignoreLint(
    "NdbOperation.js", 27, "'gather' was used before it was defined.");

harness.ignoreLint(
    "NdbConnectionPool.js", 15,
    "Expected a conditional expression and instead saw an assignment.");
harness.ignoreLint(
    "NdbConnectionPool.js", 17,
    "Expected a conditional expression and instead saw an assignment.");

module.exports.tests = tests;
