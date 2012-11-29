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

"use strict";

var assert = require("assert"),
    commonDBTableHandler = require("./DBTableHandler.js"),
    apiSession = require("../../api/Session.js"),
    udebug     = unified_debug.getLogger("UserContext.js");
    util       = require("util");


/** Create a function to manage the context of a user's asynchronous call.
 * All asynchronous user functions make a callback passing
 * the user's extra parameters from the original call as extra parameters
 * beyond the specified parameters of the call. For example, the persist function
 * is specified to take two parameters: the data object itself and the callback.
 * The result of persist is to call back with parameters of an error object, 
 * and the same data object which was passed. 
 * If extra parameters are passed to the persist function, the user's function
 * will be called with the specified parameters plus all extra parameters from
 * the original call. 
 * The constructor remembers the original user callback function and the original
 * parameters of the function.
 * The user callback function is always the last required parameter of the function call.
 * Additional context is added as the function progresses.
 * @param user_arguments the original arguments as supplied by the user
 * @param required_parameter_count the number of required parameters 
 * NOTE: the user callback function must be the last of the required parameters
 * @param returned_parameter_count the number of required parameters returned to the callback
 * @param session the Session which may be null for SessionFactory functions
 * @param session_factory the SessionFactory which may be null for Session functions
 * @param execute (optional; defaults to true) whether to execute the operation immediately;
 *        if execute is false, the operation is constructed and is available via the "operation"
 *        property of the user context.
 */
exports.UserContext = function(user_arguments, required_parameter_count, returned_parameter_count,
    session, session_factory, execute) {
  if (arguments.length !== 6) {
    if (arguments.length === 5) {
      this.execute = true;
    } else {
    throw new Error(
        'Fatal internal exception: wrong parameter count ' + arguments.length + ' for UserContext constructor' +
        '; expected 5 or 6)');
    }
  }
  this.user_arguments = user_arguments;
  this.user_callback = user_arguments[required_parameter_count - 1];
  this.required_parameter_count = required_parameter_count;
  this.extra_arguments_count = user_arguments.length - required_parameter_count;
  this.returned_parameter_count = returned_parameter_count;
  this.session = session;
  this.session_factory = session_factory;
  /* indicates that batch.clear was called before this context had executed */
  this.clear = false;
  if (this.session !== null) {
    this.autocommit = !this.session.tx.isActive();
  }
};


/** Get table metadata.
 * Delegate to DBConnectionPool.getTableMetadata.
 */
exports.UserContext.prototype.getTableMetadata = function() {
  var userContext = this;
  var getTableMetadataOnTableMetadata = function(err, tableMetadata) {
    userContext.applyCallback(err, tableMetadata);
  };
  
  var databaseName = this.user_arguments[0];
  var tableName = this.user_arguments[1];
  var dbSession = (this.session)?this.session.dbSession:null;
  this.session_factory.dbConnectionPool.getTableMetadata(
      databaseName, tableName, dbSession, getTableMetadataOnTableMetadata);
};


/** List all tables in the default database.
 * Delegate to DBConnectionPool.listTables.
 */
exports.UserContext.prototype.listTables = function() {
  var userContext = this;
  var listTablesOnTableList = function(err, tableList) {
    userContext.applyCallback(err, tableList);
  };

  var databaseName = this.user_arguments[0];
  var dbSession = (this.session)?this.session.dbSession:null;
  this.session_factory.dbConnectionPool.listTables(databaseName, dbSession, listTablesOnTableList);
};

/** Get the table handler for a domain object, table name, or constructor.
 */
