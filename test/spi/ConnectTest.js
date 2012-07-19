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
  return true; // test is complete
};

/**** getDBServiceProvider and getDefaultConnectionProperties ***/
var getPropertiesTest = function() {
  var provider = spi.getDBServiceProvider(this.impl);
  provider.getDefaultConnectionProperties();
  return true; // test is complete
};

/**** Actually connect using the default properties.  
      Requires something to connect to. 
***/
var connectSyncTest = function() {
  var provider = spi.getDBServiceProvider(this.impl);
  var properties = provider.getDefaultConnectionProperties();
  var conn = provider.connectSync(properties);
  assert(conn.isConnected(), "failed to connect");
  return true; // test is complete
};


var connectAsyncTest = function() {
  var testCase = this;
  var provider = spi.getDBServiceProvider(this.impl);
  var properties = provider.getDefaultConnectionProperties();
  var connection = provider.connect(properties, function(err, connection) {
    if(err) testCase.fail(err);
    else {
      connection.closeSync();
      testCase.pass();
    }
  });
};


/*** spi.ndb.getSPI ***/
var t1 = new harness.Test("getSPI");
t1.impl= "ndb";
t1.run = getSPItest;

/*** spi.ndb.getProperties ***/
var t2 = new harness.Test("getProperties");
t2.impl= "ndb";
t2.run = getPropertiesTest;

/*** spi.ndb.connectSync ***/
var t3 = new harness.Test("connectSync");
t3.impl = "ndb";
t3.run = connectSyncTest;

/*** spi.mysql.getSPI ***/
var t4 =  new harness.Test("getSPI");
t4.impl= "mysql";
t4.run = getSPItest;

/*** spi.mysql.getProperties ***/
var t5 = new harness.Test("getProperties");
t5.impl = "mysql";
t5.run = getPropertiesTest;

/*** spi.mysql.connectSync ***/
var t6 = new harness.Test("connectSync");
t6.impl = "mysql";
t6.run = connectSyncTest;

/** spi.ndb.connect ***/
var t7 = new harness.Test("connect");
t7.impl = "ndb";
t7.run = connectAsyncTest;


/******************* TEST GROUPS ********/

var ndb_group = new harness.Test("ndb").makeTestGroup(t1, t2, t3, t7);

var mysql_group = new harness.Test("mysql").makeTestGroup(t4, t5, t6);

var group = new harness.Test("spi").makeTestGroup(ndb_group, mysql_group);


/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports = group;
