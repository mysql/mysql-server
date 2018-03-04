Jones-NDB
=========

Introduction
------------
Jones-NDB is the Database Jones service provider for MySQL Cluster.

Jones-NDB uses C++ native code to link with the NDB API and provide direct, 
high-performance access to MySQL Cluster. A Node.js process running jones-ndb
will join the cluster as an API node. All data operations are executed with
direct communication between this API node and the NDB data nodes where the 
data is stored.  The MySQL node of the cluster is not used for data operations,
only for metadata operations such as CREATE and DROP table.

In order to support these metadata operations, Jones-Ndb relies on [Jones-MySQL](../jones-mysql) as a dependency.


Building Jones-NDB
------------------
The C++ native code component of Jones-NDB must be built before it can be used
by JavaScript.  This component shared library file object file is named
ndb_adapter.node.  Jones-NDB is built with node-gyp, the Node.JS build tool.
You can download and install node-gyp via npm:
  + `npm install -g node-gyp`

You can build the adapter by running the following commands.
  + Change to the top-level "jones-ndb" directory:
    + `cd path-to/jones/jones-ndb`

+ The configure script is an interactive program that will prompt you to enter the path to an installed version of MySQL Cluster that includes NDB API header files and shared libraries.  It supports tab-completion for pathname entry. Choose the path that includes subdirectories: bin, docs, include, and lib. The product of the configure script is a config.gypi file for use by node-gyp.
    + `node configure`
+  In the next step, node-gyp will create the build environment.
    + `node-gyp configure`
+  Build a *Release* (or, with the -d flag, a *Debug*) version of ndb_adapter.node.  Note that on some Windows platforms the Release build will not run and therefore a Debug build is required. 
    + `node-gyp build` or `node-gyp build -d`

Running and Testing Jones-NDB 
-----------------------------

### Setting the run-time library load path

The NDB adapter has a run-time dependency on the NDB API library, libndbclient. 
On some platforms this dependency can only be satisfied when an environment
variable points to the appropriate directory: LD_LIBRARY_PATH on most Unix
derivatives, but DYLD_LIBRARY_PATH on Mac OS X. This is the path that ends with: /lib.

### Configuring a simple MySQL Cluster

Testing Jones-NDB requires, at minimum, a simple MySQL Cluster containing a management node, a MySQL node, and at least one NDB data node.  A standard mysql distribution contains several simple ways to quickly configure such a cluster.

#### Using the NDB Memcache Sandbox environment

The sandbox.sh script in the Memcache distribution can be used to start and stop a cluster with an NDB management server and MySQL server running on their default ports and single NDB data node.
From the MySQL base directory:
+ `cd share`
+ `cd memcache-api`
+ `sh sandbox.sh start`
+ To shut down this cluster, use `sh sandbox.sh stop`
+ The data in this environment is stored under share/memcache-api/sandbox and will persist from one run to the next.

#### Using the mysql-test environment

MySQL's test tool, `mysql-test-run`, can also be used to start a simple cluster.  In this configuration, ndb_mgmd and mysqld will listen on non-standard ports. mysql-test-run will assign these port numbers dynamically at run-time, but in most cases we will find a management server on port 13000 and a MySQL server at port 13001.  
Jones connection properties are commonly managed using a jones_deployments.js file.  The standard [jones_deployments.js](../jones_deployments.js) includes a deployment named "mtr" configured especially for the cluster created by mysql-test-run.
From the MySQL base directory:
+ `cd mysql-test`
+ `./mtr --start ndb.ndb_basic`
+ To shut down this cluster, use ctrl-c.
+ The data in this environment is stored in mysql-test/var and is normally deleted after every test run.


#### MySQL Cluster in Production

The best way to manage a production MySQL Cluster is using "MCM",
[MySQL Cluster Manager](http://www.mysql.com/products/cluster/mcm/).


### Running the test suite

To test that jones-ndb is fully installed:
+ `cd test`
+ By default, jones-ndb looks for ndb_mgmd and mysqld servers at their default ports on the local machine.  If this is the case, simply type `node driver`
+ If you are using mysql-test-run, use the `-E` option to select the mtr deployment:  `node driver -E mtr`
+ For some other configuration, define a deployment with appropriate connection properties in [jones_deployments.js](../jones_deployments.js) and use it:  `node driver -E my_test_deployment`

#### Test results

The final output from a succesful test run should look something like this:

```
Adapter:  ndb
Elapsed:  11.634 sec.
Started:  627
Passed:   625
Failed:   0
Skipped:  2
```


NDB Connection Properties
-------------------------
Each Jones Service Provider supports a different set of connection properties, based on the data source it supports.  These properties, and their default values, are documented in the file [DefaultConnectionProperties.js](DefaultConnectionProperties.js)






