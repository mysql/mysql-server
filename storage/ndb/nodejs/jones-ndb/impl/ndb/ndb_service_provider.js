/*
 Copyright (c) 2014, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

"use strict";

var path = require("path");
var fs = require("fs");
var assert = require("assert");
var conf = require("./path_config");

var DatetimeConverter =
    require(path.join(conf.converters_dir, "NdbDatetimeConverter"));
var TimeConverter = require(path.join(conf.converters_dir, "NdbTimeConverter"));
var DateConverter = require(path.join(conf.converters_dir, "NdbDateConverter"));
var propertiesDocFile = path.join(conf.root_dir, "DefaultConnectionProperties");
var unified_debug = require("unified_debug");
var jones = require("database-jones");
var udebug = unified_debug.getLogger("ndb_service_provider.js");
var gypConfigFile = path.join(conf.root_dir, "config.gypi");


/* Let unmet module dependencies be caught by loadRequiredModules() */
var NdbMetadataManager, DBConnectionPool;
try {
  NdbMetadataManager = require("./NdbMetadataManager.js");
} catch (ignore) {
}

try {
  DBConnectionPool = require("./NdbConnectionPool.js").DBConnectionPool;
} catch (ignore) {
}


exports.loadRequiredModules = function() {
  var err, ldp, msg, jsonconfig, libpath;
  var existsSync = fs.existsSync || path.existsSync;

  /* Load the binary */
  try {
    require(conf.binary);
  } catch (e) {
    ldp =
        process.platform === 'darwin' ? 'DYLD_LIBRARY_PATH' : 'LD_LIBRARY_PATH';
    msg = "\n\n" +
        "  The ndb adapter cannot load the compiled module ndb_adapter.node.\n";
    if (existsSync(conf.binary)) {
      msg +=
          "  This module has been built, but was not succesfully loaded.  Perhaps \n" +
          "  setting " + ldp + " to the mysql lib directory (containing \n" +
          "  libndbclient) will resolve the problem.\n\n";
      if (existsSync(gypConfigFile)) {
        try {
          /* 'utf8' here is a json, rather than MySQL, setting. */
          jsonconfig = fs.readFileSync(gypConfigFile, 'utf8');
          jsonconfig = JSON.parse(jsonconfig);
          libpath = path.join(jsonconfig.variables.mysql_path, "lib");
          msg += "  e.g.:\n";
          msg += "  export " + ldp + "=" + libpath + "\n\n";
        } catch (ignore) {
        }
      }
    } else {
      msg += "  For help building jones-ndb, run \"node configure\" \n\n";
    }
    msg += "Original error: \n--------------- \n" + e.message;
    err = new Error(msg);
    err.cause = e;
    throw err;
  }

  /* Load jones-mysql */
  try {
    require("jones-mysql");
  } catch (e2) {
    msg = "jones-ndb requires mysql for metadata operations.\n";
    msg += "Original error: " + e2.message;
    err = new Error(msg);
    err.cause = e2;
    throw err;
  }
};


exports.getDefaultConnectionProperties = function() {
  return require(propertiesDocFile);
};


function registerDefaultTypeConverters(dbConnectionPool) {
  dbConnectionPool.registerTypeConverter("DATETIME", DatetimeConverter);
  dbConnectionPool.registerTypeConverter("TIME", TimeConverter);
  dbConnectionPool.registerTypeConverter("DATE", DateConverter);
  dbConnectionPool.registerTypeConverter(
      "JSON", jones.converters.SerializedObjectConverter);
  // TODO: converter for Timestamp microseconds <==> JS Date
}


exports.connect = function(properties, user_callback) {
  udebug.log("connect");
  assert(properties.implementation === "ndb");
  var dbconn = new DBConnectionPool(properties);
  registerDefaultTypeConverters(dbconn);
  dbconn.connect(user_callback);
};


exports.getFactoryKey = function(properties) {
  udebug.log("getFactoryKey");
  assert(properties.implementation === "ndb");
  var key = properties.implementation + "://" + properties.ndb_connectstring;
  return key;
};


exports.getDBMetadataManager = function(properties) {
  return new NdbMetadataManager(properties);
};

exports.config = conf;
