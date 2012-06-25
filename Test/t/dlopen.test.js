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

var path = require("path");

var harness = require(global.test_harness_module);

var t = new harness.Test("dlopen");

t.order = 1;

t.run = function() {
  var module_path = path.join(global.adapter,"spi","ndb","tie",
                              "build","Release","ndbapi.node");
  var ndbapi = require(module_path);
};

module.exports = t;