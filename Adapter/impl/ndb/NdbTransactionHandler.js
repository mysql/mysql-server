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
var adapter    = require("../build/Release/ndb/ndb_adapter.node"),
    ndbsession = require("./NdbSession.js"), 
    proto = {};

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


var DBTransactionHandler = function(dbsession) {
  udebug.log("DBTransactionHandler constructor");
  this.session = dbsession;
  this.success = null;
  this.state = TransactionStatusCodes[0];
  this.operations = [];
  this.error = {};
  this.callback = {};
}

proto.addOperation = function(op) {
  udebug.log("DBTransactionHandler addOperation " + op.opcode);
  this.operations.push(op);
}

proto.execute = function(execmode, callback) {
  udebug.log("DBTransactionHandler execute " + execmode);
  var exec_flag = TransactionExecuteModes[execmode];
  
  this.callback = callback;
  var txhandler = this;

  // fixme: only get the NdbTransaction if it needs gettin
  var table = null; // get the table from the partitionKey or first operation
  table = this.operations[0].tableHandler;
  udebug.log("NdbTransactionHandler.js line 65");

  var ndbtx_callback = function(err, ndbtx) {
    udebug.log("In ndbtx_callback.  TC is node: " + ndbtx.getConnectedNodeId());
    udebug.log("exec_flag: " + exec_flag);

    // Prepare the operations 
    for(var i = 0 ; i < txhandler.operations.length; i ++) {
      var op = txhandler.operations[i];
      var helper = adapter.impl.DBOperationHelper(
        { row_record : op.tableHandler.record ,
          row_buffer : op.buffers.row 
        });
      udebug.log("Got helper");
      
      helper.insertTuple(ndbtx);   // fixme
      op.state = "PREPARED";
    }

    var tx_complete_callback = function(err, result) {
       udebug.log("In tx_complete_callback");
       txhandler.callback(null, null);
    }
    console.dir(ndbtx);
    // execute(ExecFlag, AbortOption, ForceSend, callback)
    ndbtx.execute(exec_flag, adapter.ndbapi.AbortOnError, 0, tx_complete_callback);
  };

  // TODO: partitionKey
  var ndb = adapter.impl.DBSession.getNdb(this.session.impl);
  ndb.startTransaction(table, 0, 0, ndbtx_callback);
}
  
  


DBTransactionHandler.prototype = proto;
exports.DBTransactionHandler = DBTransactionHandler;

