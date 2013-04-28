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

/*global unified_debug, path, build_dir, api_dir, spi_doc_dir, assert */

"use strict";

var adapter       = require(path.join(build_dir, "ndb_adapter.node")).ndb,
    doc           = require(path.join(spi_doc_dir, "DBOperation")),
    stats_module  = require(path.join(api_dir,"stats.js")),
  QueuedAsyncCall = require("../common/QueuedAsyncCall.js").QueuedAsyncCall,
prepareFilterSpec = require("./NdbScanFilter.js").prepareFilterSpec,
//   getBoundHelper = require("./IndexBounds.js").getBoundHelper, 
    stats         = stats_module.getWriter(["spi","ndb","DBOperation"]),
    index_stats   = stats_module.getWriter(["spi","ndb","key_access"]),
    COMMIT        = adapter.ndbapi.Commit,
    NOCOMMIT      = adapter.ndbapi.NoCommit,
    ROLLBACK      = adapter.ndbapi.Rollback,
    constants     = adapter.impl,
    OpHelper      = constants.OpHelper,
    ScanHelper    = constants.Scan.helper,
    opcodes       = doc.OperationCodes,
    udebug        = unified_debug.getLogger("NdbOperation.js");


/* Constructors.
   All of these use prototypes directly from the documentation.
*/
var DBResult = function() {};
DBResult.prototype = doc.DBResult;

// DBOperationError
var errorClassificationMap = {
  "ConstraintViolation" : "23000",
  "NoDataFound"         : "02000",
  "UnknownResultError"  : "08000"
};

function DBOperationError(ndb_error) {
  var mappedCode = errorClassificationMap[ndb_error.classification];
  this.message = ndb_error.message + " [" + ndb_error.code + "]";
  this.sqlstate = mappedCode || "NDB00";
  this.ndb_error = ndb_error;
  this.cause = null;
}

DBOperationError.prototype = doc.DBOperationError;
exports.DBOperationError = DBOperationError;

function IndirectError(dbOperationErr) {
  this.message = "Error";
  this.sqlstate = dbOperationErr.sqlstate;
  this.ndb_error = null;
  this.cause = dbOperationErr;
}
IndirectError.prototype = doc.DBOperationError;


var DBOperation = function(opcode, tx, indexHandler, tableHandler) {
  assert(tx);
 
  this.opcode         = opcode;
  this.userCallback   = null;
  this.transaction    = tx;
  this.keys           = {}; 
  this.values         = {};
  this.lockMode       = "";
  this.state          = doc.OperationStates[0];  // DEFINED
  this.result         = new DBResult();
  this.indexHandler   = indexHandler;   
  if(indexHandler) { 
    this.tableHandler = indexHandler.tableHandler; 
    this.index        = indexHandler.dbIndex;
  }      
  else {
    this.tableHandler = tableHandler;
    this.index        = {};  
  }
  
  /* NDB Impl-specific properties */
  this.query        = null;
  this.ndbop        = null;
  this.autoinc      = null;
  this.buffers      = { 'row' : null, 'key' : null  };
  this.columnMask   = [];

  stats.incr(["created",opcode]);
};


function allocateKeyBuffer(op) {
  assert(op.buffers.key === null);
  op.buffers.key = new Buffer(op.index.record.getBufferSize());

  index_stats.incr([ op.tableHandler.dbTable.database,
                     op.tableHandler.dbTable.name,
                    (op.index.isPrimaryKey ? "PrimaryKey" : op.index.name)
                   ]);
}

function releaseKeyBuffer(op) {
  if(op.opcode !== 2) {  /* all but insert use a key */
    op.buffers.key = null;
  }
}

function encodeKeyBuffer(op) {
  udebug.log("encodeKeyBuffer with keys", op.keys);
  var i, offset, value, nfields, col;
  var record = op.index.record;

  nfields = op.indexHandler.getMappedFieldCount();
  col = op.indexHandler.getColumnMetadata();
  for(i = 0 ; i < nfields ; i++) {
    value = op.keys[i];
    if(value !== null) {
      record.setNotNull(i, op.buffers.key);
      offset = record.getColumnOffset(i);
      if(col.typeConverter) {
        value = col.typeConverter.toDB(value);
      }
      adapter.impl.encoderWrite(col[i], value, op.buffers.key, offset);
    }
    else {
      udebug.log("encodeKeyBuffer ", i, "NULL.");
      record.setNull(i, op.buffers.key);
    }
  }
}


