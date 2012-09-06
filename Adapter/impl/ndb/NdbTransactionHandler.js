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

var adapter       = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    ndbsession    = require("./NdbSession.js"),
    proto;

var TransactionExecuteModes = {
  "NoCommit" : adapter.ndbapi.NoCommit ,
  "Commit"   : adapter.ndbapi.Commit ,
  "Rollback" : adapter.ndbapi.Rollback
};

var TransactionStatusCodes = [];
TransactionStatusCodes[adapter.ndbapi.NotStarted] = "NotStarted";
TransactionStatusCodes[adapter.ndbapi.Started]    = "Started";
TransactionStatusCodes[adapter.ndbapi.Committed]  = "Committed";
TransactionStatusCodes[adapter.ndbapi.Aborted]    = "Aborted";
TransactionStatusCodes[adapter.ndbapi.NeedAbort]  = "NeedAbort";


function DBTransactionHandler(dbsession) {
  udebug.log("NdbTransactionHandler constructor");
  this.session = dbsession;
  this.operations = [];
}

proto = {
  session    : null,
  success    : null,
  state      : TransactionStatusCodes[0],
  operations : {},
  error      : {},
  callback   : {},
  ndbtx      : null
};

proto.addOperation = function(op) {
  this.operations.push(op);
  udebug.log("NdbTransactionHandler addOperation", 
              this.operations.length, op.opcode);
};

/* close()
   ASYNC, NO CALLBACK, EMITS 'close' EVENT ON COMPLETION
*/
proto.close = function() {
  udebug.log("NdbTransactionHandler close");
  function onClose(err, i) {
    /* NdbTransaction::close() returns void.  i == 1. */
    udebug.log("NdbTransactionHandler close onClose");
  }  

  this.ndbtx.close(onClose);
};


/* execute(TransactionExecuteMode mode, 
           function(error, DBTransactionHandler) callback);
   ASYNC
   
   Executes all of the DBOperations that have been added to the 
   transaction's list -- see DBTransactionHandler.addOperation().
*/
proto.execute = function(execmode, callback) {
  udebug.log("NdbTransactionHandler execute " + execmode);

  var txhandler = this;
  var exec_flag = TransactionExecuteModes[execmode];
  var table = this.operations[0].tableHandler.dbTable;

  this.callback = callback;

  function onCompleteTx(err, result) {
    udebug.log("NdbTransactionHandler execute onCompleteTx");

    /* TODO: attach results to their operations */
    txhandler.callback(err, txhandler);
  }

  function onStartTx(err, ndbtx) {
    var op, helper, i;
    if(err) {
      udebug.log("NdbTransactionHandler execute onStartTx [ERROR].");
      txhandler.callback(err, txhandler);
      return;
    }

    txhandler.ndbtx = ndbtx; 
    udebug.log("NdbTransactionHandler execute onStartTx.  TC node: " +
                ndbtx.getConnectedNodeId() + " exec_flag: " + exec_flag
                + " operations: " + txhandler.operations.length);

    // Prepare the operations 
    for(i = 0 ; i < txhandler.operations.length; i++) {
      txhandler.operations[i].prepare(ndbtx);
    }

    // execute(ExecFlag, AbortOption, ForceSend, callback)
    udebug.log("NdbTransactionHandler execute ready to execute.");
    ndbtx.execute(exec_flag, adapter.ndbapi.AbortOnError, 0, onCompleteTx);
  }

  // TODO: partitionKey
  var ndb = adapter.impl.DBSession.getNdb(this.session.impl);
  ndb.startTransaction(table, 0, 0, onStartTx);
};
  

DBTransactionHandler.prototype = proto;
exports.DBTransactionHandler = DBTransactionHandler;

