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

/*global udebug */

var adapter    = require("../build/Release/ndb/ndb_adapter.node"),
    ndbsession = require("./NdbSession.js"), 
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
  udebug.log("DBTransactionHandler constructor");
  this.session = dbsession;
}

proto = {
  success    : null,
  state      : TransactionStatusCodes[0],
  operations : [],
  error      : {},
  callback   : {}
};

proto.addOperation = function(op) {
  udebug.log("DBTransactionHandler addOperation " + op.opcode);
  this.operations.push(op);
};

proto.execute = function(execmode, callback) {
  udebug.log("DBTransactionHandler execute " + execmode);

  var txhandler = this;
  var exec_flag = TransactionExecuteModes[execmode];
  var table = this.operations[0].tableHandler.dbTable;

  this.callback = callback;

  function onCompleteTx(err, result) {
    udebug.log("DBTransactionHandler execute onCompleteTx: " + result);
    txhandler.callback(err, null);
  }

  function onStartTx(err, ndbtx) {
    var op, helper, i;
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

