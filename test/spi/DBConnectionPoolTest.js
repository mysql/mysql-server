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

"use strict";
/*global spi_module, harness, assert */

try {
  require("./suite_config.js");
} catch(e) {} 

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


/***** 
  t2:  get a connection
  t3:  get a session
*****/

var t2 = new harness.ConcurrentSubTest("connect");

var t3 = new harness.ConcurrentTest("openDBSession");
t3.run = function() {
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      x_conn = null,
      x_session = null;
    
  this.teardown = function() {
    // FIXME
    // session.close() does not exist in the spi?
    // if(x_session !== null) x_session.close(); 

    // WARNING -- "Deleting Ndb_cluster_connection with Ndb-object not deleted"
    if(x_conn !== null) { x_conn.closeSync(); }
  };

  var tcb1 = function(err, connection) {
    if(err) {
      t2.fail(err);
      t3.fail();
    }
    else {
      t2.pass();
    }
    x_conn = connection; // for teardown  
    var tcb2 = function(err, dbsessionhandler) {
      if(err) {
        t3.fail(err);
      }
      else {
        t3.pass();
        x_session = dbsessionhandler;   // for teardown
      }
    }; 
    connection.getDBSession(0, tcb2);
  };
  provider.connect(properties, tcb1);
};


/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3];

