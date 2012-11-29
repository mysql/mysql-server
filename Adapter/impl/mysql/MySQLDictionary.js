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

/*global unified_debug, exports */

"use strict";


/* Requires version 2.0 of Felix Geisendoerfer's MySQL client */

var util   = require('util'),
    udebug = unified_debug.getLogger("MySQLDictionary.js");

exports.DataDictionary = function(pooledConnection) {
  this.connection = pooledConnection;
};

exports.DataDictionary.prototype.listTables = function(databaseName, user_callback) {
  var callback = user_callback;
  var showTables_callback = function(err, rows) {
    if (err) {
      callback(err);
    } else {
      var result = [];
      var propertyName = 'Tables_in_' + databaseName;
      rows.forEach(function(row) {
        result.push(row[propertyName]);
      });
      udebug.log('listTables function result:', result);
      callback(err, result);
    }
  };
  this.connection.query('show tables', showTables_callback);
};


exports.DataDictionary.prototype.getTableMetadata = function(databaseName, tableName, user_callback) {

  // get precision from columnSize e.g. 10,2
  var getPrecision = function(columnSize) {
    var precision = columnSize.split(',')[0];
    return parseInt(precision, 10);
  };

  // get scale from columnSize e.g. 10,2
  var getScale = function(columnSize) {
    var scale = columnSize.split(',')[1];
    return parseInt(scale, 10);
  };

  var decodeIndexColumnNames = function(columnNames) {
    var columnNamesSplit = columnNames.split('`');
    var indexColumnNames = [];
    var k;
    udebug.log_detail('decodeIndexColumnNames columnNamesSplit: ',
                       columnNamesSplit.length, ' ', columnNamesSplit);
    for (k = 1; k < columnNamesSplit.length; k += 2) {
      indexColumnNames.push(columnNamesSplit[k]);
    }
    udebug.log_detail('decodeIndexColumnNames indexColumnNames:', indexColumnNames);
    return indexColumnNames;
  };

  var convertColumnNamesToNumbers = function(columnNames, columns) {
    var result = [];
    var i, j;
    for (i = 0; i < columnNames.length; ++i) {
      udebug.log_detail('convertColumnNamesToNumbers looking for: ', 
                         columnNames[i],' in ', columns);
      for (j = 0; j < columns.length; ++j) {
        if (columnNames[i] == columns[j].name) {
          result.push(j);
          break;
        }
      }
    }
    return result;
  };

  var parseCreateTable = function(tableName, statement) {
    udebug.log_detail('parseCreateTable: ', statement);
    var columns = [];
    var indexes = [];
    var result = {'name' : tableName,
        'database' : databaseName,
        'columns' : columns,
        'indexes' : indexes};
    
    // split lines by '\n'
    var lines = statement.split('\n');
    var i;
    var columnNumber = 0;
    // first line has table name which we ignore because we already know it
    for (i = 1; i < lines.length; ++i) {
      var line = lines[i];
      udebug.log_detail('\n parseCreateTable:', line);
      var tokens = line.split(' ');
      var j = 0; // index into tokens in the line
      var token = tokens[j];
      // remove empty tokens
      while (token.length == 0) {
        token = tokens[++j];
      }
      var unique = false;
      udebug.log_detail('parseCreateTable token:', token);
      switch (token) {
      case 'PRIMARY':
        // found primary key definition
        j+= 2; // skip 'PRIMARY KEY'
        var index = {};
        index['name'] = 'PRIMARY';
        udebug.log_detail('parseCreateTable PRIMARY:', token);
        index['isPrimaryKey'] = true;
        index['isUnique'] = true;
        index['isOrdered'] = true;
        var columnNames = tokens[j];
        var indexColumnNames = decodeIndexColumnNames(columnNames);
        udebug.log_detail('parseCreateTable PRIMARY indexColumnNames:', indexColumnNames);
        var indexColumnNumbers = convertColumnNamesToNumbers(indexColumnNames, result['columns']);
        udebug.log_detail('parseCreateTable PRIMARY indexColumnNumbers: ', indexColumnNumbers);
        index['columnNumbers'] = indexColumnNumbers;
        // mark primary key index columns with 'isInPrimaryKey'
        for (var columnNumberIndex = 0; columnNumberIndex < indexColumnNumbers.length; ++columnNumberIndex) {
          var columnNumber = indexColumnNumbers[columnNumberIndex];
          var column = columns[columnNumber];
          udebug.log_detail('parseCreateTable marking column', columnNumber,
                             columns[columnNumber].name);
          column.isInPrimaryKey = true;
        }
        indexes.push(index);
        break;

      case 'UNIQUE':
        // found unique key definition
        udebug.log_detail('parseCreateTable UNIQUE:', token);
        unique = true;
        ++j;
        // continue with KEY handling

      case 'KEY':
        ++j;
        // found key definition, same as unique
        var index = {};
        var indexName = tokens[j].split('`')[1];
        index['name'] = indexName;
        if (unique) {
          index['isUnique'] = true;
        }
        // get column names
        var columnNames = tokens[++j];
        var indexColumnNames = decodeIndexColumnNames(columnNames);
        udebug.log_detail('parseCreateTable KEY indexColumnNames:', indexColumnNames);
        var indexColumnNumbers = convertColumnNamesToNumbers(indexColumnNames, result['columns']);
        udebug.log_detail('parseCreateTable KEY indexColumnNumbers:', indexColumnNumbers);
        index['columnNumbers'] = indexColumnNumbers;

        var usingHash = false;
        // get using statement
        if (++j < tokens.length) {
          // more tokens
          usingHash = -1 != tokens[++j].indexOf('HASH');
        }
        if (usingHash) {
          // TODO create two index objects for unique btree index
          // only HASH
        } else {
          // btree or both
          index['isOrdered'] = true;
        }
        udebug.log_detail('parseCreateTable for ', indexName, 'KEY USING HASH:', usingHash);
        indexes.push(index);
        break;

      case ')':
        // TODO found engine; get default charset
        break;

      default:
        // found column definition
        var nullable = true; // default if no 'NOT NULL' clause
        var unsigned = false; // default if no 'unsigned' clause
        var column = {};

        column.columnNumber = columnNumber++;
        // decode the column name
        var columnName = (token.split('`'))[1];
        udebug.log_detail('parseCreateTable: columnName:', columnName);
        column.name = columnName;
        // analyze column type
        var columnTypeAndSize = tokens[++j];
        udebug.log_detail('parseCreateTable: columnDefinition:', columnTypeAndSize);
        var columnTypeAndSizeSplit = columnTypeAndSize.split('(');
        var columnType = columnTypeAndSizeSplit[0];
        udebug.log_detail('parseCreateTable for: ', columnName, ': columnType: ', columnType);
        column.columnType = columnType;
        if (columnTypeAndSizeSplit.length > 1) {
          var columnSize = columnTypeAndSizeSplit[1].split(')')[0];
          udebug.log_detail('parseCreateTable for: ', columnName, ': columnSize: ', columnSize);
        }
        ++j;

        // check for unsigned
        if (tokens[j] == 'unsigned') {
          var unsigned = true;
          ++j;
        }
        udebug.log_detail('parseCreateTable for:', columnName, ': unsigned: ', unsigned);
        column.isUnsigned = unsigned;

        // check for character set
        if (tokens[j] == 'CHARACTER') {
          var charset = tokens[j + 2];
          udebug.log_detail('parseCreateTable for:', columnName, ': charset: ', charset);
          j += 3; // skip 'CHARACTER SET charset'
          column.charsetName = charset;
          // check for collation
          if (tokens[j] == 'COLLATE') {
            var collation = tokens[j + 1];
            udebug.log_detail('parseCreateTable for: ', columnName, ': collation: ', collation);
            column['collationName'] = collation;
            j+= 2; // skip 'COLLATE collation'
          }
        }
        if (tokens[j] == 'NOT') { // 'NOT NULL' clause
          nullable = false;
          j += 2; // skip 'not null'
        }
        udebug.log_detail('parseCreateTable for: ', columnName, ' NOT NULL: ', !nullable);
        column.isNullable = nullable;
        if (tokens[j] == 'DEFAULT') {
          udebug.log_detail('parseCreateTable for: ', columnName, ': DEFAULT: ', tokens[j]);
        }

        // add extra metadata specific to type
        switch (columnType) {
        case 'tinyint':   column['intSize'] = 1; column['isIntegral'] = true; break;
        case 'smallint':  column['intSize'] = 2; column['isIntegral'] = true; break;
        case 'mediumint': column['intSize'] = 3; column['isIntegral'] = true; break;
        case 'int':       column['intSize'] = 4; column['isIntegral'] = true; break;
        case 'bigint':    column['intSize'] = 8; column['isIntegral'] = true; break;

        case 'decimal' :
          column['precision'] = getPrecision(columnSize); 
          column['scale'] = getScale(columnSize); 
          break;
        case 'binary':
        case 'varbinary':
          column['isBinary'] = true;
          // continue to set columnSize
        case 'char':
        case 'varchar':
          column['length'] = parseInt(columnSize);
          break;
        case 'blob':
          column['isBinary'] = true;
          break;
        }
        // add the column description metadata
        columns.push(column);
        break;
      }
    }

    return result;
  };

  var callback = user_callback;
  var showCreateTable_callback = function(err, rows) {
    var result;
    if (err) {
      udebug.log_detail('MySQLDictonary error from SHOW CREATE TABLE: ' + err);
      callback(err);
    } else {
      udebug.log_detail(rows);
      var row = rows[0];
      // result of show create table is of the form:
      // [ { Table: 'tbl1',
      // 'Create Table': 'CREATE TABLE `tbl1` (\n  `i` int(11) NOT NULL,\n  `j` int(11) DEFAULT NULL,\n  PRIMARY KEY (`i`)\n) ENGINE=ndbcluster DEFAULT CHARSET=latin1' } ]
      // the create table statement is the attribute named 'Create Table'
      var createTableStatement = row['Create Table'];
      var metadata = parseCreateTable(tableName, createTableStatement);
      udebug.log_detail('showCreateTable_callback.forEach metadata:', metadata);
      result = metadata;
      
      callback(err, result);
    }
  };

  this.connection.query('show create table ' + tableName + '', showCreateTable_callback);
};

