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

var SessionFactory = function(key) {
  this.key = key;
  this.dbconnection = {};
  this.annotations = {};
  this.properties = {};
  this.sessions = [];
};


//openSession(Annotations annotations, Function(Object error, Session session, ...) callback, ...);
// Open new session or get one from a pool
SessionFactory.prototype.openSession = function(annotations, user_callback, extra1, extra2, extra3, extra4) {
  // allocate a new session slot in sessions
  for (var i = 0; i < this.sessions.length; ++i) {
    if (this.sessions[i] == null) break;
  }
  var newDBSession = this.dbconnection.getDBSession(i);
  var newSession = new session.Session(i, this, newDBSession);
  this.sessions[i] = newSession;
  user_callback(null, newSession, extra1, extra2, extra3, extra4);  // todo: extra user parameters
};


SessionFactory.prototype.close = function() {
  // TODO: close all sessions first
  this.delete_callback(this.key, this.properties.database);
};


SessionFactory.prototype.closeSession = function(index, session) {
  // TODO put session back into pool
  this.sessions[index] = null;
};


exports.SessionFactory = SessionFactory;
