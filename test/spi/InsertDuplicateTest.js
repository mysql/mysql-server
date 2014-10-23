/*
 Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights
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

var spi_lib = require("./lib.js");
var dbtablehandler = require(mynode.common.DBTableHandler);
var udebug = unified_debug.getLogger("InsertDuplicateTest.js");
var TableMapping = require(mynode.api.TableMapping).TableMapping;

var dbSession = null;
var table = null;
var dbt = null;
var mapping = new TableMapping("test.tbl4");
mapping.mapField("id", "i");
mapping.mapField("uk", "k");
mapping.mapField("name", "c");

var t1 = new harness.SerialTest("Insert1"),
    t2 = new harness.SerialTest("Insert1_DupPK"),
    t3 = new harness.SerialTest("Insert1_DupUK"),
    t4 = new harness.SerialTest("Insert2_OK_dupPK"),
    t5 = new harness.SerialTest("Insert2_OK_dupUK"),
    t6 = new harness.SerialTest("Insert2_dupPK_dupUK"),
    t7 = new harness.SerialTest("Delete"),
    close = new harness.SerialTest("CloseSession");

/// Common prep

function prepare(testCase, runTestMethod, testObj) {
  
  var connection = null;

  if(dbSession && table) {  // already set up
    runTestMethod(testCase, testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("prepare onTable");
    table = dbTable;         // set global
    if(err) {  
      testCase.fail(err);               
    }
    else    {   /* set global dbt */
      dbt = new dbtablehandler.DBTableHandler(table, mapping, null);
      runTestMethod(testCase, testObj);  }
  }

  function onSession(err, sess) {
    udebug.log("prepare onSession");
    dbSession = sess; // set global
    if(err) {   testCase.fail(err);   }
    else    {   
      dbSession.getConnectionPool().getTableMetadata("test", "tbl4", dbSession, onTable); 
    }
  }

  function onConnect(err, conn) {
    udebug.log("prepare onConnect");
    connection = conn;
    connection.getDBSession(spi_lib.allocateSessionSlot(), onSession);
  }
  
  spi_lib.getConnectionPool(onConnect);
}


function do_insert_op(testCase, dataObj) {
  udebug.log("do_insert_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var op = dbSession.buildInsertOperation(dbt, dataObj, tx, null);
  tx.execute([ op ], testCase.checkResult);
}

function insert_two_rows(testCase, data) {
  udebug.log("insert_two_rows for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var op1 = dbSession.buildInsertOperation(dbt, data[0], tx, null);
  var op2 = dbSession.buildInsertOperation(dbt, data[1], tx, null);
  tx.execute([ op1 , op2 ], testCase.checkResult);
}

function do_delete_op(testCase, keyObj) {
  udebug.log("do_delete_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var dbix = dbt.getIndexHandler(keyObj);
  var op = dbSession.buildDeleteOperation(dbix, keyObj, tx, null);  
  tx.execute([ op ], testCase.checkResult);
}

// INSERT
t1.run = function() {
  var insertObj = { id : 501 , uk: 511, name : "Henry" };
  prepare(t1, do_insert_op, insertObj);
};
t1.checkResult = function(err, tx) {
  if (err) {
    t1.appendErrorMessage("t1 unexpected error" + err);
  }
  t1.failOnError();
};


// INSERT; DUPLICATE VALUE FOR PRIMARY KEY
t2.run = function() {
  var insertObj = { id : 501 , uk: 999 , name : "Henry II" };
  prepare(t2, do_insert_op, insertObj);
};

t2.checkResult = function(err, tx) {
  try {
    t2.errorIfNotEqual("t2 cause.sqlstate", '23000', err.cause.sqlstate);
    t2.errorIfNotEqual("t2 operation error sqlstate", '23000', tx.executedOperations[0].result.error.sqlstate);
  }
  catch(e) {
    t2.appendErrorMessage("t2 exception " + e.message);
  }
  t2.failOnError();
};


// INSERT; DUPLICATE VALUE FOR UNIQUE KEY
t3.run = function() {
  var insertObj = { id : 999 , uk: 511 , name : "Henry II" };
  prepare(t3, do_insert_op, insertObj);
};

t3.checkResult = function(err, tx) {
  try {
      t3.errorIfNotEqual("t3 operation error sqlstate", '23000', tx.executedOperations[0].result.error.sqlstate);
  } 
  catch(e) {
    t3.appendErrorMessage('t3 exception ' + e.message);
  }
  t3.failOnError();
};


// INSERT.  Row1 is OK; Row2 is duplicate PK.
t4.run = function() {
  var data = [];
  data[0] = { id : 502, uk: 512 , name : "George" };
  data[1] = { id : 501, uk: 515 , name : "Edward" };
  prepare(t4, insert_two_rows, data);
};
t4.checkResult = function(err, tx) {
  // The transaction and second op must have errors. 
  // We make no claim on the status of the first operation
  try {
      t4.errorIfNotEqual("t4 Transaction Error Sqlstate", "23000", err.cause.sqlstate);
      t4.errorIfNotEqual("t4 operation sqlstate", "23000", 
                        tx.executedOperations[1].result.error.sqlstate);
  }
  catch(e) {
    t4.appendErrorMessage("t4 exception " + e.message);
  }
  t4.failOnError();
};


// INSERT. Row1 is OK; Row2 is duplicate unique key
t5.run = function() {
  var data = [];
  data[0] = { id : 503, uk: 513 , name : "George" };
  data[1] = { id : 505, uk: 511 , name : "Edward" };
  prepare(t5, insert_two_rows , data);
};
t5.checkResult = function(err, tx) {
  // The Transaction & second op must have errors
  // We make no claim on the status of the first operation
  try {
    t5.errorIfNotEqual("t5 Transaction Error Sqlstate", "23000", err.cause.sqlstate);
    t5.errorIfNotEqual("t5 Operation Error Sqlstate", "23000",
                      tx.executedOperations[1].result.error.sqlstate);
  }
  catch(e) {
    t5.appendErrorMessage("t5 exception " + e.message);
  }
  t5.failOnError();
};

// INSERT. Row1 is duplicate PK; Row2 is duplicate unique key
t6.run = function() {
  var data = [];
  data[0] = { id : 501, uk: 513, name : "George" };
  data[1] = { id : 505, uk: 511, name : "Edward" };
  prepare(t6, insert_two_rows , data);
};
t6.checkResult = function(err, tx) {
  // Transaction and both operations must have an error
  try {
    t6.errorIfNotEqual("t6 Transaction ErrorSqlstate", "23000", err.cause.sqlstate);
    if (!tx.executedOperations[0].result.error) {
      t6.appendErrorMessage("t6 operation 0 error did not occur.");
    }
    if (!tx.executedOperations[1].result.error) {
      t6.appendErrorMessage("t6 operation 1 error did not occur.");
    }
  }
  catch(e) {
    t6.appendErrorMessage("t6 exception " + e.message);
  }

  t6.failOnError();
};


   
// DELETE BY PK
t7.run = function() {
  var deleteKey = { id : 501 };
  prepare(t7 , do_delete_op , deleteKey);
};
t7.checkResult = function(err, tx) {
  if (err) {
    t7.appendErrorMessage("t7 ExecuteCommit failed: " + err);
  }
  t7.failOnError();
};
  
/** This test function must be the last in the test file.
 */
close.run = function() {
  dbSession.close(function(err) {
    if (err) {
      close.fail("Close got error: " + err);
    } else {
      close.pass();
    }
  });
};


exports.tests = [ t1, t2, t3, t4, t5, t6, t7, close ];
