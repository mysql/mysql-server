/*
 Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights
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

var stats = {
	"TableHandlerFactory" : 0,
	"TableHandler" : {
		"success"    : 0,
		"idempotent" : 0,
		"cache_hit"  : 0
	}
};

var util           = require("util"),
    mynode         = require("./mynode.js"),
    DBTableHandler = require(mynode.common.DBTableHandler).DBTableHandler,
    apiSession     = require("./Session.js"),
    sessionFactory = require("./SessionFactory.js"),
    query          = require("./Query.js"),
    spi            = require(mynode.spi),
    udebug         = unified_debug.getLogger("UserContext.js"),
    stats_module   = require(mynode.api.stats);

stats_module.register(stats, "api", "UserContext");

function Promise() {
  // implement Promises/A+ http://promises-aplus.github.io/promises-spec/
  // until then is called, this is an empty promise with no performance impact
}

function emptyFulfilledCallback(result) {
  return result;
}
function emptyRejectedCallback(err) {
  throw err;
}
/** Fulfill or reject the original promise via "The Promise Resolution Procedure".
 * original_promise is the Promise from this implementation on which "then" was called
 * new_promise is the Promise from this implementation returned by "then"
 * if the fulfilled or rejected callback provided by "then" returns a promise, wire the new_result (thenable)
 *  to fulfill the new_promise when new_result is fulfilled
 *  or reject the new_promise when new_result is rejected
 * otherwise, if the callback provided by "then" returns a value, fulfill the new_promise with that value
 * if the callback provided by "then" throws an Error, reject the new_promise with that Error
 */
var thenPromiseFulfilledOrRejected = function(original_promise, fulfilled_or_rejected_callback, new_promise, result, isRejected) {
  var new_result;
  try {
    if(udebug.is_detail()) { udebug.log(original_promise.name, 'thenPromiseFulfilledOrRejected before'); }
    if (fulfilled_or_rejected_callback) {
      new_result = fulfilled_or_rejected_callback.call(undefined, result);
    } else {
      if (isRejected) {
        // 2.2.7.4 If onRejected is not a function and promise1 is rejected, promise2 must be rejected with the same reason.
        new_promise.reject(result);
      } else {
        // 2.2.7.3 If onFulfilled is not a function and promise1 is fulfilled, promise2 must be fulfilled with the same value.
        new_promise.fulfill(result);
      }
      return;
    }
    if(udebug.is_detail()) { udebug.log(original_promise.name, 'thenPromiseFulfilledOrRejected after', new_result); }
    var new_result_type = typeof new_result;
    if ((new_result_type === 'object' && new_result_type != null) | new_result_type === 'function') { 
      // 2.3.3 if result is an object or function
      // 2.3 The Promise Resolution Procedure
      // 2.3.1 If promise and x refer to the same object, reject promise with a TypeError as the reason.
      if (new_result === original_promise) {
        throw new Error('TypeError: Promise Resolution Procedure 2.3.1');
      }
      // 2.3.2 If x is a promise, adopt its state; but we don't care since it's also a thenable
      var then;
      try {
        then = new_result.then;
      } catch (thenE) {
        // 2.2.3.2 If retrieving the property x.then results in a thrown exception e, 
        // reject promise with e as the reason.
        new_promise.reject(thenE);
        return;
      }
      if (typeof then === 'function') {
        // 2.3.3.3 If then is a function, call it with x as this, first argument resolvePromise, 
        // and second argument rejectPromise
        // 2.3.3.3.3 If both resolvePromise and rejectPromise are called, 
        // or multiple calls to the same argument are made, the first call takes precedence, 
        // and any further calls are ignored.
        try {
          then.call(new_result,
            // 2.3.3.3.1 If/when resolvePromise is called with a value y, run [[Resolve]](promise, y).
            function(result) {
            if(udebug.is_detail()) { udebug.log(original_promise.name, 'thenPromiseFulfilledOrRejected deferred fulfill callback', new_result); }
              if (!new_promise.resolved) {
                new_promise.fulfill(result);
              }
            },
            // 2.3.3.3.2 If/when rejectPromise is called with a reason r, reject promise with r.
            function(err) {
              if(udebug.is_detail()) { udebug.log(original_promise.name, 'thenPromiseFulfilledOrRejected deferred reject callback', new_result); }
              if (!new_promise.resolved) {
                new_promise.reject(err);
              }
            }
          );
        } catch (callE) {
          // 2.3.3.3.4 If calling then throws an exception e,
          // 2.3.3.3.4.1 If resolvePromise or rejectPromise have been called, ignore it.
          if (!new_promise.resolved) {
            // 2.3.3.3.4.2 Otherwise, reject promise with e as the reason.
            new_promise.reject(callE);
          }
        }
      } else {
        // 2.3.3.4 If then is not a function, fulfill promise with x.
        new_promise.fulfill(new_result);
      }
    } else {
      // 2.3.4 If x is not an object or function, fulfill promise with x.
      new_promise.fulfill(new_result);
    }
  } catch (fulfillE) {
    // 2.2.7.2 If either onFulfilled or onRejected throws an exception e,
    // promise2 must be rejected with e as the reason.
    new_promise.reject(fulfillE);
  }
  
};

Promise.prototype.then = function(fulfilled_callback, rejected_callback, progress_callback) {
  var self = this;
  if (!self) udebug.log('Promise.then called with no this');
  // create a new promise to return from the "then" method
  var new_promise = new Promise();
  if (typeof self.fulfilled_callbacks === 'undefined') {
    self.fulfilled_callbacks = [];
    self.rejected_callbacks = [];
    self.progress_callbacks = [];
  }
  if (self.resolved) {
    var resolved_result;
    if(udebug.is_detail()) { udebug.log(this.name, 'UserContext.Promise.then resolved; err:', self.err); }
    if (self.err) {
      // this promise was already rejected
      if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then resolved calling (delayed) rejected_callback', rejected_callback); }
      global.setImmediate(function() {
        if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then resolved calling rejected_callback', fulfilled_callback); }
        thenPromiseFulfilledOrRejected(self, rejected_callback, new_promise, self.err, true);
      });
    } else {
      // this promise was already fulfilled, possibly with a null or undefined result
      if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then resolved calling (delayed) fulfilled_callback', fulfilled_callback); }
      global.setImmediate(function() {
        if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then resolved calling fulfilled_callback', fulfilled_callback); }
        thenPromiseFulfilledOrRejected(self, fulfilled_callback, new_promise, self.result);
      });
    }
    return new_promise;
  }
  // create a closure for each fulfilled_callback
  // the closure is a function that when called, calls setImmediate to call the fulfilled_callback with the result
  if (typeof fulfilled_callback === 'function') {
    if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then with fulfilled_callback', fulfilled_callback); }
    // the following function closes (this, fulfilled_callback, new_promise)
    // and is called asynchronously when this promise is fulfilled
    this.fulfilled_callbacks.push(function(result) {
      global.setImmediate(function() {
        thenPromiseFulfilledOrRejected(self, fulfilled_callback, new_promise, result);
      });
    });
  } else {
    if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then with no fulfilled_callback'); }
    // create a dummy function for a missing fulfilled callback per 2.2.7.3 
    // If onFulfilled is not a function and promise1 is fulfilled, promise2 must be fulfilled with the same value.
    this.fulfilled_callbacks.push(function(result) {
      global.setImmediate(function() {
        thenPromiseFulfilledOrRejected(self, emptyFulfilledCallback, new_promise, result);
      });
    });
  }

  // create a closure for each rejected_callback
  // the closure is a function that when called, calls setImmediate to call the rejected_callback with the error
  if (typeof rejected_callback === 'function') {
    if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then with rejected_callback', rejected_callback); }
    this.rejected_callbacks.push(function(err) {
      global.setImmediate(function() {
        thenPromiseFulfilledOrRejected(self, rejected_callback, new_promise, err);
      });
    });
  } else {
    if(udebug.is_detail()) { udebug.log(self.name, 'UserContext.Promise.then with no rejected_callback');  }
    // create a dummy function for a missing rejected callback per 2.2.7.4 
    // If onRejected is not a function and promise1 is rejected, promise2 must be rejected with the same reason.
    this.rejected_callbacks.push(function(err) {
      global.setImmediate(function() {
        thenPromiseFulfilledOrRejected(self, emptyRejectedCallback, new_promise, err);
      });
    });
  }
  // todo: progress_callbacks
  if (typeof progress_callback === 'function') {
    this.progress_callbacks.push(progress_callback);
  }

  return new_promise;
};

Promise.prototype.fulfill = function(result) {
  var name = this?this.name: 'no this';
  if (udebug.is_detail()) {
    udebug.log_detail(new Error(name, 'Promise.fulfill').stack);
  }
  if (this.resolved) {
    throw new Error('Fatal User Exception: fulfill called after fulfill or reject');
  }
  if(udebug.is_detail()) { 
    udebug.log(name, 'Promise.fulfill with result', result, 'fulfilled_callbacks length:', 
      this.fulfilled_callbacks?  this.fulfilled_callbacks.length: 0); 
  }
  this.resolved = true;
  this.result = result;
  var fulfilled_callback;
  if (this.fulfilled_callbacks) {
    while(this.fulfilled_callbacks.length > 0) {
      fulfilled_callback = this.fulfilled_callbacks.shift();
      if(udebug.is_detail()) { udebug.log('Promise.fulfill for', result); }
      fulfilled_callback(result);
    }
  }
};

