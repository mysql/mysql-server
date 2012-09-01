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

/*global path, fs, assert,
         driver_dir, suites_dir, adapter_dir, build_dir,
         spi_module, api_module, udebug_module,
         harness, mynode, udebug, debug,
         adapter, test_conn_properties,
         module, exports
*/

"use strict";

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);

udebug.level_detail();

var session = null;
var table = null;

var t1 = new harness.SerialTest("InsertInt");

t1.setup = function setup_tests(testObj) {
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      connection = null,
      test = this;

  if(session && table) {  // already set up
    this.runTestMethod(testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("InsertIntTest.js setup_tests onTable");
    table = dbTable;
    if(err) {  test.fail(err);               }
    else    {  test.runTestMethod(testObj);  }
  }

  function onSession(err, sess) {
    udebug.log("InsertIntTest.js setup_tests onSession");
    session = sess;
    if(err) {   test.fail(err);   }
    else    {   session.getConnectionPool().getTable("test", "tbl1", onTable); }
  }

  function onConnect(err, conn) {
    udebug.log("InsertIntTest.js setup_tests onConnect");
    connection = conn;
    connection.getDBSession(0, onSession);
  }
    
  provider.connect(properties, onConnect);
};

t1.runTestMethod = function do_insert_op(dataObj) {
  udebug.log("InsertIntTest.js do_insert_op");

  var tx = session.openTransaction();
  var thandler = session.getConnectionPool().createDBTableHandler(table, null);
  var test = this;
  
  var op = session.insert(thandler, dataObj);

  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    tx.close();
    if(err) { test.fail("Execute/commit failed: " + err);  }
    else    { test.pass(); }
  });  
};

t1.run = function() {
  var insertObj = { i : 13, j: 15 };
  this.setup(insertObj);
};


//// DELETE TEST

var t2 = new harness.SerialTest("DeleteIntPK");
t2.setup = t1.setup;

t2.runTestMethod = function do_delete_op(keyObj) {
  udebug.log("InsertIntTest.js do_delete_op");
  var tx = session.openTransaction();
  var thandler = session.getConnectionPool().createDBTableHandler(table, null);
  var test = this;

  var op = session.delete(thandler, keyObj);
  
  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    tx.close();
    if(err) { test.fail("Execute/commit failed: " + err); }
    else    { test.pass(); }
  });
};

t2.run = function() {
  var deleteKey = { i : 13 };
  this.setup(deleteKey);
};


exports.tests = [ t2 ];

