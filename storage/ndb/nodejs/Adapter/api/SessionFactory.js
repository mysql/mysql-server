/*
 Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights
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

var session     = require("./Session.js"),
    udebug      = unified_debug.getLogger("SessionFactory.js"),  
    userContext = require("./UserContext.js"),
    util        = require("util"),
    Db          = require("./Db.js");


var SessionFactory = function(key, dbConnectionPool, properties, mappings, delete_callback) {
  this.key = key;
  this.dbConnectionPool = dbConnectionPool;
  this.properties = properties;
  this.mappings = mappings;
  this.delete_callback = delete_callback;
  this.sessions = [];
  this.tableHandlers = {};
  this.tableMetadatas = {};
  this.tableMappings  = {}; // mappings for tables
};

SessionFactory.prototype.inspect = function() {
  var numberOfMappings = this.mappings? this.mappings.length: 0;
  var numberOfSessions = this.sessions? this.sessions.length: 0;
  return "[[API SessionFactory with key:" + this.key + ", " + 
  numberOfMappings + " mappings, " + numberOfSessions + " sessions.]]\n";
};

//openSession(Function(Object error, Session session, ...) callback, ...);
// Open new session or get one from a pool
SessionFactory.prototype.openSession = function() {
  var context = new userContext.UserContext(arguments, 2, 2, null, this);
  // delegate to context for execution
  return context.openSession();
};


/** Allocate a slot in the sessions array for a new session. 
 * If there are no empty slots, extend the
 * sessions array. Assign a placeholder
 * and return the index into the array. 
 */
SessionFactory.prototype.allocateSessionSlot = function() {
  // allocate a new session slot in sessions
  var i;
  for (i = 0; i < this.sessions.length; ++i) {
    if (this.sessions[i] === null) {
      break;
    }
  }
  this.sessions[i] = {
      'placeholder': true, 
      'index': i,
      // dummy callback in case the session is closed prematurely
      close: function(callback) {
        callback();
      }
  };
  return i;
};


/** Get metadata for a table.
 * @param dbName the name of the database
 * @param tableName the name of the table
 * @param callback
 */
SessionFactory.prototype.getTableMetadata = function() {
  var context = new userContext.UserContext(arguments, 3, 2, null, this);
  // delegate to context for execution
  return context.getTableMetadata();
};

/** Create table for a table mapping.
 * @param tableMapping
 * @param user_callback
 * @return promise
 */
SessionFactory.prototype.createTable = function(tableMapping, user_callback) {
  var context = new userContext.UserContext(arguments, 2, 1, null, this);
  return context.createTable();
};

SessionFactory.prototype.close = function(user_callback) {
  var self = this;
  udebug.log('close for key', self.key, 'database', self.properties.database);
  var i;
  var session;
  var numberOfSessionsToClose = 0;
  var closedSessions = 0;

  function closeOnConnectionClose() {
    if(typeof(user_callback) === 'function') { 
      udebug.log_detail('closeOnConnectionClose calling user_callback');
      user_callback();
    }
  }
    
  function closeConnection() {
    if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('closeConnection calling mynode.delete_callback for key', self.key,
        'database', self.properties.database);
    self.delete_callback(self.key, self.properties.database, closeOnConnectionClose);
  }
  
  var onSessionClose = function(err) {
    if (++closedSessions === numberOfSessionsToClose) {
      closeConnection();
    }
  };
  
  // count the number of sessions to close
  for (i = 0; i < self.sessions.length; ++i) {
    if (self.sessions[i]) {
      ++numberOfSessionsToClose;
    }
  }
  udebug.log('session factory', self.key, 'found', numberOfSessionsToClose, 'sessions to close.'); 
  // if no sessions to close, go directly to close dbConnectionPool
  if (numberOfSessionsToClose === 0) {
    closeConnection();
  }    
  // close the sessions
  for (i = 0; i < self.sessions.length; ++i) {
    if (self.sessions[i]) {
      self.sessions[i].close(onSessionClose);
      self.sessions[i] = null;
    }
  }
};


SessionFactory.prototype.closeSession = function(index, session) {
  this.sessions[index] = null;
};


SessionFactory.prototype.getOpenSessions = function() {
  var result = [];
  var i;
  for (i = 0; i < this.sessions.length; ++i) {
    if (this.sessions[i]) {
      result.push(this.sessions[i]);
    }
  }
  return result;
};


SessionFactory.prototype.registerTypeConverter = function(type, converter) {
  return this.dbConnectionPool.registerTypeConverter(type, converter);
};

/** Get a proxy for a db object similar to "easy to use" api.
 * 
 * @param db_name optional database name to use
 * @return db
 */
SessionFactory.prototype.db = function(db_name) {
  return new Db(this, db_name);
};

/** Associate a table mapping with a table name. This is used for cases where users
 * prefer to use their own table mapping and possibly specify forward mapping meta.
 * This function is immediate.
 */
SessionFactory.prototype.mapTable = function(tableMapping) {
  var database = tableMapping.database || this.properties.database;
  var qualifiedTableName = database + '.' + tableMapping.table;
  this.tableMappings[qualifiedTableName] = tableMapping;
  udebug.log('mapTable', util.inspect(tableMapping), this.properties, qualifiedTableName);
};

exports.SessionFactory = SessionFactory;
