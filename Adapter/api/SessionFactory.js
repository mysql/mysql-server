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

var session = require("./Session.js");

var SessionFactory = function(key, dbConnectionPool, properties, annotations, delete_callback) {
  this.key = key;
  this.dbConnectionPool = dbConnectionPool;
  this.properties = properties;
  this.annotations = annotations;
  this.delete_callback = delete_callback;
  this.sessions = [];
};


//openSession(Annotations annotations, Function(Object error, Session session, ...) callback, ...);
// Open new session or get one from a pool
SessionFactory.prototype.openSession = function(annotations, user_callback, extra1, extra2, extra3, extra4) {
  var callback = user_callback; // save user_callback for use inside dbSessionCreated_callback
  callback_arguments = arguments;
  // allocate a new session slot in sessions
  for (var i = 0; i < this.sessions.length; ++i) {
    if (this.sessions[i] == null) break;
  }
  this.sessions[i] = {'placeholder':true, 'index':i};
  var sessions = this.sessions;
  var sessionFactory = this;

  var dbSessionCreated_callback = function(err, dbSession) {
    var newSession = new session.Session(i, sessionFactory, dbSession);
    sessions[i] = newSession;
    callback_arguments[0] = err;
    callback_arguments[1] = newSession;
    // TODO: use user_callback.apply to return extra arguments but what is "this" for the apply function?
    callback(err, newSession, extra1, extra2, extra3, extra4);  // todo: extra user parameters
  };
  var newDBSession = this.dbConnectionPool.getDBSession(i, dbSessionCreated_callback);

};


SessionFactory.prototype.close = function() {
  // TODO: close all sessions first
  this.dbConnectionPool.closeSync();
  this.delete_callback(this.key, this.properties.database);
};


SessionFactory.prototype.closeSession = function(index, session) {
  // TODO put session back into pool
  this.sessions[index] = null;
};


exports.SessionFactory = SessionFactory;
