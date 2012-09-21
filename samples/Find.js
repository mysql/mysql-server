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

var mynode = require('..');

var adapter = 'ndb';

//dump the values of the object
var onData = function(err, data) {
  if (err) {
    console.log(err);
    process.exit(0);
  }
  console.log('Found: ', JSON.stringify(data));
  process.exit(0);
};

//find an object
var onSession = function(err, session) {
  
  if (err) {
    console.log(err);
    process.exit(0);
  }
  session.find('t_basic', 0, onData);
};

//analyze command line

var usageMessage = 
  "Usage: node Find [options]\n" +
  "       -h or --help: print this message\n" +
  "      -d or --debug: set the debug flag\n" +
  "           --detail: set the detail debug flag\n" +
  "--adapter=<adapter>: run on the named adapter (e.g. ndb or mysql)\n"
  ;

// handle command line arguments
var i, exit, val, values;

for(i = 2; i < process.argv.length ; i++) {
  val = process.argv[i];
  switch (val) {
  case '--debug':
  case '-d':
    unified_debug.on();
    unified_debug.level_debug();
    break;
  case '--detail':
    unified_debug.on();
    unified_debug.level_detail();
    break;
  case '--help':
  case '-h':
    exit = true;
    break;
  default:
    values = val.split('=');
    if (values.length === 2) {
      switch (values[0]) {
      case '--adapter':
        adapter = values[1];
        break;
      default:
        console.log('Invalid option ' + val);
        exit = true;
      }
    } else {
      console.log('Invalid option ' + val);
      exit = true;
   }
  }
}

if (exit) {
  console.log(usageMessage);
  process.exit(0);
}

console.log('Running find with adapter', adapter);

//create a database properties object

var dbProperties = mynode.ConnectionProperties(adapter);


// connect to the database
mynode.openSession(dbProperties, null, onSession);

