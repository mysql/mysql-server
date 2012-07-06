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

/** This is the smoke test for the t_basic suite.
 */
var path = require("path");

var harness = require(global.test_harness_module);

var test = new harness.Test("ClearSmokeTest");

test.phase = 3;

test.run = function() {
  var module_path = path.join(global.adapter,"spi",
                              "build","Release","ndbapi.node");
  var ndbapi = require(module_path);
  var t = this;
  dropSQL(this.suite, function(error) {
    if (error) {
      t.fail('dropSQL failed: ' + error);
    } else {
      t.pass();
    }
  });
  return true;
};

module.exports = test;
