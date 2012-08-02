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

try {
  require("./suite_config.js");
} catch(e) {};

var test = new harness.SmokeTest("LoadModule");

test.run = function() {
  var response;
  // FIXME:
  // ask the {adapter}_service_provider for all required native modules
  // and try to load them
  var dbpath = path.join(build_dir, "common", "debug_dlopen.node");
  if(debug) console.log("Loading: " + dbpath);

  // LOAD the debug_dlopen module
  try {
    var db = require(dbpath);
  }
  catch(e) {
    console.log(e.stack);
    this.appendErrorMessage(e.message);
    this.fail();
  }
  
  // Then use debug_dlopen() to load the ndbapi module
  var ndb_module_path = path.join(build_dir, "ndb", "ndbapi.node");
  if(debug) console.log("Loading: " + ndb_module_path);
  response = db.debug_dlopen(ndb_module_path);

  // iterate over the module list here
  if(response != "OK") {
  // message = module_name + ":" + response
    this.appendErrorMessage(response);
  }

  return true;
}

module.exports.tests = [test];
