/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_CLIENT_XPROTOCOL_IMPL_H_
#define PLUGIN_X_CLIENT_XPROTOCOL_IMPL_H_

#include <sys/types.h>
#include <zlib.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/mysqlxclient/xmessage.h"
#include "plugin/x/client/mysqlxclient/xprotocol.h"
#include "plugin/x/client/stream/connection_input_stream.h"
#include "plugin/x/client/xcompression_impl.h"
#include "plugin/x/client/xconnection_impl.h"
#include "plugin/x/client/xpriority_list.h"
#include "plugin/x/client/xprotocol_factory.h"
#include "plugin/x/client/xquery_instances.h"

namespace xcl {

class Result;

class Protocol_impl : public XProtocol,
                      public std::enable_shared_from_this<Protocol_impl> {
 public:
  Protocol_impl(std::shared_ptr<Context> context, Protocol_factory *factory);

 public:  // Implementation of XProtocol methods
  Handler_id add_notice_handler(
      Notice_handler handler,
      const Handler_position position = Handler_position::Begin,
      const Handler_priority priority = Handler_priority_medium) override;

  Handler_id add_received_message_handler(
      Server_message_handler handler,
      const Handler_position position = Handler_position::Begin,
      const Handler_priority priority = Handler_priority_medium) override;

  Handler_id add_send_message_handler(
      Client_message_handler handler,
      const Handler_position position = Handler_position::Begin,
      const Handler_priority priority = Handler_priority_medium) override;

  void remove_notice_handler(const Handler_id id) override;

  void remove_received_message_handler(const Handler_id id) override;

  void remove_send_message_handler(const Handler_id id) override;

  XConnection &get_connection() override { return *m_connection; }

  XError send(const Client_message_type_id mid, const Message &msg) override;
  XError send_compressed_frame(const Client_message_type_id mid,
                               const Message &msg) override;
  XError send_compressed_multiple_frames(
      const std::vector<std::pair<Client_message_type_id, const Message *>>
          &messages) override;

  XError send(const Header_message_type_id mid, const uint8_t *buffer,
              const std::size_t length) override;

  // Overrides for Client Session Messages
  XError send(const Mysqlx::Session::AuthenticateStart &m) override {
    return send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, m);
  }

