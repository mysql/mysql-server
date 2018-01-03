/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef X_CLIENT_MYSQLXCLIENT_XPROTOCOL_H_
#define X_CLIENT_MYSQLXCLIENT_XPROTOCOL_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "mysqlxclient/xargument.h"
#include "mysqlxclient/xconnection.h"
#include "mysqlxclient/xerror.h"
#include "mysqlxclient/xmessage.h"
#include "mysqlxclient/xquery_result.h"

#ifdef USE_MYSQLX_FULL_PROTO
#define HAVE_MYSQLX_FULL_PROTO(Y, N) Y
#else
#define HAVE_MYSQLX_FULL_PROTO(Y, N) N
#endif

#define XCL_CLIENT_ID_NOT_VALID  0
#define XCL_SESSION_ID_NOT_VALID 0


namespace xcl {

/**
  Enum that defines result of dispatching a message or a notice
  to handler registred by user or XSession.
*/
enum class Handler_result {
  /**
    No action, dispatch the message/notice to next handler or
    return the message.
  */
  Continue,
  /**
    Message consumed. Stop dispatching the message. Requester
    is going to wait for next message/notice.
  */
  Consumed,
  /**
    Message consumed. Stop dispatching the message. Requester
    is going to receive an error.
  */
  Error
};

/**
  Enum that defines the position inside priority group
  where the handler is going to be appended.
*/
enum class Handler_position {
  Begin,
  End,
};


/**
  Enum that defines a execution priority of a handler.

  User handlers should be pushed on stack with "medium" priority. To overwrite
  behavior defined by XSession, XQuery_result, XProtocol objects, user can push
  their handler to other priorities.
*/
enum Handler_priority {
  /** Priority used by XSession object */
  Handler_priority_high = 100,
  /** Priority for handlers added by user */
  Handler_priority_medium = 200,
  /** Priority used by XSession object */
  Handler_priority_low = 300,
};

/**
  Interface defining X Protocol operations.

  It is responsible for building, serialization, deserialization of protobuf
  messages also it defines some basic X Protocol flows. The interface can be
  used for:

  * all X Protocol specific featured CRUD, pipelining, notices
  * sending messages that were recently added to proto files and 'send'
    method was not created for them
  * flows that were not implemented in XSession or XProtocol
*/
class XProtocol {
 public:
  /** Data type used on the wire for transferring Message_type_id */
  using Header_message_type_id = uint8_t;

  /** Aliases for types that represents different X Protocols message */
  using Server_message_type_id = Mysqlx::ServerMessages::Type;
  using Client_message_type_id = Mysqlx::ClientMessages::Type;

  /** Identification (representation) of notice handler placed in queue */
  using Handler_id = int;

  /** Alias for type that is able to hold X Plugins client identifier */
  using Client_id  = uint64_t;

  /** Alias for protobuf message type used by the lite or
      full version of the library */
  using Message    = HAVE_MYSQLX_FULL_PROTO(
      ::google::protobuf::Message,
      ::google::protobuf::MessageLite);

  /** Function wrapper that can be used for X Protocol notice processing */
  using Notice_handler = std::function<Handler_result (
      XProtocol *protocol,
      const bool,
      const Mysqlx::Notice::Frame::Type,
      const char *,
      const uint32_t)>;

  /** Function wrapper that can be used for X Protocol message processing. */
  using Client_message_handler = std::function<Handler_result (
      XProtocol *protocol,
      const Client_message_type_id,
      const Message &)>;
  using Server_message_handler = std::function<Handler_result (
      XProtocol *protocol,
      const Server_message_type_id,
      const Message &)>;

  using Capabilities   = ::Mysqlx::Connection::Capabilities;

 public:
  virtual ~XProtocol() = default;