Promise.prototype.reject = function(err) {
  if (this.resolved) {
    throw new Error('Fatal User Exception: reject called after fulfill or reject');
  }
  var name = this?this.name: 'no this';
  if(udebug.is_detail()) {
    udebug.log(name, 'Promise.reject with err', err, 'rejected_callbacks length:', 
      this.rejected_callbacks?  this.rejected_callbacks.length: 0);
  }
  this.resolved = true;
  this.err = err;
  var rejected_callback;
  if (this.rejected_callbacks) {
    while(this.rejected_callbacks.length > 0) {
      rejected_callback = this.rejected_callbacks.shift();
      if(udebug.is_detail()) { udebug.log('Promise.reject for', err); }
      rejected_callback(err);
    }
  }
//  throw err;
};

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
  this.execute = (typeof execute === 'boolean' ? execute : true);
  this.user_arguments = user_arguments;
  this.user_callback = user_arguments[required_parameter_count - 1];
  this.required_parameter_count = required_parameter_count;
  this.extra_arguments_count = user_arguments.length - required_parameter_count;
  this.returned_parameter_count = returned_parameter_count;
  this.session = session;
  this.session_factory = session_factory;
  /* indicates that batch.clear was called before this context had executed */
  this.clear = false;
  if (this.session) {
    this.autocommit = ! this.session.tx.isActive();
  }
  this.errorMessages = '';
  this.promise = new Promise();
};

exports.UserContext.prototype.appendErrorMessage = function(message) {
  this.errorMessages += '\n' + message;
};

/** Get table metadata.
 * Delegate to DBConnectionPool.getTableMetadata.
 */
exports.UserContext.prototype.getTableMetadata = function() {
  var userContext = this;
  var err, databaseName, tableName, dbSession;
  var getTableMetadataOnTableMetadata = function(metadataErr, tableMetadata) {
    udebug.log('UserContext.getTableMetadata.getTableMetadataOnTableMetadata with err', metadataErr);
    userContext.applyCallback(metadataErr, tableMetadata);
  };
  databaseName = userContext.user_arguments[0];
  tableName = userContext.user_arguments[1];
  dbSession = (userContext.session)?userContext.session.dbSession:null;
  if (typeof databaseName !== 'string' || typeof tableName !== 'string') {
    err = new Error('getTableMetadata(databaseName, tableName) illegal argument types (' +
        typeof databaseName + ', ' + typeof tableName + ')');
    userContext.applyCallback(err, null);
  } else {
    this.session_factory.dbConnectionPool.getTableMetadata(
        databaseName, tableName, dbSession, getTableMetadataOnTableMetadata);
  }
  return userContext.promise;
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
  return userContext.promise;
};

/** Create schema from a table mapping. 
 * promise = createTable(tableMapping, callback);
 */
exports.UserContext.prototype.createTable = function() {
  var userContext = this;
  var tableMapping, dbSession;
  function createTableOnTableCreated(err) {
    userContext.applyCallback(err);
  }

  // createTable starts here
  tableMapping = userContext.user_arguments[0];
  userContext.session_factory.dbConnectionPool.createTable(tableMapping, userContext.session, userContext.session_factory,
      createTableOnTableCreated);
  return userContext.promise;
};


/** Resolve properties. Properties might be an object, a name, or null.
 * If null, use all default properties. If a name, use default properties
 * of the named service provider. Otherwise, return the properties object.
 */
var resolveProperties = function(properties) {
  // Properties can be a string adapter name.  It defaults to 'ndb'.
  if(typeof properties === 'string') {
    properties = spi.getDBServiceProvider(properties).getDefaultConnectionProperties();
  }
  else if (properties === null) {
    properties = spi.getDBServiceProvider('ndb').getDefaultConnectionProperties();
  }
  return properties;
};

function getTableSpecification(defaultDatabaseName, tableName) {
  var split = tableName.split('\.');
  var result = {};
  if (split.length == 2) {
    result.dbName = split[0];
    result.unqualifiedTableName = split[1];
    result.qualifiedTableName = tableName;
  } else {
    // if split.length is not 1 then this error will be caught later
    result.dbName = defaultDatabaseName;
    result.unqualifiedTableName = tableName;
    result.qualifiedTableName = defaultDatabaseName + '.' + tableName;
  }
  udebug.log_detail('getTableSpecification for', defaultDatabaseName, ',', tableName, 'returned', result);
  return result;
}
/** Construct the table name from possibly empty database name and table name.
 */
function constructDatabaseDotTable(databaseName, tableName) {
  var result = databaseName ? databaseName + '.' + tableName : tableName;
  return result;
}

/** Get the table handler for a domain object, table name, or constructor.
 */
