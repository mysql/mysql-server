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

/*global unified_debug */

"use strict";

var session     = require("./Session.js"),
    udebug      = unified_debug.getLogger("SessionFactory.js"),  
    userContext = require('../impl/common/UserContext.js'),
    util        = require("util");

var SessionFactory = function(key, dbConnectionPool, properties, mappings, delete_callback) {
  this.key = key;
  this.dbConnectionPool = dbConnectionPool;
  this.properties = properties;
  this.mappings = mappings;
  this.delete_callback = delete_callback;
  this.sessions = [];
  this.tableHandlers = {};
  this.tableMetadatas = {};
};


//openSession(Function(Object error, Session session, ...) callback, ...);
// Open new session or get one from a pool
SessionFactory.prototype.openSession = function() {
  var context = new userContext.UserContext(arguments, 2, 2, null, this);
  // delegate to context for execution
  context.openSession();
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
 * @param tableName the name of the table
 * @param callback
 */
SessionFactory.prototype.getTableMetadata = function() {
  var context = new userContext.UserContext(arguments, 2, 2, null, this);
  // delegate to context for execution
  context.getTableMetadata();
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
    udebug.log_detail('closeConnection calling mynode.delete_callback for key', self.key,
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


exports.SessionFactory = SessionFactory;
