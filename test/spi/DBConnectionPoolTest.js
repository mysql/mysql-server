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

require("./suite_config.js");

var spi = require(spi_module);


/**** Actually connect using the default properties.  
      Requires something to connect to. 
***/
var t1 = new harness.SerialTest("connectSync");
t1.run = function() {
  var provider = spi.getDBServiceProvider(global.adapter);
  var properties = provider.getDefaultConnectionProperties();
  var conn = provider.connectSync(properties);
  assert(conn.isConnected(), "failed to connect");
  conn.closeSync();
  return true; // test is complete
};

var t2 = new harness.SerialTest("connect");
t2.run = function() {
  var testCase = this;
  var provider = spi.getDBServiceProvider(global.adapter);
  var properties = provider.getDefaultConnectionProperties();
  var connection = provider.connect(properties, function(err, connection) {
    if(err) testCase.fail(err);
    else {
      connection.closeSync();
      testCase.pass();
    }
  });
};


/**** Connect, leave the connection open, and get a DBSession
*/

var t3 = new harness.SerialTest("openDBSession");
t3.run = function() {
  var testCase = this;
  var provider = spi.getDBServiceProvider(global.adapter);
  var properties = provider.getDefaultConnectionProperties();

  var tcb1 = function(err, connection) {
    udebug.log("DBConnectionPoolTest.js tcb1() 64");
    if(err) testCase.fail(err);
    
    var tcb2 = function(err, dbsessionhandler) {
      udebug.log("DBConnectionPoolTest.js tcb2() 68");
      if(err) testcase.fail(err);
      else testCase.pass();
    }
    connection.openSessionHandler(tcb2);
 
  }
  provider.connect(properties, tcb1);
}


/*************** EXPORT THE TOP-LEVEL GROUP ********/
// module.exports.tests = [t1, t2, t3];

module.exports.tests = [t3];
