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


/// RUN A SINGLE TEST, SUPPLIED AS A FILENAME ON THE COMMAND LINE 

var fs = require("fs");
var path = require("path");
var parent = path.dirname(__dirname);

global.test_harness_module = path.join(__dirname, "harness.js");
global.adapter = path.join(parent, "Adapter");

global.api_module = path.join(parent, "Adapter", "api", "mynode.js");
global.spi_module = path.join(parent, "Adapter", "spi", "SPI.js");

var Test = require(global.test_harness_module);

var result = new Test.Result();
result.listener = new Test.Listener();

var testfile = path.join(__dirname, process.argv[2]);

var testcase = require(testfile);

testcase.test(result);