var getTableHandler = function(domainObjectTableNameOrConstructor, session, onTableHandler) {

  // the table name might be qualified if the mapping specified a qualified table name
  // if unqualified, use sessionFactory.properties.database to qualify the table name
  var TableHandlerFactory = function(mynode, tableSpecification,
      sessionFactory, dbSession, mapping, ctor, onTableHandler) {
    this.sessionFactory = sessionFactory;
    this.dbSession = dbSession;
    this.onTableHandler = onTableHandler;
    this.mapping = mapping;
    this.mynode = mynode;
    this.ctor = ctor;
    this.tableSpecification = tableSpecification;
    stats.TableHandlerFactory++;
    
    this.createTableHandler = function() {
      var tableHandlerFactory = this;
      var tableHandler;
      var tableMetadata;
      var tableMapping;
      
      var onTableMetadata = function(err, tableMetadata) {
        var tableHandler;
        var tableKey = tableHandlerFactory.tableSpecification.qualifiedTableName;
        if(udebug.is_detail()) {
          udebug.log('TableHandlerFactory.onTableMetadata for ',
            tableHandlerFactory.tableSpecification.qualifiedTableName + ' with err: ' + err);
        }
        if (err) {
          tableHandlerFactory.onTableHandler(err, null);
        } else {
          // check to see if the metadata has already been processed
          if (typeof tableHandlerFactory.sessionFactory.tableMetadatas[tableKey] === 'undefined') {
            // put the table metadata into the table metadata map
            tableHandlerFactory.sessionFactory.tableMetadatas[tableKey] = tableMetadata;
          }
          // we have the table metadata; now create the table handler if needed
          // put the table handler into the session factory
          if (typeof(tableHandlerFactory.sessionFactory.tableHandlers[tableKey]) === 'undefined') {
            if(udebug.is_detail()) {
              udebug.log('UserContext caching the table handler in the sessionFactory for ', 
                tableHandlerFactory.tableName);
            }
            tableHandler = new DBTableHandler(tableMetadata, tableHandlerFactory.mapping,
                tableHandlerFactory.ctor);
            tableHandlerFactory.sessionFactory.tableHandlers[tableKey] = tableHandler;
          } else {
            tableHandler = tableHandlerFactory.sessionFactory.tableHandlers[tableKey];
            if(udebug.is_detail()) {
              udebug.log('UserContext got tableHandler but someone else put it in the cache first for ', 
                tableHandlerFactory.tableName);
            }
          }
          if (tableHandlerFactory.ctor) {
            if (typeof(tableHandlerFactory.ctor.prototype.mynode.tableHandler) === 'undefined') {
              // if a domain object mapping, cache the table handler in the prototype
              stats.TableHandler.success++;
              tableHandler = new DBTableHandler(tableMetadata, tableHandlerFactory.mapping,
                  tableHandlerFactory.ctor);
              if (tableHandler.isValid) {
                tableHandlerFactory.ctor.prototype.mynode.tableHandler = tableHandler;
                if(udebug.is_detail()) {
                  udebug.log('UserContext caching the table handler in the prototype for constructor.');
                }
              } else {
                tableHandlerFactory.err = tableHandler.err;
                if(udebug.is_detail()) { udebug.log('UserContext got invalid tableHandler', tableHandler.errorMessages); }
              }
            } else {
              tableHandler = tableHandlerFactory.ctor.prototype.mynode.tableHandler;
              stats.TableHandler.idempotent++;
              if(udebug.is_detail()) {
                udebug.log('UserContext got tableHandler but someone else put it in the prototype first.');
              }
            }
          }
          tableHandlerFactory.onTableHandler(tableHandlerFactory.err, tableHandler);
        }
      };

      function tableHandlerFactoryOnCreateTable(err) {
        if (err) {
          onTableMetadata(err, null);
        } else {
          sessionFactory.dbConnectionPool.getTableMetadata(tableHandlerFactory.tableSpecification.dbName,
              tableHandlerFactory.tableSpecification.unqualifiedTableName, session.dbSession, onTableMetadata);
        }
      }
      
      // start of createTableHandler
      
      // get the table metadata from the cache of table metadatas in session factory
      tableMetadata = 
        tableHandlerFactory.sessionFactory.tableMetadatas[tableHandlerFactory.tableSpecification.qualifiedTableName];
      if (tableMetadata) {
        // we already have cached the table metadata
        onTableMetadata(null, tableMetadata);
      } else {
        // create the schema if it does not already exist
        tableMapping = sessionFactory.tableMappings[tableSpecification.qualifiedTableName];
        if (tableMapping) {
          udebug.log('tableHandlerFactory.createTableHandler creating schema using tableMapping: ', tableMapping);
          sessionFactory.createTable(tableMapping, tableHandlerFactoryOnCreateTable);
          return;
        }
        // get the table metadata from the db connection pool
        // getTableMetadata(dbSession, databaseName, tableName, callback(error, DBTable));
        udebug.log('TableHandlerFactory.createTableHandler for ', 
            tableHandlerFactory.tableSpecification.dbName,
            tableHandlerFactory.tableSpecification.unqualifiedTableName);
        this.sessionFactory.dbConnectionPool.getTableMetadata(
            tableHandlerFactory.tableSpecification.dbName,
            tableHandlerFactory.tableSpecification.unqualifiedTableName, session.dbSession, onTableMetadata);
      }
    };
  };
    
  // start of getTableHandler 
  var err, mynode, tableHandler, tableMapping, tableHandlerFactory, tableIndicatorType, tableSpecification, databaseDotTable;

  tableIndicatorType = typeof(domainObjectTableNameOrConstructor);
  if (tableIndicatorType === 'string') {
    if(udebug.is_detail()) {
      udebug.log('UserContext.getTableHandler for table ', domainObjectTableNameOrConstructor); 
    }
    tableSpecification = getTableSpecification(session.sessionFactory.properties.database,
        domainObjectTableNameOrConstructor);

    // parameter is a table name; look up in table name to table handler hash
    tableHandler = session.sessionFactory.tableHandlers[tableSpecification.qualifiedTableName];
    if (typeof(tableHandler) === 'undefined') {
      udebug.log('UserContext.getTableHandler did not find cached tableHandler for table ',
          tableSpecification.qualifiedTableName);
      // get a table mapping from session factory
      tableMapping = session.sessionFactory.tableMappings[tableSpecification.qualifiedTableName];
      // create a new table handler for a table name with no mapping
      // create a closure to create the table handler
      tableHandlerFactory = new TableHandlerFactory(
          null, tableSpecification, session.sessionFactory, session.dbSession,
          tableMapping, null, onTableHandler);
      tableHandlerFactory.createTableHandler(null);
    } else {
      if(udebug.is_detail()) {
        udebug.log('UserContext.getTableHandler found cached tableHandler for table ',
          tableSpecification.qualifiedTableName);
      }
      // send back the tableHandler
      onTableHandler(null, tableHandler);
    }
  } else if (tableIndicatorType === 'function') {
    if(udebug.is_detail()) { udebug.log('UserContext.getTableHandler for constructor.'); }
    mynode = domainObjectTableNameOrConstructor.prototype.mynode;
    // parameter is a constructor; it must have been annotated already
    if (typeof(mynode) === 'undefined') {
      err = new Error('User exception: constructor for ' +  domainObjectTableNameOrConstructor.prototype.constructor.name +
      ' must have been annotated (call TableMapping.applyToClass).');
      onTableHandler(err, null);
    } else {
      tableHandler = mynode.tableHandler;
      if (typeof(tableHandler) === 'undefined') {
        udebug.log('UserContext.getTableHandler did not find cached tableHandler for constructor.',
            domainObjectTableNameOrConstructor);
        // create the tableHandler
        // getTableMetadata(dbSession, databaseName, tableName, callback(error, DBTable));
        databaseDotTable = constructDatabaseDotTable(mynode.mapping.database, mynode.mapping.table);
        tableSpecification = getTableSpecification(session.sessionFactory.properties.database, databaseDotTable);
        tableHandlerFactory = new TableHandlerFactory(
            mynode, tableSpecification, session.sessionFactory, session.dbSession, 
            mynode.mapping, domainObjectTableNameOrConstructor, onTableHandler);
        tableHandlerFactory.createTableHandler();
      } else {
        stats.TableHandler.cache_hit++;
        if(udebug.is_detail()) { udebug.log('UserContext.getTableHandler found cached tableHandler for constructor.'); }
        // prototype has been annotated; return the table handler
        onTableHandler(null, tableHandler);
      }
    }
  } else if (tableIndicatorType === 'object') {
    if(udebug.is_detail()) { udebug.log('UserContext.getTableHandler for domain object.'); }
    // parameter is a domain object; it must have been mapped already
    mynode = domainObjectTableNameOrConstructor.constructor.prototype.mynode;
    if (typeof(mynode) === 'undefined') {
      err = new Error('User exception: constructor for ' +  domainObjectTableNameOrConstructor.constructor.name +
          ' must have been annotated (call TableMapping.applyToClass).');
      onTableHandler(err, null);
    } else {
      tableHandler = mynode.tableHandler;
      if (typeof(tableHandler) === 'undefined') {
        if(udebug.is_detail()) {
          udebug.log('UserContext.getTableHandler did not find cached tableHandler for object\n',
                      util.inspect(domainObjectTableNameOrConstructor),
                     'constructor\n', domainObjectTableNameOrConstructor.constructor);
        }
        databaseDotTable = constructDatabaseDotTable(mynode.mapping.database, mynode.mapping.table);
        tableSpecification = getTableSpecification(session.sessionFactory.properties.database, databaseDotTable);
        // create the tableHandler
        // getTableMetadata(dbSession, databaseName, tableName, callback(error, DBTable));
        tableHandlerFactory = new TableHandlerFactory(
            mynode, tableSpecification, session.sessionFactory, session.dbSession, 
            mynode.mapping, domainObjectTableNameOrConstructor.constructor, onTableHandler);
        tableHandlerFactory.createTableHandler();
      } else {
        if(udebug.is_detail()) { udebug.log('UserContext.getTableHandler found cached tableHandler for constructor.'); }
        // prototype has been annotated; return the table handler
        onTableHandler(null, tableHandler);
      }
    }
  } else {
    err = new Error('User error: parameter must be a domain object, string, or constructor function.');
    onTableHandler(err, null);
  }
};

/** Try to find an existing session factory by looking up the connection string
 * and database name. Failing that, create a db connection pool and create a session factory.
 * Multiple session factories share the same db connection pool.
 * This function is used by both connect and openSession.
 */