var getTableHandler = function(domainObjectTableNameOrConstructor, session, onTableHandler) {

  var TableHandlerFactory = function(mynode, tableName, sessionFactory, dbSession, mapping, ctor, onTableHandler) {
    this.tableName = tableName;
    this.sessionFactory = sessionFactory;
    this.dbSession = dbSession;
    this.onTableHandler = onTableHandler;
    this.mapping = mapping;
    this.dbName = sessionFactory.properties.database;
    this.tableKey = this.dbName + '.' + tableName;
    this.mynode = mynode;
    this.ctor = ctor;
    
    this.createTableHandler = function() {
      var tableHandlerFactory = this;
      var tableHandler;
      var tableMetadata;
      
      var onTableMetadata = function(err, tableMetadata) {
        var tableHandler;
        if (err) {
          tableHandlerFactory.onTableHandler(err, null);
        } else {
          udebug.log('TableHandlerFactory.onTableMetadata for ', tableHandlerFactory.tableKey);
          // check to see if the metadata has already been processed
          if (typeof tableHandlerFactory.sessionFactory.tableMetadatas[tableHandlerFactory.tableKey] === 'undefined') {
            // put the table metadata into the table metadata map
            tableHandlerFactory.sessionFactory.tableMetadatas[tableHandlerFactory.tableKey] = tableMetadata;
          }
          // we have the table metadata; now create the table handler if needed
          if (tableHandlerFactory.mapping === null) {
            // put the default table handler into the session factory
            if (typeof(tableHandlerFactory.sessionFactory.tableHandlers[tableHandlerFactory.tableName]) === 'undefined') {
              udebug.log_detail('UserContext caching the table handler in the sessionFactory for ', 
                  tableHandlerFactory.tableName);
              tableHandler = new commonDBTableHandler.DBTableHandler(tableMetadata, tableHandlerFactory.mapping,
                  tableHandlerFactory.ctor);
              tableHandlerFactory.sessionFactory.tableHandlers[tableHandlerFactory.tableName] = tableHandler;
            } else {
              tableHandler = tableHandlerFactory.sessionFactory.tableHandlers[tableHandlerFactory.tableName];
              udebug.log_detail('UserContext got tableHandler but someone else put it in the cache first for ', 
                  tableHandlerFactory.tableName);
            }
          } else {
            if (tableHandlerFactory.ctor) {
              if (typeof(tableHandlerFactory.ctor.prototype.mynode.tableHandler) === 'undefined') {
                // if a domain object mapping, cache the table handler in the prototype
                tableHandler = new commonDBTableHandler.DBTableHandler(tableMetadata, tableHandlerFactory.mapping,
                    tableHandlerFactory.ctor);
                tableHandlerFactory.ctor.prototype.mynode.tableHandler = tableHandler;
                udebug.log_detail('UserContext caching the table handler in the prototype for constructor.');
              } else {
                tableHandler = tableHandlerFactory.ctor.prototype.mynode.tableHandler;
                udebug.log_detail('UserContext got tableHandler but someone else put it in the prototype first.');
              }
            }
          }
        }
        tableHandlerFactory.onTableHandler(null, tableHandler);
      };
      
      // start of createTableHandler
      
      // get the table metadata from the cache of table metadatas in session factory
      tableMetadata = tableHandlerFactory.sessionFactory.tableMetadatas[tableHandlerFactory.tableKey];
      if (tableMetadata) {
        // we already have cached the table metadata
        onTableMetadata(null, tableMetadata);
      } else {
        // get the table metadata from the db connection pool
        // getTableMetadata(dbSession, databaseName, tableName, callback(error, DBTable));
        udebug.log('TableHandlerFactory.createTableHandler for ', tableHandlerFactory.tableKey);
        this.sessionFactory.dbConnectionPool.getTableMetadata(
            tableHandlerFactory.dbName, tableHandlerFactory.tableName, session.dbSession, onTableMetadata);
      }
    };
  };
    
  // start of getTableHandler 
  var err, mynode, tableHandler, tableHandlerFactory, tableIndicatorType;

  tableIndicatorType = typeof(domainObjectTableNameOrConstructor);
  if (tableIndicatorType === 'string') {
    udebug.log_detail('UserContext.getTableHandler for table ', domainObjectTableNameOrConstructor);
    // parameter is a table name; look up in table name to table handler hash
    tableHandler = session.sessionFactory.tableHandlers[domainObjectTableNameOrConstructor];
    if (typeof(tableHandler) === 'undefined') {
      udebug.log_detail('UserContext.getTableHandler did not find cached tableHandler for table ', domainObjectTableNameOrConstructor);
      // create a new table handler for a table name with no mapping
      // create a closure to create the table handler
      tableHandlerFactory = new TableHandlerFactory(
          null, domainObjectTableNameOrConstructor, session.sessionFactory, session.dbSession,
          null, null, onTableHandler);
      tableHandlerFactory.createTableHandler(null);
    } else {
      udebug.log_detail('UserContext.getTableHandler found cached tableHandler for table ', domainObjectTableNameOrConstructor);
      // send back the tableHandler
      onTableHandler(null, tableHandler);
    }
  } else if (tableIndicatorType === 'function') {
    udebug.log_detail('UserContext.getTableHandler for constructor.');
    mynode = domainObjectTableNameOrConstructor.prototype.mynode;
    // parameter is a constructor; it must have been annotated already
    if (typeof(mynode) === 'undefined') {
      err = new Error('User exception: constructor must have been annotated.');
      onTableHandler(err, null);
    } else {
      tableHandler = mynode.tableHandler;
      if (typeof(tableHandler) === 'undefined') {
        udebug.log_detail('UserContext.getTableHandler did not find cached tableHandler for constructor.');
        // create the tableHandler
        // getTableMetadata(dbSession, databaseName, tableName, callback(error, DBTable));
        tableHandlerFactory = new TableHandlerFactory(
            mynode, mynode.mapping.table, session.sessionFactory, session.dbSession, 
            mynode.mapping, domainObjectTableNameOrConstructor, onTableHandler);
        tableHandlerFactory.createTableHandler();
      } else {
        udebug.log_detail('UserContext.getTableHandler found cached tableHandler for constructor.');
        // prototype has been annotated; return the table handler
        onTableHandler(null, tableHandler);
      }
    }
  } else if (tableIndicatorType === 'object') {
    udebug.log_detail('UserContext.getTableHandler for domain object.');
    // parameter is a domain object; it must have been mapped already
    mynode = domainObjectTableNameOrConstructor.mynode;
    if (typeof(mynode) === 'undefined') {
      err = new Error('User exception: constructor must have been annotated.');
      onTableHandler(err, null);
    } else {
      tableHandler = mynode.tableHandler;
      if (typeof(tableHandler) === 'undefined') {
        udebug.log_detail('UserContext.getTableHandler did not find cached tableHandler for constructor.');
        // create the tableHandler
        // getTableMetadata(dbSession, databaseName, tableName, callback(error, DBTable));
        tableHandlerFactory = new TableHandlerFactory(
            mynode, mynode.mapping.table, session.sessionFactory, session.dbSession, 
            mynode.mapping, mynode.constructor, onTableHandler);
        tableHandlerFactory.createTableHandler();
      } else {
        udebug.log_detail('UserContext.getTableHandler found cached tableHandler for constructor.');
        // prototype has been annotated; return the table handler
        onTableHandler(null, tableHandler);
      }
    }
  } else {
    err = new Error('User error: parameter must be a domain object, string, or constructor function.');
    throw err;
  }
};

