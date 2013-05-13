/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

var adapter         = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    ndbsession      = require("./NdbSession.js"),
    ndboperation    = require("./NdbOperation.js"),
    doc             = require(path.join(spi_doc_dir, "DBTransactionHandler")),
    stats_module    = require(path.join(api_dir,"stats.js")),
    stats           = stats_module.getWriter(["spi","ndb","DBTransactionHandler"]),
    udebug          = unified_debug.getLogger("NdbTransactionHandler.js"),
    QueuedAsyncCall = require("../common/QueuedAsyncCall.js").QueuedAsyncCall,
    AutoIncHandler  = require("./NdbAutoIncrement.js").AutoIncHandler,
    proto           = doc.DBTransactionHandler,
    COMMIT          = adapter.ndbapi.Commit,
    NOCOMMIT        = adapter.ndbapi.NoCommit,
    ROLLBACK        = adapter.ndbapi.Rollback,
    AO_ABORT        = adapter.ndbapi.AbortOnError,
    AO_IGNORE       = adapter.ndbapi.AO_IgnoreError,
    AO_DEFAULT      = adapter.ndbapi.DefaultAbortOption,
    modeNames       = [],
    serial          = 1;

modeNames[COMMIT] = 'commit';
modeNames[NOCOMMIT] = 'noCommit';
modeNames[ROLLBACK] = 'rollback';

function DBTransactionHandler(dbsession) {
  this.dbSession          = dbsession;
  this.autocommit         = true;
  this.ndbtx              = null;
  this.sentNdbStartTx     = false;
  this.execCount          = 0;   // number of execute calls 
  this.nTxRecords         = 1;   // 1 for non-scan, 2 for scan
  this.pendingOpsLists    = [];  // [ execCallNumber : dbOperationList, ... ]
  this.executedOperations = [];  // All finished operations 
  this.execAfterOpenQueue = [];  // exec calls waiting on startTransaction()
  this.asyncContext       = dbsession.parentPool.asyncNdbContext;
  this.canUseNdbAsynch    = dbsession.parentPool.properties.use_ndb_async_api;
  this.serial             = serial++;
  this.moniker            = "(" + this.serial + ")";
  udebug.log("NEW ", this.moniker);
  stats.incr("created");
}
DBTransactionHandler.prototype = proto;

function onAsyncSent(a,b) {
  // udebug.log("execute onAsyncSent");
}

/* NdbTransactionHandler internal run():
   Create a QueuedAsyncCall on the Ndb's execQueue.
*/
function run(self, execMode, abortFlag, callback) {
  var qpos;
  var apiCall = new QueuedAsyncCall(self.dbSession.execQueue, callback, null);
  apiCall.tx = self;
  apiCall.execMode = execMode;
  apiCall.abortFlag = abortFlag;
  apiCall.description = "execute_" + modeNames[execMode];
  apiCall.run = function runExecCall() {
    /* NDB Execute.
       "Sync" execute is an async operation for the JavaScript user,
        but the uv worker thread uses synchronous NDBAPI execute().
        In "Async" execute, the uv worker thread uses executeAsynch(),
        and the DBConnectionPool listener thread runs callbacks.
    */
    var force_send = 1;

    if(this.tx.canUseNdbAsynch) {
      stats.incr(["run","async"]);
      this.tx.asyncContext.executeAsynch(this.tx.ndbtx, 
                                         this.execMode, this.abortFlag,
                                         force_send, this.callback, 
                                         onAsyncSent);
    }
    else {
      stats.incr(["run","sync"]);
      this.tx.ndbtx.execute(this.execMode, this.abortFlag, force_send, this.callback);
    }
  };

  qpos = apiCall.enqueue();
  udebug.log("run()", self.moniker, "queue position:", qpos);
}

/* runExecAfterOpenQueue()
*/
function runExecAfterOpenQueue(dbTxHandler) {
  var queue = dbTxHandler.execAfterOpenQueue;
  var item = queue.shift();
  if(item) {
    udebug.log("runExecAfterOpenQueue - remaining", queue.length);
    dbTxHandler.execute(item.dbOperationList, item.callback);
  }  
}

/* Error handling after NdbTransaction.execute() 
*/
function attachErrorToTransaction(dbTxHandler, err) {
  if(err) {
    dbTxHandler.success = false;
    dbTxHandler.error = new ndboperation.DBOperationError(err.ndb_error);
    /* Special handling for duplicate value in unique index: */
    if(err.ndb_error.code === 893) {
      dbTxHandler.error.cause = dbTxHandler.error;
    }
  }
  else {
    dbTxHandler.success = true;
  }
}