var getSessionFactory = function(userContext, properties, tableMappings, callback) {
  var database;
  var connectionKey;
  var connection;
  var factory;
  var newSession;
  var sp;
  var i;
  var m;

  var resolveTableMappingsOnSession = function(err, session) {
    var mappings = [];
    var mappingBeingResolved = 0;

    var resolveTableMappingsOnTableHandler = function(err, tableHandler) {
      if(udebug.is_detail()) {
        udebug.log('UserContext.resolveTableMappinsgOnTableHandler', mappingBeingResolved + 1,
                   'of', mappings.length, mappings[mappingBeingResolved]);
      }
      if (err) {
        userContext.appendErrorMessage(err);
      }
      if (++mappingBeingResolved === mappings.length || mappingBeingResolved > 10) {
        // close the session the hard way (not using UserContext)
        session.dbSession.close(function(err) {
          if (err) {
            callback(err, null);
          } else {
            // now remove the session from the session factory's open connections
            session.sessionFactory.closeSession(session.index);
            // mark this session as unusable
            session.closed = true;
            // if any errors during table mapping, report them
            if (userContext.errorMessages) {
              err = new Error(userContext.errorMessages);
              callback(err, null);
            } else {
              // no errors
              callback(null, factory);
            }
          }
        });
      } else {
        // get the table handler for the next one, and so on until all are done
        getTableHandler(mappings[mappingBeingResolved], session, resolveTableMappingsOnTableHandler);
      }
    };

    // resolveTableMappingsOnSession begins here
    
    var tableMappingsType = typeof(tableMappings);
    var tableMapping;
    var tableMappingType;
    switch (tableMappingsType) {
    case 'string': 
      mappings.push(tableMappings); 
      break;
    case 'function': 
      mappings.push(tableMappings);
      break;
    case 'object': 
      if (tableMappings.length) {
        for (m = 0; m < tableMappings.length; ++m) {
          tableMapping = tableMappings[m];
          tableMappingType = typeof(tableMapping);
          if (tableMappingType === 'function' || tableMappingType === 'string') {
            mappings.push(tableMapping);
          } else {
            userContext.appendErrorMessage('unknown table mapping' + util.inspect(tableMapping));
          }
        }
      } else {
        userContext.appendErrorMessage('unknown table mappings' + util.inspect(tableMappings));
      }
      break;
    default:
      userContext.appendErrorMessage('unknown table mappings' + util.inspect(tableMappings));
      break;
    }
    if (mappings.length === 0) {
      if(udebug.is_detail()) { udebug.log('resolveTableMappingsOnSession no mappings!'); }
      callback(null, factory);
    }
    // get table handler for the first; the callback will then do the next one...
    if(udebug.is_detail()) { udebug.log('getSessionFactory resolving mappings:', mappings); }
    getTableHandler(mappings[0], session, resolveTableMappingsOnTableHandler);
  };

  var resolveTableMappingsAndCallback = function() {
    if (!tableMappings) {
      callback(null, factory);
    } else {
      // get a session the hard way (not using UserContext) to resolve mappings
      var sessionSlot = factory.allocateSessionSlot();
      factory.dbConnectionPool.getDBSession(userContext.session_index, function(err, dbSession) {
        if (err) {
          // report error
          userContext.appendErrorMessage(err);
          err = new Error(userContext.errorMessages);
          callback(err, null);          
        } else {
          var newSession = new apiSession.Session(sessionSlot, factory, dbSession);
          factory.sessions[sessionSlot] = newSession;
          resolveTableMappingsOnSession(err, newSession);
        }
      });
    }
  };

  var createFactory = function(dbConnectionPool) {
    var newFactory;
    udebug.log('connect createFactory creating factory for', connectionKey, 'database', database);
    newFactory = new sessionFactory.SessionFactory(connectionKey, dbConnectionPool,
        properties, tableMappings, mynode.deleteFactory);
    return newFactory;
  };
  
  var dbConnectionPoolCreated_callback = function(error, dbConnectionPool) {
    if (connection.isConnecting) {
      // the first requester for this connection
      connection.isConnecting = false;
      if (error) {
        callback(error, null);
      } else {
        udebug.log('dbConnectionPool created for', connectionKey, 'database', database);
        connection.dbConnectionPool = dbConnectionPool;
        factory = createFactory(dbConnectionPool);
        connection.factories[database] = factory;
        connection.count++;
        resolveTableMappingsAndCallback();
      }
      // notify all others that the connection is now ready (or an error was signaled)
      for (i = 0; i < connection.waitingForConnection.length; ++i) {
        if(udebug.is_detail()) { udebug.log('dbConnectionPoolCreated_callback notifying...'); }
        udebug.log('dbConnectionPoolCreated', error, dbConnectionPool);
        connection.waitingForConnection[i](error, dbConnectionPool);
      }
    } else {
      // another user request created the dbConnectionPool and session factory
      if (error) {
        callback(error, null);
      } else {
        udebug.log('dbConnectionPoolCreated_callback', database, connection.factories);
        factory = connection.factories[database];
        if (!factory) {
          factory = createFactory(dbConnectionPool);
          connection.factories[database] = factory;
          connection.count++;
        }
        resolveTableMappingsAndCallback();
      }
    }
  };

  // getSessionFactory starts here
  database = properties.database;
  connectionKey = mynode.getConnectionKey(properties);
  connection = mynode.getConnection(connectionKey);

  if(typeof(connection) === 'undefined') {
    // there is no connection yet using this connection key    
    udebug.log('connect connection does not exist; creating factory for',
               connectionKey, 'database', database);
    connection = mynode.newConnection(connectionKey);
    sp = spi.getDBServiceProvider(properties.implementation);
    sp.connect(properties, dbConnectionPoolCreated_callback);
  } else {
    // there is a connection, but is it already connected?
    if (connection.isConnecting) {
      // wait until the first requester for this connection completes
      udebug.log('connect waiting for db connection by another for', connectionKey, 'database', database);
      connection.waitingForConnection.push(dbConnectionPoolCreated_callback);
    } else {
      // there is a connection, but is there a SessionFactory for this database?
      factory = connection.factories[database];
      if (typeof(factory) === 'undefined') {
        // create a SessionFactory for the existing dbConnectionPool
        udebug.log('connect creating factory with existing', connectionKey, 'database', database);
        factory = createFactory(connection.dbConnectionPool);
        connection.factories[database] = factory;
        connection.count++;
      }
//    resolve all table mappings before returning
      resolveTableMappingsAndCallback();
    }
  }
  
};

exports.UserContext.prototype.connect = function() {
  var userContext = this;
  // properties might be null, a name, or a properties object
  this.user_arguments[0] = resolveProperties(this.user_arguments[0]);

  var connectOnSessionFactory = function(err, factory) {
    userContext.applyCallback(err, factory);
  };

  getSessionFactory(this, this.user_arguments[0], this.user_arguments[1], connectOnSessionFactory);
  return userContext.promise;
};

function checkOperation(err, dbOperation) {
  var sqlstate, message, result, result_code;
  result = null;
  result_code = null;
  message = 'Unknown Error';
  sqlstate = '22000';
  if (err) {
    udebug.log('checkOperation returning existing err:', err);
    return err;
  } 
  if (dbOperation.result.success !== true) {
    if(dbOperation.result.error) {
      sqlstate = dbOperation.result.error.sqlstate;
      message = dbOperation.result.error.message || 'Operation error';
      result_code = dbOperation.result.error.code;
    }
    result = new Error(message);
    result.code = result_code;
    result.sqlstate = sqlstate;
    udebug.log('checkOperation returning new err:', result);
  }
  return result;
}

/** Create a sector object for a domain object in a projection.
 * The topmost outer loop projection's sectors are all created, creating one sector
 * for each inner loop projection, then the outer projection for the next level down.
 * For each inner loop projection, a sector is constructed
 * and then the sector for the included relationship is constructed by recursion.
 * The sector contains a list of primary key fields and a list of non-primary key fields,
 * and if this is not the root sector, the name of the field and the field in the previous sector.
 * The fields are references to the field objects in DBTableHandler and contain names, types,
 * and converters.
 * This function is synchronous. When complete, this function returns to the caller.
 * @param inout parameter: outerLoopProjection the projection at the outer loop
 *   which is modified by this function to add the sectors field
 * @param inout parameter: innerLoopProjection the projection at the inner loop
 *   which is modified by this function to add the sectors field
 * @param sectors the outer loop projection.sectors which will grow as createSector is called recursively
 * @param index the index into sectors for the sector being constructed
 * @param offset the number of fields in all sectors already processed
 */
