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

"use strict";
/*global unified_debug */

var spi = require("../impl/SPI.js");
var annotations = require("./Annotations.js");
var sessionfactory = require("./SessionFactory.js");
var udebug = unified_debug.getLogger("mynode.js");

/** connections is a hash of connectionKey to Connection */
var connections = {};

var deleteFactory;

/** Connection contains a hash of database to SessionFactory */
var Connection = function(dbConnectionPool) {
  this.dbConnectionPool = dbConnectionPool;
  this.factories = {};
  this.count = 0;
};

exports.Annotations = function() {
  return new annotations.Annotations();
};

function spi_connect_sync(properties) {
  var db = spi.getDBServiceProvider(properties.implementation);
  return db.connectSync(properties);  
}

exports.ConnectionProperties = function(name) {
  var sp = spi.getDBServiceProvider(name);
  return sp.getDefaultConnectionProperties();
};

exports.connect = function(properties, annotations, user_callback, extra1, extra2, extra3, extra4) {
  var mynode = this;

  // Properties can be a string adapter name.  It defaults to 'ndb'.
  if(typeof properties === 'string') {
    properties = this.ConnectionProperties(properties);
  }
  else if (properties === null) {
    properties = this.ConnectionProperties('ndb');
  }

  var sp = spi.getDBServiceProvider(properties.implementation);
  var connectionKey = sp.getFactoryKey(properties);
  var database = properties.database;
  var connection = connections[connectionKey];
  var factory;
  
  var createFactory = function(dbConnectionPool) {
    var newFactory;
    udebug.log('connect createFactory creating factory for', connectionKey, 'database', database);
    newFactory = new sessionfactory.SessionFactory(connectionKey, dbConnectionPool, properties, annotations, deleteFactory);
    return newFactory;
  };
  
  var dbConnectionPoolCreated_callback = function(error, dbConnectionPool) {
    if(! error) {
      udebug.log('connect dbConnectionPoolCreated creating factory for', connectionKey, 'database', database);
      var connection = new Connection(dbConnectionPool);
      connections[connectionKey] = connection;
      
      factory = createFactory(dbConnectionPool);
      connection.factories[database] = factory;
      connection.count++;
    }
    user_callback(error, factory, extra1, extra2, extra3, extra4);
  };

  if(typeof(connection) === 'undefined') {
    // there is no connection yet using this connection key    
    udebug.log('connect connection does not exist; creating factory for',
               connectionKey, 'database', database);
    sp.connect(properties, dbConnectionPoolCreated_callback);
  } else {
    // there is a connection, but is there a SessionFactory for this database?
    factory = connection.factories[database];
    if (typeof(factory) === 'undefined') {
      // create a SessionFactory for the existing dbConnectionPool
      udebug.log('connect creating factory with existing', connectionKey, 'database', database);
      factory = createFactory();
      connection.factories[database] = factory;
      connection.count++;
    } 
    user_callback(null, factory, extra1, extra2, extra3, extra4);   //todo: extra parameters
  }
};


exports.openSession = function(properties, annotations, user_callback, extra1, extra2, extra3, extra4) {
  exports.connect(properties, annotations, function(err, factory) {
    if(! err) {
      var session = factory.openSession(annotations, user_callback, extra1, extra2, extra3, extra4);
    } else {
      user_callback(err, session, extra1, extra2, extra3, extra4);   // todo: extra parameters
    }
  });
};


exports.getOpenSessionFactories = function() {
  var result = [];
  var x, y;
  for (x in connections) {
    for (y in connections[x].factories) {
      var factory = connections[x].factories[y];
      result.push(factory);
    }
  }
  return result;
};

/** deleteFactory is called only from SessionFactory.close() */
deleteFactory = function(key, database) {
  udebug.log('deleteFactory for key', key, 'database', database);
  var connection = connections[key];
  var factory = connection.factories[database];
  var dbConnectionPool = factory.dbConnectionPool;
  
  delete connection.factories[database];
  if (--connection.count == 0) {
    // no more factories in this connection
    udebug.log('deleteFactory closing dbConnectionPool for key', key, 'database', database);
    if (dbConnectionPool != null) {
      dbConnectionPool.closeSync();
      dbConnectionPool = null;
      delete connections[key];
    }
  };
  
};
