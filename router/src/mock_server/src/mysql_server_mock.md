# MySQL Server Mock {#PAGE_MYSQL_SERVER_MOCK}

The MySQL Server Mock

* speaks (a subset of) the MySQL Client/Server protocol
* knows how to

  * authenticate
  * respond to statements send by the client with

    * resultsets
    * status (Ok, last-insert-id, ...)
    * Error

It reads the protocol trace from a JSON file which contains:

* sequence of statements to expect
* responses to send
* extra context information like execution-times

## Motivation

### Testing in isolation

Testing MySQL InnoDB Cluster components like

* MySQL Shell
* MySQL Router
* Connectors

involves:

1. initialize a MySQL Server via the MySQL Shell `dba.deploySandboxInstance`

  * which runs `mysqld --initialize`

2. use the MySQL Shell to

  * builds a cluster with ``dba.createCluster``
  * add more nodes with ``cluster.addInstance``

  (10-20 seconds).

4. execute the statements to test the right behaviour (100ms)
5. shut down the whole setup again (1s)

The setup and shutdown costs (20s) outscale the actual
test runtime (100ms) by a great margin.

### Mocking the MySQL Server

On the other side all the components speak to the MySQL Server
over the MySQL %Protocol only. Statements are sent, responses
are received. What the MySQL Server did inbetween to generate
the response isn't visible from the outside.

There is no difference from the point of view of the clients
if the response comes from a genuine MySQL Server or from
another program that returns the same bytes over the wire.

## Simple Demo

A simple trace-file:

```{.json}
{"stmts": [
  {
    "stmt": "select @@version_comment limit 1",
    "result": {
      "columns": [{
        "type": "STRING",
        "name": "@@version_comment"
      }],
      "rows": [
        ["ImAMock"]
      ]}
  },
  {
    "stmt": "select USER()",
    "result": {
      "columns": [{
        "type": "STRING",
        "name": "USER()"
      }],
      "rows": [
        ["mock"]
      ]
    }
  }]
}
```

Starting the mock:

```
$ ./mysql_server_mock ./simple.json 5500
```

Use a client to talk to the mock:

```
$ mysql --port=5500
Server version: 8.0.0-mock ImAMock

[...]

mock@localhost:5500 (none)>
```

# Design Goals

## Allow Faster Testing

When using a MySQL Server the execution time of a statement is driven
by the system that is used (storage-io, network-io, cpu speed, ...).

In cases where the execution time doesn't affect the behaviour
(timeout testing, ...) the test setup could pretend the system
has zero latency (storage in RAM, super fast CPUs, local networks).

It would be required to:

* capture the statement exchanged between client and server
* provide a MySQL Server mock which

  * speaks the MySQL %Protocol
  * listens on a TCP port
  * returns the predefined results depending on the statement as a MySQL Server would.

## More Error Cases

By using the Server Mock it is easier to create error-cases that
are hard to create in real setups:

* disk full
* server dead
* nodes out of sync
* ...

## Less options for failure

By using the MySQL Server Mock in the Router tests, the tests don't need
the rely on installing

* MySQL Shell
* MySQL Server

which involves:

* ensure the right version
* check they actually work on the target
* using the right paths

# MySQL Router Demo

## Bootstrapping a router

To bootstrap a router, the router expects to talk to a MySQL Server. It:

* logs in with `root` and some password
* executes 15 statements
* writes ``mysqlrouter.conf``

Start the "MySQL Server" mock on port ``5050`` with the ``bootstrapper.yaml``

    $ ./mysql_server_mock --mysqld-port=5050 --stmt-file=./bootstrapper.yaml

Let the router do the bootstrap

    $ mysqlrouter --bootstrap localhost:5050 -d router-conf
    Bootstrapping MySQL Router instance at <stripped>/rounter-conf...
    MySQL Router  has now been configured for the InnoDB cluster 'test'.

    The following connection information can be used to connect to the cluster.

    Classic MySQL protocol connections to cluster 'test':
    - Read/Write Connections: localhost:6446
    - Read/Only Connections: localhost:6447

    X protocol connections to cluster 'test':
    - Read/Write Connections: localhost:64460
    - Read/Only Connections: localhost:64470

