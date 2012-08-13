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
var adapter = require("../build/Release/ndb/ndb_adapter.node"),
    ndbdictionary = require("./NdbDictionary.js");



/* Private Constructor
Instances are actually constructed 
   by NdbConnectionPool.getSessionHandler().
*/
exports.DBSession = function() { 
  udebug.log("DBSession constructor");
};



/** get data dictionary.
 *  IMMEDIATE
 *  Immediately returns a DBDictionary object.  The underlying 
 *  local data dictionary may be empty, or may contain cached entries from
 *  earlier calls.
 * 
 *  @return DBDictionary
 */
exports.DBSession.prototype.getDataDictionary = function() {
  udebug.log("DBSession getDataDictionary");
  var dict = new ndbdictionary.DBDictionary();
  dict.session = this;
  dict.impl = adapter.impl.DBDictionary.create(this.impl);
  return dict;
}