/* EXECUTE PATH FOR KEY OPERATIONS
   -------------------------------
   Start NdbTransaction 
   Fetch needed auto-increment values
   Prepare each operation (synchronous)
   Execute the NdbTransaction
   Close the NdbTransaction
   Attach results to operations, and run operation callbacks
   Run the transaction callback
   
   EXECUTE PATH FOR SCAN OPERATIONS
   --------------------------------
   Start NdbTransaction 
   Prepare the NdbScanOperation (async)
   Execute NdbTransaction NoCommit
   Fetch results from scan
   Execute the NdbTransaction (commit or rollback)
   Close the NdbTransaction
   Attach results to query operation
   Run query operation callback
   Run the transaction callback
*/


/* Common callback for execute, commit, and rollback 
*/
function onExecute(dbTxHandler, execMode, err, execId, userCallback) {
  /* Update our own success and error objects */
  attachErrorToTransaction(dbTxHandler, err);
  udebug.log("onExecute", modeNames[execMode], dbTxHandler.moniker,
             "success:", dbTxHandler.success);
  
  /* If we just executed with Commit or Rollback, close the NdbTransaction 
     and register the DBTransactionHandler as closed with DBSession
  */
  if(execMode === COMMIT || execMode === ROLLBACK) {
    if(dbTxHandler.ndbtx) {       // May not exist on "stub" commit/rollback
      dbTxHandler.ndbtx.close();
      ndbsession.closeNdbTransaction(dbTxHandler, dbTxHandler.nTxRecords);
    }
  }

  /* send the next exec call on its way */
  runExecAfterOpenQueue(dbTxHandler);

  /* Attach results to their operations */
  ndboperation.completeExecutedOps(dbTxHandler, execMode, 
                                   dbTxHandler.pendingOpsLists[execId]);

  /* Next callback */
  if(typeof userCallback === 'function') {
    userCallback(dbTxHandler.error, dbTxHandler);
  }
}


function getExecIdForOperationList(self, operationList) {
  var execId = self.execCount++;
  self.pendingOpsLists[execId] = operationList;
  return execId;
}


/* NOTE: Until we have a Batch.createQuery() API, there will only ever be
   one scan in an operationList.  And there will never be key operations
   and scans combined in a single operationList.
*/

function executeScan(self, execMode, abortFlag, dbOperationList, callback) {
  var op = dbOperationList[0];

  /* Execute NdbTransaction after reading from scan */
  function executeNdbTransaction() {
    udebug.log("executeScan executeNdbTransaction");
    var execId = getExecIdForOperationList(self, dbOperationList);

    function onCompleteExec(err) {
      onExecute(self, execMode, err, execId, callback);
    }
    
    run(self, execMode, abortFlag, onCompleteExec);
  }

  /* Fetch is complete. */
  function onFetchComplete(err) {
    if(execMode == NOCOMMIT) {
      onExecute(self, execMode, err, execId, callback);
    }
    else {
      executeNdbTransaction();
    }
  }
  
  /* Fetch results */
  function getScanResults(err) {
    udebug.log("executeScan getScanResults");
    if(err) {
      onFetchComplete(err);
    }
    else {
      ndboperation.getScanResults(op, onFetchComplete);
    }
  }
  
  /* Execute NoCommit so that you can start reading from scans */
  function executeScanNoCommit(err, ndbop) {
    udebug.log("executeScan executeScanNoCommit");
    if(! ndbop) {
      fatalError = self.ndbtx.getNdbError();
      callback(new ndboperation.DBOperationError(fatalError), self);
      return;  /* is that correct? */
    }

    op.ndbop = ndbop;
    run(self, NOCOMMIT, AO_IGNORE, getScanResults);
  }

  /* executeScan() starts here */
  udebug.log("executeScan");
  op.prepareScan(self.ndbtx, executeScanNoCommit);
}


function executeNonScan(self, execMode, abortFlag, dbOperationList, callback) {

  function executeNdbTransaction() {
    var execId = getExecIdForOperationList(self, dbOperationList);

    function onCompleteExec(err) {
      onExecute(self, execMode, err, execId, callback);
    }
    
    run(self, execMode, abortFlag, onCompleteExec);
  }

  function prepareOperations() {
    udebug.log("executeNonScan prepareOperations", self.moniker);
    var i, op, fatalError;
    for(i = 0 ; i < dbOperationList.length; i++) {
      op = dbOperationList[i];
      op.prepare(self.ndbtx);
      if(! op.ndbop) {
        fatalError = self.ndbtx.getNdbError();
        callback(new ndboperation.DBOperationError(fatalError), self);
        return;  /* is that correct? */
      }
    }
    executeNdbTransaction();
  }

  function getAutoIncrementValues() {
    var autoIncHandler = new AutoIncHandler(dbOperationList);
    if(autoIncHandler.values_needed > 0) {
      udebug.log("executeNonScan getAutoIncrementValues", autoIncHandler.values_needed);
      autoIncHandler.getAllValues(prepareOperations);
    }
    else {
      prepareOperations();
    }  
  }

  getAutoIncrementValues();
}