  /**
    Add handler to the notice-handler list.

    Notice handlers are going to be held on a three different priority lists.
    Handler can be pushed at front or back of the list. Each notice/message
    received through this interface is going through all pushed handler
    in sequence defined by priorities and order of front/back pushes.
    Handlers are called in case when the message type is a notice and "message
    recv-handlers" didn't drop the message. When the handler returns:

    * "Handler_continue", the received notice is processed as usual.
    * "Handler_consumed", the dispatch to next handler is stopped. Received
       notice is dropped causing the receive function to wait for next notice.
    * "Handler_error", the dispatch to next handler is stopped. Received message
       is dropped and the receive functions gets and error CR_X_INTERNAL_ABORTED.

    @param handler     callback which is going to be called on each
                       notice received through current instance of XProtocol
    @param position    chooses where the handler is going to be inserted at
                       "begin" or "end" of selected priority list
    @param priority    chooses to which priority list the handler is going
                       to be added

    @return position ID of notice handler
  */
  virtual Handler_id add_notice_handler(
      Notice_handler handler,
      const Handler_position position = Handler_position::Begin,
      const Handler_priority priority = Handler_priority_medium) = 0;

  /**
    Removes a handler represented by 'id' from the notice hander container.

    @param id          id of header which should be removed
  */
  virtual void remove_notice_handler(const Handler_id id) = 0;

  /**
    Add handler to the recv-handler list.

    Received message handlers are going to be held on a three different
    priority lists. Handler can be pushed at front or back of the list.
    Each message received through this interface is going through all pushed
    handler in sequence defined by priorities and order of front/back pushes.
    Handlers are called after message deserialization. When the handler
    returns:

    * "Handler_continue", the received message is processed as usual.
    * "Handler_consumed", the dispatch to next handler is stopped, received
       message is dropped causing the receive function to wait for next message.
    * "Handler_error", the dispatch to next handler is stopped, received message
       is dropped and the receive functions gets and error CR_X_INTERNAL_ABORTED.

    @param handler     callback which is going to be called on each
                       message received through current instance of XProtocol
    @param position    chooses where the handler is going to be inserted at
                       "begin" or "end" of selected priority list
    @param priority    chooses to which priority list the handler is going
                       to be added

    @return position ID of notice handler
  */
  virtual Handler_id add_received_message_handler(
      Server_message_handler handler,
      const Handler_position position = Handler_position::Begin,
      const Handler_priority priority = Handler_priority_medium) = 0;

  /**
    Removes a handler represented by 'id' from the received
    handler container.

    @param id          id of header which should be removed
  */
  virtual void remove_received_message_handler(const Handler_id id) = 0;

  /**
    Add handler to the send-handler list.

    Send message handlers are going to be held on a three different
    priority lists. Handler can be pushed at front or back of the list.
    Each message send through this interface is going through all pushed
    handler in sequence defined by priorities and order of front/back pushes.
    Handlers are called before message serialization. Handlers return value
    is ignored.

    @param handler     callback which is going to be called on each
                       message sent through current instance of XProtocol
    @param position    chooses where the handler is going to be inserted:
                       "begin" or "end" of selected priority list
    @param priority    chooses to which priority list the handler is going
                       to be added

    @return position ID of notice handler
  */
  virtual Handler_id add_send_message_handler(
      Client_message_handler handler,
      const Handler_position position = Handler_position::Begin,
      const Handler_priority priority = Handler_priority_medium) = 0;

  /**
    Removes a handler represented by 'id' from the send
    handler container.

    @param id          id of header which should be removed
  */
  virtual void remove_send_message_handler(const Handler_id id) = 0;

  /**
    Get connection layer of XProtocol.

    The lower layer can by used do direct I/O operation on the
    socket.
  */
  virtual XConnection &get_connection() = 0;

  //
  // Methods that send or receive single message

