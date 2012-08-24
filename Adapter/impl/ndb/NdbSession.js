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
var adapter       = require("../build/Release/ndb/ndb_adapter.node"),
    ndbdictionary = require("./NdbDictionary.js"),
    ndboperation  = require("./NdbOperation.js"),
    dbtxhandler   = require("./NdbTransactionHandler.js"),
    util          = require("util"),
    proto = {};



/* Private Constructor
Instances are actually constructed 
   by NdbConnectionPool.getSessionHandler().
*/
exports.DBSession = function() { 
  udebug.log("DBSession constructor");
  this.tx = null;
};

/* Private method ndbsession.getNdb(DBSession) */
exports.getNdb = function(dbsession) {
  udebug.log("ndbsession getNdb(DBSession)");
  udebug.log("Session: " + util.inspect(dbsession, true, 2, true));
  var ndb = dbsession.impl.getNdb();
  udebug.log("Ndb: " + util.inspect(ndb, true, 2, true));
};


/** get data dictionary.
 *  IMMEDIATE
 *  Immediately returns a DBDictionary object.  The underlying 
 *  local data dictionary may be empty, or may contain cached entries from
 *  earlier calls.
 * 
 *  @return DBDictionary
 */
proto.getDataDictionary = function() {
  udebug.log("DBSession getDataDictionary");
  var dict = new ndbdictionary.DBDictionary();
  dict.session = this;
  dict.impl = adapter.impl.DBDictionary.create(this.impl);
  return dict;
};

proto.openTransaction = function() {
  udebug.log("DBSession openTransaction");
  this.tx = new dbtxhandler.DBTransactionHandler(this);
  return this.tx;
};

proto.read = function(table, keys) {
  udebug.log("DBSession read "+ table.name);
  lockMode = "SHARED";
  var op = ndboperation.getReadOperation(tx, table, keys, lockMode);
  return op;
};

proto.insert = function(table, row) {
  udebug.log("DBSession insert " + table.name);

  var op = ndboperation.getInsertOperation(this.tx, table, row);
  return op;
};

exports.DBSession.prototype = proto;
