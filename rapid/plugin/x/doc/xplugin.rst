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

MySQL X Plugin Architecture
===========================

The MySQL X Plugin can be divided in 3 major layers:

* Session or Network layer
* `Execution layer`_
* Data Access layer


Session or Network Layer
------------------------

The network layer implements a TCP server that accept connections from clients that speak the X protocol.

At session establishment time, the network layer will perform the necessary handshake steps and authentication, using delegates implemented
at higher layers, which should do the appropriate username/password checks.

Once a session is established, it will read incoming requests into a buffer, decode them into Message objects and hand them over to higher layers.

It is implemented as a generic framework using the boost::asio library and is usable from other projects.

The network layer has a asio runloop, which runs on its own thread and is shared across multiple client connections.
When a message is handed over to the higher layer, it is put into a dispatch queue, which is served by a pool of worker threads.

See the NGS documentation for more details.

The network layer basically has 2 independent pipelines, an incoming and one outgoing pipeline. 
The incoming pipeline is triggered by incoming data from the socket.
The outgoing pipeline receives data from the higher layers and schedules them to be sent through the socket whenever possible.

Multiplexing
............

If multiplexing is enabled, a Demultiplexer object is inserted below the ProtocolDecoder object. Each session
will have its own ProtocolDecoder object, but will share the same Demultiplexer and socket objects.

The Demultiplexer will read frames and forward their payload to the target session's ProtocolDecoder.

A Multiplexer object is inserted between the ProtocolEncoder and the lower level objects for the opposite data flow direction.

Session Class
.............

The abstract Session object from NGS is subclassed in the Plugin. It is the entry point for client request message objects.

* When the Session object is created by the server, it should hold a pointer to an allocated but unitialized THD object.
* The Session object starts in the Authenticating state.

  * It can only accept Authentication related objects when it is in that state.
  * If an unexpected message is received, the session is closed.
* The authentication delegate should try to authenticate the previously created THD with the MySQL Server when it receives messages with the credentials.
* If auth succeeds, the Session enters the Connected state and can receive other messages. Else it is disconnected.
* Various auxiliary objects are created for the Session, such as a PreparedStatementManager object and CursorManager.
* The lower network layers will call Session::handle_message() with the message object from the network thread.
* The Session places messages in a request queue and also schedules a process_message_queue() call by a worker thread.

  * Multiple messages can be placed in the queue at once to achieve pipelining.
    * Due to Windows OS TCP stack characteristics, the client should not queue anything after the message that is closing the session. This can lead to undefined behavior (like errors from the server for the messages preceding session close) if the server runs on that OS.
* process_message_queue() will take messages from the queue from another thread and:

  * Will attempt to coalesce Prepare/Execute/Close statement sequences into a single direct execution command.
  * Dispatch commands to higher layers.
  * Some commands are executed by the Session object itself, eg. SessionClose and SessionReset
* When the higher layer executes a command, it will take a pointer to the ProtocolEncoder object, which will be used to encode server replies and others into messages in the X protocol and get them sent to the client.

Connection Interface
....................

The interface provides abstraction mechanism for different types of connections with and without ciphering. Creation of this interface
was enforce by "boost::asio" which supplies different connection types without common interface. Connection interface wraps "boost::asio::ssl::stream"
from application point of view (connection, accepting, reading data, writing data and turning on TLS). "boost::asio::ssl::stream" always start with tls enabled
still there is possibility to use lower layer and initialize it with socket.

Derived classes should always go for TLS at startup (connect, accept). Manual activation of TLS on such classes should cause an error. If application wants to turn on TLS at runtime, it should use a dynamic TLS Connection derived class, which
uses "Decorator" design pattern. Dynamic TLS classes decorates connection TLS class and switches from RAW layer to TLS layer at application request.

There are two different classes for TLS. First one depends on external library: OpenSSL and second depends on library that is distributed with mysqld: YaSSL. 
which class is going to be used is determined by Mysqld build options. 

Connection_raw class 
....................

This class provides basic TCP functionality without ciphering. XPlugin is going to use it when SSL is disabled on mysqld. Most methods call directly boost::asio::ip::tcp::socket.
The socket field type is a template parameter, thus this class is also used while creating lower layer connection instances by putting a reference type as template parameter. The reference causes that
release of lower layer Connection_raw instances won't release the socket.   

Connection_yassl class 
......................

This class provide TCP functionality with YaSSL. In default YaSSL socket function pointers (YaSSL - lower layer interface) are set to posix socket functions. This class override
those settings by providing own callbacks which  use asynchronous IO from Connectin_raw.
SSL handshake is made after connecting, accepting. 

Connection_openssl class 
........................

This class provide TCP functionality with SSL using boost::asio::ssl::stream (OpenSSL), which methods are called directly by this class. 
SSL handshake is made after connecting, accepting.

Connection_dynamic_tls class 
............................

This class decorates other connections with ability to move from lower layer(RAW) to upper (SSL), This approach simplifies implementation of Yassl & openssl connection classes in common way.
Connection always start in raw mode and after calling "activate_tls" it switches to upper layer and triggering SSL handshake.

