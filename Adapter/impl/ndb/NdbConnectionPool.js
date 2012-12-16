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

/*global path, build_dir, assert, spi_dir, api_dir, unified_debug */

"use strict";

var adapter        = require(path.join(build_dir, "ndb_adapter.node")),
    ndbsession     = require("./NdbSession.js"),
    dbtablehandler = require("../common/DBTableHandler.js"),
    udebug         = unified_debug.getLogger("NdbConnectionPool.js"),
    ndb_is_initialized = false,
    stats_module   = require(path.join(api_dir,"stats.js")),
    stats          = stats_module.getWriter("spi","ndb","DBConnectionPool"),
    proto;


function initialize_ndb() {
  if(! ndb_is_initialized) {
    adapter.ndb.ndbapi.ndb_init();                       // ndb_init()
    // adapter.ndb.util.CharsetMap_init();           // CharsetMap::init()
    unified_debug.register_client(adapter.debug);
    ndb_is_initialized = true;
  }
}


/* Load-Time Function Asserts */

assert(typeof adapter.ndb.ndbapi.Ndb_cluster_connection === 'function');
assert(typeof adapter.ndb.impl.DBSession.create === 'function');
assert(typeof adapter.ndb.impl.DBDictionary.listTables === 'function');
assert(typeof adapter.ndb.impl.DBDictionary.getTable === 'function');


/* Each NdbSession object has a lock protecting dictionary access 
*/ 
function getDictionaryLock(ndbSession) {
  if(ndbSession.lock === 0) {
    ndbSession.lock = 1;
    return true;
  }
  return false;
}

function releaseDictionaryLock(ndbSession) {
  assert(ndbSession.lock === 1);
  ndbSession.lock = 0;
}



/* DBConnectionPool constructor.
   IMMEDIATE.
   Does not perform any IO. 
   Throws an exception if the Properties object is invalid.
*/   
exports.DBConnectionPool = function(props) {
  udebug.log("constructor");
  stats.incr("created");

  initialize_ndb();
  
  this.properties = props;
  this.ndbconn = 
    new adapter.ndb.ndbapi.Ndb_cluster_connection(props.ndb_connectstring);
  this.ndbconn.set_name("nodejs");
  udebug.log("constructor returning");
};

/* NdbConnectionPool prototype 
*/
proto = {
  properties           : null,
  ndbconn              : null,
  is_connected         : false,
  dict_sess            : null,
  dictionary           : null,
  pendingListTables    : {},
  pendingGetMetadata   : {},
};

exports.DBConnectionPool.prototype = proto;


/* Blocking connect.  
   SYNC.
   Returns true on success and false on error.
*/
proto.connectSync = function() {
  stats.incr("connect", "sync");
  var r, nnodes;
  var db = this.properties.database;
  r = this.ndbconn.connect(this.properties.ndb_connect_retries,
                           this.properties.ndb_connect_delay,
                           this.properties.ndb_connect_verbose);
  if(r === 0) {
    stats.incr("connections","successful");
    nnodes = this.ndbconn.wait_until_ready(1, 1);
    if(nnodes < 0) {
      stats.incr("on_ready","timeouts");
      throw new Error("Timeout waiting for cluster to become ready.");
    }
    else {
      this.is_connected = true;
      if(nnodes > 0) {
        udebug.log_notice("Warning: only", nnodes, "data nodes are running.");
      }
      udebug.log_notice("Connected to cluster as node id:", this.ndbconn.node_id());
      stats.push("node_ids", this.ndbconn.node_id());
    }

    /* Get sessionImpl and session for dictionary use */
    this.dictionary = adapter.ndb.impl.DBSession.create(this.ndbconn, db);  
    this.dict_sess  = ndbsession.newDBSession(this, this.dictionary);
  }
  else {
    stats.incr("connections","failed");
  }

    
  return this.is_connected;
};


