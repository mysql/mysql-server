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

var assert = require("assert");
var util = require("util");
var spi_lib = require("./lib.js");
var dbtablehandler = require(mynode.common.DBTableHandler);
var udebug = unified_debug.getLogger("UniqueKeyTest.js");
var TableMapping = require(mynode.api.TableMapping).TableMapping;

var dbSession = null;
var table = null;
var dbt = null;
var mapping = new TableMapping("test.tbl4");
mapping.mapField("id", "i");
mapping.mapField("uk", "k");
mapping.mapField("name", "c");

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
    else { // set global
      dbt = new dbtablehandler.DBTableHandler(table, mapping, null); 
      runTestMethod(testCase, testObj);
    }
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

function ReadTest(key, value, result_col, result_val) {
  var test = this;
  this.name = "Read_" + key + "_" + value + "_" + result_val;
  this.run = function() {
    var readObj = {};
    readObj[key] = value;
    prepare(test, do_read_op, readObj);
  };

  this.checkResult = function(err, tx) {
    var op;
    if(err) {
      test.appendErrorMessage(err.message);
    }
    else { 
      op = tx.executedOperations.pop();
      test.errorIfNull("Null op", op);
      test.errorIfNull("Null op.result", op.result);
      test.errorIfNull("Null op.result.value", op.result.value);
      test.errorIfNotEqual("Read Test", result_val, op.result.value[result_col]);
    }
    test.failOnError();
  };
}
ReadTest.prototype = new harness.SerialTest("Read");


function checkSuccess(testCase) {
  return function(err, tx) {
    var op;
    if(err) { 
      testCase.appendErrorMessage('UniqueKeyTest.checkSuccess', util.inspect(err));
    }
    else {
      op = tx.executedOperations.pop();
      testCase.errorIfNotEqual("operation failed", true, op.result.success);
    }
    testCase.failOnError();
  };
}


var 
    t0 = new harness.SerialTest("Prelim_Delete"),
    t1 = new harness.SerialTest("Insert"),
    t2 = new ReadTest("uk",11,"name","Henry"),
    t3 = new harness.SerialTest("UK_Update"),
    t4 = new ReadTest("uk",11,"name","Henrietta"),
    t5 = new harness.SerialTest("UK_Write"),
    t6 = new ReadTest("uk",11,"name","Henry V"),
    t7 = new ReadTest("uk",11,"id",1),
    t8 = new harness.SerialTest("UK_Delete"),
    t9 = new harness.SerialTest("UK_Delete_NotFound"),
    close = new harness.SerialTest("CloseSession");


// PRELIMINARY DELETE.  THIS TEST WILL ALWAYS PASS.
t0.run = function() {
  prepare(t0, do_delete_op, {id : 1});
};
t0.checkResult = function(err, tx) {
  t0.pass();
};


/// INSERT
t1.run = function() {
  var insertObj = { id : 1 , uk: 11, name : "Henry" };
  prepare(t1, do_insert_op, insertObj);
};

t1.checkResult = checkSuccess(t1);


// ACCESS BY UNIQUE KEY AND UPDATE 
t3.run = function() {
  var dataObj = { keys: { uk : 11 }, values: {name: "Henrietta" } };
  prepare(t3, do_update_op, dataObj);
};

t3.checkResult = checkSuccess(t3);


// ACCESS BY UNIQUE KEY AND WRITE
t5.run = function() {
  prepare(t5, do_write_op, { uk: 11, name: "Henry V" });
};

t5.checkResult = checkSuccess(t5);

// DELETE BY UK
t8.run = function() {
  var deleteKey = { uk : 11 };
  prepare(t8, do_delete_op, deleteKey);
};

t8.checkResult = checkSuccess(t8);

// DELETE AGAIN (EXPECT ERROR)
t9.run = function() {
  var deleteKey = { uk : 11 };
  prepare(t9, do_delete_op, deleteKey);
};

t9.checkResult = function(err, tx) {
  var op = tx.executedOperations.pop();
  t9.errorIfNotEqual("t9 operation result error sqlstate", "02000", op.result.error.sqlstate);
  t9.failOnError();
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


exports.tests = [ t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, close ];
