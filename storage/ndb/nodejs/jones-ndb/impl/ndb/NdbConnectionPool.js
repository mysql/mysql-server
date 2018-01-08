/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

"use strict";

var stats = {
  "created"                 : 0,
  "connect"                 : 0,
  "refcount"                : {},
  "ndb_session_pool"        : { "hits" : 0, "misses" : 0, "closes" : 0 },
  "ndb_session_prefetch"    : { "attempts" : 0, "errors" : 0, "success" : 0 },
  "group_callbacks_created" : 0,
  "list_tables"             : 0,
  "get_table_metadata"      : 0,
  "create_table"            : 0
};

var conf             = require("./path_config"),
    assert           = require("assert"),
    adapter          = require(conf.binary),
    ndbsession       = require("./NdbSession.js"),
    NdbConnection    = require("./NdbConnection.js"),
    MetadataManager  = require("./NdbMetadataManager.js"),
    jones            = require("database-jones"),
    SQLBuilder       = require(jones.common.SQLBuilder),
    sqlBuilder       = new SQLBuilder(),
    dbtablehandler   = require(jones.common.DBTableHandler),
    autoincrement    = require("./NdbAutoIncrement.js"),
    udebug           = unified_debug.getLogger("NdbConnectionPool.js"),
    stats_module     = require(jones.api.stats),
    isValidConverterObject = require(jones.api.TableMapping).isValidConverterObject,
    QueuedAsyncCall  = require(jones.common.QueuedAsyncCall).QueuedAsyncCall,
    DictionaryCall   = require(jones.common.DictionaryCall),
    baseConnections  = {},
    initialized      = false;

var ColumnTypes =  [
  "TINYINT",  "SMALLINT",  "MEDIUMINT",  "INT",  "BIGINT",
  "FLOAT",  "DOUBLE",  "DECIMAL",
  "CHAR",  "VARCHAR",  "BLOB",  "TEXT", "JSON",
  "DATE",  "TIME",  "DATETIME",  "YEAR",  "TIMESTAMP",
  "BIT",  "BINARY",  "VARBINARY"
];


/* Load-Time Function Asserts */
assert(typeof adapter.ndb.ndbapi.Ndb_cluster_connection === 'function');
assert(typeof adapter.ndb.impl.DBDictionary.listTables === 'function');

stats_module.register(stats, "spi","ndb","DBConnectionPool");


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
    stats.refcount[connectString]++;
  }
  else {
    baseConnections[connectString] = new NdbConnection(connectString);
    stats.refcount[connectString] = 1;
  }
  
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
  stats.refcount[connectString] -= 1;
  assert(ndbConnection.referenceCount >= 0);

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


function closeDbSessionImpl(execQueue, impl, callbackOnClose) {
  stats.ndb_session_pool.closes++;
  impl.freeTransactions();
  var apiCall = new QueuedAsyncCall(execQueue, callbackOnClose);
  apiCall.description = "closeDbSessionImpl";
  apiCall.impl = impl;
  apiCall.run = function() {
    this.impl.destroy(this.callback);
  };
  apiCall.enqueue();
}


function closeNdb(execQueue, ndb, callbackOnClose) {
  var apiCall = new QueuedAsyncCall(execQueue, callbackOnClose);
  apiCall.description = "closeNdb";
  apiCall.ndb = ndb;
  apiCall.run = function() {
    this.ndb.close(this.callback);
  };
  apiCall.enqueue();
}


exports.closeNdbSession = function(ndbSession, userCallback) {
  var ndbPool = ndbSession.parentPool;
  var ndbConn = ndbPool.ndbConnection;
  var ndbSessionImpl;

  if(ndbSession.isOpenNdbSession === false)
  {
    /* The session is already closed */
    userCallback();
  }
  else if(! ndbConn.isConnected)
  {
    /* The parent connection is already gone. */
    userCallback();
  }
  else if( ndbConn.isDisconnecting ||
           ( ndbPool.ndbSessionFreeList.length > 
             ndbPool.properties.ndb_session_pool_max))
  {
    /* (A) The connection is going to close, or (B) The freelist is full. 
       Either way, enqueue a close call. */
    ndbSessionImpl = ndbSession.impl;
    ndbSession.impl = null;
    closeDbSessionImpl(ndbConn.execQueue, ndbSessionImpl, userCallback);
  }
  else 
  { 
    /* Mark the session as closed and put it on the freelist */
    ndbSession.isOpenNdbSession = false;
    ndbPool.ndbSessionFreeList.push(ndbSession);
    userCallback();
  }
};


