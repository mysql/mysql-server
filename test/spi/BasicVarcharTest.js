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

/*global udebug, path, fs, assert, spi_module, harness, adapter_dir, spi_dir */

"use strict";

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);
var dbtablehandler = require(path.join(spi_dir, "common", "DBTableHandler.js"));

var annotations = new mynode.Annotations;
var mapping = annotations.newTableMapping("test.tbl3");
mapping.mapField("id", "i");
mapping.mapField("name", "c");

var dbSession = null;
var table = null;
var dbt = null;


var t1 = new harness.SerialTest("Insert"),
    t2 = new harness.SerialTest("Read_1"),
    t3 = new harness.SerialTest("Update"),
    t4 = new harness.SerialTest("Read_2"),
    t5 = new harness.SerialTest("Insert_Duplicate"), 
    t6 = new harness.SerialTest("Delete"),
    t7 = new harness.SerialTest("Delete_NotFound");

/// Common prep
t1.prepare = function prepare(testObj) {
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      connection = null,
      test = this;

  if(dbSession && table) {  // already set up
    this.runTestMethod(testObj);
    return;
  }

  function onTable(err, dbTable) {
    udebug.log("BasicVarcharTest.js prepare onTable");
    table = dbTable;         // set global
    dbt = new dbtablehandler.DBTableHandler(table, mapping);   // set global
    if(err) {  test.fail(err);               }
    else    {  test.runTestMethod(testObj);  }
  }

  function onSession(err, sess) {
    udebug.log("BasicVarcharTest.js prepare onSession");
    dbSession = sess; // set global
    if(err) {   test.fail(err);   }
    else    {   dbSession.getConnectionPool().getTableMetadata("test", "tbl3", null, onTable); }
  }

  function onConnect(err, conn) {
    udebug.log("BasicVarcharTest.js prepare onConnect");
    connection = conn;
    connection.getDBSession(0, onSession);
  }
  
  provider.connect(properties, onConnect);
};


/// INSERT
t1.runTestMethod = function do_insert_op(dataObj) {
  udebug.log("BasicVarcharTest.js do_insert_op");
  var test = this;
  var tx = dbSession.createTransaction();
  var op = dbSession.buildInsertOperation(dbt, dataObj, tx, null);

  tx.executeCommit([ op ], function(err, tx) {
    tx.close();
    if(err) { test.fail("ExecuteCommit failed: " + err);  }
    else    { test.pass(); }
  });  
};

t1.run = function() {
  var insertObj = { id : 1 , name : "Henry" };
  this.prepare(insertObj);
};


// READ
t2.runTestMethod = function do_read_op(keyObj) {
  udebug.log("BasicVarcharTest.js do_read_op");
  var test = this;
  var tx = dbSession.createTransaction();
  var op = dbSession.buildReadOperation(dbt, keyObj, tx);
  
  tx.executeCommit([ op ], function(err, tx) {
    var op, value;
    if(err) { test.fail("ExecuteCommit failed: " + err);  }
    else { 
      op = tx.executedOperations.pop();
      value = op.result.value;
      if(value && value.id === 1 && value.name === 'Henry') {
        test.pass();
      }
      else {
        test.fail("Bad value " + value);
      }
    }
    tx.close();
  });
};

t2.run = function() {
  var readObj = { id: 1 };
  this.prepare = t1.prepare;
  this.prepare(readObj);
};


// DELETE 
t6.runTestMethod = function do_delete_op(keyObj) {
  udebug.log("InsertIntTest.js do_delete_op");
  var test = this;
  var tx = dbSession.createTransaction();
  var op = dbSession.buildDeleteOperation(dbt, keyObj, tx, null);
  
  tx.executeCommit([ op ], function(err, tx) {
    if(err) { test.fail("ExecuteCommit failed: " + err); }
    else    { test.pass(); }
    tx.close();
  });
};

t6.run = function() {
  var deleteKey = { id : 1 };
  this.prepare = t1.prepare;
  this.prepare(deleteKey);
};
  


exports.tests = [ t1 , t2, t6 ];

exports.tests[exports.tests.length - 1].teardown = function() {
  if(dbSession) {
    dbSession.close();
  }
};
