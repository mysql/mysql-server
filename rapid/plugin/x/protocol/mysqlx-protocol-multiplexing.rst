.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Multiplexing
============

Multiplexing allows to handle more than one session over a established connection.

It helps to counter the impact of opening a TLS connection (which may be quite expensive)
by running several sessions in parallel over the same security context.

To allow sending message from different connections in parallel, the large messages
have to split into smaller fragments

.. code-block:: c

  struct {
    uint32  length
    uint32  session-id
    opaque  fragment[MySQLMultiplexFrame.length]
  } MySQLMultiplexFrame;

The sequence of Frames that make up a Message is terminated by an empty, zero-length Frame.

This session-id is local the connection.

Max Frame Length
----------------

The maximum frame length may be changed between 1 and 2 :sup:`32`-1 Bytes.

If the client or server receive a frame that is larger than the current Max Frame Length
it **MUST** close the connection.

``frame.maxSendLength``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    * ``1048576``: current max frame length 1Mbyte
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
    * ``2097152``: asks to increase the max frame length to 2Mbyte

``frame.maxReceiveLength``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    * ``1048576``: current max receiving frame length 1Mbyte
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
    * ``2097152``: asks to increase the max receiving frame length to 2Mbyte

Enabling Multiplexing
---------------------

Multiplexing is enabled by sending :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet` `{ "multiplex" : 1 }`.

After a successful response from the server, both client and server will use multiplexing frames to wrap messages.

Creating a new session
----------------------

The first message after multiplexing is enabled should be a :protobuf:msg:`Mysqlx.Session::AuthenticateStart`
which will authenticate a session.

The session-id provide to this command has to be:

* unique across all sessions of this connection
* must not belong to session that is authenticated already

Closing a session
-----------------

Using :protobuf:msg:`Mysqlx.Session::Close` in a authenticated session closes the session and
releases the session-id.

