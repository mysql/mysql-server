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
/*global spi_module, path, suites_dir, spi_doc_dir, udebug, harness, assert */

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);
var doc_parser  = require(path.join(suites_dir, "lib", "doc_parser"));


/**** Actually connect using the default properties.  
      Requires something to connect to. 
***/
var t1 = new harness.SerialTest("connectSync");
t1.run = function() {
  var provider = spi.getDBServiceProvider(global.adapter);
  var properties = provider.getDefaultConnectionProperties();
  var conn = provider.connectSync(properties);
  assert(conn.isConnected(), "failed to connect");
  conn.closeSync();
  return true; // test is complete
};


var t4 = new harness.ConcurrentSubTest("PublicFunctions");

function runSPIDocTest(dbConnection) {
  var docFile = path.join(spi_doc_dir, "DBConnectionPool");
  var functionList = doc_parser.listFunctions(docFile);
  var tester = new doc_parser.ClassTester(dbConnection, "DBConnectionPool");
  tester.test(functionList, t4);
}


/***** 
  t2:  get a connection
  t3:  get a session
*****/

var t2 = new harness.ConcurrentSubTest("connect");

var t3 = new harness.ConcurrentTest("openDBSession");

t3.run = function() {
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      x_conn = null,
      x_session = null;
    
  this.teardown = function() {
    if(x_session !== null) { x_session.close(); }
  };

  var tcb1 = function(err, connection) {
    if(err) {
      t2.fail(err);
      t3.fail();
    }
    else {
      t2.pass();
    }
    runSPIDocTest(connection);
    
    x_conn = connection; // for teardown  
    var tcb2 = function(err, dbsessionhandler) {
      if(err) {
        t3.fail(err);
      }
      else {
        t3.pass();
        x_session = dbsessionhandler;   // for teardown
      }
    }; 
    connection.getDBSession(0, tcb2);
  };
  provider.connect(properties, tcb1);
};




/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4];