function checkOperation(err, dbOperation) {
  if (err) {
    return err;
  } else if (!dbOperation.result.success) {
    var code = dbOperation.result.error.code;
    return new Error('Operation error: ' + code);
  } else {
    return null;
  }
}

/** Find the object by key.
 * 
 */
exports.UserContext.prototype.find = function() {
  var userContext = this;
  var tableHandler;
  if (typeof(this.user_arguments[0]) === 'function') {
    userContext.domainObject = true;
  }

  function findOnResult(err, dbOperation) {
    udebug.log('find.findOnResult');
    var result, values;
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(err, null);
    } else {
      if (userContext.domainObject) {
        values = dbOperation.result.value;
        result = userContext.dbTableHandler.newResultObject(values);
      } else {
        result = dbOperation.result.value;
      }
      userContext.applyCallback(null, result);      
    }
  }

  function findOnTableHandler(err, dbTableHandler) {
    var dbSession, keys, index, transactionHandler;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err, null);
    } else {
      userContext.dbTableHandler = dbTableHandler;
      keys = userContext.user_arguments[1];
      index = dbTableHandler.getIndexHandler(keys, true);
      if (index === null) {
        err = new Error('UserContext.find unable to get an index to use for ' + JSON.stringify(keys));
        userContext.applyCallback(err, null);
      } else {
        // create the find operation and execute it
        dbSession = userContext.session.dbSession;
        transactionHandler = dbSession.getTransactionHandler();
        userContext.operation = dbSession.buildReadOperation(index, keys, transactionHandler, findOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            udebug.log_detail('find transactionHandler.execute callback.');
          });
        } else if (typeof(userContext.operationDefinedCallback) === 'function') {
          userContext.operationDefinedCallback(1);
        }
      }
    }
  }

  // find starts here
  // session.find(prototypeOrTableName, key, callback)
  // get DBTableHandler for prototype/tableName
  getTableHandler(userContext.user_arguments[0], userContext.session, findOnTableHandler);
};


