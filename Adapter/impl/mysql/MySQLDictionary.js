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
var table = require('./MySQLTable');

exports.DataDictionary = function(pooledConnection) {
  this.connection = pooledConnection;
};

exports.DataDictionary.prototype.listTables = function(databaseName, user_callback) {
  callback = user_callback;
  showTables_callback = function(err, rows) {
    if (err) {
      callback(err);
    } else {
      result = [];
      var propertyName = 'Tables_in_' + databaseName;
      rows.forEach(function(row) {
        result.push(row[propertyName]);
      });
      udebug.log('MySQLDictionary.listTables function result: ' + result);
      callback(err, result);
    }
  }
  this.connection.query('show tables', showTables_callback);
};


exports.DataDictionary.prototype.getTable = function(databaseName, tableName, user_callback) {
  var parseCreateTable = function(tableName, statement) {
    udebug.log('MySQLDictionary.parseCreateTable: ' + statement);
    // split lines by '\n'
    var lines = statement.split('\n');
    // first line has table name which we ignore because we already know it
    for (var i = 1; i < lines.length; ++i) {
      //   `i` int(11) NOT NULL,
      // remove leading white space
      // split by '`'; first token is column name
      // remove leading white space
      // analyze column type including width
      // split by ' '; iterate remaining items NOT NULL DEFAULT
    }
    return {'table' : tableName};
  };
  callback = user_callback;
  showCreateTable_callback = function(err, rows) {
    if (err) {
      callback(err);
    } else {
      udebug.log(rows);
      result = [];
      rows.forEach(function(row) {
        // result of show create table is of the form:
        // [ { Table: 'tbl1',
        // 'Create Table': 'CREATE TABLE `tbl1` (\n  `i` int(11) NOT NULL,\n  `j` int(11) DEFAULT NULL,\n  PRIMARY KEY (`i`)\n) ENGINE=ndbcluster DEFAULT CHARSET=latin1' } ]
        // the create table statement is the attribute named 'Create Table'
        var createTableStatement = row['Create Table'];
        var metadata = parseCreateTable(tableName, createTableStatement);
        result.push(metadata);
        
      });
      callback(err, result);
    }
  callback(null, new table.Table);
  };
  this.connection.query('show create table ' + tableName + '', showCreateTable_callback);
};

