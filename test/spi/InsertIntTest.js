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
      session = null,
      test = this;

  function onTable(err, table) {
    udebug.log("InsertIntTest.js onTable");
    if(err) {   test.fail(err);    }
    else    {   do_insert_op(session, table);  }
  }

  function onSession(err, sess) {
    udebug.log("InsertIntTest.js onSession");
    session = sess;
    if(err) {   test.fail(err);   }
    else    {   session.getConnectionPool().getTableMetadata("test", "tbl1", null, onTable); }
  }

  function onConnect(err, conn) {
    udebug.log("InsertIntTest.js onConnect");
    connection = conn;
    connection.getDBSession(0, onSession);
  }
    
  provider.connect(properties, onConnect);
};


t2.run = function do_delete_op() {
  udebug.log("InsertIntTest.js do_delete_op");
  var tx = this.session.openTransaction();
  var thandler = this.session.getConnectionPool().createDBTableHandler(this.table, null);
  
  var row = { i: 13 };
  var op = this.session.delete(thandler, row);
  
  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    if(err) { t2.fail("Execute/commit failed: " + err); }
    else    { t2.pass(); }
  });
};

exports.tests = [ t1,t2 ];