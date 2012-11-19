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

/* Requires version 2.0 of Felix Geisendoerfer's MySQL client */

/*global unified_debug, util */

"use strict";

var mysql  = require("mysql"),
    udebug = unified_debug.getLogger("MySQLConnection.js");


/** MySQLConnection wraps a mysql connection and implements the DBSession contract */
exports.DBSession = function(pooledConnection, connectionPool) {
  if (arguments.length !== 2) {
    throw new Error('Fatal internal exception: expected 2 arguments; got ' + arguments.length);
  } else {
    if (typeof(pooledConnection) === 'undefined') {
      throw new Error('Fatal internal exception: got undefined for pooledConnection');
    }
    if (pooledConnection === null) {
      throw new Error('Fatal internal exception: got null for pooledConnection');
    }
    this.pooledConnection = pooledConnection; 
    this.connectionPool = connectionPool;
    this.transactionHandler = null;
    this.autocommit = true;
    this.index = -1;
  }
};

exports.DBSession.prototype.TransactionHandler = function(dbSession) {

  var transactionHandler = this;
  this.isOpen = true;
  this.dbSession = dbSession;
  this.executedOperations = [];
  this.firstTime = !dbSession.autocommit;
  this.autocommit = dbSession.autocommit;
  this.pendingOperationsList = [];

  this.executeOperations = function() {
    // transactionHandler.operationsList must have been set before calling executeOperations
    // transactionHandler.transactionExecuteCallback must also have been set
    transactionHandler.isCommitting = false;
    transactionHandler.numberOfOperations = transactionHandler.operationsList.length;
    udebug.log('MySQLConnection.TransactionHandler.executeOperations numberOfOperations: ',
        transactionHandler.numberOfOperations);
    transactionHandler.operationsList.forEach(function(operation) {
      if (transactionHandler.dbSession.pooledConnection === null) {
        throw new Error(
            'Fatal internal exception: MySQLConnection.TransactionHandler.executeOperations ' +
            'got null for pooledConnection');
      }
      operation.execute(transactionHandler.dbSession.pooledConnection, transactionHandler.operationCompleteCallback);
    });
  };

  this.execute = function(operationsList, transactionExecuteCallback) {
    transactionHandler = this;
    
    var executeOnBegin = function(err) {
      if (err) {
        transactionHandler.transactionExecuteCallback(err);
      }
      transactionHandler.firstTime = false;
      transactionHandler.executeOperations();
    };

    // execute begin operation the first time for non-autocommit
    if (this.firstTime) {
      transactionHandler.operationsList = operationsList;
      transactionHandler.transactionExecuteCallback = transactionExecuteCallback;
      this.dbSession.pooledConnection.query('begin', executeOnBegin);
    } else {
      if (transactionHandler.numberOfOperations > 0) {
        // there are pending operations, so just put this request on the list
        transactionHandler.pendingOperationsList.push(
            {list: operationsList, 
             callback: transactionExecuteCallback
            });
      } else {
        // this is the first (only) so execute it now
        transactionHandler.operationsList = operationsList;
        transactionHandler.transactionExecuteCallback = transactionExecuteCallback;
        transactionHandler.executeOperations();
      }
    }
  };

  
  this.close = function() {
  };

  this.operationCompleteCallback = function(completedOperation) {
    transactionHandler.executedOperations.push(completedOperation);
    var complete = transactionHandler.executedOperations.length;
    if (complete === transactionHandler.numberOfOperations) {
      udebug.log('MySQLConnection.TransactionHandler.operationCompleteCallback done: completed',
                 complete, 'of', transactionHandler.numberOfOperations);
      if (typeof(transactionHandler.transactionExecuteCallback) === 'function') {
        transactionHandler.transactionExecuteCallback(null, transactionHandler);
      } 
      // reset executedOperations if the transaction execute callback did not pop them
      transactionHandler.executedOperations = [];
      // reset number of operations (after callbacks are done)
      transactionHandler.numberOfOperations = 0;
      // if we committed the transaction, tell dbSession we are gone
      if (transactionHandler.isCommitting) {
        transactionHandler.dbSession.transactionHandler = null;
      }
      // see if there are any pending operations to execute
      // each pending operation consists of an operation list and a callback
      if (transactionHandler.pendingOperationsList.length !== 0) {
        // remove the first pending operations from the list (FIFO)
        var pendingOperations = transactionHandler.pendingOperationsList.shift();
        transactionHandler.operationsList = pendingOperations.list;
        transactionHandler.transactionExecuteCallback = pendingOperations.callback;
        transactionHandler.executeOperations();
      }
    } else {
      udebug.log('MySQLConnection.TransactionHandler.operationCompleteCallback ',
          ' completed ', complete, ' of ', transactionHandler.numberOfOperations);
    }
  };

  this.commit = function(callback) {
    udebug.log('MySQLConnection.TransactionHandler.commit.');
    this.dbSession.pooledConnection.query('commit', callback);
    this.dbSession.transactionHandler = null;
  };

  this.rollback = function(callback) {
    udebug.log('MySQLConnection.TransactionHandler.rollback.');
    this.dbSession.pooledConnection.query('rollback', callback);
    this.dbSession.transactionHandler = null;
  };
};