function createSector(outerLoopProjection, innerLoopProjection, sectors, index, offset) {
  udebug.log('createSector ' + outerLoopProjection.name + ' for ' + outerLoopProjection.domainObject.name +
      ' inner: ' + innerLoopProjection.name + ' for ' + innerLoopProjection.domainObject.name +
      ' index: ' + index + ' offset: ' + offset);
  var projection = innerLoopProjection;
  var innerNestedProjection, outerNestedProjection;
  var sector = {};
  var tableHandler, relationships;
  var keyFieldCount, nonKeyFieldCount;
  var fieldNames, field;
  var indexHandler;
  var relationshipName;
  var relatedFieldMapping, relatedSector, relatedTableHandler, relatedTargetFieldName, relatedTargetField;
  var thisFieldMapping;
  var joinTable, joinTableHandler;
  var foreignKeys, foreignKey, fkIndex, foreignKeyName;
  var i, fkFound;

  // initialize sector
  sector.keyFields = [];
  sector.nonKeyFields = [];
  sector.keyFieldNames = [];
  sector.projection = projection;
  sector.offset = offset;
  tableHandler = projection.domainObject.prototype.mynode.tableHandler;
  udebug.log_detail('createSector for table handler', tableHandler.dbTable.name);
  sector.tableHandler = tableHandler;

  // relatedFieldMapping is the field mapping for the sector to the "left" of this sector
  // it contains the field in the "left" sector and mapping information including join columns
  relatedFieldMapping = projection.relatedFieldMapping;
  sector.relatedFieldMapping = relatedFieldMapping;
  udebug.log_detail('createSector thisDBTable name', tableHandler.dbTable.name, index, sectors);
  if (relatedFieldMapping && index !== 0) {
    // only perform related field mapping for nested projections
    relatedSector = sectors[index - 1];
    relatedTableHandler = relatedSector.tableHandler;
    sector.relatedTableHandler = relatedTableHandler;
    // get this optional field mapping that corresponds to the related field mapping
    // it may be needed to find the foreign key or join table
    relatedTargetFieldName = relatedFieldMapping.targetField;
    relatedTargetField = sector.tableHandler.fieldNameToFieldMap[relatedTargetFieldName];
    if (relatedFieldMapping.toMany && relatedFieldMapping.manyTo) {
      // this is a many-to-many relationship using a join table
      joinTable = relatedFieldMapping.joinTable;
      // joinTableHandler is the DBTableHandler for the join table resolved during validateProjection
      if (joinTable) {
        // join table is defined on the related side
        joinTableHandler = relatedFieldMapping.joinTableHandler;
      } else {
        // join table must be defined on this side
        thisFieldMapping = tableHandler.fieldNameToFieldMap[relatedFieldMapping.targetField];
        joinTable = thisFieldMapping.joinTable;
        if (!joinTable) {
          // error; neither side defined the join table
          projection.error += '\nMappingError: ' + relatedTableHandler.newObjectConstructor.name +
            ' field ' + relatedFieldMapping.fieldName + ' neither side defined the join table.';
        }
        joinTableHandler = thisFieldMapping.joinTableHandler;
      }
      sector.joinTableHandler = joinTableHandler;
      // many to many relationship has a join table with at least two foreign keys; 
      // one to each table mapped to the two domain objects
      if (joinTable) {
        for (foreignKeyName in joinTableHandler.foreignKeyMap) {
          if (joinTableHandler.foreignKeyMap.hasOwnProperty(foreignKeyName)) {
            foreignKey = joinTableHandler.foreignKeyMap[foreignKeyName];
            // is this foreign key for this table?
            if (foreignKey.targetDatabase === tableHandler.dbTable.database && 
                foreignKey.targetTable === tableHandler.dbTable.name) {
              // this foreign key is for the other table
              relatedFieldMapping.otherForeignKey = foreignKey;
            }
            if (foreignKey.targetDatabase === relatedTableHandler.dbTable.database && 
                foreignKey.targetTable === relatedTableHandler.dbTable.name) {
              relatedFieldMapping.thisForeignKey = foreignKey;
            }
          }
        }
        if (!(relatedFieldMapping.thisForeignKey && relatedFieldMapping.otherForeignKey)) {
          // error must have foreign keys to both this table and related table
          projection.error += '\nMappingError: ' + relatedTableHandler.newObjectConstructor.name +
          ' field ' + relatedFieldMapping.fieldName + ' join table must include foreign keys for both sides.';
        }
      }
    } else {
      // this is a relationship using a foreign key
      // resolve the columns involved in the join to the related field
      // there is either a foreign key or a target field that has a foreign key
      // the related field mapping is the field mapping on the other side
      // the field mapping on this side is not used in this projection
      foreignKeyName = relatedFieldMapping.foreignKey;
      if (foreignKeyName) {
        // foreign key is defined on the other side
        foreignKey = relatedTableHandler.getForeignKey(foreignKeyName);
        sector.thisJoinColumns = foreignKey.targetColumnNames;
        sector.otherJoinColumns = foreignKey.columnNames;
      } else {
        // foreign key is defined on this side
        // get the fieldMapping for this relationship field
        relatedTargetField = sector.tableHandler.fieldNameToFieldMap[relatedTargetFieldName];
        foreignKeyName = relatedTargetField.foreignKey;
        if (foreignKeyName) {
        foreignKey = tableHandler.getForeignKey(foreignKeyName);
        sector.thisJoinColumns = foreignKey.columnNames;
        sector.otherJoinColumns = foreignKey.targetColumnNames;
        } else {
          // error: neither side defined the foreign key
          projection.error += 'MappingError: ' + relatedTableHandler.newObjectConstructor.name +
            ' field ' + relatedFieldMapping.fieldName + ' neither side defined the foreign key.';
        }
      }
    }
  }
  // create key fields from primary key index handler
  indexHandler = tableHandler.dbIndexHandlers[0];
  keyFieldCount = indexHandler.fieldNumberToFieldMap.length;
  sector.keyFieldCount = keyFieldCount;
  for (i = 0; i < keyFieldCount; ++i) {
    field = indexHandler.fieldNumberToFieldMap[i];
    sector.keyFields.push(field);
    sector.keyFieldNames.push(field.fieldName);
  }
  // create non-key fields from projection fields excluding key fields
  fieldNames = projection.fields;
  fieldNames.forEach(function(fieldName) {
    // is this field in key fields?
    if (sector.keyFieldNames.indexOf(fieldName) == -1) {
      // non-key field; add it to non-key fields
      field = tableHandler.fieldNameToFieldMap[fieldName];
      sector.nonKeyFields.push(field);
    }
  });
  sector.nonKeyFieldCount = sector.nonKeyFields.length;
  udebug.log_detail('createSector created new sector for index', index, 'sector', sector);
  sectors.push(sector);
  
  innerNestedProjection = projection.firstNestedProjection;
  if (innerNestedProjection) {
    // recurse at the same outer loop, creating the next sector for the inner loop projection
    createSector(outerLoopProjection, innerNestedProjection,
        sectors, index + 1, offset + keyFieldCount + sector.nonKeyFieldCount);
  } else {
    // we are done at this outer projection level; 
    if (outerLoopProjection.name) {
      udebug.log('createSector for ' + outerLoopProjection.name +
          ' created ' + outerLoopProjection.sectors.length + ' sectors for ' + outerLoopProjection.domainObject.name);
    }
    // now go to the outer projection next level down and do it all over again
    outerNestedProjection = outerLoopProjection.firstNestedProjection;
    if (outerNestedProjection) {
      outerNestedProjection.sectors = [];
      createSector(outerNestedProjection, outerNestedProjection, outerNestedProjection.sectors, 0, 0);
    }
  }
}


/** Mark all projections reachable from this projection as validated. */
function markValidated(projections) {
  var projection, relationships, relationshipName;
  if (projections.length > 0) {
    // "pop" the top projection
    projection = projections.shift();
    // mark the top projection validated
    projection.validated = true;
    // if any relationships, add them to the list of projections to validate
    relationships = projection.relationships;
    if (relationships) {
      for (relationshipName in relationships) {
        if (relationships.hasOwnProperty(relationshipName)) {
          projections.push(relationships[relationshipName]);
        }
      }
    }
    // recursively mark related projections
    markValidated(projections);
  }
}

/** Collect errors from all projections reachable from this projection */
function collectErrors(projections, errors) {
  var projection, relationships, relationshipName;
  if (projections.length > 0) {
    // "pop" the top projection
    projection = projections.shift();
    // check the top projection for errors
    errors += projection.error;
    // if any relationships, add them to the list of projections to validate
    relationships = projection.relationships;
    if (relationships) {
      for (relationshipName in relationships) {
        if (relationships.hasOwnProperty(relationshipName)) {
          projections.push(relationships[relationshipName]);
        }
      }
    }
  } else {
    return errors;
  }
  return collectErrors(projections, errors);
}

/** Validate the projection for find and query operations on the domain object.
 * this.user_arguments[0] contains the projection for this operation
 * (first parameter of find or createQuery).
 * Validation occurs in two phases. The first phase individually validates 
 * each domain object associated with a projection. The second phase,
 * implemented as createSector, validates relationships among the domain objects.
 * 
 * In the first phase, get the table handler for the domain object and validate
 * that it is mapped. Then validate each field in the projection.
 * For relationships, validate the name of the relationship. Validate that there
 * is no projected domain object that would cause an infinite recursion.
 * If there is a join table that implements the relationship, validate that the
 * join table exists by loading its metadata.
 * Store the field mapping for this relationship in the related projection.
 * The related field mapping will be further processed in the second phase,
 * once the table metadata for both domain objects has been loaded.
 * Recursively validate the projection that is defined as the relationship.
 * 
 * In the second phase, validate that the relationship is mapped with valid
 * foreign keys and join tables in the database.
 * After all projections have been validated, call the callback with any errors.
 */