/** Persist the object.
 * 
 */
exports.UserContext.prototype.persist = function() {
  var userContext = this;
  var tableHandler, object;

  function persistOnResult(err, dbOperation) {
    udebug.log('persist.persistOnResult');
    // return any error code
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(error);
    } else {
      userContext.applyCallback(null);      
    }
  }

  function persistOnTableHandler(err, dbTableHandler) {
    udebug.log_detail('UserContext.persist.persistOnTableHandler ' + err);
    var transactionHandler;
    var dbSession = userContext.session.dbSession;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err);
      return;
    } else {
      transactionHandler = dbSession.getTransactionHandler();
      userContext.operation = dbSession.buildInsertOperation(dbTableHandler, userContext.values, transactionHandler, persistOnResult);
      if (userContext.execute) {
        transactionHandler.execute([userContext.operation], function() {
          udebug.log_detail('persist transactionHandler.execute callback.');
        });
      } else if (typeof(userContext.operationDefinedCallback) === 'function') {
        userContext.operationDefinedCallback(1);
      }
    }
  }

  // persist starts here
  if (userContext.required_parameter_count === 2) {
    // persist(object, callback)
    userContext.values = userContext.user_arguments[0];
  } else if (userContext.required_parameter_count === 3) {
    // persist(tableNameOrConstructor, values, callback)
    userContext.values = userContext.user_arguments[1];
  } else {
    throw new Error(
        'Fatal internal error; wrong required_parameter_count ' + userContext.required_parameter_count);
  }
  // get DBTableHandler for table indicator (domain object, constructor, or table name)
  getTableHandler(userContext.user_arguments[0], userContext.session, persistOnTableHandler);
};

/** Save the object. If the row already exists, overwrite non-pk columns.
 * 
 */
exports.UserContext.prototype.save = function() {
  var userContext = this;
  var tableHandler, object, indexHandler;

  function saveOnResult(err, dbOperation) {
    // return any error code
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(error);
    } else {
      userContext.applyCallback(null);      
    }
  }

  function saveOnTableHandler(err, dbTableHandler) {
    var transactionHandler;
    var dbSession = userContext.session.dbSession;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err);
      return;
    } else {
      transactionHandler = dbSession.getTransactionHandler();
      indexHandler = dbTableHandler.getIndexHandler(userContext.values);
      if (!indexHandler.dbIndex.isPrimaryKey) {
        userContext.applyCallback(
            new Error('Illegal argument: parameter of save must include all primary key columns.'));
        return;
      }
      userContext.operation = dbSession.buildWriteOperation(indexHandler, userContext.values, transactionHandler, saveOnResult);
      if (userContext.execute) {
        transactionHandler.execute([userContext.operation], function() {
        });
      } else if (typeof(userContext.operationDefinedCallback) === 'function') {
        userContext.operationDefinedCallback(1);
      }
    }
  }

  // save starts here

  if (userContext.required_parameter_count === 2) {
    // save(object, callback)
    userContext.values = userContext.user_arguments[0];
  } else if (userContext.required_parameter_count === 3) {
    // save(tableNameOrConstructor, values, callback)
    userContext.values = userContext.user_arguments[1];
  } else {
    throw new Error(
        'Fatal internal error; wrong required_parameter_count ' + userContext.required_parameter_count);
  }
  // get DBTableHandler for table indicator (domain object, constructor, or table name)
  getTableHandler(userContext.user_arguments[0], userContext.session, saveOnTableHandler);
};

