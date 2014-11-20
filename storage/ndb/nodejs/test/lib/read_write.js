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

var udebug = unified_debug.getLogger("lib/read_write.js");
var mysql = require("mysql");
var util = require("util");

/** This is the test for data conversions. It reads using one implementation
 * (either a mysql-js adapter or a sql driver) and writes using an
 * implementation (either a mysql-js adapter or a sql driver) and
 * then compares the results. 
 */

/** Create a driver for sql connections.
 */
function SQLDriver(connectionProperties) {
  /* Translate our properties to the driver's */
  this.props = {};
  if(connectionProperties.mysql_socket) {
    this.props.SocketPath = connectionProperties.mysql_socket;
  } else {
    this.props.host = connectionProperties.mysql_host;
    this.props.port = connectionProperties.mysql_port;
  }
  if(connectionProperties.mysql_user) {
    this.props.user = connectionProperties.mysql_user;
  }
  if(connectionProperties.mysql_password) {
    this.props.password = connectionProperties.mysql_password;
  }
  this.props.database = connectionProperties.database;
  this.props.debug = connectionProperties.mysql_debug;
  udebug.log('SQLDriver using properties: ', util.inspect(this.props));
};

SQLDriver.prototype.connect = function(callback) {
  this.connection = mysql.createConnection(this.props);
  this.connection.connect(callback);
};

SQLDriver.prototype.remove = function(tableMapping, element, callback) {
  // create delete statement and values object
  var statement = 'delete from ' + tableMapping.table + ' where ';
  var keys = [];
  var separator = '';
  tableMapping.pkFields.forEach(function(pkField) {
    statement += separator;
    statement += pkField.columnName;
    statement += ' = ?';
    
    keys.push(element[pkField.fieldName]);
    separator = ' and ';
  });
  udebug.log_detail(statement, ' with keys: ', keys);
  this.connection.query(statement, keys, callback);
};

SQLDriver.prototype.insert = function(tableMapping, element, callback, rw, index) {
  function onInsert(err) {
    callback(err, rw, index);
  }

  var insertClause = 'insert into ' + tableMapping.table + ' (';
  var valueClause = ') values (';
  var values = [];
  var value;
  var separator = '';
  tableMapping.fields.forEach(function(field) {
    value = element[field.fieldName];
    // only send defined values (including explicit nulls)
    if (typeof(value) !== 'undefined') {
      insertClause += separator;
      insertClause += field.columnName;
      valueClause += separator;
      valueClause += '? ';
      separator = ', ';
      values.push(value);
    }
  });
  insertClause += valueClause;
  insertClause += ')';
  udebug.log_detail(insertClause, ' with values: ', values);
  this.connection.query(insertClause, values, onInsert);
};

SQLDriver.prototype.select = function(tableMapping, element, callback, rw, index) {
  function onSelect(err, results) {
    callback(err, results[0], rw, index);
  }

  var selectClause = 'select ';
  var whereClause = ' from ' + tableMapping.table + ' where ';
  var keys = [];
  var separator = '';
  // append ", columnName as fieldName"
  tableMapping.fields.forEach(function(field) {
    selectClause += separator;
    selectClause += field.columnName;
    selectClause += ' as ';
    selectClause += field.fieldName;
    separator = ', ';
  });
  separator = '';
  // append ", pkField = ?"
  tableMapping.pkFields.forEach(function(pkField) {
    whereClause += separator;
    whereClause += pkField.columnName;
    whereClause += ' = ?';
    
    keys.push(element[pkField.fieldName]);
    separator = ' and ';
  });
  selectClause += whereClause;
  udebug.log_detail(selectClause, ' with keys: ', keys);
  this.connection.query(selectClause, keys, onSelect);
};

/** Augment the table mapping with additional metadata to facilitate creating
 * SQL from a domain object. Primary key columns will be identified with the
 * column name and corresponding field name.
 */
function augmentTableMapping(tableMapping, tableMetadata) {
  // create list of primary key columns
  var pkColumns = [];
  tableMetadata.columns.forEach(function(column) {
    if (column.isInPrimaryKey) {
      pkColumns.push(column.name);
    }
  });
  tableMapping.pkFields = [];
  tableMapping.fields.forEach(function(field) {
    if (pkColumns.indexOf(field.columnName) !== -1) {
      tableMapping.pkFields.push({fieldName: field.fieldName, columnName: field.columnName});
    }
  });
  udebug.log('augmentTableMapping ', tableMapping.pkFields);
};

/** Construct a new ReadWrite for a write implementation,
 * a read implementation, and test data.
 * The write implementation can be either an adapter or a driver.
 * The read implementation can be either an adapter or a driver.
 * The test data can be a domain object or a plain object.
 * 
 */
var ReadWrite = function(testCase, tableNameOrConstructor, data, session, SQLDriver) {
  this.testCase = testCase;
  this.tableNameOrConstructor = tableNameOrConstructor;
  this.data = data;
  this.session = session;
  this.SQLDriver = SQLDriver;
  this.numberChecked = 0;
  this.numberRemoved = 0;
};

/** Set up the structure for running the tests:
 * Make sure the data object is an array.
 * Get the table tableMapping for the domain object or table.
 * Connect to the sql driver.
 * Get the tableMetadata for the domain object or table.
 * Delete all rows corresponding to the data.
 */
