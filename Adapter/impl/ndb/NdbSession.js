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

/*global udebug, path, build_dir */

"use strict";

var adapter        = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    ndboperation   = require("./NdbOperation.js"),
    dbtxhandler    = require("./NdbTransactionHandler.js"),
    util           = require("util"),
    assert         = require("assert"),
    DBSession;

/*** Methods exported by this module but not in the public DBSession SPI ***/


/* getDBSession(sessionImpl) 
   Called from NdbConnectionPool.js to create a DBSession object
*/
exports.getDBSession = function(pool, impl) {
  udebug.log("ndbsession getDBSession(connectionPool, sessionImpl)");
  var dbSess = new DBSession();
  dbSess.parentPool = pool;
  dbSess.impl = impl;
  return dbSess;
};


/* getNdb(DBSession) 
*/
exports.getNdb = function(dbsession) {
  udebug.log("ndbsession getNdb(DBSession)");
  return dbsession.impl.getNdb();
};


/* DBSession Simple Constructor
*/
DBSession = function() { 
  udebug.log("DBSession constructor");
};

/* DBSession prototype 
*/
DBSession.prototype = {
  impl           : null,
  tx             : null,
  parentPool     : null,
};

/*  getConnectionPool() 
    IMMEDIATE
    RETURNS the DBConnectionPool from which this DBSession was created.
*/
DBSession.prototype.getConnectionPool = function() {
  udebug.log("DBSession getConnectionPool");
  return this.parentPool;
};


/* DBTransactionHandler createTransaction()
   IMMEDIATE

   RETURNS a DBTransactionHandler
*/
DBSession.prototype.createTransaction = function() {
  udebug.log("DBSession createTransaction");
  
  delete this.tx;  
  this.tx = new dbtxhandler.DBTransactionHandler(this);
  
  return this.tx;
};


/* buildReadOperation(DBTableHandler table, 
                      Object ResolvedKeys,
                      DBTransactionHandler transaction,
                      function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will fetch a row.
   The userCallback is stored in the DBOperation, but will not be called 
   by this layer.

   RETURNS a DBOperation 
*/
DBSession.prototype.buildReadOperation = function(table, keys,
                                                  tx, callback) {
  udebug.log("DBSession buildReadOperation "+ table.name);
  var lockMode = "SHARED";
  var op = ndboperation.newReadOperation(tx, table, keys, lockMode);
  op.userCallback = callback;
  return op;
};


/* buildInsertOperation(DBTableHandler table, Object row,
                        DBTransactionHandler transaction,
                        function(error, DBOperation) userCallback)
   IMMEDIATE
   Define an operation which when executed will insert a row.
   The userCallback is stored in the DBOperation, but will not be called 
   by this layer.
 
   RETURNS a DBOperation 
*/
DBSession.prototype.buildInsertOperation = function(tableHandler, row,
                                                    tx, callback) {
  udebug.log("DBSession buildInsertOperation " + tableHandler.dbTable.name);

  var op = ndboperation.newInsertOperation(tx, tableHandler, row);
  op.userCallback = callback;
  return op;
};


/* buildDeleteOperation(DBTableHandler table, Object primaryKey,
                        DBTransactionHandler transaction,
                        function(error, DBOperation) userCallback)
   IMMEDIATE 
   Define an operation which when executed will delete a row
   The userCallback is stored in the DBOperation, but will not be called 
   by this layer.
 
   RETURNS a DBOperation 
*/ 
DBSession.prototype.buildDeleteOperation = function(tableHandler, row,
                                                    tx, callback) {
  udebug.log("DBSession buildDeleteOperation " + tableHandler.dbTable.name);
  
  var op = ndboperation.newDeleteOperation(tx, tableHandler, row);
  op.userCallback = callback;
  return op;
};
