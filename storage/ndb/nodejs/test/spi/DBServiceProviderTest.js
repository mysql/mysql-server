/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

// TODO:  DBServiceProviderTest needs to run before DBConnectionPoolTest.

"use strict";

var path = require("path"),
    fs   = require("fs"),
    spi  = require(mynode.spi),
    service = spi.getDBServiceProvider(global.adapter),
    doc_parser  = require("../lib/doc_parser");

var t1 = new harness.ConcurrentTest("getDefaultConnectionProperties");
t1.run = function() {
  var properties = service.getDefaultConnectionProperties();
  return true; // test is complete
};


var t2 = new harness.ConcurrentTest("getFactoryKey");
t2.run = function() {
  var properties = service.getDefaultConnectionProperties();
  var key = service.getFactoryKey(properties);
  return true; // test is complete
};


/* TEST THAT ALL FUNCTIONS PUBLISHED IN THE DOCUMENTATION 
   ACTUALLY EXIST IN THE IMPLEMENTATION
*/
var t4 = new harness.ConcurrentTest("PublicFunctions");
t4.run = function() {
  var docFile = path.join(mynode.fs.spi_doc_dir, "DBServiceProvider");
  var functionList = doc_parser.listFunctions(docFile);
  var tester = new doc_parser.ClassTester(service, "DBServiceProvider");
  tester.test(functionList);
  return true; // test is complete
};

module.exports.tests = [t1, t2, t4];
