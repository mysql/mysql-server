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


global.path            = require("path");
global.fs              = require("fs");
global.assert          = require("assert");
global.util            = require("util");

global.adapter_dir     = __dirname;
global.parent_dir      = path.dirname(adapter_dir);
global.api_dir         = path.join(adapter_dir, "api");
global.spi_dir         = path.join(adapter_dir, "impl");
global.spi_doc_dir     = path.join(spi_dir, "SPI-documentation");
global.api_doc_dir     = path.join(parent_dir, "API-documentation");
global.converters_dir  = path.join(parent_dir, "Converters");

global.spi_module      = path.join(spi_dir, "SPI.js");
global.api_module      = path.join(api_dir, "mynode.js");
global.udebug_module   = path.join(api_dir, "unified_debug.js");
global.unified_debug   = require(udebug_module);


/* Find the build directory */
var build1 = path.join(spi_dir, "build");
var build2 = path.join(parent_dir, "build");
var existsSync = fs.existsSync || path.existsSync;

if(existsSync(path.join(build1, "Release", "ndb_adapter.node"))) {
  global.build_dir = path.join(build1, "Release");
}
else if(existsSync(path.join(build2, "Release", "ndb_adapter.node"))) {
  global.build_dir = path.join(build2, "Release");
}
else if(existsSync(path.join(build1, "Debug", "ndb_adapter.node"))) {
  global.build_dir = path.join(build1, "Debug");
}
else if(existsSync(path.join(build2, "Debug", "ndb_adapter.node"))) {
  global.build_dir = path.join(build2, "Debug");
}








