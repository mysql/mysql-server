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

'use strict';

var mynode = require('../Adapter/api/mynode.js');

// create a database properties object
var dbProperties = {  
    "implementation" : "ndb",
    "database" : "test",
    "ndb_connectstring" : "localhost:1186",
    "ndb_connect_retries" : 4, 
    "ndb_connect_delay" : 5,
    "ndb_connect_verbose" : 0,
    "ndb_connect_timeout_before" : 30,
    "ndb_connect_timeout_after" : 20
};

//dump the values of the object
var onData = function(err, data) {
  if (err) {
    console.log(err);
    exit;
  }
  JSON.stringify(data);
  exit;
};

//find an object
var onConnect = function(err, session) {
  if (err) {
    console.log(err);
    exit;
  }
  session.find('t_basic', 0, onData);
};

// connect to the database
mynode.connect(dbProperties, null, onConnect);

