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
var assert = require("assert");
var dbtablehandler = require(mynode.common.DBTableHandler);
var udebug = unified_debug.getLogger("BasicVarcharTest.js");
var TableMapping = require(mynode.api.TableMapping).TableMapping;

var dbSession = null;
var table = null;
var dbt = null;
var mapping = new TableMapping("test.tbl3");
mapping.mapField("id", "i");
mapping.mapField("name", "c");


var t1 = new harness.SerialTest("Insert"),
    t2 = new harness.SerialTest("Read_1"),
    t3 = new harness.SerialTest("Update"),
    t4 = new harness.SerialTest("Read_2"),
    t5 = new harness.SerialTest("Insert_Duplicate"), 
    t6 = new harness.SerialTest("Write"),
    t7 = new harness.SerialTest("Read_3"),
    t8 = new harness.SerialTest("Delete"),
    t9 = new harness.SerialTest("Delete_NotFound"),
    close = new harness.SerialTest("CloseSession");


/// Common prep

function prepare(testCase, testObj) {
  var connection = null;

  if(dbSession && table) {  // already set up
    testCase.runTestMethod(testCase, testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("prepare onTable");
    if(err) {  testCase.fail(err);               }
    else {
      table = dbTable;         // set global
      dbt = new dbtablehandler.DBTableHandler(table, mapping, null);   // set global
      testCase.runTestMethod(testCase, testObj);
    }
  }

  function onSession(err, sess) {
    udebug.log("prepare onSession");
    dbSession = sess; // set global
    if(err) {   testCase.fail(err);   }
    else    {   
      dbSession.getConnectionPool().getTableMetadata("test", "tbl3", dbSession, onTable); 
    }
  }

  function onConnect(err, conn) {
    udebug.log("prepare onConnect");
    connection = conn;
    if(dbSession) onSession(null, dbSession);
    else connection.getDBSession(spi_lib.allocateSessionSlot(), onSession);
  }
  
  spi_lib.getConnectionPool(onConnect);
}


function do_insert_op(testCase, dataObj) {
  udebug.log("do_insert_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var op = dbSession.buildInsertOperation(dbt, dataObj, tx, null);
  tx.execute([ op ], testCase.checkResult);
}

function do_read_op(testCase, keyObj) {
  udebug.log("do_read_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var index = dbt.getIndexHandler(keyObj);
  var op = dbSession.buildReadOperation(index, keyObj, tx);
  tx.execute([ op ], testCase.checkResult);
}

function do_update_op(testCase, dataObj) {
  assert(typeof testCase.checkResult === 'function');
  udebug.log("do_update_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var dbix = dbt.getIndexHandler(dataObj.keys);
  var op = dbSession.buildUpdateOperation(dbix, dataObj.keys, dataObj.values, tx, null);
  tx.execute([ op ], testCase.checkResult);
}

function do_write_op(testCase, dataObj) {
  assert(typeof testCase.checkResult === 'function');
  udebug.log("do_write_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var dbix = dbt.getIndexHandler(dataObj);
  var op = dbSession.buildWriteOperation(dbix, dataObj, tx, null);
  tx.execute([ op ], testCase.checkResult);
}

function do_delete_op(testCase, keyObj) {
  udebug.log("do_delete_op for", testCase.name);
  var tx = dbSession.getTransactionHandler();
  var dbix = dbt.getIndexHandler(keyObj);
  var op = dbSession.buildDeleteOperation(dbix, keyObj, tx, null);  
  tx.execute([ op ], testCase.checkResult);
}

/// INSERT
t1.runTestMethod = do_insert_op;

t1.checkResult = function(err, tx) {
  udebug.log("checkResult");
  var op;
  if(err) { 
    t1.appendErrorMessage("t1 ExecuteCommit failed: " + err);
  }
  else {
    op = tx.executedOperations.pop();
    t1.errorIfNotEqual("t1 operation failed", true, op.result.success);
  }
  t1.failOnError();
};

t1.run = function() {
  var insertObj = { id : 1 , name : "Henry" };
  prepare(t1, insertObj);
};


// READ
t2.runTestMethod = do_read_op;

t2.checkResult = function(err, tx) {
  udebug.log("checkResult t2");
  var op;
  if(err) { 
    t2.appendErrorMessage("t2 ExecuteCommit failed: " + err); 
  }
  else { 
    op = tx.executedOperations.pop();
    t2.errorIfNull("Null op", op);
    t2.errorIfNull("Null op.result", op.result);
    t2.errorIfNull("Null op.result.value", op.result.value);
    t2.errorIfNotEqual("Expected Henry", "Henry", op.result.value.name);
  }
  t2.failOnError();
};

t2.run = function() {
  var readObj = { id: 1 };
  prepare(t2, readObj);
};


// UPDATE 
t3.runTestMethod = do_update_op;

t3.checkResult = function(err, tx) {
  udebug.log("checkResult t3");
  if(err) { 
    t3.appendErrorMessage("t3 ExecuteCommit failed: " + err);  
  }
  else { 
    var op = tx.executedOperations.pop();
    if(op) {
      t3.errorIfNotEqual("Operation failed", true, op.result.success);
    }
  }
  t3.failOnError();
};

t3.run = function() {
  var dataObj = { keys: { id : 1 }, values: {name: "Henrietta" } };
  prepare(t3, dataObj);
};


// READ AGAIN
t4.runTestMethod = do_read_op;

t4.checkResult = function(err, tx) {
  udebug.log("checkResult t4");
  var op;
  if(err) { 
    t4.appendErrorMessage("t4 ExecuteCommit failed: " + err);  
  }
  else { 
    op = tx.executedOperations.pop();
    if (op.result.value !== null) {
      t4.errorIfNotEqual("Expected Henrietta", 'Henrietta', op.result.value.name);
    } else {
      t4.appendErrorMessage('No object found for Henrietta.');
    }
  }
  t4.failOnError();
};

t4.run = function() {
  var readObj = { id: 1 };
  prepare(t4, readObj);
};


// INSERT DUPLICATE
t5.runTestMethod = do_insert_op;

t5.checkResult = function(err, tx) {
  udebug.log("checkResult t5");
  var op;
  if(err) { 
    if (err.cause) {
      t5.errorIfNotEqual("t5 cause.sqlstate", '23000', err.cause.sqlstate);
    } else {
      t5.appendErrorMessage("t5 transaction error has no cause.");
    }
    op = tx.executedOperations.pop();
    t5.errorIfNotEqual("t5 error.sqlstate", "23000", op.result.error.sqlstate);
  } else {
    t5.appendErrorMessage("t5 transaction error did not occur.");
  }
  t5.failOnError();
};

t5.run = function() {
  var insertObj = { id : 1 , name : "Henry II" };
  prepare(t5, insertObj);
};

// WRITE 
t6.runTestMethod = do_write_op;

t6.checkResult = function(err, tx) {
  udebug.log("checkResult t6");
  if(err) {
    t6.appendErrorMessage("t6 ExecuteCommit failed: " + err);  
  }
  else { 
    var op = tx.executedOperations.pop();
    if(op) {
      t6.errorIfNotEqual("t6 Operation failed", true, op.result.success);
    }
  }
  t6.failOnError();
};

t6.run = function() {
  var dataObj =  { id : 1 , name : "Henry VI"};
  prepare(t6, dataObj);
};

// READ AGAIN
t7.runTestMethod = do_read_op;

t7.checkResult = function(err, tx) {
  udebug.log("checkResult t7");
  var op;
  if(err) { 
    t7.appendErrorMessage("t7 ExecuteCommit failed: " + err);  
  }
  else { 
    op = tx.executedOperations.pop();
    if (op.result.value !== null) {
      t7.errorIfNotEqual("t7 Wrong name", 'Henry VI', op.result.value.name);
    } else {
      t7.appendErrorMessage("t7 No object found for Henry VI.");
    }
  }
  t7.failOnError();
};

t7.run = function() {
  var readObj = { id: 1 };
  prepare(t7, readObj);
};

// DELETE 
t8.runTestMethod = do_delete_op;

t8.checkResult = function(err, tx) {
  udebug.log("checkResult t8");
  var op;
  if(err) { 
    t8.appendErrorMessage("t8 ExecuteCommit failed: " + err); 
  } else {
    op = tx.executedOperations.pop();
  }
  t8.failOnError();
};

t8.run = function() {
  var deleteKey = { id : 1 };
  prepare(t8, deleteKey);
};
  

// DELETE SAME ROW (EXPECT ERROR) 
t9.runTestMethod = do_delete_op;

t9.checkResult = function(err, tx) {
  var op;
  udebug.log("checkResult t9");
  if(err) {
    if (err.cause) {
      t9.errorIfNotEqual("t9 cause.sqlstate", '02000', err.cause.sqlstate);
    } else {
      t9.appendErrorMessage("t9 transaction error has no cause.");
    }
    op = tx.executedOperations.pop();
    t9.errorIfNotEqual("t9 sqlstate", '02000', op.result.error.sqlstate);
  } else {
    t9.appendErrorMessage("t9 transaction error did not occur.");
  }
  t9.failOnError();
};

t9.run = function() {
  var deleteKey = { id : 1 };
  prepare(t9, deleteKey);
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

exports.tests = [ t1, t2, t3, t4, t5, t6, t7, t8, t9, close];
