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

/*global assert, unified_debug, path, build_dir, api_dir, spi_doc_dir  */

"use strict";

var adapter         = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    ndbsession      = require("./NdbSession.js"),
    ndboperation    = require("./NdbOperation.js"),
    doc             = require(path.join(spi_doc_dir, "DBTransactionHandler")),
    stats_module    = require(path.join(api_dir,"stats.js")),
    stats           = stats_module.getWriter("spi","ndb","DBTransactionHandler"),
    udebug          = unified_debug.getLogger("NdbTransactionHandler.js"),
    proto           = doc.DBTransactionHandler,
    COMMIT          = adapter.ndbapi.Commit,
    NOCOMMIT        = adapter.ndbapi.NoCommit,
    ROLLBACK        = adapter.ndbapi.Rollback,
    AO_ABORT        = adapter.ndbapi.AbortOnError,
    AO_IGNORE       = adapter.ndbapi.AO_IgnoreError,
    AO_DEFAULT      = adapter.ndbapi.DefaultAbortOption,
    serial          = 1;

function DBTransactionHandler(dbsession) {
  this.dbSession          = dbsession;
  this.autocommit         = true;
  this.ndbtx              = null;
  this.pendingOperations  = [];
  this.executedOperations = [];
  this.pendingApiCalls    = [];
  this.asyncContext       = dbsession.parentPool.asyncNdbContext;
  this.canUseNdbAsynch    = dbsession.parentPool.properties.use_ndb_async_api;
  this.serial             = serial++;
  udebug.log("NEW (",this.serial,")");
  stats.incr("created");  
}
DBTransactionHandler.prototype = proto;


function onAsyncSent(a,b) {
  udebug.log("execute onAsyncSent");
}

function ApiCall(execMode, abortFlag, forceSend, callback) {
  this.execMode   = execMode;
  this.abortFlag  = abortFlag;
  this.forceSend  = forceSend;
  this.callback   = callback;
}


/* NDB Execute.
   "Sync" execute is an async operation for the JavaScript user,
    but the uv worker thread uses synchronous NDBAPI execute().
    In "Async" execute, the uv worker thread uses executeAsynch(),
    and the DBConnectionPool listener thread runs callbacks.
*/
ApiCall.prototype.run = function(tx) {
  if(tx.canUseNdbAsynch) {
    stats.incr("run","async");
    tx.asyncContext.executeAsynch(tx.ndbtx, 
                                  this.execMode, this.abortFlag,
                                  this.forceSend, this.callback, 
                                  onAsyncSent);
  }
  else {
    stats.incr("run","async");
    tx.ndbtx.execute(this.execMode, this.abortFlag, this.forceSend, this.callback);
  }
};

/* NdbTransactionHandler internal run():
   Send an execute call to NDB, or queue it if there is already one running.
*/
function run(self, execMode, abortFlag, forceSend, callback) {

  /* Take the user's callback, and wrap it in a function that checks the tail
     of the pendingApiCalls queue
  */
  function makeCallback(f) {
    function wrappedCallback(a, b) {
      self.pendingApiCalls.shift();  // Our own ApiCall
      var next = self.pendingApiCalls.shift();   // The next ApiCall
      if(next) {
        udebug.log("Run from queue (", self.serial, ")");
        next.run(self);
      }
      f(a, b);   // The user's callback function
    }
    return wrappedCallback;
  }

  /* run() starts here */
  var apiCall = new ApiCall(execMode, abortFlag, forceSend, makeCallback(callback));
  self.pendingApiCalls.push(apiCall);
  if(self.pendingApiCalls.length === 1) {
    udebug.log("Run immediate");
    self.pendingApiCalls[0].run(self);
  }
  else {
    udebug.log("Run deferred");
  }
}

/* Error handling after NdbTransaction.execute() 
*/
function attachErrorToTransaction(dbTxHandler, err) {
  if(err) {
    dbTxHandler.ndb_error = err.ndb_error;
    dbTxHandler.success = false;
    dbTxHandler.error = new ndboperation.DBOperationError(err.ndb_error);
  }
  else {
    dbTxHandler.success = true;
  }
}


