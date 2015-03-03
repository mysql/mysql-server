/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

var spi_lib = require("./lib.js");
var dbtablehandler = require(mynode.common.DBTableHandler);
var TableMapping   = require(mynode.api.TableMapping).TableMapping;
var udebug         = unified_debug.getLogger("NoCommitTest.js");

var dbSession = null;
var mapping = new TableMapping("test.tbl4");

/// Common prep

function prepare(testCase, callback) {

  function onTable(err, dbTable) {
    if(err) {  
      testCase.fail(err);
    }
    else {
      callback(new dbtablehandler.DBTableHandler(dbTable, mapping, null));
    }
  }

  function onSession(err, sess) {
    dbSession = sess; // set global
    if(err) {   testCase.fail(err);   }
    else    {   
      dbSession.getConnectionPool().getTableMetadata("test", "tbl4", dbSession, onTable); 
    }
  }

  spi_lib.fail_openDBSession(testCase, onSession);
}

var t1 = new harness.SerialTest("DeleteNoCommitThenRead"),
    close = new harness.SerialTest("CloseConnection");

t1.run = function() {
  var obj, key, index, tx, op1, op2, op3;

  prepare(t1, function(dbt) {
    key = 91;
    obj = { i: key , k: key , c : "Henry" };
    tx = dbSession.getTransactionHandler();

    /* Insert a row in tx1 */
    op1 = dbSession.buildInsertOperation(dbt, obj, tx);
    tx.execute([ op1 ], function(err) {

      function onReadThenCommit(err, op) {
        t1.errorIfTrue("read op", op.result.success);
        tx.commit(function() { t1.failOnError(); });      
      }

      function onDeleteThenRead(err) {
        t1.errorIfError(err);
        op3 = dbSession.buildReadOperation(index, key, tx, onReadThenCommit);
        tx.execute( [ op3 ] );
      }
      
      /* Start tx2 and delete the row (NoCommit) */
      udebug.log("INSERT ERR:", err);
      t1.errorIfError(err);
      dbSession.begin();
      tx = dbSession.getTransactionHandler();
      index = dbt.getIndexHandler(key);
      op2 = dbSession.buildDeleteOperation(index, key, tx, onDeleteThenRead);
      tx.execute( [ op2 ] );
    });
  });
};

close.run = function() {
  dbSession.close(function(err) {
    if (err) {
      close.fail("Close got error: " + err);
    } else {
      close.pass();
    }
  });
};


exports.tests = [ t1 , close ];
