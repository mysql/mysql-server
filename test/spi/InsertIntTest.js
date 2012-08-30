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

var t = new harness.ConcurrentTest("InsertInt");

function do_insert_op(session, table) {
  udebug.log("InsertIntTest.js do_insert_op");
  var tx = session.openTransaction();
  var thandler = session.getConnectionPool().createDBTableHandler(table, null);
  
  var row = { i: 13 , j: 14 };
  var op = session.insert(thandler, row);

  udebug.log("ready to commit");
  tx.execute("Commit", function(err, tx) {
    if(err) { t.fail("Execute/commit failed: " + err);  }
    else    { t.pass(); }
  });
}

t.run = function() {  
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      connection = null,
      session = null;

  function onTable(err, table) {
    udebug.log("InsertIntTest.js onTable");
    if(err) {   t.fail(err);    }
    else    {   do_insert_op(session, table);  }
  }

  function onSession(err, sess) {
    udebug.log("InsertIntTest.js onSession");
    session = sess;
    if(err) {   t.fail(err);   }
    else    {   session.getConnectionPool().getTable("test", "tbl1", onTable); }
  }

  function onConnect(err, conn) {
    udebug.log("InsertIntTest.js onConnect");
    connection = conn;
    connection.getDBSession(0, onSession);
  }
    
  provider.connect(properties, onConnect);
};

exports.tests = [ t ];