  /**
    Read and deserialize single message.

    Message is read using XConnection, and deserialized in implementation
    of this interface. Received message before returning is going to be
    dispatched through "message handlers" and if it is a "notice" then it is
    going to be dispatched through "notice handlers". The handlers are going
    to decide what to do with the current message: ignore, allow, fail.
    When the message is ignored the  function is going to wait for next
    message.
    Following errors can occur while reading message/abort reading of the
    message:

    * I/O error from XConnection
    * timeout error from XConnection
    * error from dispatchers (notice, message)

    @param[out] out_mid    return received message identifier
    @param[out] out_error  if error occurred, this argument if going to be set
                           to its code and description

    @return Deserialized protobuf message
      @retval != nullptr   OK
      @retval == nullptr   I/O error, timeout error or dispatcher
                           error occurred
  */
  virtual std::unique_ptr<Message> recv_single_message(
      Server_message_type_id *out_mid,
      XError     *out_error) = 0;

  /**
    Receive raw payload (of X Protocol message).

    This method receives a X Protocol message which consists of 'header' and
    'payload'. The header is received first, it holds message identifier and
    payload size(buffer size), payload(content of 'buffer') is received after
    the header. The method blocks until header and whole payload is stored
    inside user provided buffer. The length of the payload is limited by
    2^32 (length field uint32) - 5 (header size).
    When the value of expression '*buffer' is set to 'nullptr', then the method
    is going to allocate the buffer for the payload. User needs to release the
    buffer by calling 'delete[]' on it.
    Message payload received using this method isn't dispatched through
    "message handler" nor "notice handlers".

    @param[out]    out_mid     message identifier of received message
    @param[in,out] buffer      buffer for the message payload
    @param[in,out] buffer_size size of the buffer, after the call it is going
                               to hold payload length

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError recv(Header_message_type_id *out_mid,
                      uint8_t **buffer,
                      std::size_t *buffer_size) = 0;

  /**
    Deserialize X Protocol message from raw payload.

    This method deserializes the raw payload acquired by
    `XProtocol::recv` method.

    @param mid          message identifier
    @param payload      message payload
    @param payload_size message payloads size
    @param out_error    deserialization error

    @return Error code with description
      @retval != true     OK
      @retval == true     deserialization error occurred
  */
  virtual std::unique_ptr<Message> deserialize_received_message(
      const Header_message_type_id mid,
      const uint8_t *payload,
      const std::size_t payload_size,
      XError *out_error) = 0;

  /**
    Serialize and send protobuf message.

    This method builds message payload when serializing 'msg' and prepends it
    with a 'header' holding the message identifier and payload size.
    Such construction is send using XConnection interface.

    @param mid      message identifier
    @param msg      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error or timeout error occurred
  */
  virtual XError send(
      const Client_message_type_id mid,
      const Message &msg) = 0;

  /**
    Send the raw payload as message.

    This method sends a X Protocol message which consist from 'header' and
    'payload'.  The header is send first, it holds message identifier and
    payload size(buffer size), payload(content of 'buffer') is send after
    the header. The method blocks until header and payload is fully queued
    inside TCP stack. The length of the payload is limited by
    2^32 (length field uint32) - 5 (header size).

    @param mid      message identifier
    @param buffer   already serialized message payload
    @param length   size of the custom payload

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Header_message_type_id mid,
                      const uint8_t *buffer,
                      const std::size_t length) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Session::AuthenticateStart &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Session::AuthenticateContinue &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Session::Reset &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Session::Close &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Sql::StmtExecute &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::Find &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::Insert &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::Update &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::Delete &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::CreateView &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::ModifyView &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Crud::DropView &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Expect::Open &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Expect::Close &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Connection::CapabilitiesGet &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Connection::CapabilitiesSet &m) = 0;

  /**
    Serialize and send protobuf message.

    @param m      message to be serialized and sent

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error occurred
  */
  virtual XError send(const Mysqlx::Connection::Close &m) = 0;

  /*
    Methods that execute different message flows
    with the server
  */

  /**
    Get an object that is capable of reading resultsets.

    Create and return an object without doing I/O operations.

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  error occurred
  */
  virtual std::unique_ptr<XQuery_result> recv_resultset() = 0;

  /**
    Get an object that is capable of reading resultsets.

    Create and return an object which already fetched metadata.
    If server returns an error or an I/O error occurred then
    the result is "nullptr".

    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  error occurred
  */
  virtual std::unique_ptr<XQuery_result> recv_resultset(XError *out_error) = 0;

