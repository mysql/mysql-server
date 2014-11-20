/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

var path = require("path");
var fs = require("fs");
var DatetimeConverter = require(path.join(mynode.fs.converters_dir, "NdbDatetimeConverter"));
var TimeConverter = require(path.join(mynode.fs.converters_dir, "NdbTimeConverter"));
var DateConverter = require(path.join(mynode.fs.converters_dir, "NdbDateConverter"));

try {
  var DBConnectionPool = require("./NdbConnectionPool.js").DBConnectionPool;
}
catch(e) {
  console.log(e.stack);
  /* Let unmet module dependencies be caught by loadRequiredModules() */
}

var udebug  = unified_debug.getLogger("ndb_service_provider.js");

var NdbDefaultConnectionProperties = 
  require(path.join(mynode.fs.backend_doc_dir, "ndb_properties"));

/* Rely on MySQL SPI for MetadataManager */
var mysqlService = require(mynode.spi).getDBServiceProvider("mysql"),
    mysqlMetadataManager = mysqlService.getDBMetadataManager();


exports.loadRequiredModules = function() {
  var err, ldp, module, msg;
  module = path.join(mynode.fs.build_dir, "ndb_adapter.node");
  var existsSync = fs.existsSync || path.existsSync;
  try {
    require(module);
    return true;
  }
  catch(e) {
    ldp = process.platform === 'darwin' ? 'DYLD_LIBRARY_PATH' : 'LD_LIBRARY_PATH';
    msg = "\n\n" +
      "  The ndb adapter cannot load the native code module ndb_adapter.node.\n";
    if(existsSync(module)) {
      msg += 
      "  This module has been built, but was not succesfully loaded.  Perhaps \n" +
      "  setting " + ldp + " to the mysql lib directory (containing \n" +
      "  libndbclient) will resolve the problem.\n\n";
    }
    else {
      msg += 
      "  For help building the module, run " + 
      "\"setup/build.sh\" or \"npm install .\"\n\n";
    }
    msg += "Original error: " + e.message;
    err = new Error(msg);
    err.cause = e;
    throw err;
  }
};

exports.getDefaultConnectionProperties = function() {
  // Is this a bug?  Use the docs as the constructor, not as the object
  return NdbDefaultConnectionProperties;
};


function registerDefaultTypeConverters(dbConnectionPool) { 
  dbConnectionPool.registerTypeConverter("DATETIME", DatetimeConverter);
  dbConnectionPool.registerTypeConverter("TIME", TimeConverter);
  dbConnectionPool.registerTypeConverter("DATE", DateConverter);
  // TODO: converter for Timestamp microseconds <==> JS Date 
}


exports.connect = function(properties, user_callback) {
  udebug.log("connect");
  var dbconn = new DBConnectionPool(properties);
  registerDefaultTypeConverters(dbconn);
  dbconn.connect(user_callback);
};


exports.getFactoryKey = function(properties) {
  udebug.log("getFactoryKey");
  var key = properties.implementation + "://" + properties.ndb_connectstring;
  return key;
};


exports.getDBMetadataManager = function() {
  return mysqlMetadataManager;
};
