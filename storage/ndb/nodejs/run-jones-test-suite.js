/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights
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

var jones       = require("database-jones"),
    driver      = require(jones.fs.test_driver),
    index       = 0,
    adapters    = ['ndb'],
    adapter,
    a_module,
    properties;


driver.addCommandLineOption("-a", "--adapter", "only run on the named comma-separated adapter/engine(s)",
  function(thisArg) {
    if(thisArg) {
      adapters   = thisArg.split(",");
      return 1;
    }
    return -1;  // adapter is required
  });

function runAllTests(exitStatus) {
  var adapterWithExtra, split;
  var extra = '';

   if (exitStatus) {
    console.log('Abnormal exit:', exitStatus);
    process.exit(exitStatus);
  }
  if (index < adapters.length) {
    console.log('driver run ' + index + ' using adapter ' + adapters[index]);
  }
  adapterWithExtra = adapters[index++];
  if (adapterWithExtra === undefined) {
    // all adapters complete; exit
    process.exit(0);
  }
  split = adapterWithExtra.split('/');
  adapter = split[0];
  extra = split[1] || '';

  /* Reset driver.suites before each run */
  driver.resetSuites();

  /* Start with the standard Jones test suites */
  driver.addSuitesFromDirectory(jones.fs.suites_dir);

  /* Add the test suite for the specified adapter, and
   set the Connection Properties for the specified adapter.
   */
  a_module = require ("jones-" + adapter);
  driver.addSuitesFromDirectory(a_module.config.suites_dir);
  properties = driver.getConnectionProperties(adapter);

  /* Adapter-specific code goes here */
  switch(adapter) {
    case "ndb":           /* NDB also runs the MySQL Test suite */
      a_module = require("jones-mysql");
      driver.addSuitesFromDirectory(a_module.config.suites_dir);
      if (extra.indexOf('async') !== -1) { properties.use_ndb_async_api = true; }
      if (extra.indexOf('nomap') !== -1) { properties.use_mapped_ndb_record = false; }
      break;
    case "mysql":         /* MySQL uses the extra argument to set engine */
      if(extra) { properties.mysql_storage_engine = extra; }
      break;
    default:
      break;
  }

  /* Set globals */
  global.mynode               = jones;
  global.adapter              = adapter;
  global.test_conn_properties = properties;

  /* Run all tests for this adapter/extra and call back when done */
  driver.name = adapterWithExtra;
  driver.runAllTests(runAllTests);
}

/* iterate over the adapters; each adapter has optional /extra */
driver.processCommandLineOptions();
runAllTests();
