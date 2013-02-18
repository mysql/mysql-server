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
    AO_ABORT        = adapter.ndbapi.AbortOnError,
    AO_IGNORE       = adapter.ndbapi.AO_IgnoreError,
    AO_DEFAULT      = adapter.ndbapi.DefaultAbortOption;
    

function DBTransactionHandler(dbsession) {
  udebug.log("constructor");
  stats.incr("created");
  this.dbSession          = dbsession;
  this.autocommit         = true;
  this.ndbtx              = null;
  this.state              = doc.DBTransactionStates[0];  // DEFINED
  this.executedOperations = [];
  this.canUseNdbAsynch    = false;
}
DBTransactionHandler.prototype = proto;


/* Internal execute()
*/ 
function execute(self, execMode, abortFlag, dbOperationList, callback) {

  function onCompleteTx(err, result) {
    udebug.log("execute onCompleteTx", err);
    
    /* Update our own success and error objects */
    if(err) {
      self.success = false;
      self.error = new ndboperation.DBOperationError(err.ndb_error);
    }
    else {
      self.success = true;
    }     
    /* If we just executed with Commit or Rollback, close the NdbTransaction */
    if(execMode === adapter.ndbapi.Commit || execMode === adapter.ndbapi.Rollback) {
      self.ndbtx.close();
      ndbsession.txIsClosed(self);
    }

    /* NdbSession may have queued transactions waiting to execute;
       send the next one on its way */
    ndbsession.runQueuedTransaction(self);

    /* Attach results to their operations */
    ndboperation.completeExecutedOps(err, self.executedOperations);
    udebug.log("BACK IN execute onCompleteTx AFTER COMPLETED OPS");

    /* Next callback */
    if(callback) {
      callback(self.error, self);
    }
  }

  function prepareOperationsAndExecute() {
    udebug.log("execute prepareOperationsAndExecute");
    var i, op, fatalError, forceSend;
    for(i = 0 ; i < dbOperationList.length; i++) {
      op = dbOperationList[i];
      op.prepare(self.ndbtx);
      if(op.ndbop) {
        self.executedOperations.push(dbOperationList[i]);
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
    forceSend = (dbOperationList.length == 1 ? 1 : 0);
     

    /* NDB Execute.
       "Sync" execute is an async operation for the JavaScript user,
        but the uv worker thread uses syncrhonous NDBAPI execute().
        In "Async" execute, the uv worker thread uses executeAsynch(),
        and the DBConnectionPool listener thread runs callbacks.
    */
    if(self.canUseNdbAsynch) {
      self.ndbtx.executeAsynch(execMode, abortFlag, forceSend, onCompleteTx);
    }
    else {
      self.ndbtx.execute(execMode, abortFlag, forceSend, onCompleteTx);
    }
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
    self.state = doc.DBTransactionStates[1]; // STARTED
    udebug.log("execute onStartTx. TC node:", ndbtx.getConnectedNodeId(),
               "operations:",  dbOperationList.length);
    prepareOperationsAndExecute();    
  }

  /* execute() starts here */
  udebug.log("Internal execute");
  var table = dbOperationList[0].tableHandler.dbTable;

  if(self.state === "DEFINED") {
    if(ndbsession.txCanRunImmediately(self)) {
      // TODO: partitionKey
      // TODO: stat counter
      var ndb = adapter.impl.DBSession.getNdb(self.dbSession.impl);
      ndbsession.txIsOpen(self);
      ndb.startTransaction(table, 0, 0, onStartTx); 
    }
    else {          // We cannot get an NdbTransaction right now; queue one
      // TODO: stat counter
      ndbsession.enqueueTransaction(self, dbOperationList, callback);
    }
  }
  else {  /* Transaction has already been started */
    assert(self.ndbtx);
    prepareOperationsAndExecute();    
  }
}


/* execute(DBOperation[] dbOperationList,
           function(error, DBTransactionHandler) callback)
   ASYNC
   
   Executes the DBOperations in dbOperationList.
   Commits the transaction if autocommit is true.
*/
proto.execute = function(dbOperationList, userCallback) {
  udebug.log("execute");

  function onExecCommit(err, dbTxHandler) {
    udebug.log("execute onExecCommit");

    dbTxHandler.state = doc.DBTransactionStates[2]; // COMMITTED

    if(userCallback) {
      userCallback(err, dbTxHandler);
    }
  }

  if(! dbOperationList.length) {
    udebug.log("execute STUB EXECUTE (no operation list)");
    userCallback(null, this);
    return;
  }
  
  if(this.autocommit) {
    udebug.log(" -- AutoCommit");
    stats.incr("execute","commit");
    ndbsession.closeActiveTransaction(this);
    execute(this, COMMIT, AO_IGNORE, dbOperationList, onExecCommit);
  }
  else {
    udebug.log(" -- NoCommit");
    stats.incr("execute","no_commit");
    execute(this, NOCOMMIT, AO_IGNORE, dbOperationList, userCallback);
  }
};

/* Compatibility */
proto.executeCommit = proto.execute;
proto.executeNoCommit = proto.execute;


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

    if(self.ndbtx) {
      self.ndbtx.close();
    }

    self.error = err;
    self.success = err ? false : true;    
    self.state = doc.DBTransactionStates[2]; // COMMITTED
    ndbsession.txIsClosed(self);

    /* NdbSession may have queued transactions waiting to execute;
       send the next one on its way */
    ndbsession.runQueuedTransaction(self);

    /* Attach results to their operations */
    ndboperation.completeExecutedOps(err, self.executedOperations);

    /* Next callback */
    userCallback(err, self);
  }

  /* commit begins here */
  udebug.log("commit");
  ndbsession.closeActiveTransaction(this);
  if(self.ndbtx) {  
    self.ndbtx.execute(adapter.ndbapi.Commit, AO_IGNORE, 0, onNdbCommit);
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
    
    if(self.ndbtx) {
      self.ndbtx.close();
    }
    
    if(err) {
      self.success = false;
      err = new ndboperation.DBOperationError(err.ndb_error);
    }
    else {
      self.success = true;
    }
    self.state = doc.DBTransactionStates[3]; // ROLLEDBACK
    ndbsession.txIsClosed(self);

    /* NdbSession may have queued transactions waiting to execute;
       send the next one on its way */
    ndbsession.runQueuedTransaction(self);

    // TODO: Should the operation callbacks run, and if so, with what arguments?
    
    /* Next callback */
    callback(err, self);
  }

  /* rollback begins here */
  udebug.log("rollback");
  ndbsession.closeActiveTransaction(this);

  if(self.ndbtx) {
    self.ndbtx.execute(adapter.ndbapi.Rollback, AO_DEFAULT, 0, onNdbRollback);
  }
  else {
    udebug.log("rollback STUB ROLLBACK (no underlying NdbTransaction)");
    onNdbRollback();
  }
};


DBTransactionHandler.prototype = proto;
exports.DBTransactionHandler = DBTransactionHandler;

