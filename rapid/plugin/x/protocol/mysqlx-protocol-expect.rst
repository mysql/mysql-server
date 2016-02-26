.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Expectations
============

With the use of pipelining in the Mysqlx Protocol (sending messages
without waiting for successful response) only so many messages
can be pipelined without causing havoc if one of the pipelined, dependent
messages fails:

.. code-block:: javascript

  Mysqlx.Crud::PrepareFind(stmt_id=1, ...) // may fail
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // would fail implicitly as stmt_id=1 doesn't exist
  Mysqlx.PreparedStmt::Close(stmt_id=1) // would fail implicitly as stmt_id=1 doesn't exist

While implicitly failing is one thing, there are situations where it isn't
that obvious what will happen:

.. code-block:: javascript

  Mysqlx.Crud::PrepareInsert(stmt_id=1, ...) // ok
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // ok
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // duplicate key error
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // what now? abort the insert? ignore?
  Mysqlx.PreparedStmt::Close(stmt_id=1) // close the stmt_id

Setting Expectations
--------------------

Expectations let statements fail reliably until the end of the block.

Assume the :protobuf:msg:`Mysqlx.Crud::PrepareFind` fails:

* don't execute the :protobuf:msg:`Mysqlx.PreparedStmt::Execute`
* don't try to close the stmt

::

  Mysqlx.Expect::Open([+no_error])
  Mysqlx.Crud::PrepareFind(stmt_id=1, ...) // may fail
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // expectation(no_error) failed
  Mysqlx.PreparedStmt::Close(stmt_id=1) // expectation(no_error) failed
  Mysqlx.Expect::Close()

But this would also skip the close if execute fails. Not what we want.
Adding another Expect-block handles it:

::

  Mysqlx.Expect::Open([+no_error])
  Mysqlx.Crud::PrepareFind(stmt_id=1, ...) // may fail
  Mysqlx.Expect::Open([+no_error])
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // expectation(no_error) failed
  Mysqlx.Expect::Close()
  Mysqlx.PreparedStmt::Close(stmt_id=1) // expectation(no_error) failed
  Mysqlx.Expect::Close()

With these expectations pipelined, the server will handle errors
in a consistent, reliable way.

It also allows to express how a streaming insert would behave if one
of the inserts fails (e.g. duplicate key error, disk full, ...):

Either fail at first error::

  Mysqlx.Expect::Open([+no_error])
  Mysqlx.Crud::PrepareInsert(stmt_id=1, ...) // ok
  Mysqlx.Expect::Open([+no_error])
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // ok
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // duplicate_key error
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // expectation(no_error) failed
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // expectation(no_error) failed
  Mysqlx.Expect::Close()
  Mysqlx.PreparedStmt::Close(stmt_id=1) // ok
  Mysqlx.Expect::Close()

Or ignore error and continue::

  Mysqlx.Expect::Open([+no_error])
  Mysqlx.Crud::PrepareInsert(stmt_id=1, ...) // ok
  Mysqlx.Expect::Open([-no_error])
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // ok
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // duplicate_key error
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // ok
  Mysqlx.PreparedStmt::Execute(stmt_id=1, ...) // ok
  Mysqlx.Expect::Close()
  Mysqlx.PreparedStmt::Close(stmt_id=1) // expectation(no_error) failed
  Mysqlx.Expect::Close()

Behaviour
---------

A Expectation Block

* encloses client messages
* has a Condition Set
* has a parent Expectation Block
* can inherit a Condition Set from the parent Expectation Block or start with a empty Condition Set
* fails if one of the Conditions fails while the Block is started or active
* fails if one of the Conditions isn't recognized or not valid

A Condition Set

* has a set of Conditions
* allows to set/unset Conditions

A Condition

* has a key and value
* key is integer
* value format depends on the key

If a Expectation Block fails, all following messages of the Expectation block are failing with:

* error-msg: ``Expectation failed: %s``
* error-code: ...

.. todo:: define error-code and error-msg for messages in a failed block.

Conditions
----------

.. warning:: The layout of conditions are subject to change:

  * not all may be implemented yet
  * more conditions may be added

========================= ==========
Condition                 Key
========================= ==========
`no_error`_               1
`schema_version`_         2
`gtid_executed_contains`_ 3
`gtid_wait_less_than_ms`_ 4
========================= ==========


no_error
........

Fail all messages of the block after the first message returning
an error.

Example::

  Mysqlx.Expect::Open([+no_error])
  Mysqlx.Expect::Close()

schema_version
..............

Fail all messages of the block if the schema version for the
collection doesn't match.

.. note:: this is a used by the JSON schema support of the server
  to ensure client and server are in agreement of what schema
  version is *current* as it is currently planned to enforce the
  checks on the client-side.

Example::

  Mysqlx.Expect::Open([+schema_version::`schema`.`collection` = 1])
  Mysqlx.Expect::Close()

gtid_executed_contains
.......................

Fail all messages until the end of the block if the ``@@gtid_executed`` doesn't
contain the set GTID.

.. note::
  used by the *read-your-writes* to ensure another node is already
  up to date.

Example::

  Mysqlx.Expect::Open([+gtid_executed_contains = "..."])
  Mysqlx.Expect::Close()


gtid_wait_less_than_ms
......................

Used in combination with `gtid_executed_contains`_ to wait that the node caught up.

Example::

  Mysqlx.Expect::Open([+gtid_wait_less_than_ms = 1000])
  Mysqlx.Expect::Close()

sql_stateless
.............

Fail any message that executes stateful statements like:

* temporary tables
* user variables
* session variables
* stateful functions (``INSERT_ID()``, ``GET_LOCK()``)
* stateful language features (``SQL_CALC_FOUND_ROWS``)

.. note::

  depending on the implementation stored procedures may be not allowed
  as they may through levels of indirection use stateful SQL features.


