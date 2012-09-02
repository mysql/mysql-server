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

/* Requires version 2.0 of Felix Geisendoerfer's MySQL client */
var mysql = require("mysql");
var dictionary = require('./MySQLDictionary');

/** MySQLConnection wraps a mysql connection and implements the DBSession contract */
exports.DBSession = function(pooledConnection) {
  this.pooledConnection = pooledConnection;
};

// TODO proper extra parameter handling
exports.DBSession.prototype.find = function(from, key, callback, extra1, extra2, extra3, extra4) {
//  console.log('MySQLConnection.find for ' + from + ' key: ' + key);
  var err = new Error('MySQLConnection.find not implemented');
  callback(err, null, extra1, extra2, extra3, extra4);
};

//TODO proper extra parameter handling
exports.DBSession.prototype.persist = function(instance, callback, extra1, extra2, extra3, extra4) {
//  console.log('MySQLConnection.persist for ' + instance);
  var err = new Error('MySQLConnection.persist not implemented');
  callback(err, extra1, extra2, extra3, extra4);
};

exports.DBSession.prototype.closeSync = function() {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
};

//TODO

exports.DBSession.prototype.getDataDictionary = function() {
  return new dictionary.DataDictionary(this.pooledConnection);
};

exports.DBSession.prototype.getConnectionPool = function() {
  return this.pool;
};

