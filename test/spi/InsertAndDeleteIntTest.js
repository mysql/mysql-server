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

"use strict";

try {
  require("./suite_config.js");
} catch(e) {} 

var path = require("path"),
    spi_lib = require("./lib.js"),
    dbt     = require(mynode.common.DBTableHandler),
    udebug  = unified_debug.getLogger("InsertAndDeleteIntTest.js");

var dbSession = null,
    table = null;

var t1 = new harness.SerialTest("InsertInt");

t1.prepare = function prepare(testObj) {
  var connection = null,
      test = this;

  if(dbSession && table) {  // already set up
    this.runTestMethod(testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("prepare onTable");
    table = dbTable;  // set global
    if(err) {  test.fail(err);               }
    else    {  test.runTestMethod(testObj);  }
  }

  function onSession(err, sess) {
    udebug.log("prepare onSession");
    dbSession = sess; // set global
    if(err) {   test.fail(err);   }
    else    {   dbSession.getConnectionPool().getTableMetadata("test", "tbl1", dbSession, onTable); }
  }

  function onConnect(err, conn) {
    udebug.log("prepare onConnect");
    connection = conn;
    connection.getDBSession(spi_lib.allocateSessionSlot(), onSession);
  }
  
  spi_lib.getConnectionPool(onConnect);
};

t1.runTestMethod = function do_insert_op(dataObj) {
  udebug.log("do_insert_op");

  var tx = dbSession.getTransactionHandler();
  var thandler = new dbt.DBTableHandler(table, null, null);
  var test = this;
  
  var op = dbSession.buildInsertOperation(thandler, dataObj, tx, null);

  tx.execute([ op ], function(err, tx) {
    if(err) { 
      test.appendErrorMessage("ExecuteCommit failed: " + err);  
    } else {
      tx.executedOperations.pop();
    }
    test.failOnError();
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
  udebug.log("do_delete_op");
  var tx = dbSession.getTransactionHandler();
  var thandler = new dbt.DBTableHandler(table, null, null);
  var ixhandler = thandler.getIndexHandler(keyObj);
  var test = this;

  var op = dbSession.buildDeleteOperation(ixhandler, keyObj, tx, null);
  
  tx.execute([ op ], function(err, tx) {
    if(err) { 
      test.appendErrorMessage("ExecuteCommit failed: " + err); 
    } else {
      tx.executedOperations.pop();
    }
    test.failOnError();
  });
};

t2.run = function() {
  var deleteKey = { i : 13 };
  this.prepare(deleteKey);
};

/** This test function must be the last in the test file.
 */
var close = new harness.SerialTest("CloseSession");
close.run = function() {
  dbSession.close(function(err) {
    if (err) {
      close.fail("Close got error: " + err);
    } else {
      close.pass();
    }
  });
};

exports.tests = [ t1, t2, close  ];