/** Update the object.
 * 
 */
exports.UserContext.prototype.update = function() {
  var userContext = this;
  var tableHandler, object, indexHandler;

  function updateOnResult(err, dbOperation) {
    // return any error code
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(error);
    } else {
      userContext.applyCallback(null);      
    }
  }

  function updateOnTableHandler(err, dbTableHandler) {
    var transactionHandler;
    var dbSession = userContext.session.dbSession;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err);
      return;
    } else {
      transactionHandler = dbSession.getTransactionHandler();
      indexHandler = dbTableHandler.getIndexHandler(userContext.keys);
      // for variant update(object, callback) the object must include all primary keys
      if (userContext.required_parameter_count === 2 && !indexHandler.dbIndex.isPrimaryKey) {
        userContext.applyCallback(
            new Error('Illegal argument: parameter of update must include all primary key columns.'));
        return;
      }
      userContext.operation = dbSession.buildUpdateOperation(indexHandler, userContext.keys, userContext.values, transactionHandler, updateOnResult);
      if (userContext.execute) {
        transactionHandler.execute([userContext.operation], function() {
        });
      } else if (typeof(userContext.operationDefinedCallback) === 'function') {
        userContext.operationDefinedCallback(1);
      }
    }
  }

  // update starts here

  if (userContext.required_parameter_count === 2) {
    // update(object, callback)
    userContext.keys = userContext.user_arguments[0];
    userContext.values = userContext.user_arguments[0];
  } else if (userContext.required_parameter_count === 4) {
    // update(tableNameOrConstructor, keys, values, callback)
    userContext.keys = userContext.user_arguments[1];
    userContext.values = userContext.user_arguments[2];
  } else {
    throw new Error(
        'Fatal internal error; wrong required_parameter_count ' + userContext.required_parameter_count);
  }
  // get DBTableHandler for table indicator (domain object, constructor, or table name)
  getTableHandler(userContext.user_arguments[0], userContext.session, updateOnTableHandler);
};

/** Load the object.
 * 
 */
exports.UserContext.prototype.load = function() {
  var userContext = this;
  var tableHandler;

  function loadOnResult(err, dbOperation) {
    udebug.log('load.loadOnResult');
    var result, values;
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(err);
      return;
    }
    values = dbOperation.result.value;
    // apply the values to the parameter domain object
    userContext.dbTableHandler.setFields(userContext.user_arguments[0], values);
    userContext.applyCallback(null);      
  }

  function loadOnTableHandler(err, dbTableHandler) {
    var dbSession, keys, index, transactionHandler;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err);
    } else {
      userContext.dbTableHandler = dbTableHandler;
      // the domain object must provide PRIMARY or unique key
      keys = userContext.user_arguments[0];
      // ask getIndexHandler for only unique key indexes
      index = dbTableHandler.getIndexHandler(keys, true);
      if (index === null) {
        err = new Error('Illegal argument: load unable to get a unique index to use for ' + JSON.stringify(keys));
        userContext.applyCallback(err);
      } else {
        // create the load operation and execute it
        dbSession = userContext.session.dbSession;
        transactionHandler = dbSession.getTransactionHandler();
        userContext.operation = dbSession.buildReadOperation(index, keys, transactionHandler, loadOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            udebug.log_detail('load transactionHandler.execute callback.');
          });
        } else if (typeof(userContext.operationDefinedCallback) === 'function') {
          userContext.operationDefinedCallback(1);
        }
      }
    }
  }

  // load starts here
  // session.load(instance, callback)
  // get DBTableHandler for instance constructor
  if (typeof(userContext.user_arguments[0].mynode) !== 'object') {
    userContext.applyCallback(new Error('Illegal argument: load requires a mapped domain object.'));
    return;
  }
  var ctor = userContext.user_arguments[0].mynode.constructor;
  getTableHandler(ctor, userContext.session, loadOnTableHandler);
};

