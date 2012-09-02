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

/** This is the smoke test for the spi suite.
    We go just as far as getDBServiceProvider().
    This tests the loading of required compiled code in shared library files.
 */

/*global path, fs, assert, spi_module, udebug,
         harness, build_dir, adapter_dir
*/

"use strict";

try {
  require("./suite_config.js");
} catch (e) {}


var test = new harness.SmokeTest("LoadModule");

test.run = function() {
  var spi = require(spi_module),
      service = spi.getDBServiceProvider(global.adapter),
      modules = service.getNativeCodeModules(),
      test = this,
      i;

  /* Test loading the required native code modules */
  for (i = 0 ; i < modules.length ; i++) {
    require(path.join(build_dir, modules[i]));
  }

  /* Create SQL if there is a file */
  harness.SQL.create(this.suite, function() {
    test.pass();
  });
  
};

exports.tests = [test];