/* Prefetch an NdbSSession and keep it on the freelist
*/
function prefetchSession(ndbPool) {
  udebug.log("prefetchSession");
  var pool_min = ndbPool.properties.ndb_session_pool_min,
      onFetch;

  function fetch() {
    var s = new ndbsession.DBSession(ndbPool);
    stats.ndb_session_prefetch.attempts++;
    s.fetchImpl(onFetch);
  }
  
  onFetch = function(err, dbSession) {
    if(err) {
      stats.ndb_session_prefetch.errors++;
      udebug.log("prefetchSession onFetch ERROR", err);
    } else if(ndbPool.ndbConnection.isDisconnecting) {
      dbSession.close(function() {});
    } else {
      stats.ndb_session_prefetch.success++;
      udebug.log("prefetchSession adding to session pool.");
      ndbPool.ndbSessionFreeList.push(dbSession);
      /* If the pool is wanting, fetch another */
      if(ndbPool.ndbSessionFreeList.length < pool_min) {
        fetch();
      }
    }
  };

  /* prefetchSession starts here */
  if(! ndbPool.ndbConnection.isDisconnecting) {
    fetch();
  }
}


///////////       Class DBConnectionPool       /////////////

/* DBConnectionPool constructor.
   IMMEDIATE.
   Does not perform any IO. 
   Throws an exception if the Properties object is invalid.
*/   
function DBConnectionPool(props) {
  stats.created++;
  this.properties         = props;
  this.ndbConnection      = null;
  this.impl               = null;
  this.asyncNdbContext    = null;
  this.dictionaryCalls    = new DictionaryCall.Call();
  this.ndbSessionFreeList = [];
  this.typeConverters     = {};
  this.openTables         = [];
  this.metadataManager    = new MetadataManager(props);
}


/* Capabilities provided by this connection.
*/
DBConnectionPool.prototype.getCapabilities = function() {
  return {
    "UniqueIndexes"     : true,   //  Tables can have secondary unique keys
    "TableScans"        : true,   //  Query can scan a table
    "OrderedIndexScans" : true,   //  Query can scan an index
    "ForeignKeys"       : true    //  Named foreign key relationships
  };
};