The mock will log:

    2017-01-24 12:24:11,551 MAIN INFO: listening on 5050
    2017-01-24 12:24:23,357 MAIN INFO: accepted connection from ('127.0.0.1', 38684)
    2017-01-24 12:24:23,358 MAIN DEBUG: received: SELECT * FROM mysql_innodb_cluster_metadata.schema_version
    2017-01-24 12:24:23,358 MAIN DEBUG: sending Result
    2017-01-24 12:24:23,395 MAIN DEBUG: received: SELECT  ((SELECT count(*) FROM ...


After the bootstrap is finished the bootstrap-mysql-server-mock can be stopped.

## Running the router

Using the bootstrapped config:

* the metadata store is expected on port ``5500``
* the data store is expected on port ``5100``

Start the metadata-store

    $ ./mysql_server_mock --mysqld-port=5500 --stmt-file=./metadata-store.yaml

Start the data-store

    $ ./mysql_server_mock --mysqld-port=5100 --stmt-file=./group-replication.yaml

Start the router

    $ mysqlrouter --config router-conf/mysqlrouter.conf

The router log file says:

    2017-01-24 12:28:30 INFO    [7fc57d7eb700] [routing:test_default_ro] started: listening on 0.0.0.0:6447; read-only
    2017-01-24 12:28:30 INFO    [7fc57dfec700] Starting Metadata Cache
    2017-01-24 12:28:30 INFO    [7fc57cfea700] [routing:test_default_rw] started: listening on 0.0.0.0:6446; read-write
    2017-01-24 12:28:30 INFO    [7fc577fff700] [routing:test_default_x_rw] started: listening on 0.0.0.0:64460; read-write
    2017-01-24 12:28:30 INFO    [7fc5747e9700] [routing:test_default_x_ro] started: listening on 0.0.0.0:64470; read-only
    2017-01-24 12:28:30 INFO    [7fc57dfec700] Connected with metadata server running on 127.0.0.1:5500
    2017-01-24 12:28:30 INFO    [7fc57dfec700] Changes detected in cluster 'test' after metadata refresh
    2017-01-24 12:28:30 INFO    [7fc57dfec700] Metadata for cluster 'test' has 1 replicasets:
    2017-01-24 12:28:30 INFO    [7fc57dfec700] 'default' (3 members, single-primary)
    2017-01-24 12:28:30 INFO    [7fc57dfec700]     localhost:5100 / 50000 - role=HA mode=RW
    2017-01-24 12:28:30 INFO    [7fc57dfec700]     localhost:5110 / 50100 - role=HA mode=RO
    2017-01-24 12:28:30 INFO    [7fc57dfec700]     localhost:5120 / 50200 - role=HA mode=RO
    2017-01-24 12:28:30 INFO    [7fc5757fa700] Connected with metadata server running on 127.0.0.1:5500

The ``metadata-store`` mock outputs:

    2017-01-24 12:28:10,322 MAIN INFO: listening on 5500
    2017-01-24 12:28:20,774 MAIN INFO: accepted connection from ('127.0.0.1', 53278)
    2017-01-24 12:28:20,775 MAIN DEBUG: received: SELECT R.replicaset_name, I.mysql_server_uuid, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';
    2017-01-24 12:28:20,775 MAIN DEBUG: sending Result

The ``group-replication`` mock outputs:

    2017-01-24 12:28:27,851 MAIN INFO: listening on 5100
    2017-01-24 12:28:30,755 MAIN INFO: accepted connection from ('127.0.0.1', 41520)
    2017-01-24 12:28:30,755 MAIN DEBUG: received: show status like 'group_replication_primary_member'
    2017-01-24 12:28:30,755 MAIN DEBUG: sending Result
    2017-01-24 12:28:30,795 MAIN DEBUG: received: SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'
    2017-01-24 12:28:30,795 MAIN DEBUG: sending Result
    2017-01-24 12:28:30,835 MAIN INFO: closed connection to ('127.0.0.1', 41520)

## Mock Configuration

The behaviour of the ``mysql_server_mock`` is driven by

* JSON Tracefiles
* Javascript files

They describe what the mock shall respond with when it receives:

* a handshake
* a sequence of statements

### JSON Tracefiles

The structure of the JSON Tracefiles is defined in JSON-schema
and can be seen in the source at src/mock_server/src/mysql_server_mock_schema.js

### Javascript

The Javascript backed files for the mock are more powerful then the
static JSON Tracefiles as it:

* allows to interface with the REST API of the ``mysql_server_mock``
* allows "stmts" to be handled at runtime via javascript functions
