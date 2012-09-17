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

/*global mynode, unified_debug, path, fs, assert, spi_module, harness, 
         adapter_dir, spi_dir 
*/

"use strict";

try {
  require("./suite_config.js");
} catch(e) {} 

var spi_lib = require("./lib.js");
var dbtablehandler = require(path.join(spi_dir, "common", "DBTableHandler.js"));
var udebug = unified_debug.getLogger("BasicVarcharTest.js");

var dbSession = null;
var table = null;
var dbt = null;
var annotations = new mynode.Annotations();
var mapping = annotations.newTableMapping("test.tbl3");
mapping.mapField("id", "i");
mapping.mapField("name", "c");


var t1 = new harness.SerialTest("Insert"),
    t2 = new harness.SerialTest("Read_1"),
    t3 = new harness.SerialTest("Update"),
    t4 = new harness.SerialTest("Read_2"),
    t5 = new harness.SerialTest("Insert_Duplicate"), 
    t6 = new harness.SerialTest("Delete"),
    t7 = new harness.SerialTest("Delete_NotFound");


/// Common prep

function prepare(testCase, testObj) {
  var connection = null;

  if(dbSession && table) {  // already set up
    testCase.runTestMethod(testCase, testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("prepare onTable");
    table = dbTable;         // set global
    dbt = new dbtablehandler.DBTableHandler(table, mapping);   // set global
    if(err) {  testCase.fail(err);               }
    else    {  testCase.runTestMethod(testCase, testObj);  }
  }

  function onSession(err, sess) {
    udebug.log("prepare onSession");
    dbSession = sess; // set global
    if(err) {   testCase.fail(err);   }
    else    {   
      dbSession.getConnectionPool().getTableMetadata("test", "tbl3", null, onTable); 
    }
  }

  function onConnect(err, conn) {
    udebug.log("prepare onConnect");
    connection = conn;
    connection.getDBSession(0, onSession);
  }
  
  spi_lib.getConnectionPool(onConnect);
}


function do_insert_op(testCase, dataObj) {
  udebug.log("do_insert_op for", testCase.name);
  var tx = dbSession.createTransaction();
  var op = dbSession.buildInsertOperation(dbt, dataObj, tx, null);
  tx.executeCommit([ op ], testCase.checkResult);
}

function do_read_op(testCase, keyObj) {
  udebug.log("do_read_op for", testCase.name);
  var tx = dbSession.createTransaction();
  var index = dbt.getIndexHandler(keyObj);
  var op = dbSession.buildReadOperation(index, keyObj, tx);
  tx.executeCommit([ op ], testCase.checkResult);
}

function do_update_op(testCase, dataObj) {
  assert(typeof testCase.checkResult === 'function');
  udebug.log("do_update_op for", testCase.name);
  var tx = dbSession.createTransaction();
  var dbix = dbt.getIndexHandler(dataObj.keys);
  var op = dbSession.buildUpdateOperation(dbix, dataObj.keys, dataObj.values, tx, null);
  tx.executeCommit([ op ], testCase.checkResult);
}

function do_delete_op(testCase, keyObj) {
  udebug.log("do_delete_op for", testCase.name);
  var tx = dbSession.createTransaction();
  var dbix = dbt.getIndexHandler(keyObj);
  var op = dbSession.buildDeleteOperation(dbix, keyObj, tx, null);  
  tx.executeCommit([ op ], testCase.checkResult);
}

/// INSERT
t1.runTestMethod = do_insert_op;

t1.checkResult = function(err, tx) {
  udebug.log("checkResult");
  var op;
  if(err) { t1.fail("ExecuteCommit failed: " + err);  }
  else { 
    op = tx.executedOperations.pop();
    t1.errorIfNotEqual("operation failed", op.result.success, true);
    t1.failOnError();
  }
  tx.close();
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
  if(err) { t2.fail("ExecuteCommit failed: " + err);  }
  else { 
    op = tx.executedOperations.pop();
    t2.errorIfNull("Null op", op);
    t2.errorIfNull("Null op.result", op.result);
    t2.errorIfNull("Null op.result.value", op.result.value);
    t2.errorIfNotEqual("Expected Henry", op.result.value.name, "Henry");
    t2.failOnError();
  }
  tx.close();
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
    t3.fail("ExecuteCommit failed: " + err);  
  }
  else { 
    var op = tx.executedOperations.pop();
    if(op) {
      t3.errorIfNotEqual("Operation failed", op.result.success, true);
      t3.failOnError();
    }
  }
  tx.close();
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
  if(err) { t4.fail("ExecuteCommit failed: " + err);  }
  else { 
    op = tx.executedOperations.pop();
    if (op.result.value !== null) {
      t4.errorIfNotEqual("Expected Henrietta", op.result.value.name, 'Henrietta');
    } else {
      t4.appendErrorMessage('No object found for Henrietta.');
    }
    t4.failOnError();
  }
  tx.close();
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
  if(err) { t5.fail("ExecuteCommit failed: " + err);  }
  else { 
    op = tx.executedOperations.pop();
    t5.errorIfNotEqual("Expected 23000", op.result.error.code, "23000"); 
    t5.failOnError();
  }
  tx.close();
};

t5.run = function() {
  var insertObj = { id : 1 , name : "Henry II" };
  prepare(t5, insertObj);
};


// DELETE 
t6.runTestMethod = do_delete_op;

t6.checkResult = function(err, tx) {
  udebug.log("checkResult t6");
  if(err) { t6.fail("ExecuteCommit failed: " + err); }
  else    { t6.pass(); }
  tx.close();
};

t6.run = function() {
  var deleteKey = { id : 1 };
  prepare(t6, deleteKey);
};
  

// DELETE AGAIN 
t7.runTestMethod = do_delete_op;

t7.checkResult = function(err, tx) {
  var op;
  udebug.log("checkResult t7");
  if(err) { t7.fail("ExecuteCommit failed: " + err); }
  else    { 
    op = tx.executedOperations.pop();
    t7.errorIfNotEqual("Expected 02000", op.result.error.code, "02000"); 
    t7.failOnError();    
  }
  tx.close();
};

t7.run = function() {
  var deleteKey = { id : 1 };
  prepare(t7, deleteKey);
};


exports.tests = [ t1, t2, t3, t4, t5, t6, t7];

exports.tests[exports.tests.length - 1].teardown = function() {
  if(dbSession) {
    dbSession.close();
  }
};
