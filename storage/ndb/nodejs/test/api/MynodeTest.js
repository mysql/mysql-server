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

var path = require("path");
var doc_parser  = require(path.join(mynode.fs.suites_dir, "lib", "doc_parser"));

var t1 = new harness.ConcurrentTest("PublicFunctions");
t1.run = function() {
  var docFile = path.join(mynode.fs.api_doc_dir, "Mynode");
  var functionList = doc_parser.listFunctions(docFile);
  var tester = new doc_parser.ClassTester(mynode, "Mynode");
  tester.test(functionList, t1);
  return true;
};

module.exports.tests = [t1];