exports.DBSession.prototype.createTransactionHandler = function() {
  this.transactionHandler = new this.TransactionHandler(this);
  return this.transactionHandler;
};

exports.DBSession.prototype.getTransactionHandler = function() {
  if (this.transactionHandler === null) {
    this.createTransactionHandler();
  }
  return this.transactionHandler;
};

exports.DBSession.translateError = function(code) {
  switch(code) {
  case 'ER_DUP_ENTRY': return "23000";
  }  
};

function InsertOperation(sql, data, callback) {
  udebug.log('dbSession.InsertOperation with', sql, data);

  var op = this;
  this.sql = sql;
  this.data = data;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onInsert(err, status) {
    if (err) {
      op.result.error.code = exports.DBSession.translateError(err.code);
      udebug.log('dbSession.InsertOperation err code:', err.code, op.result.error.code);
      op.result.success = false;
      if (typeof(op.callback) === 'function') {
        op.callback(err, null);
      }
    } else {
      op.result.error.code = 0;
      op.result.value = op.data;
      op.result.success = true;
      if (typeof(op.callback) === 'function') {
        op.callback(null, op);
      }
    }
    // now call the transaction execution callback
    op.operationCompleteCallback(op);
  }

  this.execute = function(connection, operationCompleteCallback) {
    op.operationCompleteCallback = operationCompleteCallback;
    connection.query(this.sql, this.data, onInsert);
  };
}

function WriteOperation(sql, data, callback) {
  udebug.log('dbSession.WriteOperation with', sql, data);

  var op = this;
  this.sql = sql;
  this.data = data;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onWrite(err, status) {
    if (err) {
      op.result.error.code = exports.DBSession.translateError(err.code);
      udebug.log('dbSession.WriteOperation err code:', err.code, op.result.error.code);
      op.result.success = false;
      if (typeof(op.callback) === 'function') {
        op.callback(err, null);
      }
    } else {
      op.result.error.code = 0;
      op.result.value = op.data;
      op.result.success = true;
      if (typeof(op.callback) === 'function') {
        op.callback(null, op);
      }
    }
    // now call the transaction execution callback
    op.operationCompleteCallback(op);
  }

  this.execute = function(connection, operationCompleteCallback) {
    op.operationCompleteCallback = operationCompleteCallback;
    connection.query(this.sql, this.data, onWrite);
  };
}

function DeleteOperation(sql, keys, callback) {
  udebug.log('dbSession.DeleteOperation with ', sql, keys);
  var op = this;
  this.sql = sql;
  this.keys = keys;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onDelete(err, status) {
    if (err) {
      udebug.log('dbSession.DeleteOperation err callback:', err);
      if (typeof(op.callback) === 'function') {
        op.callback(err, op);
      }
    } else {
      udebug.log('dbSession.DeleteOperation NO ERROR callback:', status);
      if (status.affectedRows === 1) {
        op.result.success = true;
        op.result.error.code = 0;
      } else {
        udebug.log('dbSession.DeleteOperation NO ERROR callback with no deleted rows');
        op.result.success = false;
        op.result.error.code = "02000";
      }
      if (typeof(op.callback) === 'function') {
        op.callback(null, op);
      }
    }
    // now call the transaction execution callback
    op.operationCompleteCallback(op);
  }

  this.execute = function(connection, operationCompleteCallback) {
    op.operationCompleteCallback = operationCompleteCallback;
    connection.query(this.sql, this.keys, onDelete);
  };
}