exports.UserContext.prototype.validateProjection = function(callback) {
  var userContext = this;
  var session = userContext.session;
  var err;
  var domainObject, domainObjectName;
  var projections, projection;
  var mappingIds, mappingId;
  var relationships, relationshipProjection;
  var fieldMapping;
  var index, offset;
  var errors;
  var foreignKeyNames, foreignKeyName;
  var toBeChecked, toBeValidated, allValidated;
  var domainObjectMynode;
  var joinTableRelationshipField, joinTableRelationshipFields = [];
  var continueValidation;

  function validateJoinTableOnTableHandler(err, joinTableHandler) {
    udebug.log_detail('validateJoinTableOnTableHandler for', joinTableRelationshipField.joinTable, 'err:', err);
    if (err) {
      // mark the projection as broken
      errors += '\nBad projection for ' +  domainObjectName + ': field ' + joinTableRelationshipField.fieldName +
        ' join table ' + joinTableRelationshipField.joinTable + ' failed: ' + err.message;
    } else {
      // continue validating projections
      // we cannot do any more until both sides have their table handlers
      udebug.log_detail('validateJoinTableOnTableHandler resolved table handler for ', domainObjectName,
          ': field', joinTableRelationshipField.fieldName,
          'join table', joinTableRelationshipField.joinTable);
      // store the join table handler in the related field mapping
      joinTableRelationshipField.joinTableHandler = joinTableHandler;
    }
    // finished this join table; continue with more join tables or more tables mapped to domain objects
    joinTableRelationshipField = joinTableRelationshipFields.shift();
    if (joinTableRelationshipField) {
      getTableHandler(joinTableRelationshipField.joinTable, session, validateJoinTableOnTableHandler);
    } else {
      continueValidation();
    }
  }

  function validateProjectionOnTableHandler(err, dbTableHandler) {
    // currently validating projections[index] with the tableHandler for the domain object
    projection = projections[index];
    // keep track of how many times this projection has been changed so adapters know when to re-validate
    projection.id = (projection.id + 1) % (2^24);
    
    domainObject = projection.domainObject;
    domainObjectName = domainObject.prototype.constructor.name;
    domainObjectMynode = domainObject.prototype.mynode;
    if (domainObjectMynode && domainObjectMynode.mapping.error) {
      // remember errors in mapping
      errors += domainObjectMynode.mapping.error;
    }
    if (!err) {
      projection.dbTableHandler = dbTableHandler;
      // validate using table handler
      if (typeof(domainObject) === 'function' &&
          typeof(domainObject.prototype.mynode) === 'object' &&
          typeof(domainObject.prototype.mynode.mapping) === 'object') {
        // good domainObject; have we seen this one before?
        mappingId = domainObject.prototype.mynode.mappingId;
        if (mappingIds.indexOf(mappingId) === -1) {
          // have not seen this one before; add its mappingId to list of mappingIds to prevent cycles (recursion)
          mappingIds.push(mappingId);
          // validate all fields in projection are mapped
          if (projection.fields) { // field names
            projection.fields.forEach(function(fieldName) {
              fieldMapping = dbTableHandler.fieldNameToFieldMap[fieldName];
              if (fieldMapping) {
                if (fieldMapping.relationship) {
                  errors += '\nBad projection for ' +  domainObjectName + ': field' + fieldName + ' must not be a relationship';
                }
              } else {
                // error: fields must be mapped
                errors += '\nBad projection for ' +  domainObjectName + ': field ' + fieldName + ' is not mapped';
              }
            });
          }
          // validate all relationships in mapping regardless of whether they are in this projection
          
          dbTableHandler.relationshipFields.forEach(function(relationshipField) {
            // get the name and projection for each relationship
            foreignKeyName = relationshipField.foreignKey;
            if (foreignKeyName) {
              // make sure the foreign key exists
              if (!dbTableHandler.foreignKeyMap.hasOwnProperty(foreignKeyName)) {
                errors += '\nBad relationship field mapping; foreign key ' + foreignKeyName +
                    ' does not exist in table; possible foreign keys are: ' + Object.keys(dbTableHandler.foreignKeyMap);
              }
            }
            // remember this relationship in order to resolve table mapping for join table
            if (relationshipField.joinTable) {
              joinTableRelationshipFields.push(relationshipField);
            }
          });
          // add relationship domain objects to the list of domain objects
          relationships = projection.relationships;
          if (relationships) {
            Object.keys(relationships).forEach(function(key) {
              // each key is the name of a relationship that must be a field in the table handler
              fieldMapping = dbTableHandler.fieldNameToFieldMap[key];
              if (fieldMapping) {
                if (fieldMapping.relationship) {
                  relationshipProjection = relationships[key];
                  relationshipProjection.relatedFieldMapping = fieldMapping;
                  // add each relationship to list of projections
                  projections.push(relationshipProjection);
                  // record the first (currently the only) projection that is nested in this projection
                  if (!projection.firstNestedProjection) {
                    projection.firstNestedProjection = relationshipProjection;
                  }
                } else {
                  // error: field is not a relationship
                  errors += '\nBad relationship for ' +  domainObjectName + ': field ' + key + ' is not a relationship.';
                }
              } else {
                // error: relationships must be mapped
                errors += '\nBad relationship for ' +  domainObjectName + ': field ' + key + ' is not mapped.';
              }
            });
          }
        } else {
          // recursive projection
          errors += '\nRecursive projection for ' + domainObjectName;
        }
      } else {
        // domainObject was not mapped
        errors += '\nBad domain object: ' + domainObjectName + ' is not mapped.';
      } 
    } else {
      // table does not exist
        errors += '\nUnable to acquire tableHandler for ' + domainObjectName + ' : ' + err.message;
    }
    // finished validating this projection; do we have a join table to validate?
    if (joinTableRelationshipFields.length > 0) {
      // get the table handler for the first join table
      joinTableRelationshipField = joinTableRelationshipFields.shift();
      getTableHandler(joinTableRelationshipField.joinTable, session, validateJoinTableOnTableHandler);
    } else {
      continueValidation();
    }
  }
  
  // continue validation from either projection domain object or relationship join table
  continueValidation = function() {
    // are there any more?
    if (projections.length > ++index) {
      // do the next projection
      if (projections[index].domainObject.prototype.mynode.dbTableHandler) {
        udebug.log('validateProjection with cached tableHandler for', projections[index].domainObject.name);
        validateProjectionOnTableHandler(null, projections[index].domainObject.prototype.mynode.dbTableHandler);
      } else {
        udebug.log('validateProjection with no cached tableHandler for', projections[index].domainObject.name);
        getTableHandler(projections[index].domainObject, session, validateProjectionOnTableHandler);
      }
    } else {
      // there are no more projections to validate -- did another user finish table handling first?
      if (!userContext.user_arguments[0].validated) {
        // we are the first to validate table handling -- check for errors
        if (!errors) {
          projection = projections[0];
          // no errors yet
          // we are done getting all of the table handlers for the projection; now create the sectors
          projection.sectors = [];
          index = 0;
          offset = 0;
          // create the first sector; additional sectors will be created recursively
          createSector(projection, projection, projection.sectors, index, offset);
          // now look for errors found during createSector
          errors = collectErrors([userContext.user_arguments[0]], '');
          // mark all projections reachable from this projections as validated
          // projections will grow at the end as validated marking proceeds
          if (!errors) {
            // no errors in createSector
            toBeValidated = [userContext.user_arguments[0]]; 
            markValidated(toBeValidated);
            udebug.log('validateProjection complete for', projections[0].domainObject.name);
            callback(null);
            return;
          }
        }
        // report errors and call back user
        if (errors) {
          udebug.log('validateProjection had errors:\n', errors);
          err = new Error(errors);
          err.sqlstate = 'HY000';
        }
      }
      callback(err);
    }
  };


  // validateProjection starts here
  // projection: {domainObject:<constructor>, fields: [field, field], relationships: {field: {projection}, field: {projection}}
  // first check to see if the projection is already validated. If so, we are done.
  // the entire projection including all referenced relationships must be checked because a relationship
  // might have changed since it was last validated.
  // projections will grow at the end as validation checking proceeds
  // construct a new array which will grow as validation checking proceeds
  if (userContext.user_arguments[0].validated) {
    callback(null);
  } else {
    // set up to iteratively validate projection starting with the user parameter
    projections = [this.user_arguments[0]]; // projections will grow at the end as validation proceeds
    index = 0;                              // index into projections for the projection being validated
    offset = 0;                             // the number of fields in previous projections
    errors = '';                            // errors in validation
    mappingIds = [];                        // mapping ids seen so far

    // the projection is not already validated; check to see if the domain object already has its dbTableHandler
    domainObjectMynode = projections[0].domainObject.prototype.mynode;
    if (domainObjectMynode && domainObjectMynode.dbTableHandler) {
      udebug.log('validateProjection with cached tableHandler for', projections[0].domainObject.name);
      validateProjectionOnTableHandler(null, domainObjectMynode.dbTableHandler);
    } else {
      // get the dbTableHandler the hard way
      udebug.log('validateProjection with no tableHandler for', projections[0].domainObject.name);
      getTableHandler(projections[index].domainObject, userContext.session, validateProjectionOnTableHandler);
    }
  }
};

/** Use the projection to find a domain object. This is only valid in a session, not a batch.
 * Multiple operations may be needed to resolve the complete projection.
 * Take the user's projection and see if it has been resolved. For an unresolved projection, 
 * load table mappings for all included domain objects and verify the projection against
 * the resolved table mappings. 
 * Once the projection has been resolved, get the db index to use for the operation,
 * call db session to create a read with projection operation, and execute the operation.
 * The db session will process the projection to populate the result.
 */
exports.UserContext.prototype.findWithProjection = function() {
  var userContext = this;
  var session = userContext.session;
  var dbSession = session.dbSession;
  var projection = userContext.user_arguments[0];
  var keys = userContext.user_arguments[1];
  var dbTableHandler;
  var indexHandler;
  var transactionHandler;

  function findWithProjectionOnResult(err, dbOperation) {
      udebug.log('find.findWithProjectionOnResult');
      var result, values;
      var error = checkOperation(err, dbOperation);
      if (error && dbOperation.result.error.sqlstate !== '02000') {
        if (userContext.session.tx.isActive()) {
          userContext.session.tx.setRollbackOnly();
        }
        userContext.applyCallback(err, null);
      } else {
        if(udebug.is_detail()) { udebug.log('findOnResult returning ', dbOperation.result.value); }
        userContext.applyCallback(null, dbOperation.result.value);      
      }
    }


  function onValidatedProjection(err) {
    if (err) {
      udebug.log('UserContext.onValidProjection err: ', err);
      userContext.applyCallback(err, null);
    } else {
      dbTableHandler = projection.dbTableHandler;
      userContext.dbTableHandler = dbTableHandler;
      keys = userContext.user_arguments[1];
      indexHandler = dbTableHandler.getIndexHandler(keys, true);
      if (indexHandler === null) {
        err = new Error('UserContext.find unable to get an index for ' + dbTableHandler.dbTable.name +
            ' to use with ' + JSON.stringify(keys));
        userContext.applyCallback(err, null);
      } else {
        // create the find operation and execute it
        dbSession = userContext.session.dbSession;
        transactionHandler = dbSession.getTransactionHandler();
        userContext.operation = dbSession.buildReadProjectionOperation(indexHandler, keys, projection,
            transactionHandler, findWithProjectionOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            if(udebug.is_detail()) { udebug.log('find transactionHandler.execute callback.'); }
          });
        } else if (typeof(userContext.operationDefinedCallback) === 'function') {
          userContext.operationDefinedCallback(1);
        }
      }
    }
  }
  // findWithProjection starts here
  // validate the projection and construct the sectors
  userContext.validateProjection(onValidatedProjection);
  // the caller will return userContext.promise
};

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
    if (error && dbOperation.result.error.sqlstate !== '02000') {
      if (userContext.session.tx.isActive()) {
        userContext.session.tx.setRollbackOnly();
      }
      userContext.applyCallback(err, null);
    } else {
      if(udebug.is_detail()) { udebug.log('findOnResult returning ', dbOperation.result.value); }
      userContext.applyCallback(null, dbOperation.result.value);      
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
        userContext.operation = dbSession.buildReadOperation(index, keys,
            transactionHandler, findOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            if(udebug.is_detail()) { udebug.log('find transactionHandler.execute callback.'); }
          });
        } else if (typeof(userContext.operationDefinedCallback) === 'function') {
          userContext.operationDefinedCallback(1);
        }
      }
    }
  }

  // find starts here
  // session.find(projectionOrPrototypeOrTableName, key, callback)
  // validate first two parameters must be defined
  if (userContext.user_arguments[0] === undefined || userContext.user_arguments[1] === undefined) {
    userContext.applyCallback(new Error('User error: find must have at least two arguments.'), null);
  } else {
    if (userContext.user_arguments[0].constructor.name === 'Projection' &&
        typeof userContext.user_arguments[0].constructor.prototype.addRelationship === 'function') {
      // this is a projection
      userContext.findWithProjection();
    } else {
      // get DBTableHandler for prototype/tableName
      getTableHandler(userContext.user_arguments[0], userContext.session, findOnTableHandler);
    }
  }
  return userContext.promise;
};