/* Internal execute()
*/ 
function execute(self, execMode, abortFlag, dbOperationList, callback) {

  function onCompleteTx(err, result) {
    udebug.log("execute onCompleteTx", err);

    attachErrorToTransaction(self, err);
    
    /* Update our own success and error objects */
    /* If we just executed with Commit or Rollback, close the NdbTransaction */
    if(execMode === COMMIT || execMode === ROLLBACK) {
      self.ndbtx.close();
      ndbsession.txIsClosed(self);
    }

    /* NdbSession may have queued transactions waiting to execute;
       send the next one on its way */
    ndbsession.runQueuedTransaction(self);

    /* Attach results to their operations */
    ndboperation.completeExecutedOps(self);
    udebug.log("Back in execute onCompleteTx");

    /* Next callback */
    if(typeof callback === 'function') callback(err, self);
  }

  function prepareOperationsAndExecute() {
    udebug.log("execute prepareOperationsAndExecute");
    var i, op, fatalError, forceSend;
    for(i = 0 ; i < dbOperationList.length; i++) {
      op = dbOperationList[i];
      op.prepare(self.ndbtx);
      if(op.ndbop) {
        self.pendingOperations.push(dbOperationList[i]);
      }
      else {
        fatalError = self.ndbtx.getNdbError();
        callback(new ndboperation.DBOperationError(fatalError), self);
        return;
      }
    }

    /* forceSend flag:
       Single operations are sent forceSend = 1 (optimized for latency).
       Batches are sent with forceSend = 0 (optimized for throughput).
    */
    forceSend = (dbOperationList.length === 1 ? 1 : 0);
    
    run(self, execMode, abortFlag, forceSend, onCompleteTx);
  }

  function onStartTx(err, ndbtx) {
    if(err) {
      ndbsession.txIsClosed(self);
      udebug.log("execute onStartTx [ERROR].", err);
      if(callback) {
        err = new ndboperation.DBOperationError(err.ndb_error);
        callback(err, self);
      }
      return;
    }

    self.ndbtx = ndbtx;
    udebug.log("execute onStartTx. (", self.serial, 
               ") TC node:", ndbtx.getConnectedNodeId(),
               "operations:",  dbOperationList.length);
    prepareOperationsAndExecute();    
  }

  /* execute() starts here */
  udebug.log("Internal execute (", self.serial, ")" );
  var table = dbOperationList[0].tableHandler.dbTable;

  if(self.executedOperations.length) {  // Transaction has already been started
    assert(self.ndbtx);
    prepareOperationsAndExecute();
  }
  else {
    if(ndbsession.txCanRunImmediately(self)) {
      // TODO: partitionKey
      stats.incr("start","immediate");
      var ndb = adapter.impl.DBSession.getNdb(self.dbSession.impl);
      ndbsession.txIsOpen(self);
      ndb.startTransaction(table, 0, 0, onStartTx); 
    }
    else {          // We cannot get an NdbTransaction right now; queue one
      stats.incr("start","queued");
      ndbsession.enqueueTransaction(self, dbOperationList, callback);
    }
  }
}


/* execute(DBOperation[] dbOperationList,
           function(error, DBTransactionHandler) callback)
   ASYNC
   
   Executes the DBOperations in dbOperationList.
   Commits the transaction if autocommit is true.
*/
proto.execute = function(dbOperationList, userCallback) {

  function onExecCommit(err, dbTxHandler) {
    udebug.log("execute onExecCommit");

    if(userCallback) {
      userCallback(err, dbTxHandler);
    }
  }

  if(! dbOperationList.length) {
    udebug.log("Execute -- STUB EXECUTE (no operation list)");
    userCallback(null, this);
    return;
  }
  
  if(this.autocommit) {
    udebug.log("Execute -- AutoCommit");
    stats.incr("execute","commit");
    ndbsession.closeActiveTransaction(this);
    execute(this, COMMIT, AO_IGNORE, dbOperationList, onExecCommit);
  }
  else {
    udebug.log("Execute -- NoCommit");
    stats.incr("execute","no_commit");
    execute(this, NOCOMMIT, AO_IGNORE, dbOperationList, userCallback);
  }
};


/* Common code to commit and rollback 
*/
function onCommitOrRollback(userCallback, dbTxHandler, err) {
  attachErrorToTransaction(dbTxHandler, err);

  if(dbTxHandler.ndbtx) {    
    dbTxHandler.ndbtx.close();     
  }
  
  /* Register the transaction as closed 
  */
  ndbsession.txIsClosed(dbTxHandler);

  /* NdbSession may have queued transactions waiting to execute;
     send the next one on its way */
  ndbsession.runQueuedTransaction(dbTxHandler);

  /* Attach results to their operations */
  ndboperation.completeExecutedOps(dbTxHandler);

  /* Next callback */
 if(typeof userCallback === 'function') userCallback(err, dbTxHandler);
}


/* commit(function(error, DBTransactionHandler) callback)
   ASYNC 
   
   Commit work.
*/
proto.commit = function commit(userCallback) {
  assert(this.autocommit === false);
  stats.incr("commit");
  var self = this;

  function onNdbCommit(err, result) {
    udebug.log("commit onNdbCommit");
    onCommitOrRollback(userCallback, self, err);
  }

  /* commit begins here */
  udebug.log("commit");
  ndbsession.closeActiveTransaction(this);
  if(self.ndbtx) {  
    run(self, COMMIT, AO_IGNORE, 0, onNdbCommit);
  }
  else {
    udebug.log("commit STUB COMMIT (no underlying NdbTransaction)");
    onNdbCommit();
  }
};


/* rollback(function(error, DBTransactionHandler) callback)
   ASYNC 
   
   Roll back all previously executed operations.
*/
proto.rollback = function rollback(callback) {
  assert(this.autocommit === false);
  stats.incr("rollback");
  var self = this;

  ndbsession.closeActiveTransaction(this);

  function onNdbRollback(err, result) {
    udebug.log("rollback onNdbRollback");
    onCommitOrRollback(callback, self, err);
  }

  /* rollback begins here */
  udebug.log("rollback");

  if(self.ndbtx) {
    run(self, ROLLBACK, AO_DEFAULT, 0, onNdbRollback);
  }
  else {
    udebug.log("rollback STUB ROLLBACK (no underlying NdbTransaction)");
    onNdbRollback();
  }
};


DBTransactionHandler.prototype = proto;
exports.DBTransactionHandler = DBTransactionHandler;