function ReadOperation(sql, keys, callback) {
  udebug.log('dbSession.ReadOperation with', sql, keys);
  var op = this;
  this.sql = sql;
  this.keys = keys;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onRead(err, rows) {
    if (err) {
      udebug.log('dbSession.ReadOperation err callback:', err);
      op.result.value = null;
      op.result.success = false;
      if (typeof(op.callback) === 'function') {
        op.callback(err, op);
      }
    } else {
      if (rows.length > 1) {
        err = new Error('Too many results from read: ' + rows.length);
        if (typeof(op.callback) === 'function') {
          op.callback(err, op);
        }
      } else if (rows.length === 1) {
        udebug.log('dbSession.ReadOperation ONE RESULT callback:', rows[0]);
        op.result.value = rows[0];
        op.result.success = true;
        op.result.error.code = 0;
        if (typeof(op.callback) === 'function') {
          op.callback(null, op);
        }
      } else {
        udebug.log('dbSession.ReadOperation NO RESULTS callback.');
        op.result.value = null;
        op.result.success = false;
        op.result.error = "02000";
        if (typeof(op.callback) === 'function') {
          op.callback(null, op);
        }
      }
    }
    // now call the transaction execution callback
    op.operationCompleteCallback(op);
  }

  this.execute = function(connection, operationCompleteCallback) {
    op.operationCompleteCallback = operationCompleteCallback;
    connection.query(this.sql, this.keys, onRead);
  };
}

function UpdateOperation(sql, keys, values, callback) {
  udebug.log('dbSession.UpdateOperation with', sql, values, keys);
  var op = this;
  this.sql = sql;
  this.keys = keys;
  this.values = values;
  this.callback = callback;
  this.result = {};
  this.result.error = 0;

  function onUpdate(err, status) {
    if (err) {
      udebug.log('dbSession.UpdateOperation err callback:', err);
      if (typeof(op.callback) === 'function') {
        op.callback(err, op);
      }
    } else {
      udebug.log('dbSession.UpdateOperation NO ERROR callback:', status);
      if (status.affectedRows === 1) {
        op.result.success = true;
        op.result.error = 0;
      } else {
        op.result.success = false;
        op.result.error = "02000";
      }
      if (typeof(op.callback) === 'function') {
        op.callback(null, op);
      }
    }
    // now call the transaction execution callback
    op.operationCompleteCallback(op);
  }

  this.execute = function(connection, operationCompleteCallback) {
    op.operationCompleteCallback = operationCompleteCallback;
    connection.query(this.sql, this.values.concat(this.keys), onUpdate);
  };
}