/* Internal execute()
*/ 
function execute(self, execMode, abortFlag, dbOperationList, callback) {
  var startTxCall, queueItem;
  var isScan = dbOperationList[0].isScanOperation();
  self.nTxRecords = isScan ? 2 : 1;

  function executeSpecific() {
   if(isScan) {
      executeScan(self, execMode, abortFlag, dbOperationList, callback);
    }
    else {
      executeNonScan(self, execMode, abortFlag, dbOperationList, callback);
    }
  }

  function onStartTx(err, ndbtx) {
    if(err) {
      ndbsession.closeNdbTransaction(self, self.nTxRecords);
      udebug.log("execute onStartTx [ERROR].", err);
      if(callback) {
        err = new ndboperation.DBOperationError(err.ndb_error);
        callback(err, self);
      }
      return;
    }

    self.ndbtx = ndbtx;
    udebug.log("execute onStartTx. ", self.moniker, 
               " TC node:", ndbtx.getConnectedNodeId(),
               "operations:",  dbOperationList.length);
    executeSpecific();
  }

  if(self.ndbtx) {                   /* startTransaction has returned */
    executeSpecific();
  }
  else if(self.sentNdbStartTx) {     /* startTransaction has not yet returned */
    queueItem = { dbOperationList: dbOperationList, callback: callback };
    self.execAfterOpenQueue.push(queueItem);
  }
  else {                             /* call startTransaction */
    self.sentNdbStartTx = true;
    startTxCall = new QueuedAsyncCall(self.dbSession.execQueue, onStartTx);
    startTxCall.table = dbOperationList[0].tableHandler.dbTable;
    startTxCall.ndb = self.dbSession.impl;
    startTxCall.description = "startNdbTransaction";
    startTxCall.nTxRecords = self.nTxRecords;
    startTxCall.run = function() {
      // TODO: partitionKey
      this.ndb.startTransaction(this.table, 0, 0, this.callback);
    };
    
    ndbsession.queueStartNdbTransaction(self, startTxCall);
  }
}


/* execute(DBOperation[] dbOperationList,
           function(error, DBTransactionHandler) callback)
   ASYNC
   
   Executes the DBOperations in dbOperationList.
   Commits the transaction if autocommit is true.
*/
proto.execute = function(dbOperationList, userCallback) {

  if(! dbOperationList.length) {
    udebug.log("Execute -- STUB EXECUTE (no operation list)");
    userCallback(null, this);
    return;
  }
  
  if(this.autocommit) {
    udebug.log("Execute -- AutoCommit", this.moniker);
    stats.incr(["execute","commit"]);
    ndbsession.closeActiveTransaction(this);
    execute(this, COMMIT, AO_IGNORE, dbOperationList, userCallback);
  }
  else {
    udebug.log("Execute -- NoCommit", this.moniker);
    stats.incr(["execute","no_commit"]);
    execute(this, NOCOMMIT, AO_IGNORE, dbOperationList, userCallback);
  }
};


/* commit(function(error, DBTransactionHandler) callback)
   ASYNC 
   
   Commit work.
*/
proto.commit = function commit(userCallback) {
  assert(this.autocommit === false);
  stats.incr("commit");
  var self = this;
  var execId = getExecIdForOperationList(self, []);

  function onNdbCommit(err, execId) {
    onExecute(self, COMMIT, err, execId, userCallback);
  }

  /* commit begins here */
  udebug.log("commit");
  ndbsession.closeActiveTransaction(this);
  if(self.ndbtx) {  
    run(self, COMMIT, AO_IGNORE, onNdbCommit);
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
  var execId = getExecIdForOperationList(self, []);

  ndbsession.closeActiveTransaction(this);

  function onNdbRollback(err, execId) {
    onExecute(self, ROLLBACK, err, execId, callback);
  }

  /* rollback begins here */
  udebug.log("rollback");

  if(self.ndbtx) {
    run(self, ROLLBACK, AO_DEFAULT, onNdbRollback);
  }
  else {
    udebug.log("rollback STUB ROLLBACK (no underlying NdbTransaction)");
    onNdbRollback();
  }
};


DBTransactionHandler.prototype = proto;
exports.DBTransactionHandler = DBTransactionHandler;

