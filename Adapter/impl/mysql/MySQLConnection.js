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

/*global udebug, util */

"use strict";

var mysql = require("mysql");

/** MySQLConnection wraps a mysql connection and implements the DBSession contract */
exports.DBSession = function(pooledConnection, connectionPool) {
  if (arguments.length !== 2) {
    throw new Error('Fatal internal exception: expected 2 arguments; got ' + arguments.length);
  } else {
    if (pooledConnection === null) {
      throw new Error('Fatal internal exception: got null for pooledConnection');
    }
    this.pooledConnection = pooledConnection; 
    this.connectionPool = connectionPool;
  }
};

exports.DBSession.prototype.TransactionHandler = function(dbSession) {

  var transactionHandler = this;
  this.isOpen = true;
  this.dbSession = dbSession;
  this.executedOperations = [];

  this.execute = function(type, callback) {
    var err = new Error('not implemented: MySQLConnection.TransactionHandler.execute');
    callback(err, this);
  };

  this.close = function() {
  };

  function operationCompleteCallback(completedOperation) {
    transactionHandler.executedOperations.push(completedOperation);
    var complete = transactionHandler.executedOperations.length;
    if (complete === transactionHandler.numberOfOperations) {
      udebug.log('MySQLConnection.TransactionHandler.operationCompleteCallback done: ' +
          ' completed ' + complete + ' of ' + transactionHandler.numberOfOperations);
      if (typeof(transactionHandler.transactionExecuteCallback) === 'function') {
        transactionHandler.transactionExecuteCallback(null, transactionHandler);
      }
    } else {
      udebug.log('MySQLConnection.TransactionHandler.operationCompleteCallback ' +
          ' completed ' + complete + ' of ' + transactionHandler.numberOfOperations);
    }
  }

  this.executeNoCommit = function(operationsList, transactionExecuteCallback) {
    transactionHandler.transactionExecuteCallback = transactionExecuteCallback;
    transactionHandler.numberOfOperations = operationsList.length;
    operationsList.forEach(function(operation) {
      if (transactionHandler.dbSession.pooledConnection === null) {
        throw new Error('Fatal internal exception: got null for pooledConnection');
      }
      operation.execute(transactionHandler.dbSession.pooledConnection, operationCompleteCallback);
    });
  };

  this.executeCommit = function(operationsList, transactionExecuteCallback) {
    transactionHandler.transactionExecuteCallback = transactionExecuteCallback;
    transactionHandler.numberOfOperations = operationsList.length;
    operationsList.forEach(function(operation) {
      if (transactionHandler.dbSession.pooledConnection === null) {
        throw new Error('Fatal internal exception: got null for pooledConnection');
      }
      operation.execute(transactionHandler.dbSession.pooledConnection, operationCompleteCallback);
    });
  };

};


exports.DBSession.prototype.createTransaction = function() {
  var transactionHandler = new this.TransactionHandler(this);
  return transactionHandler;
};

exports.DBSession.translateError = function(code) {
  switch(code) {
  case 'ER_DUP_ENTRY': return 121;
  }  
};

