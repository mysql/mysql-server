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

var assert = require("assert");
var spi = require(global.spi_module);
var harness = require(global.test_harness_module);


/********** spi.getDBServiceProvide() ****/
var t1 = new harness.Test("getSPI");
t1.run = function() {
  spi.getDBServiceProvider("ndb");
}


/********** ndb.getDefaultConnectionProperties() ***/
var t2 = new harness.Test("ndbProps");
t2.run = function() {
  var ndb = spi.getDBServiceProvider("ndb");
  ndb.getDefaultConnectionProperties();
}


/********** ndb.connectSync() 
 * Requires a running cluster 
 */
var t3 = new harness.Test("ndbConnect");
t3.run = function() { 
  var ndb = spi.getDBServiceProvider("ndb");
  var p = ndb.getDefaultConnectionProperties();
  p.ndb_connect_retries = 1;

  var conn = ndb.connectSync(p);
  assert(conn.isConnected(), "failed to connect");
}


var suite = new harness.Test("spi").makeTestSuite();
suite.addTest(t1);
suite.addTest(t2);
suite.addTest(t3);


module.exports = suite;