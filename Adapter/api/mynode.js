/*
 Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights
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

var path           = require("path"),
    conf           = require("../adapter_config");

/** Include Path Helpers */
exports.fs = conf;

exports.common = {
  "BitMask"          : path.join(conf.spi_dir, "common", "BitMask"),
  "DBTableHandler"   : path.join(conf.spi_dir, "common", "DBTableHandler"),
  "IndexBounds"      : path.join(conf.spi_dir, "common", "IndexBounds"),
  "QueuedAsyncCall"  : path.join(conf.spi_dir, "common", "QueuedAsyncCall")
};

exports.api = {
  "TableMapping"     : path.join(conf.api_dir, "TableMapping"),
  "unified_debug"    : path.join(conf.api_dir, "unified_debug"),
  "stats"            : path.join(conf.api_dir, "stats"),
  "UserContext"      : path.join(conf.api_dir, "UserContext")
};

exports.spi = conf.spi_module;

/* Make it global */
global.mynode = exports;


var spi            = require(conf.spi_module),
    TableMapping   = require("./TableMapping").TableMapping,
    Projection     = require("./Projection").Projection,
    unified_debug  = require("./unified_debug"),
    udebug         = unified_debug.getLogger("mynode.js"),
    meta           = require("./Meta.js"), 
    userContext    = require("./UserContext.js");

var converters = {};
converters.JSONConverter       = require(path.join(conf.converters_dir, "JSONConverter.js"));
converters.JSONSparseConverter = require(path.join(conf.converters_dir, "JSONSparseConverter.js"));


/** make TableMapping public */
exports.TableMapping = TableMapping;
/** make Projection public */
exports.Projection = Projection;

/** connections is a hash of connectionKey to Connection */
var connections = {};

var deleteFactory;

/** Connection contains a hash of database to SessionFactory */
var Connection = function(connectionKey) {
  this.connectionKey = connectionKey;
  this.factories = {};
  this.count = 0;
  this.isConnecting = true;
  this.waitingForConnection = [];
};

/*jslint forin: true */
exports.ConnectionProperties = function(nameOrProperties) {
  var serviceProvider, newProperties, key, value;
  if(typeof nameOrProperties === 'string') {
    udebug.log("ConnectionProperties [default for " + nameOrProperties + "]");
    serviceProvider = spi.getDBServiceProvider(nameOrProperties);
    newProperties = serviceProvider.getDefaultConnectionProperties();
  }
  else if(typeof nameOrProperties === 'object' && 
          typeof nameOrProperties.implementation === 'string') {
    udebug.log("ConnectionProperties [copy constructor]");
    newProperties = {};
    for(key in nameOrProperties) {
      value = nameOrProperties[key];
      if(typeof value === 'string' || typeof value === 'number') {
        newProperties[key] = value;
      }
      else {
        udebug.log(" .. not copying property:",  key);
      }
    }
  }
  return newProperties;
};

exports.connect = function(properties, annotations, user_callback) {
  var context = new userContext.UserContext(arguments, 3, 2, null, null);
  return context.connect();
};

exports.getConnectionKey = function(properties) {
  var sp = spi.getDBServiceProvider(properties.implementation);
  return sp.getFactoryKey(properties);
};

exports.getConnection = function(connectionKey) {
  return this.connections[connectionKey];
};

exports.newConnection = function(connectionKey) {
  var connection = new Connection(connectionKey);
  this.connections[connectionKey] = connection;
  return connection;
};

exports.openSession = function() {
  var context = new userContext.UserContext(arguments, 3, 2, null, null);
  return context.openSession();
};


exports.getOpenSessionFactories = function() {
  var result = [];
  var x, y;
  for (x in connections) {
    if (connections.hasOwnProperty(x)) {
      for (y in connections[x].factories) {
        if (connections[x].factories.hasOwnProperty(y)) {
          var factory = connections[x].factories[y];
          result.push(factory);
        }
      }
    }
  }
  return result;
};

exports.closeAllOpenSessionFactories = function() {
  var context = new userContext.UserContext(arguments, 1, 1);
  return context.closeAllOpenSessionFactories();
};


/** deleteFactory is called only from SessionFactory.close().
 * Multiple session factories share a db connection pool. When
 * the last session factory using the db connection pool is closed,
 * this function will also close the db connection pool.
 */
deleteFactory = function(key, database, callback) {
  udebug.log('deleteFactory for key', key, 'database', database);
  var connection = connections[key];
  var factory = connection.factories[database];
  var dbConnectionPool = factory.dbConnectionPool;
  
  delete connection.factories[database];
  if (--connection.count === 0) {
    // no more factories in this connection
    udebug.log('deleteFactory closing dbConnectionPool for key', key, 'database', database);
    dbConnectionPool.close(callback);
    dbConnectionPool = null;
    delete connections[key];
  } else {
    callback();
  }
};

exports.connections = connections;
exports.Connection = Connection;
exports.deleteFactory = deleteFactory;
exports.converters = converters;
exports.meta = meta;
