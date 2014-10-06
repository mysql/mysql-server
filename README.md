MySQL-JS
========

INTRODUCTION
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


QUICK INSTALL
-------------
MySQL-JS can be installed using NPM:

```
npm install https://github.com/mysql/mysql-js/archive/2014-10-06.tar.gz
```


SUPPORTED DATABASES AND CONNECTION PROPERTIES
---------------------------------------------
MySQL-JS provides a common data management API over a variety of back-end
database connections.  Two database adapters are currently supported.
The *mysql* adapter provides generic support for any MySQL database,
based on all-JavaScript mysql connector node-mysql.
The *ndb* adapter provides optimized high-performance access to MySQL Cluster
using the NDB API.

Each backend adapter supports its own set of connection properties.
+ [MySQL Connection Properties](Backend-documentation/mysql.md)
+ [NDB Connection Properties](Backend-documentation/ndb.md)


SESSION
-------
The central concept of mysql-js is the *session* class.  A session provides
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
+ *find()* Find an instance in the database using a primary or unique key.
++ find(Constructor, keys, [callback], [...])
++ find(Projection, keys, [callback], [...])
++ find(tableName, keys, [callback], [...])
+ *load(instance, [callback], [...])* Loads a specific instance from the database 
based on the primary or unique key present in the object.
+ *persist()* Insert an instance into the database.
++ persist(instance, [callback], [...])
++ persist(Constructor, values, [callback], [...])
++ persist(tableName, values, [callback], [...])
+ *remove()* Delete an instance by primary or unique key.
++ remove(instance, [callback], [...])
++ remove(Constructor, keys, [callback], [...])
++ remove(tableName, keys, [callback], [...])
+ *update()* Update an instance by primary or unique key without necessarily retrieving it.
++ update(instance, [callback], [...])
++ update(Constructor, keys, values, [callback], [...])
++ update(tableName, keys, values, [callback], [...])
+ *save()* Write an object to the database without checking for existence; could result in either an update or an insert.
++ save(instance, [callback], [...])
++ save(Constructor, values, [callback], [...])
++ save(tableName, values, [callback], [...])
+ *createQuery()* Create an object that can be used to query the database
++ createQuery(instance, [callback], [...])
++ createQuery(Constructor, [callback], [...])
++ createQuery(tableName, [callback], [...]) 
+ *getMapping()* Resolve and fetch mappings for a table or class
++ getMapping(object, [callback], [...])
++ getMapping(Constructor, [callback], [...])
++ getMapping(tableName, [callback], [...])
+ *close([callback], [...])* Close the current session

The following methods are *immediate*:
+ createBatch().  Returns a batch.
+ listBatches().  Returns an array of batches.
+ isClosed().  Returns boolean.
+ isBatch(). Returns boolean.
+ currentTransaction().  Returns a Transaction.

See the [Complete documentation for Session](API-documentaiton/Session)


PROMISES
--------
The majority of the asynchronous API methods in mysql-js return a
<a href="promisesaplus.com">Promises/A+ compatible promise</a>.


NOSQL
-----

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

See the [Complete documentation for the top-level API](API-documentation/Mynode)


MAPPED JAVASCRIPT OBJECTS
-------------------------

 describe mapping
 put an table mapping code example
 link to table mapping docs
 

CONVERTERS
----------


BATCHES
-------


TRANSACTIONS
------------


QUERIES
-------


SESSIONFACTORY
--------------





