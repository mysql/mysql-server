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

var common = require("../build/Release/common/common_library.node");
var ndbapi = require("../build/Release/ndb/ndbapi.node");
var util = require("./util.js");

var properties;
var ndbconn;
var is_connected = false;
var ndb_is_initialized = false;

function initialize_ndb() {
  if(! ndb_is_initialized) {
    udebug.log("headed to ndb_init()");
    ndbapi.ndb_init();
    ndb_is_initialized = true;
  }
}


/* Constructor 
*/
exports.DBConnectionPool = function(props) {
  udebug.log("DBConnectionPool constructor");
  properties = props;
  
  initialize_ndb();

  ndbconn = new ndbapi.Ndb_cluster_connection(props.ndb_connectstring);
  ndbconn.set_name("nodejs");
};


/* Blocking connect.  
   SYNC.
   Returns true on success and false on error.
*/
exports.DBConnectionPool.prototype.connectSync = function() {
  var r = ndbconn.connectSync(properties.ndb_connect_retries,
                              properties.ndb_connect_delay,
                              properties.ndb_connect_verbose);
  if(r == 0) is_connected = true;

  return is_connected;
};


/* Async connect 
*/
exports.DBConnectionPool.prototype.connect = function(user_callback) {
  var theDbConn = this;
  
  var NdbConnection_callback = function(rval) { 
    var err = null;
    if(rval != 0) 
      err = new Error('NDB Connect failed' + rval);
    user_callback(err, theDbConn);
  }
  
  ndbconn.connectAsync(properties.ndb_connect_retries,
                       properties.ndb_connect_delay,
                       properties.ndb_connect_verbose,
                       NdbConnection_callback);
}


/* DBConnection.isConnected() method.
   IMMEDIATE.
   Returns bool true/false
 */
exports.DBConnectionPool.prototype.isConnected = function() {
  return is_connected;
};


exports.DBConnectionPool.prototype.closeSync = function() {
  ndbconn.delete();
}


/* openSessionHandler().
   ASYNC.
   Creates and opens a new DBSessionHandler.
   Users's callback receives (error, DBSessionHandler)
*/
exports.DBConnectionPool.prototype.openSessionHandler = function(user_callback) {
  var db = properties.database;
  assert(ndbconn);
  assert(db == "test");
  assert(user_callback)
  udebug.log("NDB openSessionHandler");

  var private_callback = function(sess) {
    udebug.log("NDB openSessionHandler private_callback");
    user_callback(null, sess);
  };

  var sessionImpl = ndbapi.NewDBSessionImpl(ndbconn, db, private_callback);
}