function allocateRowBuffer(op) {
  assert(op.buffers.row === null);
  op.buffers.row = new Buffer(op.tableHandler.dbTable.record.getBufferSize());
}  

function releaseRowBuffer(op) {
  op.buffers.row = null;
}

function encodeRowBuffer(op) {
  udebug.log("encodeRowBuffer");
  var i, offset, value, err;
  var record = op.tableHandler.dbTable.record;
  var nfields = op.tableHandler.getMappedFieldCount();
  udebug.log("encodeRowBuffer nfields", nfields);
  var col = op.tableHandler.getColumnMetadata();
  
  for(i = 0 ; i < nfields ; i++) {  
    value = op.tableHandler.get(op.values, i);
    if(value === null) {
      record.setNull(i, op.buffers.row);
    }
    else if(typeof value !== 'undefined') {
      record.setNotNull(i, op.buffers.row);
      op.columnMask.push(col[i].columnNumber);
      offset = record.getColumnOffset(i);
      if(col[i].typeConverter) {
        value = col[i].typeConverter.toDB(value);
      }
      err = adapter.impl.encoderWrite(col[i], value, op.buffers.row, offset);
      if(err) { udebug.log("encoderWrite: ", err); }
      // FIXME: What to do with this error?
    }
  }
}

function HelperSpec() {
  this.clear();
}

HelperSpec.prototype.clear = function() {
  this[OpHelper.row_buffer]  = null;
  this[OpHelper.key_buffer]  = null;
  this[OpHelper.row_record]  = null;
  this[OpHelper.key_record]  = null;
  this[OpHelper.lock_mode]   = null;
  this[OpHelper.column_mask] = null;
  this[OpHelper.value_obj]   = null;
};

var helperSpec = new HelperSpec();

function ScanHelperSpec() {
  this.clear();
}

ScanHelperSpec.prototype.clear = function() {
  this[ScanHelper.table_record] = null;
  this[ScanHelper.index_record] = null;
  this[ScanHelper.lock_mode]    = null;
  this[ScanHelper.bounds]       = null;
  this[ScanHelper.flags]        = null;
  this[ScanHelper.batch_size]   = null;
  this[ScanHelper.parallel]     = null;
  this[ScanHelper.filter_code]  = null;
}

var scanSpec = new ScanHelperSpec;


function prepareKeyOperation(op, ndbTransaction) {
  var code = op.opcode;
  var isVOwrite = (op.values && adapter.impl.isValueObject(op.values));

  /* There is one global helperSpec */
  helperSpec.clear();

  /* All operations but insert use a key. */
  if(code !== 2) {
    allocateKeyBuffer(op);
    encodeKeyBuffer(op);
    helperSpec[OpHelper.key_record]  = op.index.record;
    helperSpec[OpHelper.key_buffer]  = op.buffers.key;
  }
  
  /* If this is an update-after-read operation on a Value Object, 
     DBOperationHelper only needs the VO.
  */
  if(isVOwrite) {
    helperSpec[OpHelper.value_obj] = op.values;
  }  
  else {
    /* All non-VO operations get a row record */
    helperSpec[OpHelper.row_record] = op.tableHandler.dbTable.record;
    
    /* All but delete get an allocated row buffer, and column mask */
    if(code !== 16) {
      allocateRowBuffer(op);
      helperSpec[OpHelper.row_buffer]  = op.buffers.row;
      helperSpec[OpHelper.column_mask] = op.columnMask;

      /* Read gets a lock mode; 
         writes get the data encoded into the row buffer. */
      if(code === 1) {
        helperSpec[OpHelper.lock_mode]  = constants.LockModes[op.lockMode];
      }
      else { 
        encodeRowBuffer(op);
      }
    }
  }
  
  /* Use the HelperSpec and opcode to build the NdbOperation */
  op.ndbop = 
    adapter.impl.DBOperationHelper(helperSpec, code, ndbTransaction, isVOwrite);
}

