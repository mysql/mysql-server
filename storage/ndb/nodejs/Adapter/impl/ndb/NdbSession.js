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

var adapter        = require(path.join(build_dir, "ndb_adapter.node")),
    ndboperation   = require("./NdbOperation.js"),
    dbtxhandler    = require("./NdbTransactionHandler.js"),
    ndbconnection  = require("./NdbConnectionPool.js"),
    util           = require("util"),
    assert         = require("assert"),
    udebug         = unified_debug.getLogger("NdbSession.js"),
    stats          = require(path.join(api_dir,"stats.js")).getWriter(["spi","ndb","DBSession"]),
    NdbSession;


/** 
  A session has a single transaction visible to the user at any time: 
  NdbSession.tx, which is created in NdbSession.getTransactionHandler() 
  and persists until the user performs an execute commit or rollback, 
  at which point NdbSession.tx is reset to null.  The previous 
  TransactionHandler is still alive at this point, but it no longer 
  represents the session's current transaction. 

  THREE QUEUES
  ------------
  1. An Ndb object is "single-threaded".  All calls on the session's single Ndb 
     object are serialized in NdbSession.execQueue.
  2. All execute calls on an NdbTransaction must wait for startTransaction() 
     to return.  They are placed on NdbTransactionHandler.execAfterOpenQueue
     until startTransaction() has returned.
  3. All ndb.startTransaction() calls must wait on NdbSession.startTxQueue
     for some NdbTransaction to close, if more than N NdbTransactions are open.
     N is an argument to the Ndb() constructor and defaults to 4.
     However, scans count as two transactions because they require 
     two API Connect Records.
*/


/* newDBSession(sessionImpl) 
   Called from NdbConnectionPool.js to create a DBSession object
*/
exports.newDBSession = function(pool, impl) {
  udebug.log("newDBSession(connectionPool, sessionImpl)");
  var dbSess = new NdbSession();
  dbSess.parentPool = pool;
  dbSess.impl = impl;
  return dbSess;
};

/* DBSession Simple Constructor
*/
NdbSession = function() { 
  stats.incr("created");
  this.tx                  = null;
  this.execQueue           = [];
  this.startTxQueue        = [];
  this.maxNdbTransactions  = 4;  // do not set less than two
  this.openNdbTransactions = 0;
};

/* NdbSession prototype 
*/
NdbSession.prototype = {
  impl                : null,
  parentPool          : null,
};


/*** Functions exported by this module but not in the public DBSession SPI ***/

/* Reset the session's current transaction.
   NdbTransactionHandler calls this immediately at execute(COMMIT | ROLLBACK).
   The closed NdbTransactionHandler is still alive and running, 
   but the session can now open a new one.
*/
exports.closeActiveTransaction = function(dbTransactionHandler) {
  var self = dbTransactionHandler.dbSession;
  assert(self.tx === dbTransactionHandler);
  self.tx = null;  
};


/* Execute a StartTransaction call, or queue it if necessary
*/
exports.queueStartNdbTransaction = function(dbTransactionHandler, startTxCall) {
  var self = dbTransactionHandler.dbSession;
  var nTx = startTxCall.nTxRecords;
  
  if(self.openNdbTransactions + nTx <= self.maxNdbTransactions) {
    self.openNdbTransactions += nTx;
    udebug.log("startTransaction => exec queue");
    startTxCall.enqueue();           // go directly to the exec queue
  }
  else {
    self.startTxQueue.push(startTxCall);   // wait in the startTx queue
    udebug.log("startTransaction => start queue", self.startTxQueue.length);
  }
};


/* Close an NdbTransaction. 
*/
exports.closeNdbTransaction = function(dbTransactionHandler, nTx) {
  var self, nextTx;
  self = dbTransactionHandler.dbSession;
  udebug.log("closeNdbTransaction", nTx, self.openNdbTransactions);
  self.openNdbTransactions -= nTx;
  assert(self.openNdbTransactions >= 0);
  while(self.startTxQueue.length > 0 && 
        self.openNdbTransactions + self.startTxQueue[0].nTxRecords <= self.maxNdbTransactions) {
    /* move a waiting StartTxCall from the startTxQueue to the execQueue */
    nextTx = self.startTxQueue.shift();
    self.openNdbTransactions += nextTx.nTxRecords;
    udebug.log("closeNdbTransaction: pulled 1 from startTxQueue. Length:", self.startTxQueue.length);
    nextTx.enqueue(); 
  }
};


/*** DBSession SPI Prototype Methods ***/


/*  getConnectionPool() 
    IMMEDIATE
    RETURNS the DBConnectionPool from which this DBSession was created.
*/
NdbSession.prototype.getConnectionPool = function() {
  udebug.log("getConnectionPool");
  return this.parentPool;
};


/* close() 
   ASYNC. Optional callback.
*/
NdbSession.prototype.close = function(userCallback) {
  var callback;
  function defaultCallback() { }
  callback = typeof userCallback === 'function' ? userCallback : defaultCallback;

  ndbconnection.closeNdbSession(this.parentPool, this, callback);
};


/* buildReadOperation(DBIndexHandler dbIndexHandler, 
                      Object keys,
                      DBTransactionHandler transaction,
                      function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will fetch a row.

   RETURNS a DBOperation 
*/
NdbSession.prototype.buildReadOperation = function(dbIndexHandler, keys,
                                                   tx, callback) {
  udebug.log("buildReadOperation");
  var lockMode = "SHARED";
  var op = ndboperation.newReadOperation(tx, dbIndexHandler, keys, lockMode);
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
  udebug.log("buildInsertOperation " + tableHandler.dbTable.name);
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
  udebug.log("buildWriteOperation");
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
  udebug.log("buildUpdateOperation");
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
  udebug.log("buildDeleteOperation");  
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


/* getTransactionHandler() 
   IMMEDIATE
   
   RETURNS the current transaction handler, creating it if necessary
*/
NdbSession.prototype.getTransactionHandler = function() {
  if(this.tx) {
    udebug.log("getTransactionHandler -- return existing");
  }
  else {
    udebug.log("getTransactionHandler -- return new");
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