function InsertOperation(sql, data, callback) {
  udebug.log('MySQLConnection.dbSession.InsertOperation with ' + util.inspect(sql) + ' ' + util.inspect(data));

  var op = this;
  this.sql = sql;
  this.data = data;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onInsert(err, status) {
    if (err) {
      udebug.log('MySQLConnection.dbSession.InsertOperation err callback: ' + util.inspect(err));
      op.result.error.code = exports.DBSession.translateError(err.code);
      udebug.log('MySQLConnection.dbSession.InsertOperation err code: ' + util.inspect(err.code) + 
          ' ' + op.result.error.code);
      op.result.success = false;
      if (typeof(op.callback) === 'function') {
        op.callback(err, null);
      }
    } else {
      udebug.log('MySQLConnection.dbSession.InsertOperation NO ERROR callback: ' + JSON.stringify(status));
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

function DeleteOperation(sql, keys, callback) {
  udebug.log('MySQLConnection.dbSession.DeleteOperation with ' + util.inspect(sql) + ' ' + util.inspect(keys));
  var op = this;
  this.sql = sql;
  this.keys = keys;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onDelete(err, status) {
    if (err) {
      udebug.log('MySQLConnection.dbSession.DeleteOperation err callback: ' + err);
      if (typeof(op.callback) === 'function') {
        op.callback(err, op);
      }
    } else {
      udebug.log('MySQLConnection.dbSession.DeleteOperation NO ERROR callback: ' + JSON.stringify(status));
      if (status.affectedRows === 1) {
        op.result.success = true;
        op.result.error.code = 0;
      } else {
        udebug.log('MySQLConnection.dbSession.DeleteOperation NO ERROR callback with no deleted rows');
        op.result.success = false;
        op.result.error.code = 120;
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
  udebug.log('MySQLConnection.dbSession.ReadOperation with ' + util.inspect(sql) + ' ' + util.inspect(keys));
  var op = this;
  this.sql = sql;
  this.keys = keys;
  this.callback = callback;
  this.result = {};
  this.result.error = {};

  function onRead(err, rows) {
    if (err) {
      udebug.log('MySQLConnection.dbSession.ReadOperation err callback: ' + err);
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
        udebug.log('MySQLConnection.dbSession.ReadOperation ONE RESULT callback: ' + util.inspect(rows[0]));
        op.result.value = rows[0];
        op.result.success = true;
        op.result.error.code = 0;
        if (typeof(op.callback) === 'function') {
          op.callback(null, op);
        }
      } else {
        udebug.log('MySQLConnection.dbSession.ReadOperation NO RESULTS callback.');
        op.result.value = null;
        op.result.success = false;
        op.result.error = 120;
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
  udebug.log('MySQLConnection.dbSession.UpdateOperation with ' + util.inspect(sql) + ' ' + util.inspect(values) +
      ' ' + util.inspect(keys));
  var op = this;
  this.sql = sql;
  this.keys = keys;
  this.values = values;
  this.callback = callback;
  this.result = {};
  this.result.error = 0;

  function onUpdate(err, status) {
    if (err) {
      udebug.log('MySQLConnection.dbSession.UpdateOperation err callback: ' + err);
      if (typeof(op.callback) === 'function') {
        op.callback(err, op);
      }
    } else {
      udebug.log('MySQLConnection.dbSession.UpdateOperation NO ERROR callback: ' + JSON.stringify(status));
      if (status.affectedRows === 1) {
        op.result.success = true;
        op.result.error = 0;
      } else {
        op.result.success = false;
        op.result.error = 120;
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
  udebug.log_detail('MySQLConnection.getMetadata with columns', JSON.stringify(columns));
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
  udebug.log_detail('MySQLConnection.getMetadata insertSQL: ' + dbTableHandler.mysql.insertSQL);
  udebug.log_detail('MySQLConnection.getMetadata duplicateSQL: ' + dbTableHandler.mysql.duplicateSQL);
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
      udebug.log_detail('MySQLConnection.createDeleteSQL indexMetadata: ' + JSON.stringify(indexMetadata));
      for (j = 0; j < indexMetadata.columnNumbers.length; ++j) {
        deleteSQL += separator + columns[indexMetadata.columnNumbers[j]].name + ' = ?';
        separator = ' AND ';
      }
    }
  }
  udebug.log_detail('MySQLConnection.getMetadata deleteSQL for ' + index + ': ' + deleteSQL);
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
  udebug.log_detail('MySQLConnection.getMetadata selectSQL for ' + index + ': ' + selectSQL);
  return selectSQL;
}

function getMetadata(dbTableHandler) {
  if (dbTableHandler.mysql) {
    return;
  }
  udebug.log_detail('MySQLConnection.getMetadata with dbTableHandler ' + JSON.stringify(dbTableHandler));
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
  udebug.log_detail('MySQLConnection.dbSession.buildInsertOperation with tableHandler: ' + util.inspect(dbTableHandler) +
      ' object: ' + util.inspect(object));
  getMetadata(dbTableHandler);
  var fields = dbTableHandler.getFields(object);
  var insertSQL = dbTableHandler.mysql.insertSQL;
  return new InsertOperation(insertSQL, fields, callback);
};


exports.DBSession.prototype.buildDeleteOperation = function(dbIndexHandler, keys, transaction, callback) {
  udebug.log_detail('MySQLConnection.dbSession.buildReadOperation with indexHandler: ' + util.inspect(dbIndexHandler) +
      util.inspect(keys));
  var dbTableHandler = dbIndexHandler.tableHandler;
  var fields = dbIndexHandler.getFields(keys);
  getMetadata(dbTableHandler);
  var deleteSQL = dbTableHandler.mysql.deleteSQL[dbIndexHandler.dbIndex.name];
  return new DeleteOperation(deleteSQL, fields, callback);
};


exports.DBSession.prototype.buildReadOperation = function(dbIndexHandler, keys, transaction, callback) {
  udebug.log_detail('MySQLConnection.dbSession.buildReadOperation with indexHandler: ' + util.inspect(dbIndexHandler) +
      util.inspect(keys));
  var dbTableHandler = dbIndexHandler.tableHandler;
  var fields = dbIndexHandler.getFields(keys);
  getMetadata(dbTableHandler);
  var selectSQL = dbTableHandler.mysql.selectSQL[dbIndexHandler.dbIndex.name];
  return new ReadOperation(selectSQL, fields, callback);
};


exports.DBSession.prototype.buildUpdateOperation = function(dbIndexHandler, object, transaction, callback) {
  udebug.log_detail('MySQLConnection.dbSession.buildUpdateOperation with indexHandler: ' + util.inspect(dbIndexHandler) +
      util.inspect(object));
  var dbTableHandler = dbIndexHandler.tableHandler;
  var keyFields = dbIndexHandler.getFields(object);
  // build the SQL Update statement along with the data values
  var updateSetSQL = 'UPDATE ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' SET ';
  var updateWhereSQL = ' WHERE ';
  var separatorWhereSQL = '';
  var separatorUpdateSetSQL = '';
  var updateFields = [];
  // get an array of key field names
  var keyFieldNames = [];
  var j;
  for(j = 0 ; j < dbIndexHandler.fieldNumberToFieldMap.length ; j++) {
    keyFieldNames.push(dbIndexHandler.fieldNumberToFieldMap[j].fieldName);
  }
  var x, columnName;
  for (x in object) {
    if (object.hasOwnProperty(x)) {
      if (keyFieldNames.indexOf(x) !== -1) {
        // add the key field to the WHERE clause
        columnName = dbTableHandler.fieldNameToFieldMap[x].columnName;
        updateWhereSQL += separatorWhereSQL + columnName + ' = ? ';
        separatorWhereSQL = 'AND ';
      } else {
        // add the value in the object to the updateFields
        updateFields.push(object[x]);
        // add the value field to the SET clause
        columnName = dbTableHandler.fieldNameToFieldMap[x].columnName;
        updateSetSQL += separatorUpdateSetSQL + columnName + ' = ?';
        separatorUpdateSetSQL = ', ';
      }
    }
  }
  updateSetSQL += updateWhereSQL;
  udebug.log('MySQLConnection.dbSession.buildUpdateOperation SQL: ' + updateSetSQL);
  return new UpdateOperation(updateSetSQL, keyFields, updateFields, callback);
};


exports.DBSession.prototype.closeSync = function() {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
};


exports.DBSession.prototype.close = function(callback) {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
  if (callback) {
    callback(null, null);
  }
};


exports.DBSession.prototype.getConnectionPool = function() {
  return this.connectionPool;
};
