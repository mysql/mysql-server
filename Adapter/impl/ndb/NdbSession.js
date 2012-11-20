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

/*global unified_debug, path, build_dir */

"use strict";

var adapter        = require(path.join(build_dir, "ndb_adapter.node")),
    ndboperation   = require("./NdbOperation.js"),
    dbtxhandler    = require("./NdbTransactionHandler.js"),
    util           = require("util"),
    assert         = require("assert"),
    udebug         = unified_debug.getLogger("NdbSession.js"),
    NdbSession;

/*** Methods exported by this module but not in the public DBSession SPI ***/


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


/* getNdb(DBSession) 
*/
exports.getNdb = function(dbsession) {
  udebug.log("getNdb(DBSession)");
  return dbsession.impl.getNdb();
};


/* txDidCommit() 
*/
exports.txDidCommit = function(self, ndbTxHandler) {
  self.tx = null;
};


/* DBSession Simple Constructor
*/
NdbSession = function() { 
  udebug.log("constructor");
};

/* NdbSession prototype 
*/
NdbSession.prototype = {
  impl           : null,
  tx             : null,
  parentPool     : null,
};

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
  udebug.log("close");
  if(this.impl) {
    adapter.ndb.impl.DBSession.destroy(this.impl);
  }
  if(userCallback) {
    userCallback(null, null);
  }
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


/* getTransactionHandler() 
   IMMEDIATE
   
   RETURNS the current transaction handler, creating it if necessary
*/
NdbSession.prototype.getTransactionHandler = function() {
  udebug.log("getTransactionHandler");
  if(this.tx && this.tx.open) {
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
  assert(tx.state === "DEFINED");
  tx.autocommit = false;
};


/* commit(callback) 
   ASYNC
   
   Commit a user transaction.
   Callback is optional; if supplied, will receive (err).
*/
NdbSession.prototype.commit = function(userCallback) {
  assert(this.tx.autocommit === false);
  var self = this;
  
  function NdbSessionCommitCallback(a, b) {
    udebug.log("NdbSessionCommitCallback");
    self.tx = null;
    if(userCallback) { userCallback(a, b); }
  }
  this.tx.commit(NdbSessionCommitCallback);
};


/* rollback(callback) 
   ASYNC
   
   Roll back a user transaction.
   Callback is optional; if supplied, will receive (err).
*/
NdbSession.prototype.rollback = function (userCallback) {
  assert(this.tx.autocommit === false);
  var self = this;

  function NdbSessionRollbackCallback(a, b) {
    udebug.log("NdbSessionRollbackCallback");
    self.tx = null;
    if(userCallback) { userCallback(a, b); }
  }
  this.tx.rollback(NdbSessionRollbackCallback);
};

