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

var stats = {
	"created"		: 0,
	"run_async" : 0,
	"run_sync"  : 0,
	"execute"   : { "commit": 0, "no_commit" : 0}, 
	"commit"    : 0,
	"rollback"  : 0
};

var adapter         = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    ndboperation    = require("./NdbOperation.js"),
    doc             = require(path.join(spi_doc_dir, "DBTransactionHandler")),
    stats_module    = require(path.join(api_dir,"stats.js")),
    udebug          = unified_debug.getLogger("NdbTransactionHandler.js"),
    QueuedAsyncCall = require("../common/QueuedAsyncCall.js").QueuedAsyncCall,
    AutoIncHandler  = require("./NdbAutoIncrement.js").AutoIncHandler,
    COMMIT          = adapter.ndbapi.Commit,
    NOCOMMIT        = adapter.ndbapi.NoCommit,
    ROLLBACK        = adapter.ndbapi.Rollback,
    AO_ABORT        = adapter.ndbapi.AbortOnError,
    AO_IGNORE       = adapter.ndbapi.AO_IgnoreError,
    AO_DEFAULT      = adapter.ndbapi.DefaultAbortOption,
    modeNames       = [],
    serial          = 1;

stats_module.register(stats, "spi","ndb","DBTransactionHandler");

modeNames[COMMIT] = 'commit';
modeNames[NOCOMMIT] = 'noCommit';
modeNames[ROLLBACK] = 'rollback';

function DBTransactionHandler(dbsession) {
  this.dbSession          = dbsession;
  this.autocommit         = true;
  this.impl               = null;
  this.execCount          = 0;   // number of execute calls 
  this.pendingOpsLists    = [];  // [ execCallNumber => {}, ... ]
  this.executedOperations = [];  // All finished operations 
  this.asyncContext       = dbsession.parentPool.asyncNdbContext;
  this.serial             = serial++;
  this.moniker            = "(" + this.serial + ")";
  this.retries            = 0;
  udebug.log("NEW ", this.moniker);
  stats.created++;
}

/* NdbTransactionHandler internal run():
   Create a QueuedAsyncCall on the Ndb's execQueue.
*/
function run(self, execMode, abortFlag, callback) {
  var qpos;
  var apiCall = new QueuedAsyncCall(self.dbSession.execQueue, callback);
  apiCall.tx = self;
  apiCall.execMode = execMode;
  apiCall.abortFlag = abortFlag;
  apiCall.description = "execute_" + modeNames[execMode];
  apiCall.run = function runExecCall() {
    var force_send = 1;
    var canStartImmediate;

    if(this.tx.execCount > 1) {
      canStartImmediate = true;  // Transaction already started
    } else { 
      canStartImmediate = this.tx.impl.tryImmediateStartTransaction();
    }

    if(this.tx.asyncContext && canStartImmediate) { 
      stats.run_async++;
      this.tx.impl.executeAsynch(this.execMode, this.abortFlag,
                                 force_send, this.callback);
    }
    else { 
      stats.run_sync++;
      this.tx.impl.execute(this.execMode, this.abortFlag, 
                           force_send, this.callback);      
    }
  };

  qpos = apiCall.enqueue();
  udebug.log("run()", self.moniker, "queue position:", qpos);
}


