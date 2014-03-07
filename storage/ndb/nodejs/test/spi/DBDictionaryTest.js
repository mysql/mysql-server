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

/*global unified_debug, spi_module, harness */

var udebug = unified_debug.getLogger("DBDictionaryTest.js");
var spi_lib = require("./lib.js");

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);

var t1 = new harness.ConcurrentSubTest("listTables");
var t2 = new harness.ConcurrentTest("getTable");


t2.run = function() {  
  var conn = null,
      dbSession = null;

  function onTable(err, tab) {
    udebug.log("onTable");
    var passed;
    // TODO: Test specific properties of the table object
    
    if(tab && !err)  { passed = true; }
    else             { passed = false; }
    
    function onClose() {
      udebug.log("onTable onClose");
      if(passed)     { t2.pass(); }
      else           { t2.fail("t2 onClose error"); }
    }

    dbSession.close(onClose);
  }

  function onList(err, table_list) {
    udebug.log("onList");
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
    
    udebug.log("onList count =", count);
    t1.errorIfNotEqual("Bad table count", count, 2);
    t1.failOnError();

    dbSession.getConnectionPool().getTableMetadata("test", "tbl2", dbSession, onTable);
  }

  function onSession(err, sess) {
    udebug.log("onSession");
    dbSession = sess;
    dbSession.getConnectionPool().listTables("test", dbSession, onList);
  }
    
  function onConnect(err, connection) {
    udebug.log("onConnect");
    conn = connection;
    conn.getDBSession(spi_lib.allocateSessionSlot(), onSession);
  }
  
  spi_lib.getConnectionPool(onConnect);
};

exports.tests = [ t1, t2];