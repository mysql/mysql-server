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

/*global path, build_dir, assert, api_dir, unified_debug */

"use strict";

var adapter          = require(path.join(build_dir, "ndb_adapter.node")),
    ndbsession       = require("./NdbSession.js"),
    NdbConnection    = require("./NdbConnection.js"),
    dbtablehandler   = require("../common/DBTableHandler.js"),
    ndbencoders      = require("./NdbTypeEncoders.js"),
    udebug           = unified_debug.getLogger("NdbConnectionPool.js"),
    stats_module     = require(path.join(api_dir,"stats.js")),
    stats            = stats_module.getWriter("spi","ndb","DBConnectionPool"),
    baseConnections  = {},
    initialized      = false;

/* Load-Time Function Asserts */

assert(typeof adapter.ndb.ndbapi.Ndb_cluster_connection === 'function');
assert(typeof adapter.ndb.impl.DBSession.create === 'function');
assert(typeof adapter.ndb.impl.DBDictionary.listTables === 'function');
assert(typeof adapter.ndb.impl.DBDictionary.getTable === 'function');


function initialize() {
  adapter.ndb.ndbapi.ndb_init();                       // ndb_init()
  // adapter.ndb.util.CharsetMap_init();           // CharsetMap::init()
  unified_debug.register_client(adapter.debug);
  return true;
}


/* We keep only one actual underlying connection, 
   per distinct NDB connect string.  It is reference-counted.
*/
function getNdbConnection(connectString) {
  if(! initialized) {
    initialized = initialize();
  }

  if(baseConnections[connectString]) {
    baseConnections[connectString].referenceCount += 1;
  }
  else {
    baseConnections[connectString] = new NdbConnection(connectString);
  }
  
  stats.set(connectString, "refcount",
            baseConnections[connectString].referenceCount);
  return baseConnections[connectString];
}


/* Release an underlying connection.  If the refcount reaches zero, 
   keep it open for a msecToLinger milliseconds, then close it if no new
   clients have arrived.
   When we finally call close(), NdbConnection will close the connection at 
   some future point but we will not be notified about it.
*/
function releaseNdbConnection(connectString, msecToLinger) {
  var ndbConnection = baseConnections[connectString];
  ndbConnection.referenceCount -= 1;
  assert(ndbConnection.referenceCount >= 0);
  stats.set(connectString, "refcount", ndbConnection.referenceCount);

  function closeReally() {
    if(ndbConnection.referenceCount === 0) {        // No new customers.
      baseConnections[connectString] = null;  // Lock the door.
      ndbConnection.close();          // Then actually start shutting down.
    }
  }

  if(ndbConnection.referenceCount === 0) {
    setTimeout(closeReally, msecToLinger);
  }
}


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


exports.closeNdbSession = function(ndbPool, ndbSession) {
  if(ndbPool.ndbConnection.isDisconnecting  ||
      ( ndbPool.ndbSessionFreeList.length > 
        ndbPool.properties.ndb_session_pool_max))
  {
    adapter.ndb.impl.DBSession.destroy(ndbSession.impl);
  }
  else 
  { 
    ndbPool.ndbSessionFreeList.push(ndbSession);
  }
};


/* Prefetch an NdbSSession and keep it on the freelist
*/
function prefetchSession(ndbPool) {
  udebug.log("prefetchSession");
  var db       = ndbPool.properties.database,
      pool_min = ndbPool.properties.ndb_session_pool_min,
      ndbSession;

  function onFetch(err, ndbSessionImpl) {
    if(err) {
      stats.incr("ndbSession","prefetch","errors");
      udebug.log("prefetchSession onFetch ERROR", err);
    }
    else if(ndbPool.ndbConnection.isDisconnecting) {
       adapter.ndb.impl.DBSession.destroy(ndbSessionImpl);
    }
    else {
      stats.incr("ndbSession","prefetch","success");
      udebug.log("prefetchSession adding to session pool.");
      ndbSession = ndbsession.newDBSession(ndbPool, ndbSessionImpl);
      ndbPool.ndbSessionFreeList.push(ndbSession);
      /* If the pool is wanting, fetch another */
      if(ndbPool.ndbSessionFreeList.length < pool_min) {
        stats.incr("ndbSession","prefetch","attempts");
        adapter.ndb.impl.DBSession.create(ndbPool.impl, db, onFetch);
      }
    }
  }

  if(! ndbPool.ndbConnection.isDisconnecting) {
    stats.incr("ndbSession","prefetch","attempts");
    adapter.ndb.impl.DBSession.create(ndbPool.impl, db, onFetch);
  }
}


///////////       Class DBConnectionPool       /////////////

/* DBConnectionPool constructor.
   IMMEDIATE.
   Does not perform any IO. 
   Throws an exception if the Properties object is invalid.
*/   
function DBConnectionPool(props) {
  stats.incr("created");
  this.properties         = props;
  this.ndbConnection      = null;
  this.impl               = null;
  this.dict_sess          = null;
  this.dictionary         = null;
  this.asyncNdbContext    = null;
  this.pendingListTables  = {};
  this.pendingGetMetadata = {};
  this.ndbSessionFreeList = [];
}


