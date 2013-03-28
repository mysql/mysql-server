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

'use strict';

var mysql = require('mysql');

var tableHandlers = {
    'a': {
      insertSQL: 'INSERT INTO a(id, cint, clong, cfloat, cdouble) values (?,?,?,?,?)',
      createInsertParameterList: function(object) {
        var result = [];
        result.push(object.id);
        result.push(object.cint);
        result.push(object.clong);
        result.push(object.cfloat);
        result.push(object.cdouble);
        return result;
      },
      deleteSQL: 'DELETE FROM a WHERE id = ?',
      createDeleteParameterList: function(object) {
        if (typeof(object) === 'number') {
          return [object];
        } else if (typeof(object) === 'object') {
          return [object.id];
        } else throw new Error('createFindParameterList parameter must be number or object');
      },
      findSQL: 'SELECT id, cint, clong, cfloat, cdouble FROM a where id = ?',
      createFindParameterList: function(object) {
        if (typeof(object) === 'number') {
          return [object];
        } else if (typeof(object) === 'object') {
          return [object.id];
        } else throw new Error('createFindParameterList parameter must be number or object');
      }
    }
};

var implementation = function() {
  // the connection is initialized in function initialize
  var connection = null;
};

implementation.prototype.initialize = function(options, callback) {
  JSCRUND.udebug.log_detail('jscrund_sql.initialize', this);
  var impl = this;
  // set up the session
  var properties = {};
  // set up mysql properties
  properties.host = options.properties.mysql_host || 'localhost';
  properties.port = options.properties.mysql_port || 3306;
  properties.user = options.properties.mysql_user;
  properties.password = options.properties.mysql_password;
  properties.database = options.properties.database;
  
  JSCRUND.udebug.log_detail('jscrund_sql.initialize calling mysql.createConnection', properties);
  this.connection = mysql.createConnection(properties);
  this.connection.connect(function(err) {
    JSCRUND.udebug.log_detail('jscrund_sql implementation.initialize connection:', impl.connection);
    callback(err); // report error if any
  });
};

implementation.prototype.persist = function(parameters, callback) {
  // which object is it
  var mapping = parameters.object.constructor.prototype.mynode.mapping;
  var tableName = mapping.table;
  var object = parameters.object;
  JSCRUND.udebug.log_detail('jscrund_sql implementation.insert object:', object,
      'table', tableName);

  // find the handler for the table
  var tableHandler = tableHandlers[tableName];
  this.connection.query(tableHandler.insertSQL, tableHandler.createInsertParameterList(object), 
      function(err) {
        if (err) {
          JSCRUND.udebug.log_detail('jscrund_sql implementation.insert callback err:', err);
        } else {
          JSCRUND.udebug.log_detail('jscrund_sql implementation.insert no error');
        }
        callback(err);
      });
};

implementation.prototype.find = function(parameters, callback) {
  // which object is it
  var mapping = parameters.object.constructor.prototype.mynode.mapping;
  var tableName = mapping.table;
  var object = parameters.object;
  JSCRUND.udebug.log_detail('jscrund_sql implementation.find key:', parameters.key, 'table', tableName);

  // find the handler for the table
  var tableHandler = tableHandlers[tableName];
  this.connection.query(tableHandler.findSQL, tableHandler.createFindParameterList(parameters.key), 
      function(err, result) {
        if (err) {
          JSCRUND.udebug.log_detail('jscrund_sql implementation.find callback err:', err);
        } else {
          JSCRUND.udebug.log_detail('jscrund_sql implementation.find result:', result);
        }
        callback(err, result);
      });
};

implementation.prototype.remove = function(parameters, callback) {
  // which object is it
  var mapping = parameters.object.constructor.prototype.mynode.mapping;
  var tableName = mapping.table;
  var object = parameters.object;
  JSCRUND.udebug.log_detail('jscrund_sql implementation.remove key:', parameters.key, 'table', tableName);

  // find the handler for the table
  var tableHandler = tableHandlers[tableName];
  this.connection.query(tableHandler.deleteSQL, tableHandler.createFindParameterList(parameters.key), 
      function(err, result) {
        if (err) {
          JSCRUND.udebug.log_detail('jscrund_sql implementation.remove callback err:', err);
        } else {
          JSCRUND.udebug.log_detail('jscrund_sql implementation.remove no error');
        }
        callback(err);
  });
};

implementation.prototype.createBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.createBatch');
  this.begin(callback);
};

implementation.prototype.executeBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.begin');
  this.commit(callback);
};

implementation.prototype.begin = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.begin');
  var impl = this;
  this.connection.query('begin', function(err) {
    if (err) {
      JSCRUND.udebug.log_detail('jscrund_sql implementation.begin callback err:', err);
    } else {
      JSCRUND.udebug.log_detail('jscrund_sql implementation.begin no error');
    }
    callback(err);
  });
};

implementation.prototype.commit = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.commit');
  this.connection.query('commit', function(err) {
    if (err) {
      JSCRUND.udebug.log_detail('jscrund_sql implementation.commit callback err:', err);
    } else {
      JSCRUND.udebug.log_detail('jscrund_sql implementation.commit no error');
    }
    callback(err);
  });
};

implementation.prototype.close = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.close');
  this.connection.end(function(err) {
    callback(err);
  });
};

exports.implementation = implementation;
