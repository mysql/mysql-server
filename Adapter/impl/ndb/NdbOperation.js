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

/*global unified_debug, path, build_dir, api_dir, spi_doc_dir, assert */

"use strict";

var adapter       = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    encoders      = require("./NdbTypeEncoders.js").defaultForType,
    doc           = require(path.join(spi_doc_dir, "DBOperation")),
    stats_module  = require(path.join(api_dir,"stats.js")),
    stats         = stats_module.getWriter("spi","ndb","DBOperation"),
    udebug        = unified_debug.getLogger("NdbOperation.js");


/* Constructors.
   All of these use prototypes directly from the documentation.
*/
var DBResult = function() {};
DBResult.prototype = doc.DBResult;

var DBOperationError = function() {};
DBOperationError.prototype = doc.DBOperationError;

var DBOperation = function(opcode, tx, tableHandler) {
  assert(doc.OperationCodes.indexOf(opcode) !== -1);
  assert(tx);
  assert(tableHandler);

  stats.incr("created",opcode);

  this.opcode       = opcode;
  this.transaction  = tx;
  this.tableHandler = tableHandler;
  this.buffers      = {};  
  this.state        = doc.OperationStates[0];  // DEFINED
  this.result       = new DBResult();
  this.columnMask   = [];
};
DBOperation.prototype = doc.DBOperation;

DBOperation.prototype.prepare = function(ndbTransaction) {
  udebug.log("prepare", this.opcode);
  stats.incr("prepared");
  var helperSpec = {}, helper;
  switch(this.opcode) {
    case 'insert':
      helperSpec.mask       = this.columnMask;
      helperSpec.row_record = this.tableHandler.dbTable.record;
      helperSpec.row_buffer = this.buffers.row;
      break;
    case 'delete': 
      helperSpec.key_record = this.index.record;
      helperSpec.key_buffer = this.buffers.key;
      helperSpec.row_record = this.tableHandler.dbTable.record;
      break;
    case 'read':
    case 'update':
    case 'write':
      if(this.opcode === 'read') {
        helperSpec.lock_mode  = this.lockMode;
      }
      helperSpec.mask       = this.columnMask;
      helperSpec.key_record = this.index.record;
      helperSpec.key_buffer = this.buffers.key;
      helperSpec.row_record = this.tableHandler.dbTable.record;
      helperSpec.row_buffer = this.buffers.row;
      break; 
  }

  helper = adapter.impl.DBOperationHelper(helperSpec);
  udebug.log("prepare: got helper");
  
  switch(this.opcode) {
    case 'insert':
      this.ndbop = helper.insertTuple(ndbTransaction);
      break;
    case 'delete':
      this.ndbop = helper.deleteTuple(ndbTransaction);
      break;
    case 'read':
      this.ndbop = helper.readTuple(ndbTransaction);
      break;
    case 'update':
      this.ndbop = helper.updateTuple(ndbTransaction);
      break;
    case 'write':
      this.ndbop = helper.writeTuple(ndbTransaction);
      break;
  }

  this.state = doc.OperationStates[1];  // PREPARED
};


function encodeKeyBuffer(indexHandler, op, keys) {
  udebug.log("encodeKeyBuffer");
  var i, offset, value, encoder, record, nfields, col;
  if(indexHandler) {
    op.index = indexHandler.dbIndex;
  }
  else {
    udebug.log("encodeKeyBuffer NO_INDEX");
    return;
  }

  record = op.index.record;
  op.buffers.key = new Buffer(record.getBufferSize());

  nfields = indexHandler.getMappedFieldCount();
  col = indexHandler.getColumnMetadata();
  for(i = 0 ; i < nfields ; i++) {
    value = indexHandler.get(keys, i);  
    if(value) {
      offset = record.getColumnOffset(i);
      encoder = encoders[col[i].ndbTypeId];
      encoder.write(col[i], value, op.buffers.key, offset);
    }
    else {
      udebug.log("encodeKeyBuffer ", i, "NULL.");
      record.setNull(i, op.buffers.key);
    }
  }
}

function encodeRowBuffer(op) {
  udebug.log("encodeRowBuffer");
  var i, offset, encoder, value;
  var record = op.tableHandler.dbTable.record;
  var row_buffer_size = record.getBufferSize();
  var nfields = op.tableHandler.getMappedFieldCount();
  udebug.log("encodeRowBuffer nfields", nfields);
  var col = op.tableHandler.getColumnMetadata();
  
  // do this earlier? 
  op.buffers.row = new Buffer(row_buffer_size);
  
  for(i = 0 ; i < nfields ; i++) {  
    value = op.tableHandler.get(op.values, i);
    if(value) {
      op.columnMask.push(col[i].columnNumber);
      offset = record.getColumnOffset(i);
      encoder = encoders[col[i].ndbTypeId];
      encoder.write(col[i], value, op.buffers.row, offset);
      record.setNotNull(i, op.buffers.row);
    }
  }
}


