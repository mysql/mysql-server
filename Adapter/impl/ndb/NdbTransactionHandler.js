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

/*global assert, unified_debug, path, build_dir, spi_doc_dir */

"use strict";

var adapter         = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    ndbsession      = require("./NdbSession.js"),
    ndboperation    = require("./NdbOperation.js"),
    doc             = require(path.join(spi_doc_dir, "DBTransactionHandler")),
    udebug          = unified_debug.getLogger("NdbTransactionHandler.js"),
    proto           = doc.DBTransactionHandler,
    COMMIT          = adapter.ndbapi.Commit,
    NOCOMMIT        = adapter.ndbapi.NoCommit;
    

function DBTransactionHandler(dbsession) {
  udebug.log("constructor");
  this.dbSession          = dbsession;
  this.autocommit         = true;
  this.ndbtx              = null;
  this.state              = doc.DBTransactionStates[0];  // DEFINED
  this.open               = true;
  this.executedOperations = [];
}
DBTransactionHandler.prototype = proto;


/* Internal execute()
*/ 
function execute(self, execMode, dbOperationList, callback) {
  udebug.log("Internal execute");
  var table = dbOperationList[0].tableHandler.dbTable;

  function onCompleteTx(err, result) {
    udebug.log("execute onCompleteTx", err);
    
    // Update our own success and error objects
    self.error = err;
    self.success = err ? false : true;
    
    /* If we just executed with Commit or Rollback, close the NdbTransaction */
    if(execMode === adapter.ndbapi.Commit || execMode === adapter.ndbapi.Rollback) {
      self.open = false;
      self.ndbtx.close();
    }

    /* Attach results to their operations */
    ndboperation.completeExecutedOps(err, self.executedOperations);
    udebug.log("BACK IN execute onCompleteTx AFTER COMPLETED OPS");

    /* Next callback */
    if(callback) {
      callback(err, self);
    }
  }

  function prepareOperationsAndExecute() {
    udebug.log("execute prepareOperationsAndExecute");
    var i;
    for(i = 0 ; i < dbOperationList.length; i++) {
      dbOperationList[i].prepare(self.ndbtx);
      self.executedOperations.push(dbOperationList[i]);
    }
    // TODO: Vary AbortOption based on execMode?
    // execute(ExecFlag, AbortOption, ForceSend, callback)
    self.ndbtx.execute(execMode, adapter.ndbapi.AO_IgnoreError, 0, onCompleteTx);
  }

  function onStartTx(err, ndbtx) {
    var op, helper;
    if(err) {
      udebug.log("execute onStartTx [ERROR].");
      if(callback) {
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

  if(self.state === "DEFINED") {
    // TODO: partitionKey
    var ndb = adapter.impl.DBSession.getNdb(self.dbSession.impl);
    ndb.startTransaction(table, 0, 0, onStartTx);  
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
  var execMode = this.autocommit ? COMMIT : NOCOMMIT;

  function onExecCommit(err, dbTxHandler) {
    udebug.log("execute onExecCommit");

    dbTxHandler.state = doc.DBTransactionStates[2]; // COMMITTED
    ndbsession.txDidCommit(dbTxHandler.dbSession, dbTxHandler);

    if(userCallback) {
      userCallback(err, dbTxHandler);
    }
  }
  
  if(this.autocommit) {
    udebug.log(" -- AutoCommit");
    execute(this, execMode, dbOperationList, onExecCommit);
  }
  else {
    udebug.log(" -- NoCommit");
    execute(this, execMode, dbOperationList, userCallback);
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
  udebug.log("commit");
  var self = this;
  
  function onNdbCommit(err, result) {
    // TODO: Update our own success and error objects
    self.state = doc.DBTransactionStates[2]; // COMMITTED
    userCallback(err, self);
  }

  if(self.ndbtx) {  
    self.ndbtx.execute(adapter.ndbapi.Commit, adapter.ndbapi.AbortOnError,
                       0, onNdbCommit);
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
  udebug.log("rollback");
  var self = this;
  
  function onNdbRollback(err, result) {
    // TODO: Update our own success and error objects
    self.state = doc.DBTransactionStates[3]; // ROLLEDBACK
    callback(err, self);
  }
  
  if(self.ndbtx) {
    self.ndbtx.execute(adapter.ndbapi.Rollback, adapter.ndbapi.DefaultAbortOption,
                       0, onNdbRollback);
  }
  else {
    udebug.log("rollback STUB ROLLBACK (no underlying NdbTransaction)");
    onNdbRollback();
  }
};


DBTransactionHandler.prototype = proto;
exports.DBTransactionHandler = DBTransactionHandler;

