.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Use-Cases
=========

Prepared Statements with single round-trip
------------------------------------------

In the `MySQL C/S Protocol`_ a ``PREPARE``/``EXECUTE`` cycle required two round-trips as
the ``COM_STMT_EXECUTE`` requires data from the ``COM_STMT_PREPARE-ok`` packet.

.. uml::

  group tcp packet
  Client -> Server: COM_STMT_PREPARE(...)
  end
  group tcp packet
  Server --> Client: COM_STMT_PREPARE-ok(**stmt_id=1**)
  end
  group tcp packet
  Client -> Server: COM_STMT_EXECUTE(**stmt_id=1**)
  end
  group tcp packet
  Server --> Client: Resultset
  end

The Mysqlx Protocol is designed in a way that the ``EXECUTE`` stage doesn't
depend on the response of the ``PREPARE`` stage.

.. uml::

  group tcp packet
  Client -> Server: Sql.PrepareStmt(**stmt_id=1**, ...)
  end
  group tcp packet
  Server --> Client: Sql.PreparedStmtOk
  end
  group tcp packet
  Client -> Server: Sql.PreparedStmtExecute(**stmt_id=1**, ...)
  end
  group tcp packet
  Server --> Client: Sql.PreparedStmtExecuteOk
  end

That allows the client to send both ``PREPARE`` and ``EXECUTE`` after each
other without waiting for the server's response.

.. uml::

  group tcp packet
  Client -> Server: Sql.PrepareStmt(**stmt_id=1**, ...)
  Client -> Server: Sql.PreparedStmtExecute(**stmt_id=1**, ...)
  end

  group tcp packet
  Server --> Client: Sql.PreparedStmtOk
  Server --> Client: Sql.PreparedStmtExecuteOk
  end

.. note::

  see the :doc:`mysqlx-protocol-implementation` about how to efficiently implement
  pipelining.

Streaming Inserts
-----------------

When inserting a large set of data (data import) there has to be made a trade-off betweeen

* memory usage on client and server side
* network round-trips
* error reporting

For this example it is assumed that 1 million rows, each 1024 bytes in size have to be
transfered to the server.

Optimizing for network round-trips
..................................