/* Async connect 
*/
DBConnectionPool.prototype.connect = function(user_callback) {
  stats.connect++;
  var self = this;

  function onConnected(err) {
    udebug.log("DBConnectionPool.connect onConnected");

    if(err) {
      user_callback(err, self);
    }
    else {
      self.impl = self.ndbConnection.ndb_cluster_connection;

      /* Create Async Context */
      if(self.properties.use_ndb_async_api) {
        self.asyncNdbContext = self.ndbConnection.getAsyncContext();
      }

      /* Start filling the session pool */
      prefetchSession(self);

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
    udebug.log_detail("nclose", nclose);
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
  udebug.log(" - Closing", this.openTables.length, "per-table Ndb object(s)");
  while(table = this.openTables.pop()) {
    closeNdb(this.ndbConnection.execQueue, table.per_table_ndb , onNdbClose);
  }

  /* Close the SessionImpls from the session pool */
  udebug.log(" - Closing", this.ndbSessionFreeList.length, "NdbSessionImpls from free list");
  while(session = this.ndbSessionFreeList.pop()) {
    closeDbSessionImpl(this.ndbConnection.execQueue, session.impl, onNdbClose);
  }
};


/* getDBSession().
   ASYNC.
   Creates and opens a new DBSession.
   Users's callback receives (error, DBSession)
*/
DBConnectionPool.prototype.getDBSession = function(index, user_callback) {
  var user_session = this.ndbSessionFreeList.pop();
  if(user_session) {
    user_session.isOpenNdbSession = true;
    stats.ndb_session_pool.hits++;
    user_callback(null, user_session);
  }
  else {
    stats.ndb_session_pool.misses++;
    user_session = new ndbsession.DBSession(this);
    user_session.isOpenNdbSession = true;
    user_session.fetchImpl(user_callback);
  }
};


/*
 *  Implementation of listTables() and getTableMetadata() 
 *
 *    A single Ndb can perform one metadata lookup at a time.
 *    An NdbSession object owns a dictionary lock and a queue of dictionary calls.
 *    If we can get the lock, we run a call immediately; if not, place it on the queue.
 *    
 *    It often happens in a Batch context that a bunch of operations all need
 *    the same metadata.  Common/DictionaryCall.js takes care of running the 
 *    actual call only once for each batch.
 */

function runListTables(arg, callback) {
  adapter.ndb.impl.DBDictionary.listTables(arg.impl, arg.database, callback);
}

/** List all tables in the schema
  * ASYNC
  * 
  * listTables(databaseName, dbSession, callback(error, array));
  */
DBConnectionPool.prototype.listTables = function(databaseName, dictSession, 
                                                 user_callback) {
  var arg, key;
  assert(databaseName && dictSession && user_callback);
  arg = { "impl"     : dictSession.impl,
          "database" : databaseName
        };
  key = "listTables:" + databaseName;
  stats.list_tables++;
  if(this.dictionaryCalls.add(key, user_callback)) {
    this.dictionaryCalls.queueExecCall(dictSession.execQueue,
                                       runListTables, arg,
                                       this.dictionaryCalls.makeGroupCallback(key));
  }
};


function runGetTable(arg, callback) {

  function onTableMetadata(err, tableMetadata) {
    if (err) {
      callback(err);
    } else {
    // add the callback handling to tableMetadata
      tableMetadata.invalidateCallbacks = [];
      tableMetadata.registerInvalidateCallback = function(cb) {
        tableMetadata.invalidateCallbacks.push(cb);
      };
      tableMetadata.invalidate = function() {
        tableMetadata.invalidateCallbacks.forEach(function (cb) {
          cb(tableMetadata);
        });
        tableMetadata.invalidateCallbacks = [];
      };
      callback(err, tableMetadata);
    }
  }

  // runGetTable starts here
  adapter.ndb.impl.DBDictionary.getTable(arg.impl, arg.dbName,
                                         arg.tableName, onTableMetadata);
}

DBConnectionPool.prototype.makeMasterCallback = function(key) {
  var ndbConnectionPool, groupCallback;

  ndbConnectionPool = this;
  groupCallback = this.dictionaryCalls.makeGroupCallback(key);

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
    udebug.log_detail("drColumn:", c);
  }

  return function(err, table) {
    var error;
    if(err) {
      error = {
        sqlstate : "42S02",
        message  : "Table " + key + " not found in NDB data dictionary",
        cause    : err
      };
    } else {
      autoincrement.getCacheForTable(table);  // get AutoIncrementCache
      table.columns.forEach(drColumn);
      ndbConnectionPool.openTables.push(table);
    }
    /* Finally dispatch the group callbacks */
    groupCallback(error, table);
  };
};


/** Fetch metadata for a table
  * ASYNC
  * 
  * getTableMetadata(databaseName, tableName, dbSession, callback(error, TableMetadata));
  */
DBConnectionPool.prototype.getTableMetadata = function(dbname, tabname, 
                                                       dictSession, user_callback) {
  var key, arg;
  assert(dbname && tabname && dictSession && user_callback);
  stats.get_table_metadata++;
  key = dbname + "." + tabname;
  arg = { "impl"      : dictSession.impl,
          "dbName"    : dbname,
          "tableName" : tabname
        };
  if(this.dictionaryCalls.add(key, user_callback)) {
    this.dictionaryCalls.queueExecCall(dictSession.execQueue,
                                       runGetTable, arg,
                                       this.makeMasterCallback(key));
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


DBConnectionPool.prototype.createTable = function(tableMapping,
                                                  session,
                                                  userCallback) {
  if(! tableMapping.database) {
    tableMapping.database = this.properties.database;
  }
  var sql = sqlBuilder.getSqlForTableCreation(tableMapping, "ndb");

  stats.create_table++;
  this.metadataManager.execDDL(sql, userCallback);
};


DBConnectionPool.prototype.dropTable = function(dbName,
                                                tableName,
                                                session,
                                                userCallback) {
  this.metadataManager.execDDL("DROP TABLE IF EXISTS " + dbName + "." + tableName,
                               userCallback);
};

exports.DBConnectionPool = DBConnectionPool; 

