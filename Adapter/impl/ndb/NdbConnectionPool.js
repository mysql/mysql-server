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

/* jslint --node --white --vars --plusplus */
/*global udebug, exports */


var common = require("../build/Release/common/common_library.node"),
    adapter = require("../build/Release/ndb/ndb_adapter.node"),
    ndbsession = require("./NdbSession.js"),
    dbtablehandler = require("../common/DBTableHandler.js"),
    converters = require("./NdbTypeConverters.js"),
    assert = require("assert");
    
var ndb_is_initialized = false,
    proto;

function initialize_ndb() {
  if(! ndb_is_initialized) {
    adapter.ndbapi.ndb_init();                       // ndb_init()
    adapter.ndbapi.util.CharsetMap_init();           // CharsetMap::init()
    ndb_is_initialized = true;
  }
}


/* Load-Time Function Asserts */

assert(typeof adapter.ndbapi.Ndb_cluster_connection === 'function');
assert(typeof adapter.impl.DBSession.create === 'function');
assert(typeof adapter.impl.DBDictionary.listTables === 'function');
assert(typeof adapter.impl.DBDictionary.getTable === 'function');


/* DBConnectionPool constructor.
   IMMEDIATE.
   Does not perform any IO. 
   Throws an exception if the Properties object is invalid.
*/   
exports.DBConnectionPool = function(props) {
  udebug.log("NdbConnectionPool constructor");

  initialize_ndb();
  
  this.properties = props;
  this.ndbconn = new adapter.ndbapi.Ndb_cluster_connection(props.ndb_connectstring);
  this.ndbconn.set_name("nodejs");
};

/* NdbConnectionPool prototype 
*/
proto = {
  properties           : null,
  ndbconn              : null,
  is_connected         : false,
  dict_sess            : null,
  dictionary           : null,
  
  
};

exports.DBConnectionPool.prototype = proto;


/* Blocking connect.  
   SYNC.
   Returns true on success and false on error.
   FIXME:  NEEDS wait_until_ready() and node_id()
*/
proto.connectSync = function() {
  var r;
  throw new Error("connectSync() is not implemented"); 
  r = this.ndbconn.connectSync(this.properties.ndb_connect_retries,
                               this.properties.ndb_connect_delay,
                               this.properties.ndb_connect_verbose);
  if(r === 0) {
    this.is_connected = true;
  }
 
  return this.is_connected;
};


/* Async connect 
*/
proto.connect = function(user_callback) {
  udebug.log("NdbConnectionPool connect");
  var self = this,
      err = null;

  function onGotDictionarySession(cb_err, dsess) {
    // Got the dictionary.  Next step is the user's callback.
    if(cb_err) {
      user_callback(cb_err, null);
    }
    else {
      self.dict_sess = dsess;
      self.dictionary = self.dict_sess.impl;
      user_callback(null, self);
    }
  }

  function onReady(cb_err, nnodes) {
    // Cluster is ready.  Next step is to get the dictionary session
    udebug.log("connect() wait_until_ready internal callback/nnodes=" + nnodes);
    if(nnodes < 0) {
      err = new Error("Timeout waiting for cluster to become ready.");
      user_callback(err, self);
    }
    else {
      self.is_connected = true;
      if(nnodes > 0) {             // FIXME: How to log a console warning?
        console.log("Warning: only " + nnodes + " data nodes are running.");
      }
      console.log("Connected to cluster as node id: " + self.ndbconn.node_id());
     }
     self.getDBSession(0, onGotDictionarySession);
  }
  
  function onConnected(cb_err, rval) {
    // Connected to NDB.  Next step is wait_until_ready().
    udebug.log("connect() connectAsync internal callback/rval=" + rval);
    if(rval === 0) {
      assert(typeof self.ndbconn.wait_until_ready === 'function');
      self.ndbconn.wait_until_ready(1, 1, onReady);
    }
    else {
      err = new Error('NDB Connect failed ' + rval);       
      user_callback(err, self);
    }
  }
  
  // Fist step is to connect to the cluster
  self.ndbconn.connectAsync(self.properties.ndb_connect_retries,
                            self.properties.ndb_connect_delay,
                            self.properties.ndb_connect_verbose,
                            onConnected);
};


/* DBConnection.isConnected() method.
   IMMEDIATE.
   Returns bool true/false
 */
proto.isConnected = function() {
  return this.is_connected;
};


/* closeSync()
   SYNCHRONOUS.
*/
proto.closeSync = function() {
  // FIXME Delete NDB objects
  this.ndbconn.delete();
};


/* getDBSession().
   ASYNC.
   Creates and opens a new DBSession.
   Users's callback receives (error, DBSession)
*/
proto.getDBSession = function(index, user_callback) {
  udebug.log("NdbConnectionPool getDBSession");
  assert(this.ndbconn);
  assert(user_callback);
  var db   = this.properties.database,
      self = this;

  function private_callback(err, sessImpl) {
    var user_session;
    
    udebug.log("NDB getDBSession private_callback");
    udebug.log("Impl: " + sessImpl);

    if(err) {
      user_callback(err, null);
    }
    else {  
      user_session = ndbsession.getDBSession(self, sessImpl);
      user_callback(null, user_session);
    }
  }

  adapter.impl.DBSession.create(this.ndbconn, db, private_callback);
};


/** List all tables in the schema
  * ASYNC
  * 
  * listTables(databaseName, callback(error, array));
  */
proto.listTables = function(databaseName, user_callback) {
  udebug.log("NdbConnectionPool listTables");
  assert(databaseName && user_callback);
  adapter.impl.DBDictionary.listTables(this.dictionary, databaseName, user_callback);
};


/** Fetch metadata for a table
  * ASYNC
  * 
  * getTable(databaseName, tableName, callback(error, DBTable));
  */
proto.getTable = function(dbname, tabname, user_callback) {
  udebug.log("NdbConnectionPool getTable");
  assert(dbname && tabname && user_callback);
  adapter.impl.DBDictionary.getTable(this.dictionary, dbname, tabname, user_callback);
};


/* createDBTableHandler(dbTable, apiMapping)
   IMMEDIATE
   Creates and returns a DBTableHandler for table and mapping
*/
proto.createDBTableHandler = function(dbTable, apiMapping) { 
  udebug.log("NdbConnectionPool createDBTableHandler " + dbTable.name);
  var handler;
  handler = new dbtablehandler.DBTableHandler(this, dbTable, apiMapping);
  /* TODO: If the mapping is not a default mapping, then the DBTableHandler
     needs to be annotated with some Records */
  return handler;
};

/* getConverter 
   IMMEDIATE
   Fetch the converter for a column
   FIXME:  KEEP MAPS OF DEFAULT & REGISTERED CONVERTERS
*/
proto.getConverter = function(dbcolumn) {
  udebug.log("NdbConnectionPool getConverter " + dbcolumn.name);
  var r = converters.defaultForType[dbcolumn.columnType];
  return r;
};