/* Error handling after NdbTransaction.execute() 
*/
function attachErrorToTransaction(dbTxHandler, err) {
  if(err) {
    dbTxHandler.success = false;
    dbTxHandler.error = new ndboperation.DBOperationError().fromNdbError(err.ndb_error);
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
   Seize Transaction Context
   Fetch needed auto-increment values
   Prepare each operation (synchronous)
   Execute the NdbTransaction
   If transaction is executed Commit or Rollback, it will close
   Attach results to operations, and run operation callbacks
   Run the transaction callback
   
   EXECUTE PATH FOR SCAN OPERATIONS
   --------------------------------
   Seize Transaction Context
   Prepare & execute the NdbScanOperation (async, NoCommit)
   Fetch results from scan
   Execute the NdbTransaction (commit or rollback); it will close
   Attach results to query operation
   Run query operation callback
   Run the transaction callback
*/


/* Common callback for execute, commit, and rollback 
*/
function onExecute(dbTxHandler, execMode, err, execId, userCallback) {
  var apiCall;
  /* Update our own success and error objects */
  attachErrorToTransaction(dbTxHandler, err);
  if(udebug.is_debug()) {
    udebug.log("onExecute", modeNames[execMode], dbTxHandler.moniker,
                "success:", dbTxHandler.success);
  }

  // If we just executed with Commit or Rollback, release DBTransactionContext.
  if(execMode !== NOCOMMIT && dbTxHandler.impl) {
    dbTxHandler.dbSession.releaseTransactionContext(dbTxHandler.impl);
  }

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
  self.pendingOpsLists[execId] = {
    "operationList"       : operationList,
    "pendingOperationSet" : null   // will be filled in later
  };
  return execId;
}


/* NOTE: Until we have a Batch.createQuery() API, there will only ever be
   one scan in an operationList.  And there will never be key operations
   and scans combined in a single operationList.
*/

function executeScan(self, execMode, abortFlag, dbOperationList, callback) {
  var op = dbOperationList[0];
  var execId = getExecIdForOperationList(self, dbOperationList);

  /* Execute NdbTransaction after reading from scan */
  function executeNdbTransaction() {
    if(udebug.is_debug()) udebug.log(self.moniker, "executeScan executeNdbTransaction");

    function onCompleteExec(err) {
      onExecute(self, execMode, err, execId, callback);
    }
    
    run(self, execMode, abortFlag, onCompleteExec);
  }

  function canRetry(err) {
    return (err.ndb_error && err.ndb_error.classification == 'TimeoutExpired'
            && self.retries++ < 10);
  }


  /* Fetch is complete. */
  function onFetchComplete(err) {
    var closeScanopCallback;

    function retryAfterClose() {
      op.ndbScanOp = null;
      if(udebug.is_debug()) udebug.log(self.moniker, "retrying scan:", self.retries);
      executeScan(self, execMode, abortFlag, dbOperationList, callback);
    }
    
    function closeWithError() {
      op.result.success = false;
      op.result.error = err;
      onExecute(self, ROLLBACK, err, execId, callback);
    }

    function closeSuccess() {
      if(execMode == NOCOMMIT) {
        onExecute(self, execMode, err, execId, callback);      
      } else {
        executeNdbTransaction();
      }    
    }

    if(err) {
      closeScanopCallback = canRetry(err) ? retryAfterClose : closeWithError;
    } else {
      closeScanopCallback = closeSuccess;
    }

    op.ndbScanOp.close(false, false, closeScanopCallback);
  }
  
  /* Fetch results */
  function getScanResults(err) {
    if(udebug.is_debug()) udebug.log(self.moniker, "executeScan getScanResults");
    if(err) {
      onFetchComplete(err);
    }
    else {
      ndboperation.getScanResults(op, onFetchComplete);
    }
  }
  
  /* Execute NoCommit so that you can start reading from scans */
  function executeScanNoCommit(err, ndbScanOp) {
    var fatalError;
    if(udebug.is_debug()) udebug.log(self.moniker, "executeScan executeScanNoCommit");
    if(! ndbScanOp) {
      fatalError = self.ndbtx.getNdbError();
      callback(new ndboperation.DBOperationError().fromNdbError(fatalError), self);
      return;  /* is that correct? */
    }

    op.ndbScanOp = ndbScanOp;
    run(self, NOCOMMIT, AO_IGNORE, getScanResults);
  }

  /* executeScan() starts here */
  if(udebug.is_debug()) udebug.log(self.moniker, "executeScan");
  op.prepareScan(self.ndbtx, executeScanNoCommit);
}


function executeNonScan(self, execMode, abortFlag, dbOperationList, callback) {
  function executeNdbTransaction() {
    var execId = getExecIdForOperationList(self, dbOperationList);

    function onCompleteExec(err) {
      self.pendingOpsLists[execId].pendingOperationSet = self.impl.getPendingOperations();
      onExecute(self, execMode, err, execId, callback);
    }
    
    run(self, execMode, abortFlag, onCompleteExec);
  }

  function prepareOperations() {
    udebug.log("executeNonScan prepareOperations", self.moniker);
    ndboperation.prepareOperations(self.impl, dbOperationList);
    executeNdbTransaction();
  }

  function getAutoIncrementValues() {
    var autoIncHandler = new AutoIncHandler(dbOperationList);
    if(autoIncHandler.values_needed > 0) {
      autoIncHandler.getAllValues(prepareOperations);
    }
    else {
      prepareOperations();
    }  
  }

  // executeNonScan() starts here:
  getAutoIncrementValues();
}


/* Internal execute()
   Fetch a DBTransactionContext, then call executeScan() or executeNonScan()
*/ 
function execute(self, execMode, abortFlag, dbOperationList, callback) {
  udebug.log("internal execute");
  function executeSpecific() {
    if(dbOperationList[0].isScanOperation()) {
      executeScan(self, execMode, abortFlag, dbOperationList, callback);
    } else {
      executeNonScan(self, execMode, abortFlag, dbOperationList, callback);
    }  
  }

  if(self.impl) {
    executeSpecific();
   } else {   
    self.dbSession.seizeTransactionContext(function onContext(impl) {
      self.impl = impl;
      executeSpecific();
    });  
  }
}


/* execute(DBOperation[] dbOperationList,
           function(error, DBTransactionHandler) callback)
   ASYNC
   
   Executes the DBOperations in dbOperationList.
   Commits the transaction if autocommit is true.
*/
DBTransactionHandler.prototype.execute = function(dbOperationList, userCallback) {

  if(! dbOperationList.length) {
    if(udebug.is_debug()) udebug.log("Execute -- STUB EXECUTE (no operation list)");
    userCallback(null, this);
    return;
  }
  
  if(this.autocommit) {
    if(udebug.is_debug()) udebug.log("Execute -- AutoCommit", this.moniker);
    stats.execute.commit++;
    this.dbSession.retireTransactionHandler();
    execute(this, COMMIT, AO_IGNORE, dbOperationList, userCallback);
  }
  else {
    if(udebug.is_debug()) udebug.log("Execute -- NoCommit", this.moniker);
    stats.execute.no_commit++;
    execute(this, NOCOMMIT, AO_IGNORE, dbOperationList, userCallback);
  }
};


/* commit(function(error, DBTransactionHandler) callback)
   ASYNC 
   
   Commit work.
*/
DBTransactionHandler.prototype.commit = function commit(userCallback) {
  assert(this.autocommit === false);
  stats.commit++;
  var self = this;
  var execId = getExecIdForOperationList(self, []);

  function onNdbCommit(err) {
    onExecute(self, COMMIT, err, execId, userCallback);
  }

  /* commit begins here */
  if(udebug.is_debug()) udebug.log("commit");
  this.dbSession.retireTransactionHandler();
  if(self.ndbtx) {  
    run(self, COMMIT, AO_IGNORE, onNdbCommit);
  }
  else {
    if(udebug.is_debug()) udebug.log("commit STUB COMMIT (no underlying NdbTransaction)");
    onNdbCommit();
  }
};


/* rollback(function(error, DBTransactionHandler) callback)
   ASYNC 
   
   Roll back all previously executed operations.
*/
DBTransactionHandler.prototype.rollback = function rollback(callback) {
  assert(this.autocommit === false);
  stats.rollback++;
  var self = this;
  var execId = getExecIdForOperationList(self, []);

  this.dbSession.retireTransactionHandler();

  function onNdbRollback(err) {
    onExecute(self, ROLLBACK, err, execId, callback);
  }

  /* rollback begins here */
  if(udebug.is_debug()) udebug.log("rollback");

  if(self.ndbtx) {
    run(self, ROLLBACK, AO_DEFAULT, onNdbRollback);
  }
  else {
    if(udebug.is_debug()) udebug.log("rollback STUB ROLLBACK (no underlying NdbTransaction)");
    onNdbRollback();
  }
};

exports.DBTransactionHandler = DBTransactionHandler;