/** Remove the object.
 * 
 */
exports.UserContext.prototype.remove = function() {
  var userContext = this;
  var tableHandler, object;

  function removeOnResult(err, dbOperation) {
    udebug.log('remove.removeOnResult');
    // return any error code plus the original user object
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(error);
    } else {
      var result = dbOperation.result.value;
      userContext.applyCallback(null);
    }
  }

  function removeOnTableHandler(err, dbTableHandler) {
    var transactionHandler, object, dbIndexHandler;
    var dbSession = userContext.session.dbSession;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err);
    } else {
      dbIndexHandler = dbTableHandler.getIndexHandler(userContext.keys, true);
      if (dbIndexHandler === null) {
        err = new Error('UserContext.remove unable to get an index to use for ' + JSON.stringify(keys));
        userContext.applyCallback(err);
      } else {
        transactionHandler = dbSession.getTransactionHandler();
        userContext.operation = dbSession.buildDeleteOperation(dbIndexHandler, userContext.keys, transactionHandler, removeOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            udebug.log_detail('remove transactionHandler.execute callback.');
          });
        } else if (typeof(userContext.operationDefinedCallback) === 'function') {
          userContext.operationDefinedCallback(1);
        }
      }
    }
  }

  // remove starts here

  if (userContext.required_parameter_count === 2) {
    // remove(object, callback)
    userContext.keys = userContext.user_arguments[0];
  } else if (userContext.required_parameter_count === 3) {
    // remove(tableNameOrConstructor, values, callback)
    userContext.keys = userContext.user_arguments[1];
  } else {
    throw new Error(
        'Fatal internal error; wrong required_parameter_count ' + userContext.required_parameter_count);
  }
  // get DBTableHandler for table indicator (domain object, constructor, or table name)
  getTableHandler(userContext.user_arguments[0], userContext.session, removeOnTableHandler);
};

/** Execute a batch
 * 
 */
exports.UserContext.prototype.executeBatch = function(operationContexts) {
  var userContext = this;
  userContext.operationContexts = operationContexts;
  userContext.numberOfOperations = operationContexts.length;
  userContext.numberOfOperationsDefined = 0;

  // all operations have been executed and their user callbacks called
  // now call the Batch.execute callback
  var executeBatchOnExecute = function(err) {
    userContext.applyCallback(err);
  };

  // wait here until all operations have been defined
  // if operations are not yet defined, the onTableHandler callback
  // will call this function after the operation is defined
  var executeBatchOnOperationDefined = function(definedOperationCount) {
    userContext.numberOfOperationsDefined += definedOperationCount;
    udebug.log_detail('UserContext.executeBatch expecting', userContext.numberOfOperations, 
        'operations with', userContext.numberOfOperationsDefined, ' already defined.');
    if (userContext.numberOfOperationsDefined === userContext.numberOfOperations) {
      var operations = [];
      // collect all operations from the operation contexts
      userContext.operationContexts.forEach(function(operationContext) {
        operations.push(operationContext.operation);
      });
      // execute the batch
      var transactionHandler;
      var dbSession;
      dbSession = userContext.session.dbSession;
      transactionHandler = dbSession.getTransactionHandler();
      transactionHandler.execute(operations, executeBatchOnExecute);
    }
  };

  // executeBatch starts here
  // make sure all operations are defined
  operationContexts.forEach(function(operationContext) {
    // is the operation already defined?
    if (typeof(operationContext.operation) !== 'undefined') {
      userContext.numberOfOperationsDefined++;
    } else {
      // the operation has not been defined yet; set a callback for when the operation is defined
      operationContext.operationDefinedCallback = executeBatchOnOperationDefined;
    }
  });
  // now execute the operations
  executeBatchOnOperationDefined(0);
};

