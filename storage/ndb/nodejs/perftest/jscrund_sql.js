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
      insertSQL: 'INSERT INTO a(id, cint, clong, cfloat, cdouble) VALUES (?,?,?,?,?)',
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
      findSQL: 'SELECT id, cint, clong, cfloat, cdouble FROM a WHERE id = ?',
      createFindParameterList: function(object) {
        if (typeof(object) === 'number') {
          return [object];
        } else if (typeof(object) === 'object') {
          return [object.id];
        } else throw new Error('createFindParameterList parameter must be number or object');
      }
    },
    'b': {
      insertSQL: 'INSERT INTO b(id) VALUES (?)',
      createInsertParameterList: function(object) {
        var result = [];
        result.push(object.id);
          return result;
      },
      deleteSQL: 'DELETE FROM b WHERE id = ?',
      createDeleteParameterList: function(object) {
        if (typeof(object) === 'number') {
          return [object];
        } else if (typeof(object) === 'object') {
          return [object.id];
        } else throw new Error('createFindParameterList parameter must be number or object');
      },
      findSQL: 'SELECT id, cvarbinary_def FROM b WHERE id = ?',
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
};

implementation.prototype.getDefaultProperties = function() {
  return {
    mysql_host      : 'localhost',
    mysql_port      : 3306,
    mysql_user      : 'root',
    mysql_password  : '',
    database        : 'test'
  };
};

implementation.prototype.initialize = function(options, callback) {
  JSCRUND.udebug.log_detail('jscrund_sql.initialize', this);
  var properties = {};
  // set up mysql properties
  properties.host = options.properties.mysql_host;
  properties.port = options.properties.mysql_port;
  properties.user = options.properties.mysql_user;
  properties.password = options.properties.mysql_password;
  properties.database = options.properties.database;
  properties.multipleStatements = true;
  JSCRUND.udebug.log_detail('jscrund_sql.initialize calling mysql.createConnection', properties);
  this.inBatchMode = false;
  this.connection = mysql.createConnection(properties);
  this.connection.connect(callback);
};

implementation.prototype.exec = function(statement, values, callback) {
  var v;
  if(this.inBatchMode) {
    this.batchCallbacks.push(callback);
    this.batchQuery += statement + "; ";
    while((v = values.shift()) != undefined) {
      this.batchValues.push(v);
    }
  }
  else {
    this.connection.query(statement, values, callback);
  }

}

implementation.prototype.persist = function(parameters, callback) {
  // which object is it
  var mapping = parameters.object.constructor.prototype.mynode.mapping;
  var tableName = mapping.table;
  var object = parameters.object;
  JSCRUND.udebug.log_detail('jscrund_sql implementation.insert object:', object,
      'table', tableName);

  // find the handler for the table
  var tableHandler = tableHandlers[tableName];
  this.exec(tableHandler.insertSQL,
            tableHandler.createInsertParameterList(object),
            callback);
};

implementation.prototype.find = function(parameters, callback) {
  // which object is it
  var mapping = parameters.object.constructor.prototype.mynode.mapping;
  var tableName = mapping.table;
  var object = parameters.object;
  JSCRUND.udebug.log_detail('jscrund_sql implementation.find key:', parameters.key, 'table', tableName);

  // find the handler for the table
  var tableHandler = tableHandlers[tableName];
  this.exec(tableHandler.findSQL,
            tableHandler.createFindParameterList(parameters.key),
            callback);
};

implementation.prototype.remove = function(parameters, callback) {
  // which object is it
  var mapping = parameters.object.constructor.prototype.mynode.mapping;
  var tableName = mapping.table;
  var object = parameters.object;
  JSCRUND.udebug.log_detail('jscrund_sql implementation.remove key:', parameters.key, 'table', tableName);

  // find the handler for the table
  var tableHandler = tableHandlers[tableName];
  this.exec(tableHandler.deleteSQL,
            tableHandler.createFindParameterList(parameters.key),
            callback);
 };

implementation.prototype.createBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.createBatch');
  this.inBatchMode = true;
  this.batchQuery = "";
  this.batchValues = [];
  this.batchCallbacks = [];
  setImmediate(callback);
};

implementation.prototype.executeBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_sql implementation.executeBatch');
  var callbacks = this.batchCallbacks;
  function allCallbacks(err, results) {
    var n;
    for(n = 0 ; n < callbacks.length ; n++) {
      callbacks[n](err, results[n]);
    }
  }
  this.connection.query(this.batchQuery, this.batchValues, allCallbacks);
  this.inBatchMode = false;
  setImmediate(callback);
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
