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
/*global spi_module, path, suites_dir, spi_doc_dir, harness, assert */

try {
  require("./suite_config.js");
} catch(e) {} 

var spi_lib = require("./lib.js"),
    doc_parser  = require(path.join(suites_dir, "lib", "doc_parser"));
    


/***** 
  t2:  get a connection
  t3:  get a session
  t4:  verify that the connection implements all documented methods
*****/

var t2 = new harness.ConcurrentSubTest("connect");
var t4 = new harness.ConcurrentSubTest("PublicFunctions");
var t3 = new harness.ConcurrentTest("openDBSession");

function runSPIDocTest(dbConnection, testCase) {
  var docFile = path.join(spi_doc_dir, "DBConnectionPool");
  var functionList = doc_parser.listFunctions(docFile);
  var tester = new doc_parser.ClassTester(dbConnection, "DBConnectionPool");
  tester.test(functionList, testCase);
}


t3.run = function() {
  // work around bug where ConcurrentSubTest doesn't have its own result obj
  t2.result = this.result; 
  t4.result = this.result;
    
  function onSession(err, dbSession) {
    t3.errorIfError(err);
    dbSession.close();
    t3.failOnError();
  }

  function onConnect(err, connection) {
    if(err) {
      t2.fail(err);
      t3.fail();
    }
    else {
      t2.pass();
      runSPIDocTest(connection, t4);
    }
    
    connection.getDBSession(spi_lib.allocateSessionSlot(), onSession);
  }

  spi_lib.getConnectionPool(onConnect);
};




/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [ t2, t3, t4 ];

