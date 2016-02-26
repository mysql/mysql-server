.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

=======
Notices
=======

Notices are a way to send auxillary data from the server to the client that can be:

* global
* local to the current message sequence

Notices don't affect the state of the current message sequence, that means: the client
is may ignore notices and still be able to properly understand the message sequence.

Global Notices
--------------

Global Notices are sent by the server in case of events happen that are unrelated
to the currently active message sequence:

* server is shutting down
* node disconnected from group
* schema or table dropped
* binlog events

Server Shutting Down
....................

The server indicates that it shuts down in a clean way.

Node disconnected from Group
............................

The slave stopped to replicate changes from the master/group and
may contain stale information.

.. note::
  checking for ``SHOW SLAVE STATUS`` and ``io_thread`` and ``sql_thread`` leads
  to a race condition for the following query. One would have to check after query
  of the slave is still running to see if it didn't stop in between.

Schema or Table dropped/altered/...
...................................

If a client maintains a cache of recent queries + resultsets it would improve
the caching behaviour if the client would be notified if a underlying table or
schema was dropped/changed/...

Local Notices
-------------

Local Notices are related to the currently active Message Sequence like:

* Committed Transaction IDs
* Transaction State Changes
* SQL warnings :protobuf:msg:`Mysqlx.Notice::Warning`
* `session variable changed`_ and
  `session state changed`_

Session Variable Changed
........................

Via :protobuf:msg:`Mysqlx.Notice::SessionVariableChanged`

It allows intermediates to track state changes on the clients session
that may be otherwise unnoticable like:

.. code-block:: sql

  CREATE PROCEDURE init() BEGIN
    SET @@sql_mode=ANSI;
  END

Session variable changes are usually done from the client via
``SET @@...`` or ``SELECT @@... := 1``, but can also be done
via:

* stored proceduces
* triggers
* connection setup

  * ``@@character_set_server``

.. note:: part of this functionality is provided in the MySQL C/S Protocol via
  ``WL#4797`` ``SESSION_SYSVAR_TRACKER`` and the initial handshake packet

Session State Changed
.....................

Via :protobuf:msg:`Mysqlx.Notice::SessionStateChanged`

* Account Expired while :doc:`mysqlx-protocol-authentication`
* current schema changes: ``USE ...``
* sever-side generated primary keys (like ``AUTO_INCREMENT``)
* rows-found, rows-matched, rows-affected

CURRENT_SCHEMA
  sent after statement that changes the current schema like ``USE ...``

GENERATED_INSERT_ID
  sent after a ID was created by an INSERT-operation.

  .. note::
    Multiple ``GENERATED_INSERT_ID`` notices may be sent per message
    sequence. Stored Procedures, Multi-Row INSERTs, ..

ROWS_FOUND
  Rows that would be found if ``LIMIT`` wasn't applied (see ``SQL_CALC_FOUND_ROWS``)

ROWS_AFFECTED
  Rows affected by a modifying statement

ROWS_MATCHED
  Rows matched by the criteria of a modifying statement
  (``UPDATE``, ``INSERT``, ``DELETE``, ...)

  .. note::
    ``ROWS_AFFECTED`` and ``ROWS_MATCHED`` where sent in the MySQL C/S Protocol
    as plaintext ``info`` for a ``OK`` packet after an ``UPDATE``::

       Rows matched: 0  Changed: 0  Warnings: 0

ACCOUNT_EXPIRED
  sent after a successful authentication before :protobuf:msg:`Mysqlx.Session::AuthenticateOk`

TRX_COMMITTED
  sent after a transaction was committed. `.value` may contain a transaction identifier.

  .. note::
    used to track implicit, explicit and auto commits.

  .. seealso:: http://dev.mysql.com/doc/en/innodb-implicit-commit.html

TRX_ROLLEDBACK
  sent after a transaction was rolledback.

  .. note::
    used to track implicit and explicit rollbacks.

  .. seealso:: http://dev.mysql.com/doc/en/innodb-implicit-commit.html

SESSION_ID
  sent after a session-id is assigned by the server

.. note::

  The MySQL C/S provided some of this information via functions:

  =================== ================
  Parameter           `Information Functions`__
  =================== ================
  CURRENT_SCHEMA      DATABASE()
  GENERATED_INSERT_ID LAST_INSERT_ID()
  ROWS_FOUND          FOUND_ROWS()
  ROWS_AFFECTED       ROW_COUNT()
  SESSION_ID          CONNECTION_ID()
  =================== ================

.. __: https://dev.mysql.com/doc/en/information-functions.html

.. todo:: specify how Stored Procedures, Triggers, ... and so on leak Notices

