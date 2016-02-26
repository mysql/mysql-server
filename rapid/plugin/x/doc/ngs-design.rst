.. Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301  USA

Basic Data/Process Flow
=======================

Client States
-------------

Clients start when their connection is accepted by the server. Capabilities are negotiated,
where multiplexing can be requested and one the ``authStart`` message is received, the
authentication starts. If authentication succeeds, they go into the ``Running`` state
until the last open session (or the only one, if mx is off). If mx is enabled, new ``authStart``
messages can be received in the ``Running`` state.

Note that capabilities exchange is handled globally, not at session level.
Authentication is handled at session level. Network, protocol and session levels are all
transparent to multiplexing, only the multiplexor and clients need to know about it.


.. uml::

  title Client States
  
  [*] --> Accepted: accept()
  Accepted --> Capabilities: get/setCap
  Capabilities --> Capabilities: get/setCap
  Capabilities --> Capabilities_MX: setCap(mx)
  note left
    _MX is a boolean flag set when mx is enabled
  end note
  Capabilities_MX --> Capabilities_MX: get/setCap
  
  Capabilities --> Authenticating: authStart
  Capabilities_MX --> Authenticating_MX: authStart
  
  Authenticating --> Running: authOK
  Authenticating --> Closing: authFail
  
  Running --> Closing: session closed
  
  Authenticating_MX --> Running_MX: authOK
  Authenticating_MX --> Closing: authFail
  
  Running_MX --> Running_MX: authStart
  Running_MX --> Running_MX: authOK, authFail
  Running_MX --> Closing: session closed, nsessions=0
  
  Closing --> [*]


Session States
--------------

Following is a state diagram for the session states. A session always starts in the Authenticating state, where
only authencation related commands are accepted. Receipt of any other commands results in session disconnection.
When authentication succeeds, it changes to the Connected state, where session requests are handled.

.. uml::

  title Session States
  
  [*] --> Authenticating: authStart
  
  Authenticating --> Connected: authOK
  Authenticating --> Closing: authFail
  Authenticating --> Authenticating: authContinue
  Connected --> Closing: close
  Connected --> Connected: commands
  
  Closing --> [*]



Client Connection Handling
--------------------------

Server object binds a socket and starts an asynchronous loop, where it listen for ``accept()``
on the socket. When a client connection is accepted, the following objects are created for the
client:
* ``Buffer_reader`` 
* ``Buffer_writer``
* ``Protocol_decoder``

The first 2 will be used through the lifetime of the connection. The last one is used during handshake
and authentication only.

Once authentication is done, a ``Session`` object is created, the handshake handler is destroyed and 
replaced by a ``Protocol_decoder``, which will handle request messages from then on. If multiplexing
is enabled, the ``Protocol_handshake`` object is created every time a new session-id is detected and the
same process is repeated. Each ``Session`` has its own dedicated ``Protocol_decoder``, but they all
share the same ``Demultiplexer`` and ``Buffer_reader/writers``


Request Handling
----------------

Data is read asynchronously from a socket/file handle via ``boost::asio`` into an array of buffers
by a ``Buffer_reader`` object. The data is immediately passed up to a ``Protocol_decoder``
object, which will take ownership of the data and append it to its own message buffer. 
When there's enough data to handle the request, the message is passed up to the
owner ``Session`` via ``handle_message()``, where it is handled in a separate thread.

If multiplexing is enabled, an intermediate ``Demultiplexer`` object is introduced between
``Buffer_reader`` and ``Protocol_decoder``. The ``Demultiplexer`` will have handles to all
active sessions for a connection and pass the arriving frames to the correct one. Knowledge
about multiplexed sessions at the message level is abstracted away except in the ``Demultiplexer`` 
object.


.. uml::

  object Server
  package "Client connection 1" <<Frame>> {
    object Client
    object Socket 
    object Buffer_reader
    object Protocol_decoder
    object Session
  
    Server "1" o- "*" Client
    Client "1" o- "*" Session
    Session <.. Protocol_decoder: message
    Protocol_decoder <.. Buffer_reader: buffer
    Buffer_reader <.. Socket: bytes
  
    Client: single session client
    Buffer_reader: asynchronously reads data from socket
    Session: manages one client session
  }
  
  package "Client connection 2" <<Frame>> {
    object Client2
    object Socket2
    object Buffer_reader2
    object Demultiplexer
    object Protocol_decoder2a
    object Session2a
    object Protocol_decoder2b
    object Session2b
    
    Demultiplexer: reads a full frame and then\npass the data up to the right decoder 
    Client2: multiplexed client
    
    Client2 "*" -o "1" Server
    Session2a "1" -o "*" Client2
    Session2a <.. Protocol_decoder2a: message
    Protocol_decoder2a <.. Demultiplexer: buffer
    Session2b "1" -o "*" Client2
    Session2b <.. Protocol_decoder2b: message
    Protocol_decoder2b <.. Demultiplexer: buffer
    Demultiplexer <.. Buffer_reader2: buffer
    Buffer_reader2 <.. Socket2: bytes
  }


Control Flow
------------

Program execution flow is controlled by a event loop from the io_service object. 
When a read event occurs on a socket, the data is read and flows towards the session object,
the data is parsed into a command object and executed by the session. 

The session may use a thread from a thread pool to actually execute the request so
control returns immediately to the event loop. When the thread is done with the request,
it schedules asynchronous writes of the response data, which will be sent whenever possible.

All network level operations are done through a single io_service event loop, thus,
if it becomes a bottleneck, additional io_service loops in additional threads may be used.

Worker threads for command execution may be pooled in a separate io_service event loop,
which acts as a multi-producer, multi-reader dispatcher/request queue.

.. uml::

  title Request Handling
  
  control "io_service" as io
  participant Frame_reader as fr
  participant Frame_writer as fw
  participant Protocol_decoder as pd
  participant Session as s
  control "Worker io_service" as pool
  participant "Worker thread" as w
  
  --> io: socket becomes readable
  activate io
  
  io -> fr: read_handler()
  
  
  activate fr
  fr -> pd: handle_data()
  
  activate pd
  pd -> s: handle_message(data)
  
  activate s
  s -> pool: post(process(message), callback)
  activate pool
  pool -> pool: enqueue
  pool --> s
  deactivate pool
  
  s --> pd
  deactivate s
  pd --> fr
  deactivate pd
  fr --> io
  deactivate fr
  deactivate io
  note left
     wait for more activity
     end note
     ...
  
  pool -> pool: dequeue
  activate pool #0088ff
  pool -> w: process(message, callback)
  activate w #0088ff
  w -> w: execute
  w -> s: callback()
  activate s #0088ff
  s -> fw: send_results()
  activate fw #0088ff
  fw -> io: async_write(write_handler, data)
  activate io #0088ff
  io -> io: enqueue
  io --> fw
  deactivate io
  fw --> s
  deactivate fw
  s --> w
  deactivate s
  w --> pool
  deactivate w
  deactivate pool
  
  --> io: socket becomes writable
  activate io
  io -> io: write()
  deactivate io


Authentication
==============

 
