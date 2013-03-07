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

/*global unified_debug */

"use strict";

var session     = require("./Session.js"),
    udebug      = unified_debug.getLogger("SessionFactory.js"),  
    userContext = require('../impl/common/UserContext.js'),
    util        = require("util");

var SessionFactory = function(key, dbConnectionPool, properties, annotations, delete_callback) {
  this.key = key;
  this.dbConnectionPool = dbConnectionPool;
  this.properties = properties;
  this.annotations = annotations;
  this.delete_callback = delete_callback;
  this.sessions = [];
  this.tableHandlers = {};
  this.tableMetadatas = {};
};


//openSession(Annotations annotations, Function(Object error, Session session, ...) callback, ...);
// Open new session or get one from a pool
SessionFactory.prototype.openSession = function() {
  var context = new userContext.UserContext(arguments, 2, 2, null, this);
  // delegate to context for execution
  context.openSession();
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
  var i;
  var session;
  var numberOfSessionsToClose = 0;
  var closedSessions = 0;
  
  function onDbSessionPoolClose() {
    self.delete_callback(self.key, self.properties.database);
    if(typeof(user_callback) === 'function') { 
      user_callback();
    }
  }
  
  var closeDBConnectionPool = function() {
    var dbConnectionPool = self.dbConnectionPool;
    delete self.dbConnectionPool;
    // close the dbConnectionPool if it is still around
    if (dbConnectionPool) {
      dbConnectionPool.close(onDbSessionPoolClose);
    } else {
      // always call back
      onDbSessionPoolClose();
    }
  };

  var onSessionClose = function(err) {
    if (++closedSessions === numberOfSessionsToClose) {
      closeDBConnectionPool();
    }
  };
  
  // count the number of sessions to close
  for (i = 0; i < self.sessions.length; ++i) {
    if (self.sessions[i]) {
      ++numberOfSessionsToClose;
    }
  }
  // if no sessions to close, go directly to close dbConnectionPool
  if (self.sessions.length === 0) {
    closeDBConnectionPool();
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

exports.SessionFactory = SessionFactory;
