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
var adapter = require("../build/Release/ndb/ndb_adapter.node");


/* Do-Nothing Constructor.  Instances are actually constructed 
   by NdbSession.getDataDictionary()
*/
exports.DBDictionary = function() { 
  udebug.log("DBDictionary constructor");
  this.session = null;
  this.impl = null;
};


/** List all tables in the schema
  * ASYNC
  * 
  * listTables(databaseName, callback(error, array));
  */
exports.DBDictionary.prototype.listTables = function(databaseName, user_callback) {
  udebug.log("DBDictionary listTables");
  if(typeof adapter.impl.DBDictionary.listTables !== 'function') {
    console.log("NOT A FUNCTION!");
  }
  adapter.impl.DBDictionary.listTables(this.impl, databaseName, user_callback);
}

