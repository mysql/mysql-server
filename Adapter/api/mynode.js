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

require("../impl/build/Release/common/common_library");
var spi = require("../impl/SPI.js");
var annotations = require("./Annotations.js");
var sessionfactory = require("./SessionFactory.js");

/** connections is a hash of connectionKey to Connection */
var connections = {};

/** Connection contains a hash of database to SessionFactory */
var Connection = function(dbconnection) {
  this.dbconnection = dbconnection;
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
  var sp = spi.getDBServiceProvider(properties.implementation);
  var connectionKey = sp.getFactoryKey(properties);
  var database = properties.database;
  var connection = connections[connectionKey];
  
  var createFactory = function(dbconnection) {
    if (debug) console.log('mynode.connect createFactory creating factory for ' + connectionKey + ' database ' + database);
    newFactory = new sessionfactory.SessionFactory(connectionKey);
    newFactory.dbconnection = dbconnection;
    newFactory.properties = properties;
    newFactory.annotations = annotations;
    newFactory.delete_callback = deleteFactory;
    return newFactory;
  };
  
  var dbconnectionCreated = function(error, dbconnection) {
    if(! error) {
      if (debug) console.log('mynode.connect dbconnectionCreated creating factory for ' + connectionKey + ' database ' + database);
      var connection = new Connection(dbconnection);
      connections[connectionKey] = connection;
      
      factory = createFactory(dbconnection);
      connection.factories[database] = factory;
      connection.count++;
    }
    user_callback(error, factory); //todo: extra parameters
  };

  if(typeof(connection) == 'undefined') {
    // there is no connection yet using this connection key    
    if (debug) console.log('mynode.connect connection does not exist; creating factory for ' + connectionKey + ' database ' + database);
    sp.connect(properties, dbconnectionCreated);
  } else {
    // there is a connection, but is there a SessionFactory for this database?
    factory = connection.factories[database];
    if (typeof(factory) == 'undefined') {
      // create a SessionFactory for the existing dbconnection
      if (debug) console.log('mynode.connect creating factory with existing ' + connectionKey + ' database ' + database);
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
  if (debug) console.log('mynode.deleteFactory for key ' + key + ' database ' + database);
  var connection = connections[key];
  var factory = connection.factories[database];
  var dbconnection = factory.dbconnection;
  
  delete connection.factories[database];
  if (--connection.count == 0) {
    // no more factories in this connection
    if (debug) console.log('mynode.deleteFactory closing dbconnection for key ' + key + ' database ' + database);
    if (dbconnection != null) {
      dbconnection.closeSync();
      dbconnection = null;
      delete connections[key];
    }
  };
  
};
