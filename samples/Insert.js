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

'use strict';

var mynode = require('../Adapter/api/mynode.js');

unified_debug.level_detail();

// define a simple mapping class
var t_basic = function(id, name, age, magic) {
  this.id = id;
  this.name = name;
  this.age = age;
  this.magic = magic;
};

// check results of insert
var onInsert = function(err, object) {
  console.log('onInsert.');
  if (err) {
    console.log(err);
  } else {
    console.log('Inserted: ' + object);
  }
  process.exit(0);
};

// insert an object
var onSession = function(err, session) {
  console.log('onSession: ' + util.inspect(session));
  if (err) {
    console.log('Error onSession.');
    console.log(err);
    process.exit(0);
  } else {
    var data = new t_basic(1, 'Craig', 99, 99);
    console.log('data.mynode: ' + util.inspect(data.mynode));
    session.persist(data, onInsert);
  }
};

// *** program starts here ***

//create a database properties object
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

// create a basic mapping

var annotations = new mynode.Annotations();
var t_basic = function(id, name, age, magic) {
  this.id = id;
  this.name = name;
  this.age = age;
  this.magic = magic;
};

annotations.mapClass(t_basic, {'table' : 't_basic'});

// connect to the database
mynode.openSession(dbProperties, annotations, onSession);