/* Async connect 
*/
proto.connect = function(user_callback) {
  udebug.log("connect");
  stats.incr("connect", "async");
  var self = this,
      err = null;

  function onGotDictionarySession(cb_err, dsess) {
    udebug.log("connect onGotDictionarySession");
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
    udebug.log("connect() onReady nnodes =", nnodes);
    if(nnodes < 0) {
      stats.incr("on_ready","timeouts");
      err = new Error("Timeout waiting for cluster to become ready.");
      user_callback(err, self);
    }
    else {
      self.is_connected = true;
      if(nnodes > 0) {
        udebug.log_notice("Warning: only " + nnodes + " data nodes are running.");
      }
      udebug.log_notice("Connected to cluster as node id: " + self.ndbconn.node_id());
     }
     self.getDBSession(0, onGotDictionarySession);
  }
  
  function onConnected(cb_err, rval) {
    // Connected to NDB.  Next step is wait_until_ready().
    udebug.log("connect() onConnected rval =", rval);
    if(rval === 0) {
      stats.incr("connections","successful");
      assert(typeof self.ndbconn.wait_until_ready === 'function');
      self.ndbconn.wait_until_ready(1, 1, onReady);
    }
    else {
      stats.incr("connectios","failed");
      err = new Error('NDB Connect failed ' + rval);       
      user_callback(err, self);
    }
  }
  
  // Fist step is to connect to the cluster
  self.ndbconn.connect(self.properties.ndb_connect_retries,
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
  adapter.ndb.impl.DBSession.destroy(this.dictionary);
  this.ndbconn.delete();
};


/* getDBSession().
   ASYNC.
   Creates and opens a new DBSession.
   Users's callback receives (error, DBSession)
*/
proto.getDBSession = function(index, user_callback) {
  udebug.log("getDBSession");
  assert(this.ndbconn);
  assert(user_callback);
  var db   = this.properties.database,
      self = this;

  function private_callback(err, sessImpl) {
    udebug.log("getDBSession private_callback");

    var user_session;
    if(err) {
      user_callback(err, null);
    }
    else {  
      user_session = ndbsession.newDBSession(self, sessImpl);
      user_callback(null, user_session);
    }
  }

  adapter.ndb.impl.DBSession.create(this.ndbconn, db, private_callback);
};


/*
 *  Implementation of listTables() and getTableMetadata() 
 *
 *  Design notes: 
 *    A single Ndb can perform one metadata lookup at a time. 
 *    An NdbSession object owns a dictionary lock and a queue of dictionary calls.
 *    If we can get the lock, we run a call immediately; if not, place it on the queue.
 *    
 *    Also, it often happens in a Batch context that a bunch of operations all
 *    need the same metadata.  So, for each dictionary call, we create a group callback,
 *    and then add individual user callbacks to that group as they come in.
 * 
 *    The dictionary call and the group callback are both created by generator functions.
 * 
 *    Who runs the queue?  The group callback checks it after it all the user 
 *    callbacks have completed. 
 * 
 */


function makeGroupCallback(dbSession, container, key) {
  stats.incr("group_callbacks","created");
  var groupCallback = function(param1, param2) {
    var callbackList, i, nextCall;
    /* The Dictionay Call is complete */
    releaseDictionaryLock(dbSession);

    /* Run the user callbacks on our list */
    callbackList = container[key];
    udebug.log("GroupCallback for", key, "with", callbackList.length, "user callbacks");
    for(i = 0 ; i < callbackList.length ; i++) { 
      callbackList[i](param1, param2);
    }

    /* Then clear the list */
    delete container[key];

    /* If any dictionary calls have been queued up, run the next one. */
    nextCall = dbSession.dictQueue.shift();
    if(nextCall) {
      getDictionaryLock(dbSession);
      nextCall();
    }
  };

  return groupCallback;
}


function makeListTablesCall(dbSession, ndbConnectionPool, databaseName) {
  var container = ndbConnectionPool.pendingListTables;
  var groupCallback = makeGroupCallback(dbSession, container, databaseName);
  var impl = dbSession.impl;
  return function() {
    adapter.ndb.impl.DBDictionary.listTables(impl, databaseName, groupCallback);
  };
}


function makeGetTableCall(dbSession, ndbConnectionPool, dbName, tableName) {
  var container = ndbConnectionPool.pendingGetMetadata;
  var key = dbName + "." + tableName;
  var groupCallback = makeGroupCallback(dbSession, container, key);
  var impl = dbSession.impl;
  return function() {
    adapter.ndb.impl.DBDictionary.getTable(impl, dbName, tableName, groupCallback);
  };
}


/** List all tables in the schema
  * ASYNC
  * 
  * listTables(databaseName, dbSession, callback(error, array));
  */
proto.listTables = function(databaseName, dbSession, user_callback) {
  udebug.log("listTables");
  stats.incr("listTables");
  assert(databaseName && user_callback);
  var dictSession = dbSession || this.dict_sess; 
  var dictionaryCall;

  if(this.pendingListTables[databaseName]) {
    // This request is already running, so add our own callback to its list
    udebug.log("listTables", databaseName, "Adding request to pending group");
    this.pendingListTables[databaseName].push(user_callback);
  }
  else {
    this.pendingListTables[databaseName] = [];
    this.pendingListTables[databaseName].push(user_callback);
    dictionaryCall = makeListTablesCall(dictSession, this, databaseName);
   
    if(getDictionaryLock(dictSession)) { // Make the call directly
      udebug.log("listTables", databaseName, "New group; running now.");
      dictionaryCall();
    }
    else {  // otherwise place it on a queue
      udebug.log("listTables", databaseName, "New group; on queue.");
      dictSession.dictQueue.push(dictionaryCall);
    }
  }
};


/** Fetch metadata for a table
  * ASYNC
  * 
  * getTableMetadata(databaseName, tableName, dbSession, callback(error, TableMetadata));
  */
proto.getTableMetadata = function(dbname, tabname, dbSession, user_callback) {
  var dictSession, tableKey, dictionaryCall;
  udebug.log("getTableMetadata");
  stats.incr("getTableMetadata");
  assert(dbname && tabname && user_callback);
  dictSession = dbSession || this.dict_sess; 
  tableKey = dbname + "." + tabname;

  // TODO: Wrap the NdbError in a large explicit error message db.tbl not in ndb engine


  if(this.pendingGetMetadata[tableKey]) {
    // This request is already running, so add our own callback to its list
    udebug.log("getTableMetadata", tableKey, "Adding request to pending group");
    this.pendingGetMetadata[tableKey].push(user_callback);
  }
  else {
    this.pendingGetMetadata[tableKey] = [];
    this.pendingGetMetadata[tableKey].push(user_callback);
    dictionaryCall = makeGetTableCall(dictSession, this, dbname, tabname);
  
    if(getDictionaryLock(dictSession)) { // Make the call directly
      udebug.log("getTableMetadata", tableKey, "New group; running now.");
      dictionaryCall();
    }
    else {  // otherwise place it on a queue
      udebug.log("getTableMetadata", tableKey, "New group; on queue.");
      dictSession.dictQueue.push(dictionaryCall);
    }
  }
};


/* createDBTableHandler(tableMetadata, apiMapping)
   IMMEDIATE
   Creates and returns a DBTableHandler for table and mapping
*/
proto.createDBTableHandler = function(tableMetadata, apiMapping) { 
  udebug.log("createDBTableHandler", tableMetadata.name);
  var handler;
  handler = new dbtablehandler.DBTableHandler(tableMetadata, apiMapping);
  /* TODO: If the mapping is not a default mapping, then the DBTableHandler
     needs to be annotated with some Records */
  return handler;
};

