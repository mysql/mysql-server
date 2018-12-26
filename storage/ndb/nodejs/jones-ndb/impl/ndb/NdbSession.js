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
	"created" : 0,
  "seizeTransactionContext" : { 
    "immediate" : 0 , "queued" : 0 
  },
  "oneTableProjections" : 0
};

var conf            = require("./path_config"),
    adapter         = require(conf.binary),
    ndboperation    = require("./NdbOperation.js"),
    dbtxhandler     = require("./NdbTransactionHandler.js"),
    ndbconnpool     = require("./NdbConnectionPool.js"),
    util            = require("util"),
    assert          = require("assert"),
    jones           = require("database-jones"),
    unified_debug   = require("unified_debug"),
    udebug          = unified_debug.getLogger("NdbSession.js"),
    QueuedAsyncCall = require(jones.common.QueuedAsyncCall).QueuedAsyncCall,
    NdbSession;

require(jones.api.stats).register(stats, "spi","ndb","DBSession");

/** 
  A session has a single transaction visible to the user at any time: 
  NdbSession.tx, which is created in NdbSession.getTransactionHandler() 
  and persists until the user performs an execute commit or rollback, 
  at which point NdbSession.tx is "retired" and reset to null.  The previous 
  TransactionHandler is still alive at this point, but it no longer 
  represents the session's current transaction. 

  QUEUES
  ------
  1. An Ndb object is "single-threaded".  All execute calls on the session's
     SessionImpl are serialized in NdbSession.execQueue.  This is seen in
     run() in NdBTransactionHandler.
  2. seizeTransactionContext() calls must wait on NdbSession.seizeTxQueue
     for some transaction context to be released, if more than 
     ndb_session_concurrency contexts are open.
*/


/* DBSession Constructor. Undocumented - private to NdbConnectionPool.
*/
NdbSession = function(pool) {
  this.serial                =  stats.created++;
  this.parentPool            = pool;
  this.impl                  = null;
  this.tx                    = null;
  this.execQueue             = [];
  this.seizeTxQueue          = null;
  this.maxTxContexts         = pool.properties.ndb_session_concurrency;  
  this.openTxContexts        = 0;  // currently opened
  this.isOpenNdbSession      = false;
};

/* fetch SessionImpl. Undocumented - private to NdbConnectionPool. 
   ASYNC.
*/
NdbSession.prototype.fetchImpl = function(callback) {
  var self = this;
  var pool = this.parentPool;
  adapter.ndb.impl.DBSession.create(pool.impl, 
                                    pool.asyncNdbContext,
                                    pool.properties.database,
                                    pool.properties.ndb_session_concurrency,
                                    function(err, impl) {
    if(err) {
      callback(err, null);
    } else {
      self.impl = impl;
      callback(null, self);
    }
  });
};

/* Reset the session's current transaction.
   NdbTransactionHandler calls this immediately at execute(COMMIT | ROLLBACK).
   The closed NdbTransactionHandler is still alive and running, 
   but the session can now open a new one.
   Undocumented - private to NdbTransactionHandler.  IMMEDIATE.
*/
NdbSession.prototype.retireTransactionHandler = function() {
  this.tx = null;  
};

/* seizeTransactionContext().  Undocumented - private to NdbTransactionHandler.
   Takes callback; may be immediate or queued.
*/
NdbSession.prototype.seizeTransactionContext = function(callback) {
  var txContext;
  if(this.openTxContexts < this.maxTxContexts) {
    this.openTxContexts++;
    stats.seizeTransactionContext.immediate++;
    udebug.log_detail("seizeTransactionContext: immediate");
    txContext = this.impl.seizeTransaction();
    assert(txContext);
    callback(txContext);
  } else {
    if(this.seizeTxQueue === null) {
      this.seizeTxQueue = [];
    }
    this.seizeTxQueue.push(callback);
    stats.seizeTransactionContext.queued++;
    udebug.log("seizeTransactionContext: queued; queue length:", this.seizeTxQueue.length);
  }
};

/* releaseTransactionContext(). Undocumented - private to NdbTransactionHandler.
   IMMEDIATE.
*/
NdbSession.prototype.releaseTransactionContext = function(txContext) {
  var nextTxCallback, didRelease;

  didRelease = this.impl.releaseTransaction(txContext);
  this.openTxContexts--;
  assert(didRelease);   // false would mean that NdbTransaction was not closed.
  assert(this.openTxContexts >= 0);

  if(this.seizeTxQueue && this.seizeTxQueue.length) {
    nextTxCallback = this.seizeTxQueue.shift();
    txContext = this.impl.seizeTransaction();
    this.openTxContexts++;
    nextTxCallback(txContext);
  }
};


/*  getConnectionPool() 
    IMMEDIATE
    RETURNS the DBConnectionPool from which this DBSession was created.
*/
NdbSession.prototype.getConnectionPool = function() {
  return this.parentPool;
};


/* close() 
   ASYNC. Optional callback.
*/
NdbSession.prototype.close = function(callback) {
  udebug.log("Close.");
  ndbconnpool.closeNdbSession(this, callback);
};


