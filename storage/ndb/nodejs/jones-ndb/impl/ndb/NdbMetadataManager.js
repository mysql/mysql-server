/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

"use strict";

var path         = require("path"),
    assert       = require("assert"),
    fs           = require("fs"),
    existsSync   = fs.existsSync || path.existsSync,
    jones        = require("database-jones"),
    jonesMysql   = require("jones-mysql"),
    ndb_test_dir = require("./path_config").suites_dir,
    udebug       = unified_debug.getLogger("NdbMetadataManager.js");


function findMetadataScript(suiteName, suitePath, file) {
  var path1, path2, path3;
  path1 = path.join(ndb_test_dir, suiteName, file);   // NDB
  path2 = path.join(jonesMysql.config.suites_dir, "standard", suiteName + "-" + file);
  path3 = path.join(suitePath, file);  // MySQL

  if(existsSync(path1)) { return path1; }
  if(existsSync(path2)) { return path2; }
  if(existsSync(path3)) { return path3; }

  console.log("No path to:", suiteName, file);
}


function NdbMetadataManager(properties) {
  var sqlProps = {
    "mysql_user"                : properties.mysql_user,
    "mysql_host"                : properties.mysql_host,
    "mysql_port"                : properties.mysql_port,
    "mysql_password"            : properties.mysql_password,
    "implementation"            : "mysql",
    "isMetadataOnlyConnection"  : true
  };

  this.sqlConnectionProperties = new jones.ConnectionProperties(sqlProps);
}

NdbMetadataManager.prototype.execDDL = function(statement, callback) {
  jones.openSession(this.sqlConnectionProperties).then(function(session) {
    udebug.log("onSession");
    var driver = session.dbSession.pooledConnection;
    assert(driver);
    driver.query(statement, function(err) {
      udebug.log("onQuery // err:", err);
      session.close();
      callback(err);
    });
  }, function(err) { callback(err); });
};

NdbMetadataManager.prototype.runSQL = function(sqlPath, callback) {
  assert(sqlPath);
  udebug.log("runSQL", sqlPath);
  var statement = "set default_storage_engine=ndbcluster;\n";
  statement += fs.readFileSync(sqlPath, "ASCII");
  this.execDDL(statement, callback);
};


NdbMetadataManager.prototype.createTestTables = function(suiteName, suitePath, callback) {
  udebug.log("createTestTables", suiteName);
  var sqlPath = findMetadataScript(suiteName, suitePath, "create.sql");
  this.runSQL(sqlPath, callback);
};


NdbMetadataManager.prototype.dropTestTables = function(suiteName, suitePath, callback) {
  udebug.log("dropTestTables", suiteName);
  var sqlPath = findMetadataScript(suiteName, suitePath, "drop.sql");
  this.runSQL(sqlPath, callback);
};


module.exports = NdbMetadataManager;

