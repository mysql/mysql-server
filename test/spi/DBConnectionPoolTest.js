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

var path = require("path"),
    fs   = require("fs");

try {
  require("./suite_config.js");
} catch(e) {} 

var spi_lib    = require("./lib.js"),
    doc_parser = require("../lib/doc_parser");
    
/***** 
  t1:  get a connection
  t2:  get a session
  t3:  verify that the connection implements all documented methods
*****/

var t1 = new harness.ConcurrentTest("connect");
var t2 = new harness.ConcurrentTest("openDBSession");
var t3 = new harness.SerialTest("PublicFunctions");


t1.run = function() {  
  spi_lib.getConnectionPool(function(err, connection) {
    t1.errorIfError(err);
    t1.failOnError();
  });
};


t2.run = function() {
  spi_lib.fail_openDBSession(t2, function(err, dbSession) {
    dbSession.close(function() {
      t2.pass();
    });
  });
};


t3.run = function() {
  spi_lib.fail_openDBSession(t3, function(err, dbSession) {
    var dbConnPool, docFile, functionList, tester;
    try {
      dbConnPool = dbSession.getConnectionPool();
      docFile = path.join(mynode.fs.spi_doc_dir, "DBConnectionPool");
      functionList = doc_parser.listFunctions(docFile);
      tester = new doc_parser.ClassTester(dbConnPool, "DBConnectionPool");
      tester.test(functionList, t3);
      dbSession.close(function() {
        t3.failOnError();
      });
    } catch(e) {
      t3.fail(e);
    }  
  });
};


/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [ t1, t2, t3 ];


