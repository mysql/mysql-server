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

/*global udebug, path, fs, assert, spi_module, harness */

"use strict";

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);
var session = null;
var table = null;

var commonTableHandler = require("../../Adapter/impl/common/DBTableHandler.js");
var t1 = new harness.SerialTest("InsertInt");
var t2 = new harness.SerialTest("DeleteIntPK");

function do_insert_op(session, table) {
  udebug.log("InsertIntTest.js do_insert_op");
  /* Pass the variables on to the next test */
  t2.session = session;
  t2.table = table;

  var tx = session.openTransaction();
  var dbConnectionPool = session.getConnectionPool();
  var thandler = new commonTableHandler.DBTableHandler(dbConnectionPool, table, null);
  
  var row = { i: 13 , j: 14 };
  var op = session.insert(thandler, row);

  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    if(err) { t1.fail("Execute/commit failed: " + err);  }
    else    { t1.pass(); }
  });
}

t1.run = function() {  
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      connection = null,
      test = this;

  if(session && table) {  // already set up
    this.runTestMethod(testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("InsertIntTest.js prepare onTable");
    table = dbTable;  // set global
    if(err) {  test.fail(err);               }
    else    {  test.runTestMethod(testObj);  }
  }

  function onSession(err, sess) {
    udebug.log("InsertIntTest.js prepare onSession");
    session = sess; // set global
    if(err) {   test.fail(err);   }
    else    {   session.getConnectionPool().getTableMetadata("test", "tbl1", null, onTable); }
  }

  function onConnect(err, conn) {
    udebug.log("InsertIntTest.js prepare onConnect");
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
  
  var op = session.buildInsertOperation(thandler, dataObj);

  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    tx.close();
    if(err) { test.fail("Execute/commit failed: " + err);  }
    else    { test.pass(); }
  });  
};

t1.run = function() {
  var insertObj = { i : 13, j: 15 };
  this.prepare(insertObj);
};


//// DELETE TEST

var t2 = new harness.SerialTest("DeleteIntPK");
t2.prepare = t1.prepare;

t2.runTestMethod = function do_delete_op(keyObj) {
  udebug.log("InsertIntTest.js do_delete_op");
  var tx = session.openTransaction();
  var thandler = session.getConnectionPool().createDBTableHandler(table, null);
  var test = this;

  var op = session.buildDeleteOperation(thandler, keyObj);
  
  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    tx.close();
    if(err) { test.fail("Execute/commit failed: " + err); }
    else    { test.pass(); }
  });
};

t2.run = function() {
  var deleteKey = { i : 13 };
  this.prepare(deleteKey);
};


exports.tests = [ t1, t2  ];

