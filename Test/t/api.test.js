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

var api = require(global.api_module);
var harness = require(global.test_harness_module);

var t1 = new harness.Test("ndbProperties");
t1.run = function() {
  var properties = new api.ConnectionProperties("ndb");
};

var t2 = new harness.Test("Annotations");
t2.run = function() {
  var annotations = new api.Annotations();
};


var suite = new harness.Test("api").makeTestSuite();
suite.addTest(t1);
suite.addTest(t2);

module.exports = suite;