Connection_unix_socket & Connection_named_pipe
..............................................

To be implemented & described here!     


Class Diagram - Connection abstraction layer
............................................

.. uml::
	@startuml
	
	namespace ngs.asio {
		interface Connection {
			async_write()
			async_read()
			async_accept()
			async_connect()
			async_enable_tls()
			Connection get_lower_layer()
		}
		
		class Connection_raw
		class Connection_yassl {
			-context: yassl_context&
			-raw_layer: Connection_raw
		}
		class Connection_openssl {
			-context: yassl_context&
		}
		class Connection_dynamic_tls {
			-current_layer: &Connection_raw
			-tls_layer: Connection_raw
		}
		
		
		Connection_openssl "1" o-- openssl_context
		Connection_yassl "1" o--  yassl_context
		
		class Connection_unix_socket
		class Connection_dynamic_tls
		class Connection_names_pipe
		
		Connection <|-- Connection_raw
		Connection <|-- Connection_yassl
		Connection <|-- Connection_openssl
		Connection <|-- Connection_unix_socket
		Connection <|-- Connection_names_pipe
		
		Connection_yassl *-- Connection_raw : raw_layer
		Connection_dynamic_tls "1" *-- Connection
		Connection_dynamic_tls "1" o-- Connection
		Connection_dynamic_tls --|> Connection
	}
	@enduml
	
Factories for Connection layer
..............................

Significant number of connection classes and a different step at creation, enforce encapsulation in factories. SSL factories combine an instance of Connection_*ssl with decorator Connection_dynamic_tls.	
Connection_factory interface is base for creating raw, yassl, openssl connection. Factories also hold common data for all connections like SSL context.  

	
Class Diagram - Factories for Connection layer
..............................................

.. uml::
	@startuml
	class yassl_context
	class openssl_context
	namespace ngs.asio {
		interface Connection {
			async_write()
			async_read()
			async_accept()
			async_connect()
			async_enable_tls()
		}
		
		interface Connection_factory {
			Connection create_connection(io_service)
		}
		
		class Connection_factory_raw {
			+ Connection create_connection(io_service)
		}
		class Connection_factory_yassl {
			- context : yassl_contex;
			+ Connection create_connection(io_service)
		}
		class Connection_factory_openssl {
			- context : openssl_contex;
			+ Connection create_connection(io_service)
		}
		
		Connection <-- Connection_factory 
		Connection_factory <|-- Connection_factory_yassl
		Connection_factory <|-- Connection_factory_openssl
		Connection_factory <|-- Connection_factory_raw
		
		Connection_factory_openssl "1" *-- .openssl_context
		Connection_factory_yassl "1" *--  .yassl_context
		
		Connection_factory_yassl --> Connection_yassl
		Connection_factory_openssl --> Connection_openssl
		Connection_factory_yassl --> Connection_dynamic_tls
		Connection_factory_openssl --> Connection_dynamic_tls
	}
	@enduml


Data Access Layer
-----------------

The Data Access or Storage Layer is a thin layer that provides a high-level interface to the APIs that the MySQL server provides to APIs, so that SQL queries can be executed and results can be processed.

There are at least 4 different usage scenarios for the data access interface:

* Queries meant to serve a client request
* Queries meant to serve a client request with Prepared Statements
* Queries meant to serve a client request via Cursor (and Prepared Statements)
* Internal queries 

Both use cases have the same input interface, but differ by how their results are handled.


Client Queries
..............

Queries that are being executed to perform an action directly requested by a client. When the query results are ready, MySQL 
invokes functions from a "Protocol" object for each field of each row of the resultset. These methods should then encode the
data into the wire protocol specific format and directly send it to the client.

In this mode of operation, the data is directly streamed from MySQL to the client, with minimal or no temporary storage required.

To handle this case, an interface class for an abstract ResulSetConsumer object is made available, which should be have usage specific
implementations.


Client Queries with PS
......................

Same as above, but using the Prepared Statements interface.
Pending implementation of specific APIs in the MySQL server.


Client Queries via Cursor
.........................

If a Cursor is requested by the client, the statement may be executed via a prepared statement, so that the results
will be stored in a handle kept by the server. The resultset handle is treated like a cursor, which can be manipulated
by the cursor and have its contents streamed back to it at a later time.

Pending implementation of specific APIs in the MySQL server.


Internal Queries
................

Queries that are executed by various components of the Plugin for internal purposes or indirectly necesary to handle a client request.
For example, a insert() on a document may have to run a SHOW CREATE TABLE on the target table to find out if the table is a
document table. In such cases, it is most convenient if the results are stored in a memory structure such similar to MYSQL_RES and MYSQL_ROW
so that the Plugin code can freely iterate and manipulate its data.



Execution Layer
---------------

This is where the bulk of the plugin code is located. It is responsible for taking in command objects received by the session layer and
doing the necessary steps to translate them into commands that the Storage layer supports.

