/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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
  "created"		   : 0,
  "run_async"    : 0,
  "run_sync"     : 0,
  "execute"      : { "commit": 0, "no_commit" : 0, "scan": 0, "scan_retry": 0 },
  "failed_scans" : 0,
  "commit"       : 0,
  "rollback"     : 0
};

var path            = require("path"),
    assert          = require("assert"),
    adapter         = require(path.join(mynode.fs.build_dir, "ndb_adapter.node")).ndb,
    ndboperation    = require("./NdbOperation.js"),
    doc             = require(path.join(mynode.fs.spi_doc_dir, "DBTransactionHandler")),
    stats_module    = require(mynode.api.stats),
    udebug          = unified_debug.getLogger("NdbTransactionHandler.js"),
    QueuedAsyncCall = require(mynode.common.QueuedAsyncCall).QueuedAsyncCall,
    AutoIncHandler  = require("./NdbAutoIncrement.js").AutoIncHandler,
    COMMIT          = adapter.ndbapi.Commit,
    NOCOMMIT        = adapter.ndbapi.NoCommit,
    ROLLBACK        = adapter.ndbapi.Rollback,
    AO_ABORT        = adapter.ndbapi.AbortOnError,
    AO_IGNORE       = adapter.ndbapi.AO_IgnoreError,
    AO_DEFAULT      = adapter.ndbapi.DefaultAbortOption,
    modeNames       = [],
    serial          = 1,
    usedOperationSets = new Array(4000);

stats_module.register(stats, "spi","ndb","DBTransactionHandler");

modeNames[COMMIT] = 'commit';
modeNames[NOCOMMIT] = 'noCommit';
modeNames[ROLLBACK] = 'rollback';

function DBTransactionHandler(dbsession) {
  this.dbSession          = dbsession;
  this.autocommit         = true;
  this.impl               = null;
  this.sentSeizeImpl      = false;
  this.execCount          = 0;   // number of execute calls 
  this.pendingOpsLists    = [];  // [ execCallNumber => {}, ... ]
  this.executedOperations = [];  // All finished operations 
  this.asyncContext       = dbsession.parentPool.asyncNdbContext;
  this.serial             = serial++;
  this.moniker            = "(tx" + this.serial + ")";
  this.retries            = 0;
  udebug.log("NEW ", this.moniker);
  stats.created++;
}

function getOpSetWrapper() {
  return usedOperationSets.pop();
}

function releaseOpSetWrapper(dbOperationSet) {
  if(dbOperationSet) {
    dbOperationSet.free();  // Free the underlying native object 
    if(usedOperationSets.length < 4000) {
      usedOperationSets.push(dbOperationSet);
    } // Keep the JavaScript wrapper for recycling
  }
}

