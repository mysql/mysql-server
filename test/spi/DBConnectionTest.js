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

var spi = require(spi_module);




/**** Actually connect using the default properties.  
      Requires something to connect to. 
***/
var connectSyncTest = function() {
  var provider = spi.getDBServiceProvider(this.impl);
  var properties = provider.getDefaultConnectionProperties();
  var conn = provider.connectSync(properties);
  assert(conn.isConnected(), "failed to connect");
  return true; // test is complete
  conn.closeSync();
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


/*** spi.ndb.connectSync ***/
var t3 = new harness.SerialTest("connectSync");
t3.impl = "ndb";
t3.run = connectSyncTest;


/** spi.ndb.connect ***/
var t7 = new harness.SerialTest("connect");
t7.impl = "ndb";
t7.run = connectAsyncTest;


/******************* TEST GROUPS ********/

var ndb_group = new harness.SerialTest("ndb").makeTestGroup(t3, t7);

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports = ndb_group;