function createInsertSQL(dbTableHandler) {
  // create the insert SQL statement from the table metadata
  var insertSQL = 'INSERT INTO ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' (';
  var valuesSQL = ' VALUES (';
  var duplicateSQL = ' ON DUPLICATE KEY UPDATE ';
  var columns = dbTableHandler.getColumnMetadata();
  udebug.log_detail('getMetadata with columns', columns);
  // loop over the columns and extract the column name
  var columnSeparator = '';
  var duplicateSeparator = '';
  var i, column;  
  for (i = 0; i < columns.length; ++i) {
    column = columns[i];
    insertSQL += columnSeparator + column.name;
    valuesSQL += columnSeparator + '?';
    columnSeparator = ', ';
    if (!column.isInPrimaryKey) {
      duplicateSQL += duplicateSeparator + column.name + ' = VALUES (' + column.name + ') ';
      duplicateSeparator = ', ';
    }
  }
  valuesSQL += ')';
  insertSQL += ')' + valuesSQL;
  dbTableHandler.mysql.insertSQL = insertSQL;
  dbTableHandler.mysql.duplicateSQL = insertSQL + duplicateSQL;
  udebug.log_detail('getMetadata insertSQL:', dbTableHandler.mysql.insertSQL);
  udebug.log_detail('getMetadata duplicateSQL:', dbTableHandler.mysql.duplicateSQL);
  return insertSQL;
}

function createDeleteSQL(dbTableHandler, index) {
  // create the delete SQL statement from the table metadata for the named index
  var deleteSQL = 'DELETE FROM ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' WHERE ';
  // find the index metadata from the dbTableHandler index section
  // loop over the columns in the index and extract the column name
  var indexMetadatas = dbTableHandler.dbTable.indexes;
  var columns = dbTableHandler.getColumnMetadata();
  var separator = '';
  var i, j, indexMetadata;
  for (i = 0; i < indexMetadatas.length; ++i) {
    if (indexMetadatas[i].name === index) {
      indexMetadata = indexMetadatas[i];
      udebug.log_detail('createDeleteSQL indexMetadata: ', indexMetadata);
      for (j = 0; j < indexMetadata.columnNumbers.length; ++j) {
        deleteSQL += separator + columns[indexMetadata.columnNumbers[j]].name + ' = ?';
        separator = ' AND ';
      }
    }
  }
  udebug.log_detail('getMetadata deleteSQL for', index, ':', deleteSQL);
  return deleteSQL;
}

function createSelectSQL(dbTableHandler, index) {
  // create the select SQL statement from the table metadata for the named index
  var selectSQL = 'SELECT ';
  var whereSQL =   ' FROM ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' WHERE ';
  // loop over the mapped column names in order
  var separator = '';
  var i, j, columns, column, fields, field;
  columns = dbTableHandler.getColumnMetadata();
  fields = dbTableHandler.fieldNumberToFieldMap;
  for (i = 0; i < fields.length; ++i) {
    field = fields[i].fieldName;
    column = fields[i].columnName;
    selectSQL += separator + column + ' AS ' + field;
    separator = ', ';
  }

  // loop over the index columns
  // find the index metadata from the dbTableHandler index section
  // loop over the columns in the index and extract the column name
  var indexMetadatas = dbTableHandler.dbTable.indexes;
  separator = '';
  for (i = 0; i < indexMetadatas.length; ++i) {
    if (indexMetadatas[i].name === index) {
      var indexMetadata = indexMetadatas[i];
      for (j = 0; j < indexMetadata.columnNumbers.length; ++j) {
        whereSQL += separator + columns[indexMetadata.columnNumbers[j]].name + ' = ? ';
        separator = ' AND ';
      }
    }
  }
  selectSQL += whereSQL;
  udebug.log_detail('getMetadata selectSQL for', index +':', selectSQL);
  return selectSQL;
}

function getMetadata(dbTableHandler) {
  if (dbTableHandler.mysql) {
    return;
  }
  udebug.log_detail('getMetadata with dbTableHandler', dbTableHandler);
  dbTableHandler.mysql = {};
  dbTableHandler.mysql.indexes = {};
  dbTableHandler.mysql.deleteSQL = {};
  dbTableHandler.mysql.selectSQL = {};
  createInsertSQL(dbTableHandler);
  var i, indexes, index;
  // create a delete statement and select statement per index
  indexes = dbTableHandler.dbTable.indexes;
  for (i = 0; i < indexes.length; ++i) {
    index = dbTableHandler.dbTable.indexes[i];
    dbTableHandler.mysql.deleteSQL[index.name] = createDeleteSQL(dbTableHandler, index.name);
    dbTableHandler.mysql.selectSQL[index.name] = createSelectSQL(dbTableHandler, index.name);
  }
}

exports.DBSession.prototype.buildInsertOperation = function(dbTableHandler, object, transaction, callback) {
  udebug.log_detail('dbSession.buildInsertOperation with tableHandler:', 
                    dbTableHandler, 'object:', object);
  getMetadata(dbTableHandler);
  var fields = dbTableHandler.getFields(object);
  var insertSQL = dbTableHandler.mysql.insertSQL;
  return new InsertOperation(insertSQL, fields, callback);
};


exports.DBSession.prototype.buildDeleteOperation = function(dbIndexHandler, keys, transaction, callback) {
  udebug.log_detail('dbSession.buildDeleteOperation with indexHandler:', dbIndexHandler, keys);
  var dbTableHandler = dbIndexHandler.tableHandler;
  var fields = dbIndexHandler.getFields(keys);
  getMetadata(dbTableHandler);
  var deleteSQL = dbTableHandler.mysql.deleteSQL[dbIndexHandler.dbIndex.name];
  return new DeleteOperation(deleteSQL, fields, callback);
};


exports.DBSession.prototype.buildReadOperation = function(dbIndexHandler, keys, transaction, callback) {
  udebug.log_detail('dbSession.buildReadOperation with indexHandler:' + dbIndexHandler, keys);
  var dbTableHandler = dbIndexHandler.tableHandler;
  var fields = dbIndexHandler.getFields(keys);
  getMetadata(dbTableHandler);
  var selectSQL = dbTableHandler.mysql.selectSQL[dbIndexHandler.dbIndex.name];
  return new ReadOperation(selectSQL, fields, callback);
};


//exports.DBSession.prototype.buildUpdateOperation = function(dbIndexHandler, object, transaction, callback) {
//udebug.log_detail('dbSession.buildUpdateOperation with indexHandler:', dbIndexHandler, object);
//var dbTableHandler = dbIndexHandler.tableHandler;
//var keyFields = dbIndexHandler.getFields(object);
//// build the SQL Update statement along with the data values
//var updateSetSQL = 'UPDATE ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' SET ';
//var updateWhereSQL = ' WHERE ';
//var separatorWhereSQL = '';
//var separatorUpdateSetSQL = '';
//var updateFields = [];
//// get an array of key field names
//var keyFieldNames = [];
//var j;
//for(j = 0 ; j < dbIndexHandler.fieldNumberToFieldMap.length ; j++) {
//keyFieldNames.push(dbIndexHandler.fieldNumberToFieldMap[j].fieldName);
//}
//var x, columnName;
//for (x in object) {
//if (object.hasOwnProperty(x)) {
//  if (keyFieldNames.indexOf(x) !== -1) {
//    // add the key field to the WHERE clause
//    columnName = dbTableHandler.fieldNameToFieldMap[x].columnName;
//    updateWhereSQL += separatorWhereSQL + columnName + ' = ? ';
//    separatorWhereSQL = 'AND ';
//  } else {
//    // add the value in the object to the updateFields
//    updateFields.push(object[x]);
//    // add the value field to the SET clause
//    columnName = dbTableHandler.fieldNameToFieldMap[x].columnName;
//    updateSetSQL += separatorUpdateSetSQL + columnName + ' = ?';
//    separatorUpdateSetSQL = ', ';
//  }
//}
//}
//updateSetSQL += updateWhereSQL;
//udebug.log('dbSession.buildUpdateOperation SQL:', updateSetSQL);
//return new UpdateOperation(updateSetSQL, keyFields, updateFields, callback);
//};


exports.DBSession.prototype.buildUpdateOperation = function(dbIndexHandler, keys, values, transaction, callback) {
  udebug.log('dbSession.buildUpdateOperation with indexHandler:', dbIndexHandler.dbIndex, keys, values);
  var object;
  var dbTableHandler = dbIndexHandler.tableHandler;
  getMetadata(dbTableHandler);
  // build the SQL Update statement along with the data values
  var updateSetSQL = 'UPDATE ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' SET ';
  var updateWhereSQL = ' WHERE ';
  var separatorWhereSQL = '';
  var separatorUpdateSetSQL = '';
  var updateFields = [];
  var keyFields = [];
  // get an array of key field names
  var valueFieldName, keyFieldNames = [];
  var j, field;
  for(j = 0 ; j < dbIndexHandler.fieldNumberToFieldMap.length ; j++) {
    keyFieldNames.push(dbIndexHandler.fieldNumberToFieldMap[j].fieldName);
  }
  // get an array of persistent field names
  var valueFieldNames = [];
  for(j = 0 ; j < dbTableHandler.fieldNumberToFieldMap.length ; j++) {
    field = dbTableHandler.fieldNumberToFieldMap[j];
    valueFieldName = field.fieldName;
    // exclude not persistent fields and fields that are part of the index
    if (!field.NotPersistent && keyFieldNames.indexOf(valueFieldName) === -1) {
      valueFieldNames.push(valueFieldName);
    }
  }
  
  var x, columnName;
  for (x in keys) {
    if (keys.hasOwnProperty(x)) {
      if (keyFieldNames.indexOf(x) !== -1) {
        // add the key value to the keyFields
        keyFields.push(keys[x]);
        // add the key field to the WHERE clause
        columnName = dbTableHandler.fieldNameToFieldMap[x].columnName;
        updateWhereSQL += separatorWhereSQL + columnName + ' = ? ';
        separatorWhereSQL = 'AND ';
      }
    }
  }
  for (x in values) {
    if (values.hasOwnProperty(x)) {
      if (valueFieldNames.indexOf(x) !== -1) {
        // add the value in the object to the updateFields
        updateFields.push(values[x]);
        // add the value field to the SET clause
        columnName = dbTableHandler.fieldNameToFieldMap[x].columnName;
        updateSetSQL += separatorUpdateSetSQL + columnName + ' = ?';
        separatorUpdateSetSQL = ', ';
      }
    }
  }

updateSetSQL += updateWhereSQL;
udebug.log('dbSession.buildUpdateOperation SQL:', updateSetSQL);
return new UpdateOperation(updateSetSQL, keyFields, updateFields, callback);
};

exports.DBSession.prototype.buildWriteOperation = function(dbIndexHandler, values, transaction, callback) {
  udebug.log_detail('buildWriteOperation with indexHandler:', dbIndexHandler, values);
  var dbTableHandler = dbIndexHandler.tableHandler;
  var fieldValues = dbTableHandler.getFields(values);
  getMetadata(dbTableHandler);
  // get the field metadata object for each value in field metadata order
  var fieldIndexes = dbTableHandler.allFieldsIncluded(fieldValues);
  udebug.log_detail('buildWriteOperation fieldValues: ', fieldValues, ' fieldIndexes: ', fieldIndexes);
  // if the values include all mapped fields, use the pre-built dbTableHandler.mysql.duplicateSQL
  if (fieldValues.length === fieldIndexes.length) {
    return new WriteOperation(dbTableHandler.mysql.duplicateSQL, fieldValues, callback);
  }
  // build the SQL insert statement along with the data values
  var writeSQL = 'INSERT INTO ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' (';
  var valuesSQL = ') VALUES (';
  var duplicateSQL = ' ON DUPLICATE KEY UPDATE ';
  var duplicateClause = '';
  var separator = ' ';
  var statementValues = [];
  var i;
  var fieldIndex;
  var f;
  
  for (i = 0; i < fieldIndexes.length; ++i) {
    fieldIndex = fieldIndexes[i];
    f = dbTableHandler.fieldNumberToFieldMap[fieldIndex];
    // add the column name to the write SQL
    writeSQL += separator + f.columnName;
    // add the column name to the duplicate SQL
    // add the value to the values SQL
    duplicateClause = separator + f.columnName + ' = VALUES (' + f.columnName + ')';
    duplicateSQL += duplicateClause;
    valuesSQL += separator + '?';
    separator = ', ';
    statementValues.push(fieldValues[fieldIndex]);
  }
  
  valuesSQL += ')';
  writeSQL += valuesSQL;
  writeSQL += duplicateSQL;

  udebug.log_detail('dbSession.buildWriteOperation SQL:', writeSQL);
  return new WriteOperation(writeSQL, statementValues, callback);
};

exports.DBSession.prototype.begin = function() {
  udebug.log('dbSession.begin');
  this.autocommit = false;
  this.transactionHandler = new this.TransactionHandler(this);
  this.transactionHandler.autocommit = false;
};

exports.DBSession.prototype.commit = function(callback) {
  this.transactionHandler.commit(callback);
  this.autocommit = true;
};

exports.DBSession.prototype.rollback = function(callback) {
  this.transactionHandler.rollback(callback);
  this.autocommit = true;
};

exports.DBSession.prototype.closeSync = function() {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
};


exports.DBSession.prototype.close = function(callback) {
  udebug.log('MySQLConnection.close');
  if (this.pooledConnection) {
    // TODO put the pooled connection back into the pool instead of ending it
    this.pooledConnection.end();
    this.pooledConnection = null;
    this.connectionPool.pooledConnections[this.index] = null;
  }
  if (callback) {
    callback(null, null);
  }
};


exports.DBSession.prototype.getConnectionPool = function() {
  return this.connectionPool;
};