/** Commit an active transaction. 
 * 
 */
exports.UserContext.prototype.commit = function() {
  var userContext = this;

  var commitOnCommit = function(err) {
    udebug.log('UserContext.commitOnCommit.')
    userContext.applyCallback(err);
  };

  // commit begins here
  if (userContext.session.tx.isActive()) {
    udebug.log('UserContext.commit tx is active.')
    userContext.session.dbSession.commit(commitOnCommit);
  } else {
    userContext.applyCallback(
        new Error('Fatal Internal Exception: UserContext.commit with no active transaction.'));
  }
};


/** Roll back an active transaction. 
 * 
 */
exports.UserContext.prototype.rollback = function() {
  var userContext = this;

  var rollbackOnRollback = function(err) {
    udebug.log('UserContext.rollbackOnRollback.')
    userContext.applyCallback(err);
  };

  // rollback begins here
  if (userContext.session.tx.isActive()) {
    udebug.log('UserContext.rollback tx is active.')
    var transactionHandler = userContext.session.dbSession.getTransactionHandler();
    transactionHandler.rollback(rollbackOnRollback);
  } else {
    userContext.applyCallback(
        new Error('Fatal Internal Exception: UserContext.rollback with no active transaction.'));
  }
};


/** Open a session. Allocate a slot in the session factory sessions array.
 * Call the DBConnectionPool to create a new DBSession.
 * Wrap the DBSession in a new Session and return it to the user. 
 */
exports.UserContext.prototype.openSession = function() {
  var userContext = this;

  var openSessionOnSessionCreated = function(err, dbSession) {
    if (err) {
      userContext.applyCallback(err, null);
    } else {
      userContext.session = new apiSession.Session(userContext.session_index, userContext.session_factory, dbSession);
      userContext.session_factory.sessions[userContext.session_index] = userContext.session;
      userContext.applyCallback(err, userContext.session);
    }
  };

  var i;
  // allocate a new session slot in sessions
  for (i = 0; i < this.session_factory.sessions.length; ++i) {
    if (this.session_factory.sessions[i] === null) {
      break;
    }
  }
  this.session_factory.sessions[i] = {'placeholder':true, 'index':i};
  // remember the session index
  this.session_index = i;
  // get a new DBSession from the DBConnectionPool
  this.session_factory.dbConnectionPool.getDBSession(i, openSessionOnSessionCreated);
};

/** Close a session. Close the dbSession which might put the underlying connection
 * back into the connection pool. Then, remove the session from the session factory's
 * open connections.
 * 
 */
exports.UserContext.prototype.closeSession = function() {
  var userContext = this;

  var closeSessionOnDBSessionClose = function(err) {
    // now remove the session from the session factory's open connections
    userContext.session_factory.closeSession(userContext.session.index);
    // mark this session as unusable
    userContext.session.closed = true;
    userContext.applyCallback(err);
  };
  // first, close the dbSession
  userContext.session.dbSession.close(closeSessionOnDBSessionClose);
};


/** Complete the user function by calling back the user with the results of the function.
 * Apply the user callback using the current arguments and the extra parameters from the original function.
 * Create the args for the callback by copying the current arguments to this function. Then, copy
 * the extra parameters from the original function. Finally, call the user callback.
 */
exports.UserContext.prototype.applyCallback = function() {
  if (arguments.length !== this.returned_parameter_count) {
    throw new Error(
        'Fatal internal exception: wrong parameter count ' + arguments.length +' for UserContext applyCallback' + 
        '; expected ' + this.returned_parameter_count);
  }
  var args = [];
  var i, j;
  for (i = 0; i < arguments.length; ++i) {
    args.push(arguments[i]);
  }
  for (j = this.required_parameter_count; j < this.user_arguments.length; ++j) {
    args.push(this.user_arguments[j]);
  }
  this.user_callback.apply(null, args);
};