(Assuming the `MySQL C/S Protocol`_ in this case) Network round-trips can be
minimized by creating a huge SQL statements of up to 1Gbyte in chunks of
16Mbyte (the protocol's maximum frame size).


.. code-block:: sql

  INSERT INTO tbl VALUES
    ( 1, "foo", "bar" ),
    ( 2, "baz", "fuz" ),
    ...;

and sending it over the wire and letting the server execute it.

* the client can generate the SQL statement in chunks of 16Mbyte and write them to the network
* *(memory usage)* the server waits until the full 1GByte is received
* *(execution delay)* before it can start parsing and executing it
* *(error-reporting)* in case an error (parse-error, duplicate key error, ...) the whole 1Gbyte message
  will be denied without any good way to know where the error in that big message
  happened.

The *Execution Time* for inserting all rows in one batch is::

  1 * RTT +
  (num_rows * Row Size / Network Bandwidth) +
  num_rows * Row Parse Time +
  num_rows * Row Execution Time

Optimizing for memory usage and error-reporting
...............................................

The other extreme is using single row ``INSERT`` statements.

.. code-block:: sql

  INSERT INTO tbl VALUES
    ( 1, "foo", "bar" );

  INSERT INTO tbl VALUES
    ( 2, "baz", "fuz" );

    ...

* client can generate statements as it receives data
* streams it to the server
* *(execution delay)* server starts executing statements as soon as it receives the first row
* *(memory usage)* server only has to buffer a single row
* *(error-reporting)* if inserting one row fails, the client knows about it when it happens
* as each statement results in its own round-trip, the network-latency is applied for each row instead of once
* each statement has to be parsed and executed in the server

Using Prepared Statements solves the last bullet point:

.. uml::

  Client -> Server: prepare("INSERT INTO tbl VALUES (?, ?, ?)")
  Server --> Client: ok(stmt=1)
  Client -> Server: execute(1, [1, "foo", "bar"])
  Server --> Client: ok
  Client -> Server: execute(1, [2, "baz", "fuz"])
  Server --> Client: ok

The *Execution Time* for inserting all rows using prepared statements and the `MySQL C/S Protocol`_ is::

  num_rows * RTT +
  (num_rows * Row Size / Network Bandwidth) +
  1 * Row Parse Time +
  num_rows * Row Execution Time

Optimizing for execution time and error-reporting
.................................................

In the Mysqlx Protocol pipelining can be utilized stream messages
to the server while the server is executing the previous message.

.. uml::

  group tcp packet
  Client -> Server: Sql.PrepareStmt(stmt_id=1, ...)
  Client -> Server: Sql.PreparedStmtExecute(stmt_id=1, values= [ .. ])
  Client -> Server: Sql.PreparedStmtExecute(stmt_id=1, values= [ .. ])
  Client -> Server: Sql.PreparedStmtExecute(stmt_id=1, values= [ .. ])
  end
  note over Client, Server
  data too large to be merged into one TCP packet
  end note

  group tcp packet
  Server --> Client: Sql.PreparedStmtOk
  Server --> Client: Sql.PreparedStmtExecuteOk
  Server --> Client: Sql.PreparedStmtExecuteOk
  end

  group tcp packet
  Client -> Server: Sql.PreparedStmtExecute(stmt_id=1, values= [ .. ])
  Client -> Server: Sql.PreparedStmtExecute(stmt_id=1, values= [ .. ])
  end

  group tcp packet
  Server --> Client: Sql.PreparedStmtExecuteOk
  Server --> Client: Sql.PreparedStmtExecuteOk
  Server --> Client: Sql.PreparedStmtExecuteOk
  end

The *Execution Time* for inserting all rows using prepared statements and using pipelining is (assuming that the network is
not saturated)::

  1 * RTT +
  (1 * Row Size / Network Bandwidth) +
  1 * Row Parse Time +
  num_rows * Row Execution Time

* ``one`` *network latency* to get the initial ``prepare``/``execute`` across the wire
* ``one`` *network bandwith* to get the initial ``prepare``/``execute`` across the wire. All further commands arrive at the server
  before the executor needs them thanks to pipelining.
* ``one`` *row parse time* to parse the ``prepare``
* ``num_rows`` *row execution time* stays as before

In case *error reporting* isn't a major topic one can combine ``multi-row INSERT`` with pipelining
and reduce the per-row network overhead. This is important in case the network is saturated.

SQL with multiple resultsets
----------------------------

.. uml::

  group prepare
  Client -> Server: Sql.PrepareStmt(stmt_id=1, "CALL multi_resultset_sp()")
  Server --> Client: Sql.PrepareStmtOk()
  end
  group execute
  Client -> Server: Sql.PreparedStmtExecute(stmt_id=1, cursor_id=1)
  Server --> Client: Sql.PreparedStmtExecuteOk()
  end
  group fetch rows
  Client -> Server: Cursor::FetchResultset(cursor_id=1)
  Server --> Client: Resultset::ColumnMetaData
  Server --> Client: Resultset::Row
  Server --> Client: Resultset::Row
  Server --> Client: Resultset::DoneMoreResultsets
  end
  group fetch last resultset
  Client -> Server: Cursor::FetchResultset(cursor_id=1)
  Server --> Client: Resultset::ColumnMetaData
  Server --> Client: Resultset::Row
  Server --> Client: Resultset::Row
  Server --> Client: Resultset::Done
  end
  group close cursor
  Client -> Server: Cursor::Close(cursor_id=1)
  Server --> Client: Cursor::Ok
  end

Inserting CRUD data in a batch
------------------------------

Inserting multiple documents into a collection ``col1``.

#. prepare the insert
#. pipeline the execute messages

.. uml::

  Client -> Server: Crud::PrepareInsert(stmt_id=1, Collection(name="col1"))
  Server --> Client: PreparedStmt::PrepareOk
  loop
  Client -> Server: PreparedStmt::Execute(stmt_id=1, values=[ doc ])
  Server --> Client: PreparedStmt::ExecuteOk
  end loop
  Client -> Server: PreparedStmt::Close(stmt_id=1)
  Server --> Client: PreparedStmt::CloseOk


By utilizing pipelining the ``execute`` message can be batched without waiting for
the corresponding ``executeOk`` message to be returned.

Cross-Collection Update/Delete
------------------------------

Deleting documents from collection ``col2`` based on data from collection ``col1``.

Instead of fetching all rows from ``col1`` first to construct a big ``delete`` message
it can also be run in nested loop:

.. code-block:: python

  Crud.PrepareDelete(stmt_id=2, Collection(name="col2"), filter={ id=? })
  Crud.PrepareFind(stmt_id=1, Collection(name="col1"), filter={ ... })

  Sql.PreparedStmtExecute(stmt_id=1, cursor_id=2)

  while ((rows = Sql.CursorFetch(cursor_id=2))):
    Sql.PreparedStmtExecute(stmt_id=2, values = [ rows.col2_id ])

  Sql.PreparedStmtClose(stmt_id=2)
  Sql.PreparedStmtClose(stmt_id=1)


.. uml::

  Client -> Server: Crud::PrepareFind(stmt_id=1, filter=...)
  Client -> Server: Crud::PrepareDelete(stmt_id=2, filter={ id=? })
  Server --> Client: PrepareStmt::PrepareOk
  Server --> Client: PrepareStmt::PrepareOk
  Client -> Server: PreparedStmt::ExecuteIntoCursor(stmt_id=1, cursor_id=2)
  Server --> Client: PreparedStmt:::ExecuteOk
  Client -> Server: Cursor::FetchResultset(cursor_id=2, limit=batch_size)
  loop batch_size
  Server --> Client: Resultset::Row
  Client -> Server: PreparedStmt::Execute(stmt_id=2, values=[ ? ])
  break
  alt
  Server --> Client: Resultset::Suspended
  else
  Server --> Client: Resultset::Done
  end alt
  end break
  end loop
  loop batch_size
  Server --> Client: PreparedStmt::ExecuteOk
  end loop

.. _`MySQL C/S Protocol`: http://dev.mysql.com/doc/internals/en/client-server-protocol.html
