
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

'use strict';

var nosql = require("../..");

function getDefaultAdapter() {
  return "ndb";
  // return "mysql";
}
function getProperties(adapter) {
  var properties = new nosql.ConnectionProperties(adapter);

  // properties.ndb_connectstring = "localhost:1186";
  // properties.mysql_socket = null;
  // properties.mysql_port = null;
  // properties.mysql_user = null;
  // properties.mysql_host = null;
  // properties.mysql_password = null;

  return properties;
}

exports.getProperties = getProperties;
exports.getDefaultAdapter = getDefaultAdapter;