  /**
    Read "Mysqlx.Ok" message.

    Expect to receive "Ok" message.
    * in case of any other message return out of sync error
    * in case "Mysqlx.Error" message translate it to "XError"
    "Ok" message is used in several different situations like: setting
    capabilities, creating views, expectations.

    @return Error code with description
      @retval != true     Received OK message
      @retval == true     I/O error, timeout error, dispatch error
                           or received "Mysqlx.Error" message
   */
  virtual XError recv_ok() = 0;

  /**
    Execute session closing flow.

    Send "Mysqlx::Session::Close" message and expect successful confirmation
    from the X Plugin by reception of "Mysqlx::Ok". Synchronization errors and
    "Mysqlx::Error" are returned through return values.

    @return Error code with description
      @retval != true     Received OK message
      @retval == true     I/O error, timeout error, dispatch error
                           or received "Mysqlx.Error" message
   */
  virtual XError execute_close() = 0;

  /**
    Send custom message and expect resultset as response.

    @param mid             id of the message to be serialized
    @param msg             message to be serialized and sent
    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<XQuery_result> execute_with_resultset(
      const Client_message_type_id mid,
      const Message &msg,
      XError *out_error) = 0;

  /**
    Send statement execute and expect resultset as response.

    @param msg             "StmtExecute" message to be serialized and sent
    @param[out] out_error  in case of error, the method is going to return error
                           code and description
    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<XQuery_result> execute_stmt(
      const Mysqlx::Sql::StmtExecute &msg,
      XError *out_error) = 0;

  /**
    Send crud find and expect resultset as response.

    @param msg             "Find" message to be serialized and sent
    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<XQuery_result> execute_find(
      const Mysqlx::Crud::Find &msg,
      XError *out_error) = 0;

  /**
    Send crud update and expect resultset as response.

    @param msg             "Update" message to be serialized and sent
    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<XQuery_result> execute_update(
      const Mysqlx::Crud::Update &msg,
      XError *out_error) = 0;

  /**
    Send crud insert and expect resultset as response.

    @param msg             "Insert" message to be serialized and sent
    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<XQuery_result> execute_insert(
      const Mysqlx::Crud::Insert &msg,
      XError *out_error) = 0;

  /**
    Send crud delete and expect resultset as response.

    @param msg             "Delete" message to be serialized and sent
    @param[out] out_error  in case of error, the method is going to return error
                           code and description
    @return Object responsible for fetching "resultset/s" from the server
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<XQuery_result> execute_delete(
      const Mysqlx::Crud::Delete &msg,
      XError *out_error) = 0;

  /**
    Send "CapabilitiesGet" and expect Capabilities as response.

    @param[out] out_error  in case of error, the method is going to return error
                           code and description

    @return X Protocol message containing capabilities available exposed
            by X Plugin
      @retval != nullptr  OK
      @retval == nullptr  I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual std::unique_ptr<Capabilities> execute_fetch_capabilities(
      XError *out_error) = 0;

  /**
    Execute "CapabilitiesSet" and expect "Ok" as response.

    @param capabilities   message containing cababilities to be set

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual XError execute_set_capability(
      const Mysqlx::Connection::CapabilitiesSet &capabilities) = 0;

  /**
    Execute authentication flow

    @param user    MySQL Server account name
    @param pass    MySQL Server accounts authentication string
    @param schema  schema which should be "used"
    @param method  X Protocol authentication method, for example:
                  "PLAIN", "MYSQL41"

    @return Error code with description
      @retval != true     OK
      @retval == true     I/O error, timeout error, dispatch error
                          or received "Mysqlx.Error" message
  */
  virtual XError execute_authenticate(const std::string &user,
                                      const std::string &pass,
                                      const std::string &schema,
                                      const std::string &method = "") = 0;
};

}  // namespace xcl

#endif  // X_CLIENT_MYSQLXCLIENT_XPROTOCOL_H_