/** Create a query object.
 * 
 */
exports.UserContext.prototype.createQuery = function() {
  var userContext = this;
  var tableHandler;
  var queryDomainType;

  function createQueryOnTableHandler(err, dbTableHandler) {
    if (err) {
      userContext.applyCallback(err, null);
    } else {
      // create the query domain type and bind it to this session
      queryDomainType = new query.QueryDomainType(userContext.session, dbTableHandler, userContext.domainObject);
      if(udebug.is_detail()) { udebug.log('UserContext.createQuery queryDomainType:', queryDomainType); }
      userContext.applyCallback(null, queryDomainType);
    }
  }

  // createQuery starts here
  // session.createQuery(constructorOrTableName, callback)
  // if the first parameter is a query object then copy the interesting bits and create a new object
  if (this.user_arguments[0].mynode_query_domain_type) {
    // TODO make sure this sessionFactory === other.sessionFactory
    queryDomainType = new query.QueryDomainType(userContext.session);
  }
  // if the first parameter is a table name the query results will be literals
  // if not (constructor or domain object) the query results will be domain objects
  userContext.domainObject = typeof(this.user_arguments[0]) !== 'string';
  // get DBTableHandler for constructor/tableName
  getTableHandler(userContext.user_arguments[0], userContext.session, createQueryOnTableHandler);

  return userContext.promise;
};

/** maximum skip and limit parameters are some large number */
var MAX_SKIP = Math.pow(2, 52);
var MAX_LIMIT = Math.pow(2, 52);

/** Execute a query. 
 * 
 */
exports.UserContext.prototype.executeQuery = function(queryDomainType) {
  var userContext = this;
  var dbSession, transactionHandler, queryType;
  userContext.queryDomainType = queryDomainType;

  // transform query result
  function executeQueryKeyOnResult(err, dbOperation) {
    udebug.log('executeQuery.executeQueryPKOnResult');
    var result, values, resultList;
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(error, null);
    } else {
      if (userContext.queryDomainType.mynode_query_domain_type.domainObject) {
        values = dbOperation.result.value;
        result = userContext.queryDomainType.mynode_query_domain_type.dbTableHandler.newResultObject(values);
      } else {
        result = dbOperation.result.value;
      }
      if (result !== null) {
        // TODO: filter in memory if the adapter didn't filter all conditions
        resultList = [result];
      } else {
        resultList = [];
      }
      userContext.applyCallback(null, resultList);      
    }
  }

  // transform query result
  function executeQueryScanOnResult(err, dbOperation) {
    if(udebug.is_detail()) { udebug.log('executeQuery.executeQueryScanOnResult'); }
    var result, values, resultList;
    var error = checkOperation(err, dbOperation);
    if (error) {
      userContext.applyCallback(error, null);
    } else {
      if(udebug.is_detail()) { udebug.log('executeQuery.executeQueryScanOnResult', dbOperation.result.value); }
      // TODO: filter in memory if the adapter didn't filter all conditions
      userContext.applyCallback(null, dbOperation.result.value);      
    }
  }

  // executeScanQuery is used by both index scan and table scan
  var executeScanQuery = function() {
    // validate order, skip, and limit parameters
    var params = userContext.user_arguments[0];
    var orderToUpperCase;
    var order = params.order, skip = params.skip, limit = params.limit;
    var error;
    if (typeof(limit) !== 'undefined') {
      if (limit < 0 || limit > MAX_LIMIT) {
        // limit is out of valid range
        error = new Error('Bad limit parameter \'' + limit + '\'; limit must be >= 0 and <= ' + MAX_LIMIT + '.');
      }
    }
    if (typeof(skip) !== 'undefined') {
      if (skip < 0 || skip > MAX_SKIP) {
        // skip is out of valid range
        error = new Error('Bad skip parameter \'' + skip + '\'; skip must be >= 0 and <= ' + MAX_SKIP + '.');
      } else {
        if (!order) {
          // skip is in range but order is not specified
          error = new Error('Bad skip parameter \'' + skip + '\'; if skip is specified, then order must be specified.');
        }
      }
    }
    if (typeof(order) !== 'undefined') {
      if (typeof(order) !== 'string') {
        error = new Error('Bad order parameter \'' + order + '\'; order must be ignoreCase asc or desc.');
      } else {
        orderToUpperCase = order.toUpperCase();
        if (!(orderToUpperCase === 'ASC' || orderToUpperCase === 'DESC')) {
          error = new Error('Bad order parameter \'' + order + '\'; order must be ignoreCase asc or desc.');
        }
      }
    }
    if (error) {
      userContext.applyCallback(error, null);
    } else {
      dbSession = userContext.session.dbSession;
      transactionHandler = dbSession.getTransactionHandler();
      userContext.operation = dbSession.buildScanOperation(
          queryDomainType, userContext.user_arguments[0], transactionHandler,
          executeQueryScanOnResult);
      // TODO: this currently does not support batching
      transactionHandler.execute([userContext.operation], function() {
        if(udebug.is_detail()) { udebug.log('executeQueryPK transactionHandler.execute callback.'); }
      });
    }
//  if (userContext.execute) {
//  transactionHandler.execute([userContext.operation], function() {
//    if(udebug.is_detail()) udebug.log('find transactionHandler.execute callback.');
//  });
//} else if (typeof(userContext.operationDefinedCallback) === 'function') {
//  userContext.operationDefinedCallback(1);
//}    
  };    

  // executeKeyQuery is used by both primary key and unique key
  var executeKeyQuery = function() {
    // create the find operation and execute it
    dbSession = userContext.session.dbSession;
    transactionHandler = dbSession.getTransactionHandler();
    var dbIndexHandler = queryDomainType.mynode_query_domain_type.queryHandler.candidateIndex.dbIndexHandler;
    var keys = queryDomainType.mynode_query_domain_type.queryHandler.getKeys(userContext.user_arguments[0]);
    userContext.operation = dbSession.buildReadOperation(dbIndexHandler, keys, transactionHandler,
        executeQueryKeyOnResult);
    // TODO: this currently does not support batching
    transactionHandler.execute([userContext.operation], function() {
      if(udebug.is_detail()) { udebug.log('executeQueryPK transactionHandler.execute callback.'); }
    });
//    if (userContext.execute) {
//      transactionHandler.execute([userContext.operation], function() {
//        if(udebug.is_detail()) udebug.log('find transactionHandler.execute callback.');
//      });
//    } else if (typeof(userContext.operationDefinedCallback) === 'function') {
//      userContext.operationDefinedCallback(1);
//    }    
  };
  
  // executeQuery starts here
  // query.execute(parameters, callback)
  udebug.log('QueryDomainType.execute', queryDomainType.mynode_query_domain_type.predicate, 
      'with parameters', userContext.user_arguments[0]);
  // execute the query and call back user
  queryType = queryDomainType.mynode_query_domain_type.queryType;
  switch(queryType) {
  case 0: // primary key
    executeKeyQuery();
    break;

  case 1: // unique key
    executeKeyQuery();
    break;

  case 2: // index scan
    executeScanQuery();
    break;

  case 3: // table scan
    executeScanQuery();
    break;

  default: 
    throw new Error('FatalInternalException: queryType: ' + queryType + ' not supported');
  }
  
  return userContext.promise;
};


/** Persist the object.
 * 
 */
