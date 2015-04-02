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

var domainClass = function(id, name, age, magic) {
  this.id = id;
  this.name = name;
  this.age = age;
  this.magic = magic;
};

var t1 = new harness.ConcurrentTest("NewTableMappingFromLiteral");
t1.run = function() {
  var tablemapping = new mynode.TableMapping(
    {
    "table" : "t_basic",
    "database" : "test",
    "mapAllColumns" : false,
    "fields" : {
      "fieldName" : "id",
      "columnName" : "id",
      "persistent" : true
      }
    });
  tablemapping.applyToClass(domainClass);
  
  return true; // test is complete
};

var t2 = new harness.ConcurrentTest("PublicFunctions");
t2.run = function() {
  var tableMapping = new mynode.TableMapping("t_basic");
  var tableMappingDocumentedFunctions = doc_parser.listFunctions(
    path.join(mynode.fs.api_doc_dir, "TableMapping"));
  var tester = new doc_parser.ClassTester(tableMapping, "TableMapping");
  tester.test(tableMappingDocumentedFunctions, t2);
  return true; // test is complete
};

module.exports.tests = [t1,t2];
