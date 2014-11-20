MySQL-JS
========

Introduction
------------
This package provides a fast, easy, and safe framework for building 
database applications in Node.js.  It is organized around the concept
of a database *session*, which allows standard JavaScript objects to be
read from and written to a database.

This example uses a session to store a single object into a MySQL table:
```
var nosql = require("mysql-js");

var connectionProperties = {
  "implementation" : "mysql",
  "database"       : "test",
  "mysql_host"     : "localhost",
  "mysql_port"     : 3306,
  "mysql_user"     : "test",
  "mysql_password" : "",    
};

nosql.openSession(connectionProperties).then(
  function(session) {
    var user = { id: 1, name: "Database Jones"};
    return session.persist("user", user);
  }
).then(
  function() { 
    console.log("Complete");
    nosql.closeAllOpenSessionFactories();
  }
);
```


Quick Install
-------------
```
npm install https://github.com/mysql/mysql-js/archive/2014-10-06.tar.gz
```


Supported Databases and Connection Properties
---------------------------------------------
MySQL-JS provides a common data management API over a variety of back-end
database connections.  Two database adapters are currently supported.
The *mysql* adapter provides generic support for any MySQL database,
based on all-JavaScript mysql connector node-mysql.
The *ndb* adapter provides optimized high-performance access to MySQL Cluster
using the NDB API.

Each backend adapter supports its own set of connection properties.
+ [MySQL Connection Properties](Backend-documentation/mysql_properties.js)
+ [NDB Connection Properties](Backend-documentation/ndb_properties.js)


Session
-------
The central concept of mysql-js is the **Session**.  A session provides
a context for database operations and transactions.  Each independent user 
context should have a distinct session.  For instance, in a web application, 
handling each HTTP request involves opening a session, using the session to
access the database, and then closing the session.


### Session methods

Most methods on session() are available on several varieties: they may take 
either a mapped object or a literal table name; they may take a callback, or 
rely on the returned promise for continuation; and they may take any number
of extra arguments after a callback.

Each of the following methods is *asynchronous* and *returns a promise*:
+ **find()** Find an instance in the database using a primary or unique key.
  + find(Constructor, keys, [callback], [...])
  + find(Projection, keys, [callback], [...])
  + find(tableName, keys, [callback], [...])
+ **load(instance, [callback], [...])** Loads a specific instance from the database 
based on the primary or unique key present in the object.
+ **persist()** Insert an instance into the database.
  + persist(instance, [callback], [...])
  + persist(Constructor, values, [callback], [...])
  + persist(tableName, values, [callback], [...])
+ **remove()** Delete an instance by primary or unique key.
  + remove(instance, [callback], [...])
  + remove(Constructor, keys, [callback], [...])
  + remove(tableName, keys, [callback], [...])
+ **update()** Update an instance by primary or unique key without necessarily retrieving it.
  + update(instance, [callback], [...])
  + update(Constructor, keys, values, [callback], [...])
  + update(tableName, keys, values, [callback], [...])
+ **save()** Write an object to the database without checking for existence; could result in either an update or an insert.
  + save(instance, [callback], [...])
  + save(Constructor, values, [callback], [...])
  + save(tableName, values, [callback], [...])
+ **createQuery()** Create an object that can be used to query the database
  + createQuery(instance, [callback], [...])
  + createQuery(Constructor, [callback], [...])
  + createQuery(tableName, [callback], [...])
+ **getMapping()** Resolve and fetch mappings for a table or class
  + getMapping(object, [callback], [...])
  + getMapping(Constructor, [callback], [...])
  + getMapping(tableName, [callback], [...])
+ **close([callback], [...])** Close the current session

The following methods are *immediate*:
+ createBatch().  Returns a batch.
+ listBatches().  Returns an array of batches.
+ isClosed().  Returns boolean.
+ isBatch(). Returns boolean.
+ currentTransaction().  Returns a Transaction.

See the [Complete documentation for Session](API-documentation/Session)


SessionFactory
--------------
A [SessionFactory](API-documentaiton/SessionFactory) is a heavyweight master 
connection to a database, *i.e.* for a whole process or application.  

A SessionFactory generally makes use of network resources such as TCP connections.
A node.js process will often not exit until all SessionFactories have been 
closed.