/* NdbTransactionHandler internal run():
   Create a QueuedAsyncCall on the Ndb's execQueue.
*/
function run(self, operationSet, execMode, abortFlag, callback) {
  var qpos;
  var apiCall = new QueuedAsyncCall(self.dbSession.execQueue, callback);
  apiCall.tx = self;
  apiCall.operations = operationSet;
  apiCall.execMode = execMode;
  apiCall.abortFlag = abortFlag;
  apiCall.description = "execute_" + modeNames[execMode];
  apiCall.txIsOpen = (self.execCount > 1);
  apiCall.run = function runExecCall() {
    var force_send = 1;
    var canStartImmediate;

    if(this.txIsOpen) {
      canStartImmediate = true;  // Transaction already started
    } else if(this.tx.asyncContext) { 
      canStartImmediate = this.operations.tryImmediateStartTransaction();
    }

    if(this.tx.asyncContext && canStartImmediate) { 
      stats.run_async++;
      this.operations.executeAsynch(this.execMode, this.abortFlag,
                                    force_send, this.callback);
    }
    else { 
      stats.run_sync++;
      this.operations.execute(this.execMode, this.abortFlag, 
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
  var pendingOpsList = dbTxHandler.pendingOpsLists[execId];

  /* Update our own success and error objects */
  attachErrorToTransaction(dbTxHandler, err);
  if(udebug.is_debug()) {
    udebug.log("onExecute", modeNames[execMode], dbTxHandler.moniker,
                "success:", dbTxHandler.success);
  }

  // If we just executed with Commit or Rollback, release DBTransactionContext.
  if(execMode !== NOCOMMIT) {
    dbTxHandler.dbSession.releaseTransactionContext(dbTxHandler.impl);
    dbTxHandler.impl = null;
  }

  /* Attach results to their operations */
  ndboperation.completeExecutedOps(dbTxHandler, execMode, pendingOpsList);

  /* Next callback */
  if(typeof userCallback === 'function') {
    userCallback(dbTxHandler.error, dbTxHandler);
  }
}


function getExecIdForOperationList(self, operationList, pendingOpSet) {
  var execId = self.execCount++;
  self.pendingOpsLists[execId] = {
    "operationList"       : operationList,
    "pendingOperationSet" : pendingOpSet
  };
  return execId;
}


/* We assume there will only ever be one scan in an operationList, and there 
   will never be key operations and scans combined in a single operationList.
*/
function executeScan(self, execMode, abortFlag, dbOperationList, callback) {
  var op, execId, scanOperation, apiCall;

  /* After reading from the scan, execute the NdbTransaction with an 
     empty operation list. 
  */
  function executeNdbTransaction() {
    function onCompleteExec(err) {
      onExecute(self, execMode, err, execId, callback);
    }
    
    udebug.log(self.moniker, "executeScan executeNdbTransaction");
    var emptyOpSet = self.impl.getEmptyOperationSet();
    execId = getExecIdForOperationList(self, dbOperationList, emptyOpSet);
    run(self, emptyOpSet, execMode, abortFlag, onCompleteExec);
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
      stats.execute.scan_retry++;
      udebug.log(self.moniker, "retrying scan:", self.retries);
      executeScan(self, execMode, abortFlag, dbOperationList, callback);
    }
    
    function closeWithError() {
      stats.failed_scans++;
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

    /* Close the Scan Operation */
    apiCall = new QueuedAsyncCall(self.dbSession.execQueue, closeScanopCallback);
    apiCall.description = "ScanOperation.close";
    apiCall.run = function() {
      scanOperation.close(this.callback);
    };
    apiCall.enqueue();
  }
  
  /* Fetch results */
  function getScanResults(err) {
    udebug.log(self.moniker, "executeScan getScanResults");
    if(err) {
      onFetchComplete(err);
    }
    else {
      ndboperation.getScanResults(op, onFetchComplete);
    }
  }
  
  function onExecNoCommit(err) {
    var fatalError;
    udebug.log(self.moniker, "executeScan onExecNoCommit");
    if(err) {
      fatalError = self.impl.getNdbError();
      callback(new ndboperation.DBOperationError().fromNdbError(fatalError), self);
    } else {
      getScanResults(null);
    }
  }

  /* executeScan() starts here */
  udebug.log(self.moniker, "executeScan");
  op = dbOperationList[0];
  execId = getExecIdForOperationList(self, dbOperationList);
  if(op.scanOp) { //  No need to rebuild if retrying after error
    scanOperation = op.scanOp;
  } else {
    scanOperation = op.prepareScan(self.impl);
  }
  apiCall = new QueuedAsyncCall(self.dbSession.execQueue, onExecNoCommit);
  apiCall.description = "ScanOperation.prepareAndExecute";
  apiCall.run = function() {
    scanOperation.prepareAndExecute(this.callback);
  };
  apiCall.enqueue();
}


function executeNonScan(self, execMode, abortFlag, dbOperationList, callback) {
  var pendingOps;

  function executeNdbTransaction() {
    var execId = getExecIdForOperationList(self, dbOperationList, pendingOps);

    function onCompleteExec(err) {
      onExecute(self, execMode, err, execId, callback);
      releaseOpSetWrapper(pendingOps);
    }
    
    run(self, pendingOps, execMode, abortFlag, onCompleteExec);
  }

  function prepareOperations() {
    udebug.log("executeNonScan prepare", dbOperationList.length, 
               "operations", self.moniker);
    pendingOps = ndboperation.prepareOperations(self.impl, dbOperationList, 
                                                getOpSetWrapper());
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
      stats.execute.scan++;
      executeScan(self, execMode, abortFlag, dbOperationList, callback);
    } else {
      executeNonScan(self, execMode, abortFlag, dbOperationList, callback);
    }  
  }
 
  // Execute calls are queued, so we can create one even if 
  // seizeTransactionContext has not yet returned.
  if(self.sentSeizeImpl) {
    executeSpecific();
  } else {                           // seize a DBTransactionContext 
    self.sentSeizeImpl = true;
    self.dbSession.seizeTransactionContext(function onContext(impl) {
      self.impl = impl;
      executeSpecific();
    });  
  }
}


// executeNoOperations(): used by commit() and rollback()
function executeNoOperations(self, execMode, userCallback) {
  var execId, pendingOps;
  pendingOps = self.impl.getEmptyOperationSet();
  execId = getExecIdForOperationList(self, [], pendingOps);
  run(self, pendingOps, execMode, AO_IGNORE,  function onNdbExec(err) {
    onExecute(self, execMode, err, execId, userCallback);
  });
}


/* execute(DBOperation[] dbOperationList,
           function(error, DBTransactionHandler) callback)
   ASYNC
   
   Executes the DBOperations in dbOperationList.
   Commits the transaction if autocommit is true.
*/
DBTransactionHandler.prototype.execute = function(dbOperationList, userCallback) {

  if(! dbOperationList.length) {
    udebug.log("Execute -- STUB EXECUTE (no operation list)");
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
  udebug.log("commit");
  stats.commit++;
  var execId;

  this.dbSession.retireTransactionHandler();
  if(this.impl) {
    executeNoOperations(this, COMMIT, userCallback);
  } else {
    udebug.log("commit STUB COMMIT (no DBTransactionContext)");
    userCallback(null, this);
  }
};


/* rollback(function(error, DBTransactionHandler) callback)
   ASYNC 
   
   Roll back all previously executed operations.
*/
DBTransactionHandler.prototype.rollback = function rollback(userCallback) {
  assert(this.autocommit === false);
  udebug.log("rollback");
  stats.rollback++;
  var execId;

  this.dbSession.retireTransactionHandler();
  if(this.impl) {
    executeNoOperations(this, ROLLBACK, userCallback);
  } else {
    udebug.log("rollback STUB ROLLBACK (no DBTransactionContext)");
    userCallback(null, this);
  }
};

exports.DBTransactionHandler = DBTransactionHandler;

