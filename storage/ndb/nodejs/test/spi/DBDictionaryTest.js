/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

try {
  require("./suite_config.js");
} catch(e) {} 

var t1 = new harness.ConcurrentTest("listTables");
var t2 = new harness.ConcurrentTest("getTable");

t1.run = function() {  
  var mySession;

  function onList(err, table_list) {
    var count = 0;
    t1.errorIfError(err);
 
    function countTables(tableName) {
      if (tableName === 'tbl1') {  count++;  }
      if (tableName === 'tbl2') {  count++;  }
    }
    table_list.forEach(countTables);

    t1.errorIfNotEqual("Bad table count", count, 2);
    mySession.close(function() {
      t1.failOnError();
    });
  }
  
  spi_lib.fail_openDBSession(t1, function(error, dbSession) {
    mySession = dbSession;
    dbSession.getConnectionPool().listTables("test", dbSession, onList);
  });
};

t2.run = function() {
  var mySession;

  // TODO: Test specific properties of the table object
  function onTable(err, tab) {    
    t2.errorIfError(err);
    mySession.close(function() {
      t2.failOnError();
    });
  }
    
  spi_lib.fail_openDBSession(t2, function (err, dbSession) {
    mySession = dbSession;
    dbSession.getConnectionPool().getTableMetadata("test", "tbl2", dbSession, onTable);
  });
};


module.exports.tests = [ t1 , t2 ];
