.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

Lifecycle
=========

Transport
  transport layer which exchanges data: TCP sockets, Unix Sockets, Named Pipes, TLS, ...

Connection
  a lower-level connection between two Endpoints

Session
  the session maintains the state. User-Variables, Temporary Tables, ...

Messages
  :doc:`mysqlx-protocol-messages` are exchanged between Endpoints. On a higher level they build
  a sequence of Messages with a initial and final Message.

Endpoints
  a Client or a Server

Connection
----------

A default connection supports:

* supports connection capability negotiation via :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet` and :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
* must support at least one of

  * `TLS Extension`_ and PLAIN :doc:`mysqlx-protocol-authentication` or
  * `TLS Extension`_ and EXTERNAL :doc:`mysqlx-protocol-authentication` or
  * SCRAM :doc:`mysqlx-protocol-authentication` or another challenge response authentication mechanism

Session
-------

A session owns state like:

* current schema
* current character set
* temporary tables
* user variables
* open transactions
* ...

and it is used by the server and the protocol to manage state.

Sessions are:

* opened with :protobuf:msg:`Mysqlx.Session::AuthenticateStart`
* reset with :protobuf:msg:`Mysqlx.Session::Reset`
* closed with :protobuf:msg:`Mysqlx.Session::Close`

Closing a session releases all session related data.

By default one `Connection`_ can have one `Session`_ active at a time. Using
the :doc:`mysqlx-protocol-multiplexing` extension multiple sessions can be
run over the same physical connection.

Stages of Session Setup
-----------------------

After a client connects to the server it:

* may ask for the servers capabilities with :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
* may ask the server to use optional protocol features with :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
* MUST authenticate
* may send commands

.. uml::

  == Negotiation ==
  Client -> Server: CapabilitiesGet()
  Server --> Client: { "tls": 0, ... }

  Client -> Server: CapabilitiesSet({"tls" : 1})
  Server --> Client: Ok

  == Authentication ==
  Client -> Server: AuthenticateStart(mech="MYSQL41", ...)
  Server --> Client: AuthenticateContinue(auth_data="...")
  Client -> Server: AuthenticateContinue(auth_data="...")
  Server --> Client: AuthenticateOk()

  == Commands ==
  ...

In the **Negotiation** step the client checks which features the
server supports on the protocol side.

After a successful finish of the **Authentication** step the previous
Session is discarded and a new Session is created.

Further **Command** Messages run within a Session.

Authentication
--------------

:doc:`mysqlx-protocol-authentication` supports several authentication mechanisms which can be discovered
with :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`

``authentication.mechanisms``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    server side supported SASL mechanism

    * before TLS connection established: ``[ ]``
    * after TLS connection established: ``[ "EXTERNAL", "PLAIN" ]``

    required mechanisms

    * EXTERNAL (X.509 client certificates) :rfc:`4422#appendix-A` (required)
    * PLAIN (over SSL) :rfc:`4616` (required)

    other known mechanisms

    * SCRAM-SHA-1 :rfc:`5802`
    * SCRAM-SHA-256 (https://tools.ietf.org/html/draft-hansen-scram-sha256-02)
    * MYSQL41 (MySQL 4.1 auth mechanism)

.. uml::

  Client -> Server: AuthenticateStart()
  loop
  Server --> Client: AuthenticateContinue()
  Client -> Server: AuthenticateContinue()
  end
  alt
  Server --> Client: Error()
  else
  Server --> Client: AuthenticateOk()
  end


Pipelining
----------

The messages may be pipelined:

* the client may send the messages without waiting for a reply first
* the client should only send messages which safely trigger an Error packet

For the server it is no difference if the messages from client where sent in a bulk
or if the client waited. The network and send/receive buffers of the Operation
System will act as queue.

:doc:`mysqlx-protocol-expect` help to control the behaviour of following messages
if a pipelined message fails.

.. seealso:: :doc:`mysqlx-protocol-implementation`.

Max Message Length
------------------

If the server receives a message that is larger than the current *Max Message Length*
it **MUST** close the connection.

``message.maxSendLength``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    * ``1048576``: current max message length 1Mbyte
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
    * ``2097152``: asks to increase the max message length to 2Mbyte

``message.maxReceiveLength``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    * ``1048576``: current max receiving message length 1Mbyte
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
    * ``2097152``: asks to increase the max receiving message length to 2Mbyte

.. note::

  As clients and servers may have to buffer the entire message before it can be
  processed these limits allow protect against excessive resource usage.

Extensions
----------

If the result of :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet` contains a extension key from the table below
it supports the feature.

=============== ========================
name            extension
=============== ========================
``tls``         `TLS extension`_
``compression`` `Compression extension`_
``multiplex``   :doc:`mysqlx-protocol-multiplexing`
=============== ========================

.. note::

  More extensions can be added in futher iterations as long as:

  * they are announced in CapabilitiesGet() and documented

TLS extension
.............

The client may assume that the server supports a set of features by default
and skip the :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet` step:

* if the TLS extension isn't supported the :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet` will fail
* if it is supported, it will succeed.

.. code-block:: cucumber

  Feature: extensions
    Scenario: connecting with TLS, fast path
      Given a client side X.509 certificate is provided with username "foo"
      And client certificate is valid
      When connecting with TLS established
      Then handshake should be single-step

.. uml::

  == Negotiation ==
  Client -> Server: CapabilitiesSet({"tls" : 1})
  Server --> Client: Ok
  note over Client, Server: TLS handshake

  == Authentication ==
  Client -> Server: AuthenticateStart(mech="EXTERNAL")
  Server --> Client: AuthenticateOk()

  == Commands ==
  ...


:protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
  * ``0``: supported, not in use
  * ``1``: supported, in use

:protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
  * ``1``: switch to TLS connection after server-side Ok

  If the server doesn't support the capability, it will return an Error.

  .. note::

    disabling TLS on a connection may not be supported by the
    server and should result in an Error.

Compression extension
.....................

The compression extension allows client and server to announce which
codecs they support (and prefer) when receiving data. This may be
used by other :doc:`mysqlx-protocol-messages` to encode their large data.

``compression.supportedServerMethods``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    server may receive data compressed with these methods

    * e.g. ``[ "identity", "zlib", "lz4" ]``

``compression.supportedClientMethods``
  :protobuf:msg:`Mysqlx.Connection::CapabilitiesGet`
    client may receive data compressed with these methods

    * default ``[ "identity" ]``

  :protobuf:msg:`Mysqlx.Connection::CapabilitiesSet`
    client may receive data compressed with these methods

    * e.g. ``[ "identity", "zlib", "lz4" ]``

Compression Methods
~~~~~~~~~~~~~~~~~~~

``identity``
  uncompressed content

``zlib``
  DEFLATE algorithm

``lz4``
  LZ4 algorithm