/* buildReadOperation(DBIndexHandler dbIndexHandler, 
                      Object keys,
                      DBTransactionHandler transaction,
                      Bool loadResultIntoKeys,
                      function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will fetch a row.

   RETURNS a DBOperation 
*/
NdbSession.prototype.buildReadOperation = function(dbIndexHandler, keys,
                                                   tx, isLoad, callback) {
  if(udebug.is_debug()) {
    udebug.log("Read",
               dbIndexHandler.tableHandler.dbTable.name,
               "using", dbIndexHandler.dbIndex.name);
  }
  var lockMode = "SHARED";
  var op = ndboperation.newReadOperation(tx, dbIndexHandler, keys, lockMode, isLoad);
  op.userCallback = callback;
  return op;
};


/* buildInsertOperation(DBTableHandler tableHandler, 
                        Object row,
                        DBTransactionHandler transaction,
                        function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will insert a row.
 
   RETURNS a DBOperation 
*/
NdbSession.prototype.buildInsertOperation = function(tableHandler, row,
                                                    tx, callback) {
  assert.equal(typeof row, "object");
  if(udebug.is_debug()) {
    udebug.log("Insert into", tableHandler.dbTable.name);
  }
  var op = ndboperation.newInsertOperation(tx, tableHandler, row);
  op.userCallback = callback;
  return op;
};


/* buildWriteOperation(DBIndexHandler dbIndexHandler, 
                       Object row,
                       DBTransactionHandler transaction,
                       function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will update or insert
 
   RETURNS a DBOperation 
*/
NdbSession.prototype.buildWriteOperation = function(dbIndexHandler, row, 
                                                    tx, callback) {
  if(udebug.is_debug()) {
    udebug.log("Write to",
               dbIndexHandler.tableHandler.dbTable.name,
               "using", dbIndexHandler.dbIndex.name);
  }
  var op = ndboperation.newWriteOperation(tx, dbIndexHandler, row);
  op.userCallback = callback;
  return op;
};


/* buildUpdateOperation(DBIndexHandler dbIndexHandler,
                        Object keys, 
                        Object values,
                        DBTransactionHandler transaction,
                        function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will access a row using the keys
   object and update the values provided in the values object.
  
   RETURNS a DBOperation 
*/
NdbSession.prototype.buildUpdateOperation = function(dbIndexHandler, 
                                                     keys, row, tx, userData) {
  if(udebug.is_debug()) {
    udebug.log("Update",
               dbIndexHandler.tableHandler.dbTable.name,
               "using", dbIndexHandler.dbIndex.name);
  }
  var op = ndboperation.newUpdateOperation(tx, dbIndexHandler, keys, row);
  op.userCallback = userData;
  return op;
};


/* buildDeleteOperation(DBIndexHandler dbIndexHandler, 
                        Object keys,
                        DBTransactionHandler transaction,
                        function(error, DBOperation) userCallback)
   IMMEDIATE 
   Define an operation which when executed will delete a row
 
   RETURNS a DBOperation 
*/  
NdbSession.prototype.buildDeleteOperation = function(dbIndexHandler, keys,
                                                     tx, callback) {
  if(udebug.is_debug()) {
    udebug.log("Delete from",
               dbIndexHandler.tableHandler.dbTable.name,
               "using", dbIndexHandler.dbIndex.name);
  }
  var op = ndboperation.newDeleteOperation(tx, dbIndexHandler, keys);
  op.userCallback = callback;
  return op;
};

/* buildScanOperation(QueryHandler queryHandler,
                      Object properties,
                      DBTransactionHandler transaction,
                      function(error, result) userCallback)
   IMMEDIATE
*/
NdbSession.prototype.buildScanOperation = function(queryHandler, properties, 
                                                   tx, callback) {
  udebug.log("buildScanOperation");
  var op = ndboperation.newScanOperation(tx, queryHandler, properties);
  op.userCallback = callback;
  return op;
};

/* buildReadProjectionOperation
   IMMEDIATE
*/
NdbSession.prototype.buildReadProjectionOperation = function(dbIndexHandler,
                                            keys, projection, tx, callback) {
  /* If the "join" involves only one table, it is more efficient to run
     it as a ReadOperation */
  if(projection.sectors.length == 1) {
    stats.oneTableProjections++;
    return this.buildReadOperation(dbIndexHandler, keys, tx, false, callback);
  }

  if(udebug.is_debug()) {
    udebug.log("Projection Read from",
               dbIndexHandler.tableHandler.dbTable.name,
               "using", dbIndexHandler.dbIndex.name);
  }
  var op = ndboperation.newProjectionOperation(this.impl, tx, dbIndexHandler,
                                               keys, projection);
  op.userCallback = callback;
  return op;
};


/* getTransactionHandler() 
   IMMEDIATE
   
   RETURNS the current transaction handler, creating it if necessary
*/
NdbSession.prototype.getTransactionHandler = function() {
  if(! this.tx) {
    this.tx = new dbtxhandler.DBTransactionHandler(this);
  }
  return this.tx;
};


/* begin() 
   IMMEDIATE
   
   Begin a user transaction context; exit autocommit mode.
*/
NdbSession.prototype.begin = function() {
  var tx = this.getTransactionHandler();
  assert(tx.executedOperations.length === 0);
  tx.autocommit = false;
};


/* commit(callback) 
   ASYNC
   
   Commit a user transaction.
   Callback is optional; if supplied, will receive (err).
*/
NdbSession.prototype.commit = function(userCallback) {
  this.tx.commit(userCallback);
};


/* rollback(callback) 
   ASYNC
   
   Roll back a user transaction.
   Callback is optional; if supplied, will receive (err).
*/
NdbSession.prototype.rollback = function (userCallback) {
  this.tx.rollback(userCallback);
};


exports.DBSession = NdbSession;