/* Blocking connect.  
   SYNC.
   Returns true on success and false on error.
*/
DBConnectionPool.prototype.connectSync = function() {
  udebug.log("connectSync");
  var db = this.properties.database;
  
  this.ndbConnection = getNdbConnection(this.properties.ndb_connectstring);

  if(this.ndbConnection.connectSync(this.properties)) {
    this.impl = this.ndbConnection.ndb_cluster_connection;
    
    /* Get sessionImpl and session for dictionary use */
    this.dictionary = adapter.ndb.impl.DBSession.create(this.impl, db);
    this.dict_sess  = ndbsession.newDBSession(this, this.dictionary);
    
    /* Start filling the session pool */
    prefetchSession(this);

    /* Create Async Context */
    if(this.properties.use_ndb_async_api) {
      this.asyncNdbContext = this.ndbConnection.getAsyncContext();
    }
  }
    
  return this.ndbConnection.isConnected;
};


/* Async connect 
*/
DBConnectionPool.prototype.connect = function(user_callback) {
  stats.incr("connect", "async");
  var self = this;

  function onGotDictionarySession(cb_err, dsess) {
    udebug.log("DBConnectionPool.connect onGotDictionarySession");
    self.dict_sess = dsess;
    self.dictionary = self.dict_sess.impl;
    
    if(cb_err) {
      user_callback(cb_err, null);
    }
    else {
      /* Start filling the session pool */
      prefetchSession(self);

      /* Create Async Context */
      if(self.properties.use_ndb_async_api) {
        self.asyncNdbContext = self.ndbConnection.getAsyncContext();
      }

      /* All done */
      user_callback(null, self);
    }
  }
  
  function onConnected(err) {
    udebug.log("DBConnectionPool.connect onConnected");
    if(err) {
      user_callback(err, self);
    }
    else {
      self.impl = self.ndbConnection.ndb_cluster_connection;
      self.getDBSession(0, onGotDictionarySession);
    }
  }
  
  /* Connect starts here */
  this.ndbConnection = getNdbConnection(this.properties.ndb_connectstring);
  this.ndbConnection.connect(this.properties, onConnected);
};


/* DBConnection.isConnected() method.
   IMMEDIATE.
   Returns bool true/false
 */
DBConnectionPool.prototype.isConnected = function() {
  return this.ndbConnection.isConnected;
};


/* close()
   ASYNC.
*/
DBConnectionPool.prototype.close = function(user_callback) {
  var i;
  adapter.ndb.impl.DBSession.destroy(this.dictionary);
  for(i = 0 ; i < this.ndbSessionFreeList.length ; i++) {
    adapter.ndb.impl.DBSession.destroy(this.ndbSessionFreeList[i].impl);
  }
  releaseNdbConnection(this.properties.ndb_connectstring,
                       this.properties.linger_on_close_msec);
  user_callback(null);
};


/* getDBSession().
   ASYNC.
   Creates and opens a new DBSession.
   Users's callback receives (error, DBSession)
*/
DBConnectionPool.prototype.getDBSession = function(index, user_callback) {
  udebug.log("getDBSession");
  assert(this.impl);
  assert(user_callback);
  var db   = this.properties.database,
      self = this,
      user_session;

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

  user_session = this.ndbSessionFreeList.pop();
  if(user_session) {
    stats.incr("ndbSession","pool","hits");
    user_callback(null, user_session);
  }
  else {
    stats.incr("ndbSession","pool","misses");
    adapter.ndb.impl.DBSession.create(this.impl, db, private_callback);
  }
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
DBConnectionPool.prototype.listTables = function(databaseName, dbSession, 
                                                 user_callback) {
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
DBConnectionPool.prototype.getTableMetadata = function(dbname, tabname, 
                                                       dbSession, user_callback) {
  var dictSession, tableKey, dictionaryCall;
  stats.incr("getTableMetadata");
  assert(dbname && tabname && user_callback);
  dictSession = dbSession || this.dict_sess; 
  tableKey = dbname + "." + tabname;
  
  function drColumn(c) {
    if(c.ndbRawDefaultValue) {
      var enc = ndbencoders.defaultForType[c.ndbTypeId];
      c.defaultValue = enc.read(c, c.ndbRawDefaultValue, 0);
    }       
    else if(c.isNullable) {
      c.defaultValue = null;
    }
    else {
      c.defaultValue = undefined;
    }
    // This could be done to clean up the structure:
    // delete(c.ndbRawDefaultValue);
  }

  function makeInternalCallback(user_function) {
    // TODO: Wrap the NdbError in a large explicit error message db.tbl not in ndb engine
    return function(err, table) {
      // Walk the table and create defaultValue from ndbRawDefaultValue
      if(table) {
        table.columns.forEach(drColumn);
      }
      user_callback(err, table);  
    };
  }

  var our_callback = makeInternalCallback(user_callback);

  if(this.pendingGetMetadata[tableKey]) {
    // This request is already running, so add our own callback to its list
    udebug.log("getTableMetadata", tableKey, "Adding request to pending group");
    this.pendingGetMetadata[tableKey].push(our_callback);
  }
  else {
    this.pendingGetMetadata[tableKey] = [];
    this.pendingGetMetadata[tableKey].push(our_callback);
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
DBConnectionPool.prototype.createDBTableHandler = function(tableMetadata, 
                                                           apiMapping) { 
  udebug.log("createDBTableHandler", tableMetadata.name);
  var handler;
  handler = new dbtablehandler.DBTableHandler(tableMetadata, apiMapping);
  /* TODO: If the mapping is not a default mapping, then the DBTableHandler
     needs to be annotated with some Records */
  return handler;
};


exports.DBConnectionPool = DBConnectionPool; 

