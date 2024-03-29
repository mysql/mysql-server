/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

"use strict";

var path = require("path");
var fs = require("fs");

var ndb_dir        = __dirname;   /* /impl/ndb */
var impl_dir       = path.dirname(ndb_dir);  /* /impl */
var root_dir       = path.dirname(impl_dir); /* / */
var converters_dir = path.join(root_dir, "Converters");
var suites_dir     = path.join(root_dir, "test");

/* Find the build directory */
var existsSync = fs.existsSync || path.existsSync;
var binary_dir;
var build1 = path.join(root_dir, "build");   // gyp builds under root dir
var build2 = path.join(impl_dir, "build");   // waf builds under impl dir
var build = existsSync(build1) ? build1 : build2;

binary_dir = path.join(build, "Release");

// The Static build is linked with ndbclient_static and created by CMake
if(existsSync(path.join(build, "Static", "ndb_adapter.node"))) {
  binary_dir = path.join(build, "Static");
}

// Prefer the Debug build if one exists
if(existsSync(path.join(build, "Debug", "ndb_adapter.node"))) {
  binary_dir = path.join(build, "Debug");
}

/* binary_dir may not exist, but that is an error that we leave for
   for loadRequiredModules() to catch.
*/

module.exports = {
  "binary"         : path.join(binary_dir, "ndb_adapter.node"),
  "root_dir"       : root_dir,
  "impl_dir"       : impl_dir,
  "impl_js_dir"    : ndb_dir,
  "converters_dir" : converters_dir,
  "suites_dir"     : suites_dir
};

