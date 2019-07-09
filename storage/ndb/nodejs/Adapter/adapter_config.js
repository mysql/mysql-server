/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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


var path               = require("path");
var fs                 = require("fs");
var mynode, parent_dir, udebug_module;

mynode                     = {};
mynode.fs                  = {};
parent_dir                 = path.dirname(__dirname);

mynode.fs.adapter_dir      = __dirname;
mynode.fs.api_dir          = path.join(mynode.fs.adapter_dir, "api");
mynode.fs.spi_dir          = path.join(mynode.fs.adapter_dir, "impl");

mynode.fs.spi_doc_dir      = path.join(mynode.fs.spi_dir, "SPI-documentation");
mynode.fs.api_doc_dir      = path.join(parent_dir, "API-documentation");
mynode.fs.backend_doc_dir  = path.join(parent_dir, "Backend-documentation");
mynode.fs.converters_dir   = path.join(parent_dir, "Converters");

mynode.fs.spi_module       = path.join(mynode.fs.spi_dir, "SPI.js");
mynode.fs.api_module       = path.join(mynode.fs.api_dir, "mynode.js");

mynode.fs.suites_dir       = path.join(parent_dir, "test");
mynode.fs.samples_dir      = path.join(parent_dir, "samples");

udebug_module              = path.join(mynode.fs.api_dir, "unified_debug.js");


/* Find the build directory */
var build1 = path.join(mynode.fs.spi_dir, "build");
var build2 = path.join(parent_dir, "build");
var existsSync = fs.existsSync || path.existsSync;

if(existsSync(path.join(build1, "Release", "ndb_adapter.node"))) {
  mynode.fs.build_dir = path.join(build1, "Release");
}
else if(existsSync(path.join(build2, "Release", "ndb_adapter.node"))) {
  mynode.fs.build_dir = path.join(build2, "Release");
}
else if(existsSync(path.join(build1, "Debug", "ndb_adapter.node"))) {
  mynode.fs.build_dir = path.join(build1, "Debug");
}
else if(existsSync(path.join(build2, "Debug", "ndb_adapter.node"))) {
  mynode.fs.build_dir = path.join(build2, "Debug");
}

/* Some compatibility with older versions of node */
if(typeof global.setImmediate !== 'function') {
  global.setImmediate = process.nextTick;
}

/* Export the filesystem config */
module.exports = mynode.fs;

/* Also make it available globally */
if(!global.mynode) { global.mynode = {} };
global.mynode.fs = mynode.fs;

/* And export unified_debug globally */
global.unified_debug   = require(udebug_module);


