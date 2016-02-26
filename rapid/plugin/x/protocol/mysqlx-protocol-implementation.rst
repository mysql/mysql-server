.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Implementation Notes
====================

Client
------

Out-of-Band Messages
....................

The client should decode the messages it receives from the server in
a generic way and track the possible messages with a state-machine.

.. code-block:: python

  def getMessage(self, message):
    ## handle out-of-band message
    msg = messageFactory(message.type).fromString(message.payload)

    if message.type is Notification:
       notification_queue.add(msg)
       raise NoMessageError()

    if message.type is Notice:
       notice_queue.add(msg)
       raise NoMessageError()

    return msg


Pipelining
..........

The client may send several messages to the server without waiting for
a response for each message.

Instead of waiting for the response to a message like in:

.. uml::

  Client -[#red]> Server: Sql::StmtPrepare
  ...1 second later...
  Server -[#red]-> Client: PreparedStmt::PrepareOk
  Client -[#blue]> Server: PreparedStmt::Execute
  ...1 second later...
  Server -[#blue]-> Client: PreparedStmt::ExecuteOk
  Client -[#green]> Server: Cursor::Fetch
  ...1 second later...
  loop
  Server -[#green]-> Client: Resultset::Row
  end loop
  Server -[#green]-> Client: Resultset::Done

the client can generate its messages and send it to the server without waiting:

.. uml::

  Client -[#red]> Server: Sql::StmtPrepare
  Client -[#blue]> Server: Sql.ExecuteStmt
  Client -[#green]> Server: Sql.CursorFetch
  ...1 second later...
  Server -[#red]-> Client: PreparedStmt::PrepareOk
  Server -[#blue]-> Client: Sql.ExecuteStmtOk
  loop
  Server -[#green]-> Client: Resultset::Row
  end loop
  Server -[#green]-> Client: Sql.ResultsetDone

The client has to ensure that when pipeline messages that in case of an error
the following messages also error out correctly:

.. uml::

  Client -[#red]> Server: Sql::StmtPrepare
  Client -[#blue]> Server: PreparedStmt::ExecuteIntoResultset
  Client -[#green]> Server: Cursor::FetchResultset
  ...1 second later...
  Server -[#red]-> Client: PrepareStmt::PrepareOk
  Server -[#blue]-> Client: Error
  Server -[#green]-> Client: Error

Vectored I/O
............

In network programming it is pretty common the to prefix the message
payload with the header:

* HTTP header + HTTP content
* a pipeline of messages
* message header + protobuf message

.. code-block:: python

  import struct
  import socket

  s = socket.create_connection(( "127.0.0.1", 33060))

  msg_type = 1
  msg_payload = "abc"
  msg_header = struct.pack(">I", len(msg_payload)) +
               struct.pack("B", msg_type)

  ## concat before send
  s.send(msg_header + msg_payload)

  ## multiple syscalls
  s.send(msg_header)
  s.send(msg_payload)

  ## vectored I/O
  s.sendmsg([ msg_header, msg_payload ])

*concat before send* leads to pretty wasteful reallocations and copy operations
if the payload is huge.

*multiple syscalls* is pretty wasteful for small messages as a few bytes only the
whole machinery of copying data between userland and kernel land has to be started.

*vectored io* combines the best of both approaches and sends multiple buffers
to the OS in one syscall and OS can optimize sending multiple buffers in
on TCP packet.

On Unix this is handled by :manpage:`writev(2)`, on Windows exists :manpage:`WSASend()`

.. note::

  Any good buffered iostream implementation should already make use of vectored I/O.

  Known good implementation:

  * Boost::ASIO
  * GIO's GBufferedIOStream

Corking
.......

Further control about how when to actual send data to the other endpoint can be
achieved with "corking":

* linux: ``TCP_CORK`` http://linux.die.net/man/7/tcp
* freebsd/macosx: ``TCP_NOPUSH`` https://www.freebsd.org/cgi/man.cgi?query=tcp&sektion=4&manpath=FreeBSD+9.0-RELEASE

They work in combination with ``TCP_NODELAY`` (aka Nagle's Algorithm).

* http://stackoverflow.com/questions/3761276/when-should-i-use-tcp-nodelay-and-when-tcp-cork?rq=1

Server
------

Pipelining
..........

The protocol is structured in a way that the messages can be decoded
completely without of knowing the state of the message sequence.

If data is available on the network, the server has to:

* read the message
* decode the message
* execute the message

Instead of a synchronous read-execution cycle:

.. uml::

  participant Network
  participant Reader
  participant Executor

  [-> Reader: message ready

  Reader -> Network: receive
  activate Reader
  activate Network
  Network --> Reader: data
  deactivate Network

  Reader -> Reader: decode(data)

  Reader -> Executor: start_execute(msg)
  deactivate Reader
  activate Executor

  Executor -> Executor: execute(msg)
  Executor -> Executor: encode(response_msg)

  [-> Reader: message ready

  Executor -> Network: send(data)
  activate Network
  Network --> Executor: ok
  deactivate Network
  deactivate Executor

  Reader -> Network: receive
  activate Reader
  activate Network
  Network --> Reader: data
  deactivate Network

  Reader -> Reader: decode(data)

  Reader -> Executor: start_execute(msg)
  deactivate Reader
  activate Executor

  Executor -> Executor: execute(msg)
  Executor -> Executor: encode(response_msg)

  Executor -> Network: send(data)
  activate Network
  Network --> Executor: ok
  deactivate Network
  deactivate Executor

the Reader and the Executor can be decoupled into seperate threads:

.. uml::

  participant Network
  participant Reader
  box "Executor Thread"
  participant ExecQueue
  participant Executor
  end box

  [-> Reader: message ready

  Executor -> ExecQueue: wait_for_msg
  activate Executor

  Reader -> Network: receive
  activate Reader
  activate Network
  Network --> Reader: data
  deactivate Network

  Reader -> Reader: decode(data)

  Reader -> ExecQueue: start_execute(msg)
  ExecQueue --> Reader: ok
  deactivate Reader
  ExecQueue --> Executor: msg

  Executor -> Executor: execute(msg)
  Executor -> Executor: encode(response_msg)

  [-> Reader: message ready

  Reader -> Network: receive
  activate Reader
  activate Network
  Network --> Reader: data
  deactivate Network

  Reader -> Reader: decode(data)

  Executor -> Network: send(data)
  activate Network
  Network --> Executor: ok
  deactivate Network
  deactivate Executor


  Reader -> ExecQueue: start_execute(msg)
  Executor -> ExecQueue: wait_for_msg
  activate Executor
  ExecQueue --> Reader: ok
  deactivate Reader

  ExecQueue --> Executor: msg

  Executor -> Executor: execute(msg)
  Executor -> Executor: encode(response_msg)

  Executor -> Network: send(data)
  activate Network
  Network --> Executor: ok
  deactivate Network
  deactivate Executor

which allows to hide cost of decoding the message behind the execution
of the previous message.


The amount of messages that are prefetched this way should be configurable
to allow a trade-off between:

* resource usage
* parallism

Common-Case Optimization
........................

The client can use pipelining to send command message sequences in one TCP packet
to the server.

If for example

* the initial :protobuf:msg:`Mysqlx.Sql::StmtPrepare` ``( stmt_id = 1, ...)`` and
* the its closing :protobuf:msg:`Mysqlx.PreparedStmt::Close` ``( stmt_id = 1, ...)``

are received in the same message sequence, the server may optimize for that
and save the creation for long-living prepared statement handle.

This applies to the sequences:

* :protobuf:msg:`Mysqlx.Sql::StmtPrepare` ``( stmt_id = 1, ...)``
* :protobuf:msg:`Mysqlx.PreparedStmt::Execute` ``( stmt_id = 1, ...)``
* :protobuf:msg:`Mysqlx.PreparedStmt::Close` ``( stmt_id = 1, ...)``

and

* :protobuf:msg:`Mysqlx.Sql::StmtPrepare` ``( stmt_id = 1, ...)``
* :protobuf:msg:`Mysqlx.PreparedStmt::Execute` ``( stmt_id = 1, cursor_id=2, ...)``
* :protobuf:msg:`Mysqlx.Cursor::FetchResultset` ``( cursor_id = 2, ...)``
* :protobuf:msg:`Mysqlx.PreparedStmt::Close` ``( stmt_id = 1, ...)``

It is implemented by:

* read from the network non-blocking
* add to exec queue
* executor thread

  * looks back in the exec-queue if any of the above conditions apply
  * applies optimizations
  * responds to each client message as without the optimization

.. uml::

  participant Network
  participant Reader
  box "Executor Thread"
  participant ExecQueue
  participant Executor
  end box

  [-> Reader: message ready

  loop
  Executor -> ExecQueue: wait_for_msg
  activate Executor

  Reader -> Network: receive
  activate Reader
  activate Network
  Network --> Reader: data
  deactivate Network

  Reader -> Reader: decode(data)

  Reader -> ExecQueue: start_execute(msg)
  ExecQueue --> Reader: ok
  deactivate Reader
  ExecQueue --> Executor: msg
  end loop

  Executor -> Executor: execute(msg[])
  loop
  Executor -> Executor: encode(response_msg)

  Executor -> Network: send(data)
  activate Network
  Network --> Executor: ok
  deactivate Network
  deactivate Executor
  end loop
