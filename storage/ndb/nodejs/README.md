MySQL Cluster NoSQL API for Node.JS
===================================

INTRODUCTION
------------
The NoSQL API for Node.JS provides a lightweight domain object model for 
JavaScript.  You write code using common JavaScript objects, and use simple
API calls to read and write those objects to the database, like this:

```
 var nosql = require("mysql-js");
 
 function onSession(err, session) {
   var data = new Tweet(username, message);
   session.persist(data);
 }

 nosql.openSession(onSession);
```

More sample code is available in the samples/ directory.

 The API includes two backend adapters:

  - The "ndb" adapter, which uses the native NDB API to provide
    high-performance native access to MySQL Cluster. 

  - The "mysql" adapter, which connects to any MySQL Server using the node-mysql 
    driver, available from https://github.com/felixge/node-mysql/


REQUIREMENTS
------------
Building the ndb backend requires an installed copy of MySQL Cluster 7.x 
including  headers and shared library files.  The MySQL architecture must match
the node architecture: if node.js is 64-bit, then MySQL must also be 64-bit.  If 
node is 32-bit, MySQL must be 32-bit.

The mysql backend requires version 2.0 of node-mysql, an all-JavaScript 
MySQL client.

### WINDOWS REQURIREMENTS LIST ###
1. Microsoft Visual Studio
2. MySQL Cluster
3. Python 2.6 or 2.7
4. Node.JS
5. node-gyp 



BUILDING
--------
The installation script is interactive.  It will try to locate a suitable copy 
of MySQL, and it will prompt you to accept its choice or enter an alternative.

* To build the module in place, type:
    ```node configure.js```

* After configuring, build a binary.  The -d argument to node-gyp makes it a 
"debug" binary
    ```node-gyp configure build -d``` 

* After testing the debug binary, on platforms other than Windows, it is 
possible to build an optimized (non-debug) binary.  *Non-debug builds are 
generally not possible on Windows, and usually result in link-time errors.* 
    ```node-gyp rebuild```


DIRECTORY STRUCTURE
-------------------
<dl compact>
 <dt> API-documentation  <dd>      Documentation of the main API
 <dt> samples/           <dd>      Sample code
 <dt> setup/             <dd>      Scripts used to build and install the adapter
 <dt> test/              <dd>      Test driver and test suite
 <dt> Adapter/           <dd>      The node.js adapter
 <dt> Adapter/api        <dd>      Implementation of the top-level API
 <dt> Adapter/impl       <dd>      Backend implementations
</dl>


TESTING
-------
The MySQL server must have the database "test" created.  The test infrastructure
currently relies on a mysql command-line client for creating and dropping tables,
so the "mysql" executable must be in your command path.

To configure the servers and ports used for testing, edit test/test_connection.js

To run the test suite using the native NDB adapter:
```
    cd test
    node driver 
```

To run the test suite using the MySQL adapter:
```
   cd test
   node driver --adapter=mysql/ndb
   node driver --adapter=mysql/innodb
```


FOR MORE INFORMATION
--------------------
See the issues and other information at http://github.com/mysql/mysql-js/

