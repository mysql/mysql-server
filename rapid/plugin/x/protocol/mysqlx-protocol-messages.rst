.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Messages
========

Message Structure
-----------------

Messages have a

* 4 byte *length* (little endian)
* 1 byte *message type*
* a ``message_payload`` of length ``.length - 1``

.. c:type:: Mysqlx.Message

  Container of all messages that are exchanged between client and server.

  :param length: length of the whole message
  :param message_type: type of the ``message_payload``
  :param message_payload: the message's payload encoded using `Google Protobuf <https://code.google.com/p/protobuf/>`_
    if not otherwise noted.

  .. code-block:: c

    struct {
      uint32          length;
      uint8           message_type;
      opaque          message_payload[Message.length - 1];
    } Message;

.. note::
  The ``message_payload`` is generated from the protobuf files using ``protoc``:

  .. code-block:: sh

    $ protoc --cpp_out=protodir mysqlx*.proto

  * :download:`mysqlx.proto`
  * :download:`mysqlx_connection.proto`
  * :download:`mysqlx_session.proto`
  * :download:`mysqlx_crud.proto`
  * :download:`mysqlx_sql.proto`
  * :download:`mysqlx_resultset.proto`
  * :download:`mysqlx_expr.proto`
  * :download:`mysqlx_datatypes.proto`
  * :download:`mysqlx_expect.proto`
  * :download:`mysqlx_notice.proto`

.. note::

  The ``message_type`` can be taken from the :protobuf:msg:`Mysqlx::ClientMessages` for client-messages and
  from :protobuf:msg:`Mysqlx::ServerMessages` of server-side messages.

  In ``C++`` they are exposed in ``mysqlx.pb.h`` in the ``ClientMessages`` class.

  .. code-block:: c

    ClientMessages.MsgCase.kMsgConGetCap
    ClientMessages.kMsgConGetCapFieldNumber

Message Sequence
----------------

Messages usually appear in a sequence.  Each initial message (one referenced by :protobuf:msg:`Mysqlx::ClientMessages`) is associated with a set of possible following messages.

A message sequence either

* finishes successfully if it reaches its end-state or
* is aborted with a `Error Message`_

At any time in between local `Notices`_ may be sent by the server as part of the message sequence.

Global `Notices`_ may be sent by the server at any time.

Common Messages
---------------

Error Message
.............

After the client sent the initial message, the server may send a :protobuf:msg:`Mysqlx::Error` message
at any time to terminate the current message sequence.

.. autopackage:: Mysqlx

Notices
.......

.. seealso:: :doc:`mysqlx-protocol-notices`

The server may send :doc:`mysqlx-protocol-notices` :protobuf:msg:`Mysqlx.Notice::Frame` to the client at any time.

A notice can be

* global (``.scope == GLOBAL``) or
* belong to the currently executed `Message Sequence`_ (``.scope == LOCAL + message sequence is active``):

.. note::

  if the Server sends a ``LOCAL`` notice while no message sequence is active, the Notice should
  be ignored

.. autopackage:: Mysqlx.Notice

Connection
----------

.. autopackage:: Mysqlx.Connection

Session
-------

.. autopackage:: Mysqlx.Session

Expectations
------------

.. seealso:: :doc:`mysqlx-protocol-expect`

.. autopackage:: Mysqlx.Expect

CRUD
----

The CRUD operations work in a similar fashion as the SQL statements
below:

* prepare the CRUD operation
* execute the operation
* get the description of the result
* fetch the rows in batches
* close the prepared operation

.. uml::

  client -> server: PrepareFind
  server --> client: PreparedStmt::PrepareOk
  ...
  client -> server: PreparedStmt::Execute
  server --> client: result
  ...
  client -> server: Cursor::FetchResultset
  server --> client: result
  ...
  client -> server: PreparedStmt::Close
  server --> client: Ok

.. autopackage:: Mysqlx.Crud

SQL
---

* prepare statement for execution
* execute the statement
* get description of the rows
* fetch the rows in batches
* close the prepared operation

.. note::

  As the ``stmt-id`` and ``cursor-id`` is assigned by the client, the client
  can pipeline the messages and assume that all the steps succeed. In case one
  command creates an error, all following commands should fail too and therefore
  it is possible to relate the errors to the right messages.

.. uml::

  client -> server: Sql::StmtPrepare
  server --> client: PreparedStmt::PrepareOk
  ...
  client -> server: PreparedStmt::ExecuteIntoCursor
  server --> client: result
  ...
  client -> server: Cursor::FetchResultset
  server --> client: result
  ...
  client -> server: PreparedStmt::Close
  server --> client: Ok

.. autopackage:: Mysqlx.Sql

Resultsets
----------

.. autopackage:: Mysqlx.Resultset

Expression
----------

.. autopackage:: Mysqlx.Expr

Datatypes
---------

.. autopackage:: Mysqlx.Datatypes