ReadWrite.prototype.setUp = function(callback) {
  var rw = this;

  function onRemove(err) {
    if (err) {
      // delete will return 02000 if no row deleted
      if (err.sqlstate !== '02000') {
        rw.testCase.appendErrorMessage(util.inspect(err));
      }
    }
    if (++rw.numberRemoved == rw.data.length) {
      // continue if no errors
      if (rw.testCase.hasNoErrors()) {
        callback();
      } else {
        rw.testCase.failOnError();
      }
    }
  }

  function onTableMetadata(err, tableMetadata) {
    if (err) {
      rw.testCase.appendErrorMessage('sql driver failed to connect.' + err);
      rw.sqlDriver = undefined;
      rw.testCase.fail();
    } else {
      rw.tableMetadata = tableMetadata;
      augmentTableMapping(rw.tableMapping, rw.tableMetadata);
      // delete all rows corresponding to data
      rw.data.forEach(function(element) {
        rw.remove(element, onRemove);
      });
    }
  }

  function onConnect(err) {
    if (err) {
      rw.testCase.appendErrorMessage('sql driver failed to connect.' + err);
      rw.sqlDriver = undefined;
      rw.testCase.fail();
    } else {
      // get metadata for table
      rw.session.getTableMetadata(rw.tableMapping.database, rw.tableMapping.table, onTableMetadata);
    }
  }

  function onTableMapping(err, tableMapping) {
    if (err) {
      rw.testCase.appendErrorMessage('failed to get table mapping.' + err);
      rw.testCase.fail();
    } else {
      rw.tableMapping = tableMapping;
      udebug.log('got mapping for ', rw.tableMapping.table);
      // set up sql driver
      if (rw.sqlDriver === undefined) {
        rw.sqlDriver = new SQLDriver(global.test_conn_properties);
        rw.sqlDriver.connect(onConnect);
      } else {
        console.log('already set up sql driver')
        // already set up sql driver
        onConnect(null);
      }
    }
  }
  // setUp starts here
  if (!Array.isArray(rw.data)) {
    rw.data = [rw.data];
  }
  rw.session.getMapping(rw.tableNameOrConstructor, onTableMapping);
};

ReadWrite.prototype.incrementCheckCountAndExit = function() {
  if (++this.numberChecked == this.data.length) {
    this.testCase.failOnError();
  }
};

ReadWrite.prototype.removeAdapter = function(element, onRemove) {
  this.session.remove(this.tableNameOrConstructor, element, onRemove);
};

ReadWrite.prototype.removeSQL = function(element, onRemove) {
  this.sqlDriver.remove(this.tableMapping, element, onRemove);
};

ReadWrite.prototype.checkResult = function(err, result, rw, index) {
  udebug.log('checkResult', err, rw.data, result, index);
  var rw = rw;
  if (err) {
    rw.testCase.appendErrorMessage('checkResult err on read: ', err);
  } else {
    if (typeof(rw.tableNameOrConstructor) === 'string') {
      // table name; check properties in data against properties in result
      var x;
      var data = rw.data[index];
      for (x in data) {
        if (data.hasOwnProperty(x)) {
          rw.testCase.errorIfNotEqual('mismatch in ' + x, data[x], result[x]);
        }
      }
    } else {
      // constructor
    }
  }
  rw.incrementCheckCountAndExit();
};

/** Write data to the table or constructor. 
 * @param callback (err, readWrite)
 * @return nothing
 */
ReadWrite.prototype.writeAdapter = function(callback) {
  var rw = this;

  // write all elements of data
  var index = 0;
  rw.data.forEach(function(element) {
    rw.session.persist(rw.tableNameOrConstructor, element, callback, rw, index++);
  });
};

ReadWrite.prototype.writeSQL = function(callback) {
  var rw = this;
  var index = 0;
  rw.data.forEach(function(element) {
    rw.sqlDriver.insert(rw.tableMapping, element, callback, rw, index++);
  });
};

ReadWrite.prototype.readAdapter = function(err, rw, index) {
  if (err) {
    rw.testCase.appendErrorMessage('readAdapter err on write: ' + err);
    rw.incrementCheckCountAndExit();    
  } else {
    rw.session.find(rw.tableNameOrConstructor, rw.data[index],
        rw.checkResult, rw, index);
  }
};

ReadWrite.prototype.readSQL = function(err, rw, index) {
  if (err) {
    rw.testCase.appendErrorMessage('readSQL err on write: ' + err);
    rw.incrementCheckCountAndExit();    
  } else {
    rw.sqlDriver.select(rw.tableMapping, rw.data[index],
        rw.checkResult, rw, index);
  }
};

ReadWrite.prototype.writeAdapterReadAdapter = function() {
  var rw = this;
  rw.remove = rw.removeAdapter;
  function run() {
    rw.writeAdapter(rw.readAdapter);
  }
  rw.setUp(run);
};

ReadWrite.prototype.writeAdapterReadSQL = function() {
  var rw = this;
  rw.remove = rw.removeAdapter;
  function run() {
    rw.writeAdapter(rw.readSQL);
  }
  rw.setUp(run);
};

ReadWrite.prototype.writeSQLReadAdapter = function() {
  var rw = this;
  rw.remove = rw.removeSQL;
  function run() {
    rw.writeSQL(rw.readAdapter);
  }
  rw.setUp(run);
};

ReadWrite.prototype.writeSQLReadSQL = function() {
  var rw = this;
  rw.remove = rw.removeSQL;
  function run() {
    rw.writeSQL(rw.readSQL);
  }
  rw.setUp(run);
};

exports.ReadWrite = ReadWrite;
