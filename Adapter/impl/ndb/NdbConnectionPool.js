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

var adapter          = require(path.join(build_dir, "ndb_adapter.node")),
    ndbsession       = require("./NdbSession.js"),
    NdbConnection    = require("./NdbConnection.js"),
    dbtablehandler   = require("../common/DBTableHandler.js"),
    autoincrement    = require("./NdbAutoIncrement.js"),
    udebug           = unified_debug.getLogger("NdbConnectionPool.js"),
    stats_module     = require(path.join(api_dir,"stats.js")),
    stats            = stats_module.getWriter(["spi","ndb","DBConnectionPool"]),
    ColumnTypes      = require(path.join(api_doc_dir,"TableMetadata")).ColumnTypes,
    isValidConverterObject = require(path.join(api_dir,"TableMapping")).isValidConverterObject,
    QueuedAsyncCall  = require("../common/QueuedAsyncCall.js").QueuedAsyncCall,
    baseConnections  = {},
    initialized      = false;

/* Load-Time Function Asserts */
assert(typeof adapter.ndb.ndbapi.Ndb_cluster_connection === 'function');
assert(typeof adapter.ndb.impl.DBDictionary.listTables === 'function');


function initialize() {
  adapter.ndb.ndbapi.ndb_init();                       // ndb_init()
  adapter.ndb.util.CharsetMap_init();           // CharsetMap::init()
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
  
  stats.set( [ connectString, "refcount" ] ,
            baseConnections[connectString].referenceCount);
  return baseConnections[connectString];
}


/* Release an underlying connection.  If the refcount reaches zero, 
   keep it open for a msecToLinger milliseconds, then close it if no new
   clients have arrived.
   When we finally call close(), NdbConnection will close the connection at 
   some future point but we will not be notified about it.
*/
function releaseNdbConnection(connectString, msecToLinger, userCallback) {
  var ndbConnection = baseConnections[connectString];
  ndbConnection.referenceCount -= 1;
  assert(ndbConnection.referenceCount >= 0);
  stats.set( [ connectString, "refcount" ] , ndbConnection.referenceCount);

  function closeReally() {
    if(ndbConnection.referenceCount === 0) {        // No new customers.
      baseConnections[connectString] = null;    // Lock the door.
      ndbConnection.close(userCallback);  // Then actually start shutting down.
    }
  }

  if(ndbConnection.referenceCount === 0) {
    setTimeout(closeReally, msecToLinger);
  }
  else {
    if(typeof userCallback === 'function') {
      userCallback();
    }
  }
}


exports.closeNdbSession = function(ndbPool, ndbSession, userCallback) {
  var ndbConn = ndbPool.ndbConnection;
  var apiCall;

  if(! ndbConn.isConnected) 
  {
    /* The parent connection is already gone. */
    userCallback();
  }
  else if( ndbConn.isDisconnecting ||
           ( ndbPool.ndbSessionFreeList.length > 
             ndbPool.properties.ndb_session_pool_max))
  {
    /* (A) The connection is going to close, or (B) The freelist is full. 
       Either way, enqueue a close call.
    */
    apiCall = new QueuedAsyncCall(ndbConn.execQueue, userCallback);
    apiCall.description = "closeNdb";
    apiCall.ndb = ndbSession.impl;
    apiCall.run = function() {
      this.ndb.close(this.callback);
    };
    apiCall.enqueue();
  }
  else 
  { 
    /* Do not actually close; just put the session on the freelist */
    ndbPool.ndbSessionFreeList.push(ndbSession);
    userCallback();
  }
};