The Execution Layer must keep track of a lot of session specific state information to properly support the X protocol, including:

* Prepared Statements
* Cursors
* Data Model mappings

The general process for executing a client request that reaches the Storage layer can be summarized in the following steps:

# Transform request into a SQL statement
# Install a custom "MySQL protocol handler" (or ResultSetConsumer), which is a set of callbacks to be called by the server when a resultset is to be sent to the client
# Execute SQL statement
# Have the ResultSetConsumer either encode and forward the resultset data to the client as it receives it or, if the request is on a prepared statement, hands over the result handle to the CursorManager


CommandDispatcher Class
.......................

A class that receives Message objects from the Session and executes it.


PrepStmtManager Class
.....................

A class that manages and executes Prepared Statements for a session. In addition to keeping track of a mapping 
between client specified statement Ids to server PS handles, it is responsible for preparing and executing
them and also destroying them when closed. 

In case the 5.7 server does not provide a Prepared Statement API, it should emulate them using SQL statements.

Results produced by execution of statements should be fed to a ResultSetConsumer object, which should be a
ResultSetStreamer (if the client did not request a cursor, by setting the cursor_id of the request to 0) or a CursorManager.


CursorManager Class
...................

Manages a mapping between client specified cursor ids and PS resultset handles that are used to implement cursors.
Per session.

Requires PS API to be present in MySQL, details of this class will probably change depending on what the API looks like.

Assuming Cursors are to be implemented, the following cases are possible when a client requests a cursor when executing a PS:

* PS results do not actually produce a resultset handle and results are directly streamed to the client. In this case, no cursor is created.
* PS results produce a resultset handle, which is turned into a cursor and can be accessed separately by the client.


ResultSetStreamer Class
.......................

Implements the ResulSetConsumer interface and makes use of the ProtocolEncoder object to send resultset metadata and row data back to the client, performing any transformations needed.



Class Diagram
=============

.. uml::

  package NGS {
      class Authenticator {
      }
      
      class Server {
      }
      Server -up-> Authenticator
      
      
      class Client {
      }
      Server "1" -> "*" Client
      
      abstract class SessionBase {
      }
      Client "1" -> "*" SessionBase
      
      interface Connection {
      }
      Client -> Connection
      
      class BufferedReader {
      }
      Connection ..> BufferedReader: handle_data()
      
      class BufferedWriter {
      }
      BufferedWriter ..> Connection: async_write()
      
      
      class ProtocolDecoder {
      }
      BufferedReader ..> ProtocolDecoder: handle_async_data()
      
      class ProtocolEncoder {
      }
      ProtocolEncoder ..> BufferedWriter: send_data()
      
      class Message {
      }
      ProtocolDecoder ..> Message: creates
  }
  
  class THDAuthenticator {
  }
  
  class Session {
      THD
      Connection
      message_queue
      process_message_queue()
      handle_message()
      on_close()
      on_reset()
  }
  Session -|> SessionBase
  ProtocolDecoder ..> Session: handle_message(Message)
  
  class DataModelMapper {
      sql_for_insert()
      sql_for_update()
      sql_for_find()
      sql_for_delete()
  }
  
  class PrepStmtManager {
      map<stmt_id, Query> without_ps
      map<stmt_id, PSHandle> with_ps
      execute_insert()
      execute_update()
      execute_find()
      execute_delete()
      prepare_insert()
      prepare_update()
      prepare_find()
      prepare_delete()
      close(stmt_id)
      execute(stmt_id, ProtocolEncoder, ResultConsumer)
  }
  PrepStmtManager ..> DataModelMapper: uses
  
  class CursorManager<ResultSetConsumer> {
      on_fetch_metadata()
      on_fetch_rows()
      on_close()
      on_poll()
  }
  
  class CommandDispatcher {
      handle_message(Message)
      on_prep_insert()
      on_prep_update()
      on_prep_delete()
      on_prep_find()
      on_prep_sql()
      on_prep_execute()
      on_prep_close()
      on_direct_insert()
      on_direct_update()
      on_direct_delete()
      on_direct_find()
      on_direct_sql()
  }
  Session -> CommandDispatcher: handle_message(Message)
  CommandDispatcher ..> ProtocolEncoder: uses
  CommandDispatcher ..> PrepStmtManager: calls
  CommandDispatcher ..> CursorManager: uses
  
  class ResultSetStreamer<ResultSetConsumer> {
  }
  ResultSetStreamer ..> ProtocolEncoder: uses
  
  abstract class ResultSetConsumer {
      store_null()
      store(int)
      store(string)
      store(bool)
      ...
  }
  
  package MySQL <<Database>> {
      class DataAccess {
          THD create_session(user, password)
          execute(Query, ResultSetConsumer)
          SimpleResultSet execute_simple(Query)
      }
  }
  PrepStmtManager ..> DataAccess: exec sql
  DataAccess ..> CursorManager: send data
  DataAccess ..> ResultSetStreamer: send data
  CursorManager ..> ProtocolEncoder: send data
  THDAuthenticator ..> DataAccess: create_session()