exports.UserContext.prototype.persist = function() {
  var userContext = this;
  var object;

  function persistOnResult(err, dbOperation) {
    udebug.log('persist.persistOnResult');
    // return any error code
    var error = checkOperation(err, dbOperation);
    if (error) {
      if (userContext.session.tx.isActive()) {
        userContext.session.tx.setRollbackOnly();
      }
      userContext.applyCallback(error);
    } else {
      if (dbOperation.result.autoincrementValue) {
        // put returned autoincrement value into object
        userContext.dbTableHandler.setAutoincrement(userContext.values, dbOperation.result.autoincrementValue);
      }
      userContext.applyCallback(null);      
    }
  }

  function persistOnTableHandler(err, dbTableHandler) {
    userContext.dbTableHandler = dbTableHandler;
    if(udebug.is_detail()){  udebug.log('UserContext.persist.persistOnTableHandler ' + err); }
    var transactionHandler;
    var dbSession = userContext.session.dbSession;
    if (userContext.clear) {
      // if batch has been cleared, user callback has already been called
      return;
    }
    if (err) {
      userContext.applyCallback(err);
    } else {
      transactionHandler = dbSession.getTransactionHandler();
      userContext.operation = dbSession.buildInsertOperation(dbTableHandler, userContext.values, transactionHandler,
          persistOnResult);
      if (userContext.execute) {
        transactionHandler.execute([userContext.operation], function() {
          if(udebug.is_detail()) { udebug.log('persist transactionHandler.execute callback.'); }
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
  return userContext.promise;
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
      if (userContext.session.tx.isActive()) {
        userContext.session.tx.setRollbackOnly();
      }
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
    } else {
      transactionHandler = dbSession.getTransactionHandler();
      indexHandler = dbTableHandler.getIndexHandler(userContext.values);
      if (!indexHandler.dbIndex.isPrimaryKey) {
        userContext.applyCallback(
            new Error('Illegal argument: parameter of save must include all primary key columns.'));
        return;
      }
      userContext.operation = dbSession.buildWriteOperation(indexHandler, userContext.values, transactionHandler,
          saveOnResult);
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
  return userContext.promise;
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
      if (userContext.session.tx.isActive()) {
        userContext.session.tx.setRollbackOnly();
      }
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
    } else {
      transactionHandler = dbSession.getTransactionHandler();
      indexHandler = dbTableHandler.getIndexHandler(userContext.keys);
      // for variant update(object, callback) the object must include all primary keys
      if (userContext.required_parameter_count === 2 && !indexHandler.dbIndex.isPrimaryKey) {
        userContext.applyCallback(
            new Error('Illegal argument: parameter of update must include all primary key columns.'));
        return;
      }
      userContext.operation = dbSession.buildUpdateOperation(indexHandler, userContext.keys,
          userContext.values, transactionHandler, updateOnResult);
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
  return userContext.promise;
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
      if (userContext.session.tx.isActive()) {
        userContext.session.tx.setRollbackOnly();
      }
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
        userContext.operation = dbSession.buildReadOperation(index, keys, transactionHandler,
            loadOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            if(udebug.is_detail()) { udebug.log('load transactionHandler.execute callback.'); }
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
  return userContext.promise;
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
      if (userContext.session.tx.isActive()) {
        userContext.session.tx.setRollbackOnly();
      }
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
        err = new Error('UserContext.remove unable to get an index to use for ' + JSON.stringify(userContext.keys));
        userContext.applyCallback(err);
      } else {
        transactionHandler = dbSession.getTransactionHandler();
        userContext.operation = dbSession.buildDeleteOperation(
            dbIndexHandler, userContext.keys, transactionHandler, removeOnResult);
        if (userContext.execute) {
          transactionHandler.execute([userContext.operation], function() {
            if(udebug.is_detail()) { udebug.log('remove transactionHandler.execute callback.'); }
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
  return userContext.promise;
};

/** Get Mapping
 * 
 */
exports.UserContext.prototype.getMapping = function() {
  var userContext = this;
  function getMappingOnTableHandler(err, dbTableHandler) {
    if (err) {
      userContext.applyCallback(err, null);
      return;
    }
    var mapping = dbTableHandler.resolvedMapping;
    userContext.applyCallback(null, mapping);
  }
  // getMapping starts here
  getTableHandler(userContext.user_arguments[0], userContext.session, getMappingOnTableHandler);  
  return userContext.promise;
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
    if(udebug.is_detail()) { 
      udebug.log('UserContext.executeBatch expecting', userContext.numberOfOperations, 
                'operations with', userContext.numberOfOperationsDefined, 'already defined.');
    }
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
  // if no operations in the batch, just call the user callback
  if (operationContexts.length == 0) {
    executeBatchOnExecute(null);
  } else {
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
  }
  return userContext.promise;
};

/** Commit an active transaction. 
 * 
 */
exports.UserContext.prototype.commit = function() {
  var userContext = this;

  var commitOnCommit = function(err) {
    udebug.log('UserContext.commitOnCommit.');
    userContext.session.tx.setState(userContext.session.tx.idle);
    userContext.applyCallback(err);
  };

  // commit begins here
  if (userContext.session.tx.isActive()) {
    udebug.log('UserContext.commit tx is active.');
    userContext.session.dbSession.commit(commitOnCommit);
  } else {
    userContext.applyCallback(
        new Error('Fatal Internal Exception: UserContext.commit with no active transaction.'));
  }
  return userContext.promise;
};


/** Roll back an active transaction. 
 * 
 */
exports.UserContext.prototype.rollback = function() {
  var userContext = this;

  var rollbackOnRollback = function(err) {
    udebug.log('UserContext.rollbackOnRollback.');
    userContext.session.tx.setState(userContext.session.tx.idle);
    userContext.applyCallback(err);
  };

  // rollback begins here
  if (userContext.session.tx.isActive()) {
    udebug.log('UserContext.rollback tx is active.');
    var transactionHandler = userContext.session.dbSession.getTransactionHandler();
    transactionHandler.rollback(rollbackOnRollback);
  } else {
    userContext.applyCallback(
        new Error('Fatal Internal Exception: UserContext.rollback with no active transaction.'));
  }
  return userContext.promise;
};


/** Open a session. Allocate a slot in the session factory sessions array.
 * Call the DBConnectionPool to create a new DBSession.
 * Wrap the DBSession in a new Session and return it to the user.
 * This function is called by both mynode.openSession (without a session factory)
 * and SessionFactory.openSession (with a session factory).
 */
exports.UserContext.prototype.openSession = function() {
  var userContext = this;

  var openSessionOnSession = function(err, dbSession) {
    if (err) {
      userContext.applyCallback(err, null);
    } else {
      userContext.session = new apiSession.Session(userContext.session_index, userContext.session_factory, dbSession);
      userContext.session_factory.sessions[userContext.session_index] = userContext.session;
      userContext.applyCallback(err, userContext.session);
    }
  };

  var openSessionOnSessionFactory = function(err, factory) {
    if (err) {
      userContext.applyCallback(err, null);
    } else {
      userContext.session_factory = factory;
      // allocate a new session slot in sessions
      userContext.session_index = userContext.session_factory.allocateSessionSlot();
      // get a new DBSession from the DBConnectionPool
      userContext.session_factory.dbConnectionPool.getDBSession(userContext.session_index, 
          openSessionOnSession);
    }
  };
  
  // openSession starts here
  if (userContext.session_factory) {
    openSessionOnSessionFactory(null, userContext.session_factory);
  } else {
    if(udebug.is_detail()) { udebug.log('openSession for', util.inspect(userContext)); }
    // properties might be null, a name, or a properties object
    userContext.user_arguments[0] = resolveProperties(userContext.user_arguments[0]);
    getSessionFactory(userContext, userContext.user_arguments[0], userContext.user_arguments[1], 
        openSessionOnSessionFactory);
  }
  return userContext.promise;
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
  return userContext.promise;
};


/** Close all open SessionFactories
 *
 */
exports.UserContext.prototype.closeAllOpenSessionFactories = function() {
  var userContext, openFactories, nToClose;

  userContext   = this;
  openFactories = mynode.getOpenSessionFactories();
  nToClose      = openFactories.length;

  function onFactoryClose() {
    nToClose--;
    if(nToClose === 0) {
      userContext.applyCallback(null);
    }
  }

  if(nToClose > 0) {
    while(openFactories[0]) {
      openFactories[0].close(onFactoryClose);
      openFactories.shift();
    }
  } else {
    userContext.applyCallback(null);
  }
  return userContext.promise;
};


/** Complete the user function by calling back the user with the results of the function.
 * Apply the user callback using the current arguments and the extra parameters from the original function.
 * Create the args for the callback by copying the current arguments to this function. Then, copy
 * the extra parameters from the original function. Finally, call the user callback.
 * If there is no user callback, and there is an error (first argument to applyCallback)
 * throw the error.
 */
exports.UserContext.prototype.applyCallback = function(err, result) {
  if (arguments.length !== this.returned_parameter_count) {
    throw new Error(
        'Fatal internal exception: wrong parameter count ' + arguments.length +' for UserContext applyCallback' + 
        '; expected ' + this.returned_parameter_count);
  }
  // notify (either fulfill or reject) the promise
  if (err) {
    if(udebug.is_detail()) { udebug.log('UserContext.applyCallback.reject', err); }
    this.promise.reject(err);
  } else {
    if(udebug.is_detail()) { udebug.log('UserContext.applyCallback.fulfill', result); }
    this.promise.fulfill(result);
  }
  if (typeof(this.user_callback) === 'undefined') {
    if(udebug.is_detail()) udebug.log('UserContext.applyCallback with no user_callback.');
    return;
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

exports.Promise = Promise;