  XError send(const Mysqlx::Session::AuthenticateContinue &m) override {
    return send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE, m);
  }

  XError send(const Mysqlx::Session::Reset &m) override {
    return send(Mysqlx::ClientMessages::SESS_RESET, m);
  }

  XError send(const Mysqlx::Session::Close &m) override {
    return send(Mysqlx::ClientMessages::SESS_CLOSE, m);
  }

  // Overrides for SQL Messages
  XError send(const Mysqlx::Sql::StmtExecute &m) override {
    return send(Mysqlx::ClientMessages::SQL_STMT_EXECUTE, m);
  }

  // Overrides for CRUD operations
  XError send(const Mysqlx::Crud::Find &m) override {
    return send(Mysqlx::ClientMessages::CRUD_FIND, m);
  }

  XError send(const Mysqlx::Crud::Insert &m) override {
    return send(Mysqlx::ClientMessages::CRUD_INSERT, m);
  }

  XError send(const Mysqlx::Crud::Update &m) override {
    return send(Mysqlx::ClientMessages::CRUD_UPDATE, m);
  }

  XError send(const Mysqlx::Crud::Delete &m) override {
    return send(Mysqlx::ClientMessages::CRUD_DELETE, m);
  }

  XError send(const Mysqlx::Crud::CreateView &m) override {
    return send(Mysqlx::ClientMessages::CRUD_CREATE_VIEW, m);
  }

  XError send(const Mysqlx::Crud::ModifyView &m) override {
    return send(Mysqlx::ClientMessages::CRUD_MODIFY_VIEW, m);
  }

  XError send(const Mysqlx::Crud::DropView &m) override {
    return send(Mysqlx::ClientMessages::CRUD_DROP_VIEW, m);
  }

  // Overrides for Connection
  XError send(const Mysqlx::Connection::CapabilitiesGet &m) override {
    return send(Mysqlx::ClientMessages::CON_CAPABILITIES_GET, m);
  }

  XError send(const Mysqlx::Connection::CapabilitiesSet &m) override {
    return send(Mysqlx::ClientMessages::CON_CAPABILITIES_SET, m);
  }

  XError send(const Mysqlx::Connection::Close &m) override {
    return send(Mysqlx::ClientMessages::CON_CLOSE, m);
  }

  XError send(const Mysqlx::Expect::Open &m) override {
    return send(Mysqlx::ClientMessages::EXPECT_OPEN, m);
  }

  XError send(const Mysqlx::Expect::Close &m) override {
    return send(Mysqlx::ClientMessages::EXPECT_CLOSE, m);
  }

  XError send(const Mysqlx::Cursor::Open &m) override {
    return send(Mysqlx::ClientMessages::CURSOR_OPEN, m);
  }

  XError send(const Mysqlx::Cursor::Close &m) override {
    return send(Mysqlx::ClientMessages::CURSOR_CLOSE, m);
  }

  XError send(const Mysqlx::Cursor::Fetch &m) override {
    return send(Mysqlx::ClientMessages::CURSOR_FETCH, m);
  }

  XError send(const Mysqlx::Prepare::Prepare &m) override {
    return send(Mysqlx::ClientMessages::PREPARE_PREPARE, m);
  }

  XError send(const Mysqlx::Prepare::Execute &m) override {
    return send(Mysqlx::ClientMessages::PREPARE_EXECUTE, m);
  }

  XError send(const Mysqlx::Prepare::Deallocate &m) override {
    return send(Mysqlx::ClientMessages::PREPARE_DEALLOCATE, m);
  }

  XError recv(Header_message_type_id *out_mid, uint8_t **buffer,
              std::size_t *buffer_size) override;

  std::unique_ptr<Message> deserialize_received_message(
      const Header_message_type_id mid, const uint8_t *payload,
      const std::size_t payload_size, XError *out_error) override;

  std::unique_ptr<Message> recv_single_message(Server_message_type_id *out_mid,
                                               XError *out_error) override;

  XError recv_ok() override;

  std::unique_ptr<XQuery_result> recv_resultset() override;
  std::unique_ptr<XQuery_result> recv_resultset(XError *out_error) override;

  XError execute_close() override;
  std::unique_ptr<XQuery_result> execute_with_resultset(
      const Client_message_type_id mid, const Message &msg,
      XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_stmt(const Mysqlx::Sql::StmtExecute &m,
                                              XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_find(const Mysqlx::Crud::Find &m,
                                              XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_update(const Mysqlx::Crud::Update &m,
                                                XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_insert(const Mysqlx::Crud::Insert &m,
                                                XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_delete(const Mysqlx::Crud::Delete &m,
                                                XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_prep_stmt(
      const Mysqlx::Prepare::Execute &m, XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_cursor_open(
      const Mysqlx::Cursor::Open &m, XError *out_error) override;

  std::unique_ptr<XQuery_result> execute_cursor_fetch(
      const Mysqlx::Cursor::Fetch &m,
      std::unique_ptr<XQuery_result> cursor_open_result,
      XError *out_error) override;

  std::unique_ptr<Capabilities> execute_fetch_capabilities(
      XError *out_error) override;

  XError execute_set_capability(
      const Mysqlx::Connection::CapabilitiesSet &capabilities_set) override;

  XError execute_authenticate(const std::string &user, const std::string &pass,
                              const std::string &schema,
                              const std::string &method = "") override;

  void use_compression(const Compression_algorithm algo) override;
  void use_compression(const Compression_algorithm algo,
                       const int32_t level) override;

  void reset_buffering() override;

 private:
  using CodedInputStream = google::protobuf::io::CodedInputStream;
  template <typename Handler>
  class Handler_with_id {
   public:
    Handler_with_id(const Handler_id id, const int priority,
                    const Handler handler)
        : m_id(id), m_priority(priority), m_handler(handler) {}

    Handler_id m_id;
    int m_priority;
    Handler m_handler;

    static bool compare(const Handler_with_id &lhs,
                        const Handler_with_id &rhs) {
      return lhs.m_priority < rhs.m_priority;
    }
  };

  using ZeroCopyInputStream = google::protobuf::io::ZeroCopyInputStream;
  using ZeroCopyOutputStream = google::protobuf::io::ZeroCopyOutputStream;

  using Notice_handler_with_id = Handler_with_id<Notice_handler>;
  using Server_handler_with_id = Handler_with_id<Server_message_handler>;
  using Client_handler_with_id = Handler_with_id<Client_message_handler>;
  using Sid = Mysqlx::ServerMessages;

 private:
  template <typename Message_type>
  std::unique_ptr<XQuery_result> execute(const Message_type &message,
                                         XError *out_error) {
    *out_error = send(message);

    if (*out_error) return {};

    return recv_resultset(out_error);
  }

  std::unique_ptr<XProtocol::Message> deserialize_message(
      const Header_message_type_id mid, CodedInputStream *input_stream,
      XError *out_error);
  std::unique_ptr<Message> alloc_message(const Header_message_type_id mid);

  XError recv_id(const XProtocol::Server_message_type_id id);
  Message *recv_id(const XProtocol::Server_message_type_id id,
                   XError *out_error);
  XError recv_header(Header_message_type_id *out_mid,
                     uint32_t *out_buffer_size);
  Message *recv_payload(const Server_message_type_id mid, const uint32_t msglen,
                        XError *out_error);
  Message *recv_message_with_header(Server_message_type_id *out_mid,
                                    XError *out_error);

  template <typename Auth_continue_handler>
  XError authenticate_challenge_response(const std::string &user,
                                         const std::string &pass,
                                         const std::string &db);

  XError authenticate_plain(const std::string &user, const std::string &pass,
                            const std::string &db);
  XError authenticate_mysql41(const std::string &user, const std::string &pass,
                              const std::string &db);
  XError authenticate_sha256_memory(const std::string &user,
                                    const std::string &pass,
                                    const std::string &db);

  /**
    Dispatch notice to each registered handler. If the handler processed the
    message it should return "Handler_consumed" to stop dispatching to other
    handlers. Latest pushed handlers should be called first (called in
    reversed-pushed-order)
  */
  Handler_result dispatch_received_notice(const Mysqlx::Notice::Frame &frame);

  /**
    Dispatch received messages to each registered handler. If the handler
    processed the message it should return "Handler_consumed" to stop
    dispatching to other handlers. Latest pushed handlers should be called first
    (called in reversed-pushed-order).
  */
  Handler_result dispatch_received_message(const Server_message_type_id id,
                                           const Message &message);

  /**
    Method that handles both X Protocol messages and notices.
  */
  XError dispatch_received(const Server_message_type_id id,
                           const Message &message, bool *out_ignore);

  /** Dispatch send message to each registered handler.
   Latest pushed handlers should be called first (called in
   reversed-pushed-order)*/
  void dispatch_send_message(const Client_message_type_id id,
                             const Message &message);

  Message *read_compressed(Server_message_type_id *mid, XError *out_error);
  void skip_not_parsed(CodedInputStream *input_stream, XError *out_error);
  bool send_impl(const Client_message_type_id mid, const Message &msg,
                 ZeroCopyOutputStream *input_stream);

  Protocol_factory *m_factory;
  Handler_id m_last_handler_id{0};
  Priority_list<Notice_handler_with_id> m_notice_handlers;
  Priority_list<Client_handler_with_id> m_message_send_handlers;
  Priority_list<Server_handler_with_id> m_message_received_handlers;
  std::unique_ptr<Query_instances> m_query_instances;
  std::shared_ptr<Context> m_context;

  std::unique_ptr<XConnection> m_connection;
  std::shared_ptr<Connection_input_stream> m_connection_input_stream;
  std::shared_ptr<ZeroCopyInputStream> m_compressed_payload_input_stream;
  std::shared_ptr<ZeroCopyInputStream> m_compressed_input_stream;
  std::vector<uint8_t> m_static_recv_buffer;

  z_stream m_out_stream;
  std::unique_ptr<XCompression> m_compression;
  Mysqlx::Connection::Compression m_compressed;
  Server_message_type_id m_compression_inner_message_id{Sid::COMPRESSION};
};

template <typename Auth_continue_handler>
XError Protocol_impl::authenticate_challenge_response(const std::string &user,
                                                      const std::string &pass,
                                                      const std::string &db) {
  Auth_continue_handler auth_continue_handler(this);
  XError error;

  {
    Mysqlx::Session::AuthenticateStart auth;

    auth.set_mech_name(auth_continue_handler.get_name());

    error = send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, auth);

    if (error) return error;
  }

  {
    std::unique_ptr<Message> message{
        recv_id(::Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE, &error)};

    if (error) return error;

    auto &auth_continue =
        *static_cast<Mysqlx::Session::AuthenticateContinue *>(message.get());

    error = auth_continue_handler(user, pass, db, auth_continue);

    if (error) return error;
  }

  {
    std::unique_ptr<Message> message{
        recv_id(::Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK, &error)};

    if (error) return error;
  }

  return {};
}

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XPROTOCOL_IMPL_H_