Promises and Callbacks
----------------------
The majority of the asynchronous API methods in mysql-js return a 
[Promises/A+ compatible promise](http://promisesaplus.com).  

These promises are objects that implement the method **then(onFulfilled, onRejected)**:
If the asynchronous call completes succesfully, *onFulfilled* will be called with
one parameter holding the value produced by the async call; if it fails, *onRejected*
will be called with one parameter holding the error condition.  The *then()* method
also returns a promise, which allows promise calls to be chained.

Async calls also support standard node.js callbacks.  If a callback is provided,
it will be called with parameters *(error, value)* on the completion of the call.


The top level mysql-js API
--------------------------
Idiomatically the top-level API is often referred to as *nosql*:
```
var nosql = require("mysql-js");
var properties = new nosql.ConnectionProperties("mysql");
properties.mysql_host = "productiondb";
var mapping = new nosql.TableMapping("webapp.users");
nosql.connect(properties, mapping, onConnectedCallback);
```

+ *ConnectionProperties(adapterName)*: *Constructor*.  Creates a ConnectionProperties
  object containing default values for all properties.
+ *TableMapping(tableName)*: *Constructor*.  Creates a new TableMapping.
+ *Projection(mappedConstructor)*: *Constructor*.  Creates a new Projection.
+ *connect(properties, [mappings], [callback], [...]): *ASYNC*.  The callback
or promise receives a SessionFactory.
+ *openSession(properties, [mappings], [callback], [...]): *ASYNC*.  An implicit
SessionFactory is opened if needed; the callback or promise receives a Session.
+ *getOpenSessionFactories()* Returns an array
+ *closeAllOpenSessionFactories()* Returns undefined

See the [complete documentation for the top-level API](API-documentation/Mynode)


Mapped Objects
-------------------------
A **TableMapping** is an _entirely optional_ part of the API that allows you
to fine-tune the relations between JavaScript objects and database records. 
All of the data management calls available on *session* can take either a table
name (so that they work without any mapping), or a *mapped object*.  When a 
table name is used with find(), for instance, the returned object contains
one property for every database column, with each property name the same as the
corresponding column name, and the property value of a default JavaScript type 
based on the column type.  When find() is used with a TableMapping, it can
return an object with some subset of the fields from the mapped table (along 
perhaps with some *non-persistent* fields), using cusom type conversions, 
created from a particular constructor, and connected to a class prototype.


```
  function User() {     // Constructor for application object
  }

  var userTable = new nosql.TableMapping("webapp.user");  // map a table
  userTable.mapField("firstName","first_name"); // customize the mapping
  userTable.applyToClass(User);  // apply the mapping to the constructor
``` 
See the [complete documentation for TableMapping](API-documentation/TableMapping).

Converters
----------
The data types stored in a particular database do not always correspond to
native JavaScript types.  For instance, most databases support 64-bit
signed and unsigned integers, while JavaScript does not.  MySQL-JS allows 
users to customize data conversion in these cases using 
[Converter classess](API-documentation/Converter).  A Converter class marshalls
data between an *intermediate format* and a desired JavaScript format by means of
two methods, toDB() and fromDB().  

The intermediate format for each column type is defined by the backend database 
driver; e.g. the mysql and ndb drivers use *string* as the intermediate type for
BIGINT columns.

To declare a converter universally for a particular column type, use
**sessionFactory.registerTypeConverter()**.  To declare a specific converter
for a particular TableMapping, assign it to the converter property of a mapped
field.


Batches
-------
MySQL-JS allows flexible batching of operations.  Many of the *Session* operations
are also supported by [Batch](API-documentation/Batch).  A variety of operations
can be defined in a batch, and will all be executed together at once.  Callbacks
are available for each completed operation and for the batch as a whole.

```
  var batch = session.createBatch();
  for(i = 0; i < itemDetails.length ; i++) {
    batch.persist(itemDetails[i]);
  }
  batch.update(userHistory);
  batch.update(userStatistics, onStatsUpdatedCallback);
  batch.remove(unsavedCart);
  batch.execute(batchCallback);
```


Transactions
------------
Each Session includes a single current [Transaction](API-documentation/Transaction), 
which is obtained using the *session.currentTransaction()* call.  

```
  var transaction = session.currentTransaction();
  transaction.begin();
  session.update(user);
  session.update(cart);
  transaction.commit(onCommitCallback);
```

By default, operations happen in **auto-commit mode**, with each operation
enclosed in a transaction of its own.



Queries
-------
While *session.find()* can be used to fetch a single database record using 
primary or unique index access, more complex queries are provided through
the [Query class](API-documentation/Query)

Queries are defined by a filter that specifies which database rows should be 
returned. The filter is declared fluently, combining queryable columns with 
comparators and parameters.

```
session.createQuery('employee').then(function(query) {
  query.where(
   query.salary.gt(query.param('low_salary')
   .and(query.salary.lt(query.param('high_salary')))));
});
```

Query execution is governed by a parameter object that can include values 
for named parameters for the query as well as options to sort or paginate the 
result. Query execution returns a promise but can also use the standard callback
mechanism.

This query will return at most 20 objects that satisfy the filter, in 
ascending order. The same query object can be reused with different parameters 
and options.

```
query.execute({low_salary: 10000, high_salary:20000, limit: 20, order: 'asc"})
 .then(function(result) {console.log(result));
```

Standardized Errors
-------------------
MySQL-JS provides a common representation of database errors, independent 
of backend adapters.  This representation is based on SQLState, as used in
the SQL 1999 standard.  The DatabaseError object and supported SQLState codes
are described in the [Error documentation](API-documentation/Error).


