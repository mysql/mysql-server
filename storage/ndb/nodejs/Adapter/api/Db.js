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

/*global unified_debug */

"use strict";

var session     = require("./Session.js"),
    udebug      = unified_debug.getLogger("Db.js"),  
    userContext = require("./UserContext.js"),
    util        = require("util"),
    TableMapping= require("./TableMapping.js"),
    meta        = require("./Meta.js"),
    createProxy = require("./ProxyFactory.js");

/** Db implementation for easy-to-use interface implements db.table_name.insert and db.table_name.find.
 * DbImpl is a holder for Table objects that implement the insert and find methods.
 */
function DbImpl(sessionFactory, db_name) {
  this.sessionFactory = sessionFactory;
  this.db_name = db_name || sessionFactory.properties.database;
}

/** Table for db object implements insert and find
 */
function Table(table_name, dbImpl, sessionFactory) {
  this.table_name = table_name;
  this.dbImpl = dbImpl;
  this.sessionFactory = sessionFactory;
}

/** failGet is the callback from proxy when the name is not a member of the object
 * 
 * @param name the non-existing db property name: the table name
 * @return nothing
 */
DbImpl.prototype.failGet = function(name) {
  var dbImpl = this;
  var table = new Table(name, dbImpl, dbImpl.sessionFactory);
  dbImpl[name] = table;
};

/** Db proxy delegates to DbImpl for db.table_name.insert and db.table_name.find
 */
function DB(sessionFactory, db_name) {
  var dbImpl = new DbImpl(sessionFactory, db_name);
  // return a proxy for the DbImpl
  return createProxy(dbImpl);
}

function createDefaultTableMapping(db_name, table_name) {
  udebug.log('Db.createDefaultTableMapping for', db_name, table_name);
  var dbDotTable, tableMapping;
  dbDotTable = db_name?db_name + '.' + table_name: table_name;
  tableMapping = new TableMapping.TableMapping(dbDotTable);
  tableMapping.mapField('id', meta.int(32).primaryKey());
  tableMapping.mapSparseFields('SPARSE_FIELDS', meta.varchar(11000));
  return tableMapping;
}

Table.prototype.resolveTable = function(callback) {
  var table = this;
  udebug.log('Table.resolveTable for', table.dbImpl.db_name, table.table_name);
  function resolveTableOnTableMetadata(err, tableMetadata) {
    udebug.log('Table.resolveTable.resolveTableOnTableMetadata for', table.dbImpl.db_name, table.table_name);
    if (err) {
      callback(err);
    } else {
      udebug.log('Table.resolveTable.resolveTableOnTableMetadata for', table.dbImpl.db_name, table.table_name,
          ' found table metadata');
      table.tableMetadata = tableMetadata;
      callback(null);
    }
  }

  // resolveTable starts here
  if (table.tableMetadata) {
    callback(null);
  } else {
    table.sessionFactory.getTableMetadata(table.dbImpl.db_name, table.table_name, resolveTableOnTableMetadata);
  }
};

Table.prototype.insert = function(obj, callback) {
  var table = this;
  var sessionForInsert;
  function insertOnPersist(persistErr) {
    sessionForInsert.close(function(closeErr) {
      if (persistErr) {
        callback(persistErr);
      } else {
        callback(closeErr);
      }
    });
  }
  function insertOnCreateTable(err) {
    if (err) {
      callback(err);
    } else {
      udebug.log('Db.insert.insertOnCreateTable created table for: ', table.dbImpl.db_name, table.table_name);
      sessionForInsert.persist(table.table_name, obj, insertOnPersist);
    }
  }
  function insertOnResolveTable(err) {
    var tableMapping;
    if (err) {
      //  no table; create it from either session factory table mappings or the default table mapping
      tableMapping = table.sessionFactory.tableMappings[table.dbImpl.db_name + '.' + table.table_name];
      if (!tableMapping) {
        udebug.log('Db.insert.insertOnResolveTable creating table for: ', table.dbImpl.db_name, table.table_name);
        tableMapping = createDefaultTableMapping(table.dbImpl.db_name, table.table_name);
      }
      udebug.log('Db.insert.insertOnResolveTable tableMapping: ', util.inspect(tableMapping));
      table.dbImpl.sessionFactory.createTable(tableMapping, insertOnCreateTable);
    } else {
      // if table exists, persist with table name
      sessionForInsert.persist(table.table_name, obj, insertOnPersist);
    }
  }
  function insertOnSession(err, session) {
    sessionForInsert = session;
    udebug.log_detail('Table.insert.insertOnSession for', table.table_name, ' with err', err);
    if (err) {
      // we did not get a session so return the error
      callback(err);
      return;
    }
    table.resolveTable(insertOnResolveTable);
  }

  // insert starts here
  udebug.log_detail('Table.insert for', this.table_name);
  table.sessionFactory.openSession(null, insertOnSession);
};

Table.prototype.find = function(params, callback) {
  var table = this;
  console.log('Table.find for', table.table_name);
  callback(new Error('Table.find not implemented'));
};

module.exports = DB;