function prepareScanOperation(op, ndbTransaction) {
  var opcode = 33;  // How to tell from operation?
  var boundHelper = null;
 
  /* There is one global ScanHelperSpec */
  scanSpec.clear();

  scanSpec[ScanHelper.table_record] = op.query.dbTableHandler.dbTable.record;

  if(op.query.queryType == 2) {  /* Index Scan */
    scanSpec[ScanHelper.index_record] = op.query.dbIndexHandler.dbIndex.record;
//    boundHelper = getBoundHelper(op.query, op.keys);
//    if(boundHelper) {
//      scanHelper[bounds] = adapter.impl.IndexBound.create(boundHelper);
//    }
  }

  scanSpec[ScanHelper.lock_mode] = constants.LockModes[op.lockMode];

  if(typeof op.keys.order !== 'undefined') {
    var flags = constants.Scan.flags.SF_OrderBy;
    if(op.keys.order == 'desc') flags |= constants.Scan.flags.SF_Descending;
    scanSpec[ScanHelper.flags] = flags;  
  }

  if(op.query.ndbFilterSpec) {
    scanSpec[ScanHelper.filter_code] = 
      op.query.ndbFilterSpec.getScanFilterCode(op.keys);  // call them params?
  }

  /* Build the NdbScanOperation */
  op.ndbop = adapter.impl.Scan.create(scanSpec, opcode, ndbTransaction);
}


DBOperation.prototype.isScanOperation = function() {
  return (this.opcode >= 32);
}

DBOperation.prototype.prepare = function(ndbTransaction) {
  udebug.log("prepare", opcodes[this.opcode], this.values);

  var f = this.isScanOperation() ? prepareScanOperation : prepareKeyOperation;
  f(this, ndbTransaction);
  this.state = doc.OperationStates[1];  // PREPARED
  return;
}

function readResultRow(op) {
  udebug.log("readResultRow");
  var i, offset, value;
  var dbt             = op.tableHandler;
  var record          = dbt.dbTable.record;
  var nfields         = dbt.getMappedFieldCount();
  var col             = dbt.getColumnMetadata();
  var resultRow       = dbt.newResultObject();
  
  for(i = 0 ; i < nfields ; i++) {
    offset  = record.getColumnOffset(i);
  if(record.isNull(i, op.buffers.row)) {
      value = col[i].defaultValue;
    }
    else {
      value = adapter.impl.encoderRead(col[i], op.buffers.row, offset);
      if(col[i].typeConverter) {
        value = col[i].typeConverter.fromDB(value);
      }
    }

    dbt.set(resultRow, i, value);
  }
  op.result.value = resultRow;
}


function buildValueObject(op) {
  udebug.log("buildValueObject");
  var VOC = op.tableHandler.ValueObject; // NDB Value Object Constructor
  var DOC = op.tableHandler.newObjectConstructor;  // User's Domain Object Ctor
  
  if(VOC) {
    /* Turn the buffer into a Value Object */
    op.result.value = new VOC(op.buffers.row);

    /* TODO: Apply type converters here, rather than in Column Handler??? */

    /* DBT may have some fieldConverters for this object */
    op.tableHandler.applyFieldConverters(op.result.value);

    /* Finally the user's constructor is called on the new value: */
    if(DOC) {
      DOC.call(op.result.value);
    }
  }
  else {
    /* If there is a good reason to have no VOC, just call readResultRow()... */
    console.log("NO VOC!");
    process.exit();
  }
}


function fetchResults(dbSession, ndb_scan_op, buffer, callback) {
  var apiCall = new QueuedAsyncCall(dbSession.execQueue, callback);
  var force_send = true;
  apiCall.ndb_scan_op = ndb_scan_op;
  apiCall.buffer = buffer;
  apiCall.run = function runFetchResults() {
    this.ndb_scan_op.fetchResults(this.buffer, force_send, this.callback);
  }
  apiCall.enqueue();
}


