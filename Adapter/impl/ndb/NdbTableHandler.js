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

var proto = {};
/* Private Constructor. 
*/
exports.DBTableHandler = function(db_table) { 
  udebug.log("DBTableHandler constructor");
  this.dbtable = db_table;
  
  
};


proto.useAllColumns = function() {

};

proto.useMapping = function(map) {


};


proto.useColumns = function(col_def) {


};


proto.useMappedColumns = function(mapping) {


};


proto.registerConverter = function(col_name, converter_class) {



};


proto.setResultPrototype = function(proto_object) {


};










exports.DBTableHandler.prototype = proto;
