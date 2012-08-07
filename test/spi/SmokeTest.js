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

/*global path, fs, assert,
         driver_dir, suites_dir, adapter_dir, build_dir,
         spi_module, api_module, udebug_module,
         harness, mynode, udebug, debug,
         adapter, test_conn_properties,
         module, exports
*/

try {
  require("./suite_config.js");
} catch (e) {}


var test = new harness.SmokeTest("LoadModule");

function test_load_modules(modules) {
  "use strict";
  var response, i, oks = 0, modulepath = "", db;
  var dbpath = path.join(build_dir, "common", "debug_dlopen.node");

  try {
    db = require(dbpath);
  } catch (e) {
    console.log(e.stack);
    test.appendErrorMessage(e.message);
    test.fail();
    return false;
  }

  for (i = 0 ; i < modules.length ; i++) {
    modulepath = path.join(adapter_dir, modules[i]);
    response = db.debug_dlopen(modulepath);
    if(response === "OK") {
      oks++;
    }
    else {
      test.appendErrorMessage(response);
    }
  }
  return (modules.length === oks) ? true : false;
}
 

test.run = function() {
  "use strict";
  var spi = require(spi_module);
  var service = spi.getDBServiceProvider(global.adapter);
  var modules = service.getNativeCodeModules();

  /* Create SQL */
  harness.SQL.create(this.suite, function(error) {
    if (error) {
      this.fail('createSQL failed: ' + error);
      return;
    }
  });

  /* Get the list of required modules from the adapter */

  /* Test loading them */
  if(test_load_modules(modules) === false) {
    this.fail();
    return;
  }

  this.pass();
}

exports.tests = [test];