function getScanResults(scanop, userCallback) {
  var buffer;
  var dbSession = scanop.transaction.dbSession;
  var ResultConstructor = scanop.tableHandler.ValueObject;

  if(ResultConstructor == null) {
    storeNativeConstructorInMapping(scanop.tableHandler);
    ResultConstructor = scanop.tableHandler.ValueObject;
  }

  var recordSize = scanop.tableHandler.dbTable.record.getBufferSize();

  function gather(error, status) {    
    udebug.log("Scan gather", status);
    if(status !== 0) {
      results.pop();  // remove the optimistic result 
    }

    if(status < 0) { // error
      udebug.log("error", status, error);
      scanop.result.success = false;
      scanop.result.error = error;
      userCallback(error, results);
      return;
    }
    
    /* Gather more results. */
    while(status === 0) {
      buffer = new Buffer(recordSize);
      status = scanop.ndbop.nextResult(buffer);
      if(status === 0) {
        results.push(new ResultConstructor(buffer));
      }    
    }
    
    if(status == 2) {  // No more locally cached results
      fetch();
    }
    else {  // end of scan.
      assert(status === 1);
      udebug.log("Scan Result length:", results.length);
      scanop.result.success = true;
      scanop.result.value = results;
      userCallback(null, results);
      return;
    }    
  }

  function fetch() {
    buffer = new Buffer(recordSize);
    results.push(new ResultConstructor(buffer));  // Optimistic
    fetchResults(dbSession, scanop.ndbop, buffer, gather);
  }

  /* start here */
  var results = [];
  fetch();
}


function buildOperationResult(transactionHandler, op, execMode) {
  udebug.log("buildOperationResult");
  var op_ndb_error, result_code;
  
  if(! op.ndbop) {
    op.result.success = false;
    op.result.error = new IndirectError(transactionHandler.error);
    return;
  }
  
  op_ndb_error = op.ndbop.getNdbError();
  result_code = op_ndb_error.code;

  if(execMode !== ROLLBACK) {
    /* Error Handling */
    if(result_code === 0) {
      if(transactionHandler.error) {
        /* This operation has no error, but the transaction failed. */
        udebug.log("Case txErr + opOK", transactionHandler.moniker);
        op.result.success = false;
        op.result.error = new IndirectError(transactionHandler.error);      
      }
      else {
        udebug.log("Case txOK + opOK", transactionHandler.moniker);
        op.result.success = true;
      }
    }
    else {
      /* This operation has an error. */
      op.result.success = false;
      op.result.error = new DBOperationError(op_ndb_error);
      if(transactionHandler.error) {
        udebug.log("Case txErr + OpErr", transactionHandler.moniker);
        if(! transactionHandler.error.cause) {
          transactionHandler.error.cause = op.result.error;
        }
      }
      else {
        if(op.opcode === opcodes.OP_READ || execMode === NOCOMMIT) {
          udebug.log("Case txOK + OpErr [READ | NOCOMMIT]", transactionHandler.moniker);
        }
        else {
          udebug.log("Case txOK + OpErr", transactionHandler.moniker);
          transactionHandler.error = new IndirectError(op.result.error);        
        }
      }
    }

    if(op.result.success && op.opcode === opcodes.OP_READ) {
      // readResultRow(op);
      buildValueObject(op);
    } 
  }
  stats.incr( [ "result_code", result_code ] );
  udebug.log_detail("buildOperationResult finished:", op.result);
}

function completeExecutedOps(dbTxHandler, execMode, operationsList) {
  udebug.log("completeExecutedOps mode:", execMode, 
             "operations: ", operationsList.length);
  var op;
  while(op = operationsList.shift()) {
    assert(op.state === "PREPARED");

    if(! op.isScanOperation()) {
      releaseKeyBuffer(op);
      buildOperationResult(dbTxHandler, op, execMode);
      releaseRowBuffer(op);
    }

    dbTxHandler.executedOperations.push(op);
    op.state = doc.OperationStates[2];  // COMPLETED
    if(typeof op.userCallback === 'function') {
      op.userCallback(op.result.error, op);
    }
  }
  udebug.log("completeExecutedOps done");
}


