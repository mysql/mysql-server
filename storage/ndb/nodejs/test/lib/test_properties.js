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

/*global mynode, path, fs, suites_dir */

"use strict";

/* Manage test_connection.js file:
   Read it if it exists.
   If it doesn't exist, copy it from the standard template in lib/
*/
function getTestConnectionProperties() {
  var props_file     = path.join(suites_dir, "test_connection.js");
  var props_template = path.join(suites_dir, "lib", "test_connection_js.dist");
  var existsSync     = fs.existsSync || path.existsSync;
  var properties     = null;
  var f1, f2; 
  
  if(! existsSync(props_file)) {
    try {
      f1 = fs.createReadStream(props_template);
      f2 = fs.createWriteStream(props_file);
      f1.pipe(f2);
      f1.on('end', function() {});
    }
    catch(e1) {
      console.log(e1);
    }
  }

  try {
    properties = require(props_file);
  }
  catch(e2) {
  }

  return properties;
} 


function getAdapterProperties(adapter) {
  var impl = adapter || global.adapter;
  var p = new mynode.ConnectionProperties(impl);
  return p;
}


function merge(target, m) {
  var p;
  for(p in m) {
    if(m.hasOwnProperty(p)) {
      target[p] = m[p];
    }
  }
}


exports.connectionProperties = function() {
  var adapterProps  = getAdapterProperties();
  var localConnectionProps = getTestConnectionProperties();
  merge(adapterProps, localConnectionProps);
  return adapterProps;
};


