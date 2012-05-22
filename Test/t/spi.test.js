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

/***** getDBServiceProvider ***/
var getSPItest = function() { 
  var prov = spi.getDBServiceProvider(this.impl);
  assert(typeof prov.connectSync == 'function', "has no connectSync() method");
}

/**** getDBServiceProvider and getDefaultConnectionProperties ***/
var getPropertiesTest = function() {
  var provider = spi.getDBServiceProvider(this.impl);
  provider.getDefaultConnectionProperties();
}

/**** Actually connect using the default properties.  
      Requires something to connect to. 
***/
var connectSyncTest = function() {
  var provider = spi.getDBServiceProvider(this.impl);
  var properties = provider.getDefaultConnectionProperties();
  var conn = provider.connectSync(properties);
  assert(conn.isConnected(), "failed to connect");
}  


/******************* TEST SUITES ********/
var suite = new harness.Test("spi").makeTestSuite();

var ndb_suite = new harness.Test("ndb").makeTestSuite();
var mysql_suite = new harness.Test("mysql").makeTestSuite();

suite.addTest(ndb_suite);
suite.addTest(mysql_suite);

/*** spi.ndb.getSPI ***/
var t1 = new harness.Test("getSPI");
t1.impl= "ndb";
t1.run = getSPItest;
ndb_suite.addTest(t1);

/*** spi.ndb.getProperties ***/
var t2 = new harness.Test("getProperties");
t2.impl= "ndb";
t2.run = getPropertiesTest;
ndb_suite.addTest(t2);

/*** spi.ndb.connectSync ***/
var t3 = new harness.Test("connectSync");
t3.impl = "ndb";
t3.run = connectSyncTest;
ndb_suite.addTest(t3);

/*** spi.mysql.getSPI ***/
var t4 =  new harness.Test("getSPI");
t4.impl= "mysql";
t4.run = getSPItest;
mysql_suite.addTest(t4);

/*** spi.mysql.getProperties ***/
var t5 = new harness.Test("getProperties");
t5.impl = "mysql";
t5.run = getPropertiesTest;
mysql_suite.addTest(t5);

/*** spi.mysql.connectSync ***/
var t6 = new harness.Test("connectSync");
t6.impl = "mysql";
t6.run = connectSyncTest;
mysql_suite.addTest(t6);



/*************** EXPORT THE TOP-LEVEL SUITE ********/
module.exports = suite;
