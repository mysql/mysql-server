MySQL Cluster NoSQL API for Node.JS
===================================

INTRODUCTION
------------
The NoSQL API for Node.JS provides lightweight domain object mapping for 
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

 The API can be used with two separate backend adapters:

  - The "ndb" adapter, which uses the C++ NDB API to provide
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


BUILDING
--------
The installation script is interactive.  It will try to locate a suitable copy 
of MySQL, and it will prompt you to accept its choice or enter an alternative.

* To build the module in place, type:
    ```setup/build.sh```
    
* Or, to build and install the module, type:  
    ```npm install . ```
 

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
Testing currently requires a running MySQL Cluster. The MySQL server must have
the database "test" created. The mysql client executable must be in the path.
By default, all servers are on the local machine on their default ports; this 
can be customized by editing test/test_connection.js

To run the test suite:
```
    cd test
    node driver 
```
