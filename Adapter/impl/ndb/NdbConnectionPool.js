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

var common = require("../build/Release/common/common_library.node"),
    adapter = require("../build/Release/ndb/ndb_adapter.node"),
    ndbsession = require("./NdbSession.js"),
    ndbtablehandler = require("./NdbTableHandler.js");
var ndb_is_initialized = false;

function initialize_ndb() {
  if(! ndb_is_initialized) {
    adapter.ndbapi.ndb_init();                       // ndb_init()
    adapter.ndbapi.util.CharsetMap_init();           // CharsetMap::init()
    ndb_is_initialized = true;
  }
}

/* Constructor 
*/
exports.DBConnectionPool = function(props) {
  "use strict";
  udebug.log("DBConnectionPool constructor");
  
  this.properties = props;
  this.ndbconn = null;
  this.is_connected = false;
  
  initialize_ndb();

  this.ndbconn = new adapter.ndbapi.Ndb_cluster_connection(props.ndb_connectstring);
  this.ndbconn.set_name("nodejs");
};


/* Blocking connect.  
   SYNC.
   Returns true on success and false on error.
   FIXME:  NEEDS wait_until_ready() and node_id()
*/
exports.DBConnectionPool.prototype.connectSync = function() {
  var r = this.ndbconn.connectSync(this.properties.ndb_connect_retries,
                                   this.properties.ndb_connect_delay,
                                   this.properties.ndb_connect_verbose);
  if(r == 0) is_connected = true;

  return is_connected;
};


/* Async connect 
*/
exports.DBConnectionPool.prototype.connect = function(user_callback) {
  "use strict";
  var self = this;
  var ndbconn = this.ndbconn;
  var err = null;
  var ready_cb;
  
    ndbconn.connectAsync(self.properties.ndb_connect_retries,
                         self.properties.ndb_connect_delay,
                         self.properties.ndb_connect_verbose,
                         function(err, rval) {
    udebug.log("connect() connectAsync internal callback/rval=" + rval);
    if(rval == 0) {
      assert(typeof ndbconn.wait_until_ready === 'function');
      ready_cb = function(err, nnodes) {
        udebug.log("connect() wait_until_ready internal callback/nnodes=" + nnodes);
        if(nnodes < 0) {
          // FIXME: what should be the type of err? a string? an error object?
          err = "Timeout waiting for cluster to become ready."
        }
        else {
          self.is_connected = true;
          if(nnodes > 0) {
            // FIXME: How to log a console warning?
            console.log("Warning: only " + nnodes + " data nodes are running.");
          }
          console.log("Connected to cluster as node id: " + ndbconn.node_id());
         }
         user_callback(err, self);
      }
      ndbconn.wait_until_ready(1, 1, ready_cb);
    }
    else {
      err = new Error('NDB Connect failed ' + rval);       
      user_callback(err, self);
    }
  });
}


/* DBConnection.isConnected() method.
   IMMEDIATE.
   Returns bool true/false
 */
exports.DBConnectionPool.prototype.isConnected = function() {
  return is_connected;
};


exports.DBConnectionPool.prototype.closeSync = function() {
  this.ndbconn.delete();
}


/* getDBSession().
   ASYNC.
   Creates and opens a new DBSession.
   Users's callback receives (error, DBSession)

   TODO: Figure out opening of Session and SessionImpl ... 
*/
exports.DBConnectionPool.prototype.getDBSession = function(index, user_callback) {
  var db = this.properties.database;
  assert(this.ndbconn);
  assert(user_callback)
  udebug.log("NDB getDBSession");

  var private_callback = function(err, sess) {
    udebug.log("NDB getDBSession private_callback");
    udebug.log("Impl: " + sess);

    user_session = new ndbsession.DBSession();
    user_session.impl = sess;
    user_callback(err, user_session);
};

  var sessionImpl = adapter.impl.DBSession.create(this.ndbconn, db, private_callback);
}


exports.DBConnectionPool.prototype.createDBTableHandler = function(dbtable) {
  
  

}