function storeNativeConstructorInMapping(dbTableHandler) {
  var i, nfields, record, fieldNames, typeConverters;
  var VOC, DOC;  // Value Object Constructor, Domain Object Constructor
  if(dbTableHandler.ValueObject) { 
    return;
  }
  /* Step 1: Create Record
     getRecordForMapping(table, ndb, nColumns, columns array)
  */
  nfields = dbTableHandler.fieldNumberToColumnMap.length;
  record = adapter.impl.DBDictionary.getRecordForMapping(
    dbTableHandler.dbTable,
    dbTableHandler.dbTable.per_table_ndb,
    nfields,
    dbTableHandler.fieldNumberToColumnMap
  );

  /* Step 2: Get NdbRecordObject Constructor
    getValueObjectConstructor(record, fieldNames, typeConverters)
  */
  fieldNames = {};
  typeConverters = {};
  for(i = 0 ; i < nfields ; i++) {
    fieldNames[i] = dbTableHandler.resolvedMapping.fields[i].fieldName;
    typeConverters[i] = dbTableHandler.fieldNumberToColumnMap[i].typeConverter;
  }

  VOC = adapter.impl.getValueObjectConstructor(record, fieldNames, typeConverters);

  /* Apply the user's prototype */
  DOC = dbTableHandler.newObjectConstructor;
  if(DOC && DOC.prototype) {
    VOC.prototype = DOC.prototype;
  }

  /* Store the VOC in the mapping */
  dbTableHandler.ValueObject = VOC;
}

function verifyIndexHandler(dbIndexHandler) {
  if(! dbIndexHandler.tableHandler) { throw ("Invalid dbIndexHandler"); }
}

function newReadOperation(tx, dbIndexHandler, keys, lockMode) {
  verifyIndexHandler(dbIndexHandler);
  var op = new DBOperation(opcodes.OP_READ, tx, dbIndexHandler, null);
  op.keys = keys;

  if(! dbIndexHandler.tableHandler.ValueObject) {
    storeNativeConstructorInMapping(dbIndexHandler.tableHandler);
  }

  assert(doc.LockModes.indexOf(lockMode) !== -1);
  if(op.index.isPrimaryKey || lockMode === "EXCLUSIVE") {
    op.lockMode = lockMode;
  }
  else {
    op.lockMode = "SHARED";
  }
  return op;
}


function newInsertOperation(tx, tableHandler, row) {
  var op = new DBOperation(opcodes.OP_INSERT, tx, null, tableHandler);
  op.values = row;
  return op;
}


function newDeleteOperation(tx, dbIndexHandler, keys) {
  verifyIndexHandler(dbIndexHandler);
  var op = new DBOperation(opcodes.OP_DELETE, tx, dbIndexHandler, null);
  op.keys = keys;
  return op;
}


function newWriteOperation(tx, dbIndexHandler, row) {
  verifyIndexHandler(dbIndexHandler);
  var op = new DBOperation(opcodes.OP_WRITE, tx, dbIndexHandler, null);
  op.keys = dbIndexHandler.getFields(row);
  op.values = row;
  return op;
}


function newUpdateOperation(tx, dbIndexHandler, keys, row) {
  verifyIndexHandler(dbIndexHandler);
  var op = new DBOperation(opcodes.OP_UPDATE, tx, dbIndexHandler, null);
  op.keys = keys;
  op.values = row;
  return op;
}


function newScanOperation(tx, QueryTree, properties) {
  var queryHandler = QueryTree.mynode_query_domain_type.queryHandler;
  var op = new DBOperation(opcodes.OP_SCAN, tx, 
                           queryHandler.dbIndexHandler, 
                           queryHandler.dbTableHandler);
  prepareFilterSpec(queryHandler);
  op.query = queryHandler;
  op.keys = properties;
  return op;
}


exports.DBOperation         = DBOperation;
exports.newReadOperation    = newReadOperation;
exports.newInsertOperation  = newInsertOperation;
exports.newDeleteOperation  = newDeleteOperation;
exports.newUpdateOperation  = newUpdateOperation;
exports.newWriteOperation   = newWriteOperation;
exports.newScanOperation    = newScanOperation;
exports.completeExecutedOps = completeExecutedOps;
exports.getScanResults      = getScanResults;