/* Prefetch an NdbSSession and keep it on the freelist
*/
function prefetchSession(ndbPool) {
  udebug.log("prefetchSession");
  var db       = ndbPool.properties.database,
      pool_min = ndbPool.properties.ndb_session_pool_min,
      ndbSession;

  function closeCallback(x) {
    return; 
  }

  function onFetch(err, ndbSessionImpl) {
    if(err) {
      stats.incr(["ndbSession","prefetch","errors"]);
      udebug.log("prefetchSession onFetch ERROR", err);
    }
    else if(ndbPool.ndbConnection.isDisconnecting) {
      ndbSessionImpl.close(closeCallback);
    }
    else {
      stats.incr(["ndbSession","prefetch","success"]);
      udebug.log("prefetchSession adding to session pool.");
      ndbSession = ndbsession.newDBSession(ndbPool, ndbSessionImpl);
      ndbPool.ndbSessionFreeList.push(ndbSession);
      /* If the pool is wanting, fetch another */
      if(ndbPool.ndbSessionFreeList.length < pool_min) {
        stats.incr(["ndbSession","prefetch","attempts"]);
        adapter.ndb.impl.create_ndb(ndbPool.impl, db, onFetch);
      }
    }
  }

if(! ndbPool.ndbConnection.isDisconnecting) {
    stats.incr(["ndbSession","prefetch","attempts"]);
    adapter.ndb.impl.create_ndb(ndbPool.impl, db, onFetch);
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
  this.asyncNdbContext    = null;
  this.pendingListTables  = {};
  this.pendingGetMetadata = {};
  this.ndbSessionFreeList = [];
  this.typeConverters     = {};
  this.openTables         = [];
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
  stats.incr([ "connect", "async" ] );
  var self = this;

  function onConnected(err) {
    udebug.log("DBConnectionPool.connect onConnected");

    if(err) {
      user_callback(err, self);
    }
    else {
      self.impl = self.ndbConnection.ndb_cluster_connection;

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
DBConnectionPool.prototype.close = function(userCallback) {
  var session, table, properties, nclose;
  nclose = this.ndbSessionFreeList.length + this.openTables.length;
  properties = this.properties;
  udebug.log("DBConnectionPool.close()", nclose);

  function onNdbClose() {
    nclose--;
    udebug.log("nclose", nclose);
    if(nclose === 0) {
      releaseNdbConnection(properties.ndb_connectstring,
                           properties.linger_on_close_msec,
                           userCallback);    
    }  
  }
  
  /* Special case: nothing to close */
  if(nclose === 0) {
    nclose = 1; onNdbClose();
  }
  
  /* Close the NDB on open tables */
  while(table = this.openTables.pop()) {
    table.per_table_ndb.close(onNdbClose);
  }

  /* Close the NDBs from the session pool */
  while(session = this.ndbSessionFreeList.pop()) {
    session.impl.close(onNdbClose);
  }  
};


/* getDBSession().
   ASYNC.
   Creates and opens a new DBSession.
   Users's callback receives (error, DBSession)
*/
DBConnectionPool.prototype.getDBSession = function(index, user_callback) {
  var self, user_session, apiCall;
  self = this;

  function private_callback(err, sessImpl) {
    udebug.log("getDBSession private_callback");
    var userSession;

    if(err) {
      user_callback(err, null);
    }
    else {  
      userSession = ndbsession.newDBSession(self, sessImpl);
      user_callback(null, userSession);
    }
  }

  user_session = this.ndbSessionFreeList.pop();
  if(user_session) {
    stats.incr( [ "ndbSession","pool","hits" ] );
    user_callback(null, user_session);
  }
  else {
    // NOTE: It may not be necessary to serialize these.
    stats.incr( [ "ndbSession","pool","misses" ] );
    apiCall = new QueuedAsyncCall(this.ndbConnection.execQueue, private_callback);
    apiCall.description = "newNdb";
    apiCall.impl = this.ndbConnection.ndb_cluster_connection;
    apiCall.db = this.properties.database;
    apiCall.run = function() {
      adapter.ndb.impl.create_ndb(this.impl, this.db, this.callback);
    };
    apiCall.enqueue();
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
  stats.incr( [ "group_callbacks","created" ] );
  var groupCallback = function(param1, param2) {
    var callbackList, i, nextCall;

    /* Run the user callbacks on our list */
    callbackList = container[key];
    udebug.log("GroupCallback for", key, "with", callbackList.length, "user callbacks");
    for(i = 0 ; i < callbackList.length ; i++) { 
      callbackList[i](param1, param2);
    }

    /* Then clear the list */
    delete container[key];
  };
  
  return groupCallback;
}


function makeListTablesCall(dbSession, ndbConnectionPool, databaseName) {
  var container = ndbConnectionPool.pendingListTables;
  var groupCallback = makeGroupCallback(dbSession, container, databaseName);
  var apiCall = new QueuedAsyncCall(dbSession.execQueue, groupCallback);
  apiCall.impl = dbSession.impl;
  apiCall.databaseName = databaseName;
  apiCall.description = "listTables";
  apiCall.run = function() {
    adapter.ndb.impl.DBDictionary.listTables(this.impl, this.databaseName, 
                                             this.callback);
  };
  return apiCall;
}


/** List all tables in the schema
  * ASYNC
  * 
  * listTables(databaseName, dbSession, callback(error, array));
  */
DBConnectionPool.prototype.listTables = function(databaseName, dictSession, 
                                                 user_callback) {
  stats.incr("listTables");
  assert(databaseName && user_callback);

  if(this.pendingListTables[databaseName]) {
    // This request is already running, so add our own callback to its list
    udebug.log("listTables", databaseName, "Adding request to pending group");
    this.pendingListTables[databaseName].push(user_callback);
  }
  else {
    this.pendingListTables[databaseName] = [];
    this.pendingListTables[databaseName].push(user_callback);
    makeListTablesCall(dictSession, this, databaseName).enqueue();
  }
};


function makeGetTableCall(dbSession, ndbConnectionPool, dbName, tableName) {
  var container = ndbConnectionPool.pendingGetMetadata;
  var key = dbName + "." + tableName;
  var groupCallback = makeGroupCallback(dbSession, container, key);

  /* Customize Column read from dictionary */
  function drColumn(c) {
    /* Set TypeConverter for column */
    c.typeConverter = ndbConnectionPool.typeConverters[c.columnType];

    /* Set defaultValue for column */
    if(c.ndbRawDefaultValue) {
      c.defaultValue = 
        adapter.ndb.impl.encoderRead(c, c.ndbRawDefaultValue, 0);
      delete(c.ndbRawDefaultValue);
    }       
    else if(c.isNullable) {
      c.defaultValue = null;
    }
    else {
      c.defaultValue = undefined;
    }
    udebug.log_detail("drColumn:", c);
  }

  function masterCallback(err, table) {
    if(err) {
      err.notice = "Table " + key + " not found in NDB data dictionary";
    }
    if(table) {
      autoincrement.getCacheForTable(table);  // get AutoIncrementCache
      table.columns.forEach(drColumn);
      ndbConnectionPool.openTables.push(table);
    }
    /* Finally dispatch the group callbacks */
    groupCallback(err, table);
  }

  var apiCall = new QueuedAsyncCall(dbSession.execQueue, masterCallback);
  apiCall.impl = dbSession.impl;
  apiCall.dbName = dbName;
  apiCall.tableName = tableName;
  apiCall.description = "getTableMetadata";
  apiCall.run = function() {
    adapter.ndb.impl.DBDictionary.getTable(this.impl, this.dbName, 
                                           this.tableName, this.callback);
  };
  return apiCall;
}


/** Fetch metadata for a table
  * ASYNC
  * 
  * getTableMetadata(databaseName, tableName, dbSession, callback(error, TableMetadata));
  */
DBConnectionPool.prototype.getTableMetadata = function(dbname, tabname, 
                                                       dictSession, user_callback) {
  var tableKey;
  stats.incr("getTableMetadata");
  assert(dbname && tabname && user_callback);
  tableKey = dbname + "." + tabname;

  if(this.pendingGetMetadata[tableKey]) {
    // This request is already running, so add our own callback to its list
    udebug.log("getTableMetadata", tableKey, "Adding request to pending group");
    this.pendingGetMetadata[tableKey].push(user_callback);
  }
  else {
    this.pendingGetMetadata[tableKey] = [];
    this.pendingGetMetadata[tableKey].push(user_callback);
    makeGetTableCall(dictSession, this, dbname, tabname).enqueue();
  }
};


/* registerTypeConverter(typeName, converterObject) 
   IMMEDIATE
*/
DBConnectionPool.prototype.registerTypeConverter = function(typeName, converter) {
  typeName = typeName.toLocaleUpperCase(); 
  if(ColumnTypes.indexOf(typeName) === -1) {
    throw new Error(typeName + " is not a valid column type.");
  }

  if(converter === null) {
    delete this.typeConverters[typeName];
  }
  else if(isValidConverterObject(converter)) {
    this.typeConverters[typeName] = converter;  
  }
  else { 
    throw new Error("Not a valid converter");
  }
};


exports.DBConnectionPool = DBConnectionPool; 