function readResultRow(op) {
  udebug.log("readResultRow");
  var i, offset, encoder, value;
  var dbt             = op.tableHandler;
  // FIXME: Get the mapped record, not the table record
  var record          = dbt.dbTable.record;
  var nfields         = dbt.getMappedFieldCount();
  var col             = dbt.getColumnMetadata();
  var resultRow       = dbt.newResultObject();
  
  for(i = 0 ; i < nfields ; i++) {
    offset  = record.getColumnOffset(i);
    encoder = encoders[col[i].ndbTypeId];
    assert(encoder);
    if(record.isNull(i, op.buffers.row)) {
      value = null;
    }
    else {
      value = encoder.read(col[i], op.buffers.row, offset);
    }
    dbt.set(resultRow, i, value);
  }
  op.result.value = resultRow;
}

var errorMap = {
  "ConstraintViolation" : "23000",
  "NoDataFound"         : "02000",
  "UnknownResultError"  : "08000"
};

function mapError(opError) {
  udebug.log("mapError " + JSON.stringify(opError.ndb_error));
  var mappedCode = errorMap[opError.ndb_error.classification];
  if(mappedCode) {
    opError.code = mappedCode;
    opError.message = opError.ndb_error.message;
  }
  else {
    opError.code = "NDB00";
    opError.message = "See ndb_error for details";
  }
}

function completeExecutedOps(txError, txOperations) {
  udebug.log("completeExecutedOps", txOperations.length);
  var i, op, ndberror;
  for(i = 0; i < txOperations.length ; i++) {
    op = txOperations[i];
    if(op.state === "PREPARED") {
      udebug.log("completeExecutedOps op",i, op.state);
      op.result = new DBResult();
      ndberror = op.ndbop.getNdbError();
      stats.incr("result_code", ndberror.code);
      if(ndberror.code === 0) {
        op.result.success = true;
      }
      else {
        op.result.success = false;
        op.result.error = new DBOperationError();
        op.result.error.ndb_error = ndberror;
        mapError(op.result.error);
      }
      udebug.log_detail("completeExecutedOps op", i, "result", op.result);
           
      //still to do: insert_id
      if(op.result.success && op.opcode === "read") {
        readResultRow(op);
      }  

      if(op.userCallback) {
        op.userCallback(txError, op);
      }

      op.state = doc.OperationStates[2];  // COMPLETED
    }
    else {
      udebug.log("completeExecutedOps GOT AN OP IN STATE", op.state);
    }
  }
  udebug.log("completeExecutedOps done");
}

function newReadOperation(tx, dbIndexHandler, keys, lockMode) {
  udebug.log("newReadOperation", keys);
  assert(doc.LockModes.indexOf(lockMode) !== -1);
  if(! dbIndexHandler.tableHandler) { 
    throw ("Invalid dbIndexHandler");
  }
  var op = new DBOperation("read", tx, dbIndexHandler.tableHandler);
  var record = op.tableHandler.dbTable.record;
  if(dbIndexHandler.dbIndex.isPrimaryKey || lockMode === "EXCLUSIVE") {
    op.lockMode = lockMode;
  }
  else {
    op.lockMode = "SHARED";
  }
  encodeKeyBuffer(dbIndexHandler, op, keys);  

  /* The row buffer for a read must be allocated here, before execution */
  op.buffers.row = new Buffer(record.getBufferSize());
  return op;
}


function newInsertOperation(tx, tableHandler, row) {
  udebug.log("newInsertOperation");
  var op = new DBOperation("insert", tx, tableHandler);
  op.values = row;
  encodeRowBuffer(op);  
  return op;
}


function newDeleteOperation(tx, dbIndexHandler, keys) {
  udebug.log("newDeleteOperation");
  if(! dbIndexHandler.tableHandler) {
    throw ("Invalid dbIndexHandler");
  }
  var op = new DBOperation("delete", tx, dbIndexHandler.tableHandler);
  encodeKeyBuffer(dbIndexHandler, op, keys);
  return op;
}


function newWriteOperation(tx, dbIndexHandler, row) {
  udebug.log("newWriteOperation");
  if(! dbIndexHandler.tableHandler) {
    throw ("Invalid dbIndexHandler");
  }
  var op = new DBOperation("write", tx, dbIndexHandler.tableHandler);
  op.values = row;
  encodeRowBuffer(op);
  encodeKeyBuffer(dbIndexHandler, op, row);
  return op;
}


function newUpdateOperation(tx, dbIndexHandler, keys, row) {
  udebug.log("newUpdateOperation");
  if(! dbIndexHandler.tableHandler) {
    throw ("Invalid dbIndexHandler");
  }
  var op = new DBOperation("update", tx, dbIndexHandler.tableHandler);
  op.values = row;
  encodeKeyBuffer(dbIndexHandler, op, keys);
  encodeRowBuffer(op);
  return op;
}


exports.DBOperation         = DBOperation;
exports.newReadOperation    = newReadOperation;
exports.newInsertOperation  = newInsertOperation;
exports.newDeleteOperation  = newDeleteOperation;
exports.newUpdateOperation  = newUpdateOperation;
exports.newWriteOperation   = newWriteOperation;
exports.completeExecutedOps = completeExecutedOps;

