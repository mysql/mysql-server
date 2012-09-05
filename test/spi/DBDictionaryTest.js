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

/*global spi_module, harness, udebug */

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);

var t1 = new harness.ConcurrentSubTest("listTables");
var t2 = new harness.ConcurrentTest("getTable");


t2.run = function() {  
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      conn = null,
      session = null;

  function onTable(err, tab) {
    udebug.log("DBDictionaryTest onTable");
    // TODO: Test specific properties of the table object
    if(tab && ! err) { t2.pass(); }
    else             { t2.fail("getTable error"); }
  }

  function onList(err, table_list) {
    udebug.log("DBDictionaryTest onList");
    if (err) {
      t1.fail(err);
      t2.fail(err);
      return;
    }
    var count = 0;

    function countTables(tableName) {
      if (tableName === 'tbl1') {  count++;  }
      if (tableName === 'tbl2') {  count++;  }
    }
    table_list.forEach(countTables);
    
    udebug.log("DBDictionaryTest onList count = " + count);

    t1.errorIfNotEqual("Bad table count", count, 2);
    t1.failOnError();

    session.getConnectionPool().getTableMetadata("test", "tbl2", null, onTable);
  }

  function onSession(err, sess) {
    udebug.log("DBDictionaryTest onSession");
    session = sess;   // for teardown
    session.getConnectionPool().listTables("test", null, onList);
  }
    
  function onConnect(err, connection) {
    udebug.log("DBDictionaryTest onConnect");
    conn = connection; // for teardown
    conn.getDBSession(0, onSession);
  }
  
  provider.connect(properties, onConnect);
};

exports.tests = [ t1, t2];