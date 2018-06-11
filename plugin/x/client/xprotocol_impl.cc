/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/client/xprotocol_impl.h"

#include "my_config.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "errmsg.h"
#include "my_io.h"
#include "mysql_com.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/mysqlxclient/xrow.h"
#include "plugin/x/client/password_hasher.h"
#include "plugin/x/client/sha256_scramble_generator.h"
#include "plugin/x/generated/mysqlx_version.h"
#include "sha2.h"

namespace xcl {

const char *const ERR_MSG_INVALID_AUTH_METHOD =
    "Invalid authentication method ";
const char *const ERR_MSG_UNEXPECTED_MESSAGE =
    "Unexpected response received from server";
const char *const ERR_MSG_MESSAGE_NOT_INITIALIZED =
    "Message is not properly initialized: ";
const char *const ER_TEXT_HASHING_FUNCTION_FAILED =
    "Invalid result while calculating hash";
const char *const ER_TEXT_PB_SERIALIZATION_FAILED =
    "Invalid message was passed to 'protobuf', serialization failed";
const char *const ER_TEXT_DATA_TOO_LARGE =
    "Messages payload size exceeded the the value that message header can hold";
const char *const ER_TEXT_RECEIVE_HANDLER_FAILED =
    "Aborted by internal callback at received message processing";
const char *const ER_TEXT_NOTICE_HANDLER_FAILED =
    "Aborted by internal callback at send message processing";
const char *const ER_TEXT_RECEIVE_BUFFER_TO_SMALL = "Receive buffer to small";

namespace details {

XError make_xerror(const Mysqlx::Error &error) {
  bool is_fatal = error.severity() == Mysqlx::Error::FATAL;
  return XError{static_cast<int>(error.code()), error.msg(), is_fatal};
}

class Query_sequencer : public Query_instances {
 public:
  Instance_id instances_fetch_begin() override { return m_last_instance++; }

  void instances_fetch_end() override { ++m_current_instance; }

  bool is_instance_active(const Instance_id id) override {
    return id == m_current_instance;
  }

 private:
  Instance_id m_current_instance{0};
  Instance_id m_last_instance{0};
};

}  // namespace details

Protocol_impl::Protocol_impl(std::shared_ptr<Context> context,
                             Protocol_factory *factory)
    : m_factory(factory), m_context(context) {
  assert(nullptr != factory);
  m_sync_connection = factory->create_connection(context);
  m_query_instances.reset(new details::Query_sequencer);
  m_static_recv_buffer.resize(VIO_READ_BUFFER_SIZE);
}

XError Protocol_impl::execute_set_capability(
    const Mysqlx::Connection::CapabilitiesSet &capabilities_set) {
  auto result = send(capabilities_set);

  if (result) return result;

  return recv_ok();
}

XError Protocol_impl::execute_authenticate(const std::string &user,
                                           const std::string &pass,
                                           const std::string &schema,
                                           const std::string &method) {
  XError error;

  if (method == "PLAIN")
    error = authenticate_plain(user, pass, schema);
  else if (method == "MYSQL41")
    error = authenticate_mysql41(user, pass, schema);
  else if (method == "SHA256_MEMORY")
    error = authenticate_sha256_memory(user, pass, schema);
  else
    return XError(CR_X_INVALID_AUTH_METHOD,
                  ERR_MSG_INVALID_AUTH_METHOD + method);

  return error;
}

std::unique_ptr<XProtocol::Capabilities>
Protocol_impl::execute_fetch_capabilities(XError *out_error) {
  *out_error = send(Mysqlx::Connection::CapabilitiesGet());

  if (*out_error) return {};

  std::unique_ptr<Message> message(
      recv_id(Mysqlx::ServerMessages::CONN_CAPABILITIES, out_error));

  if (*out_error) return {};

  return std::unique_ptr<XProtocol::Capabilities>{
      static_cast<Mysqlx::Connection::Capabilities *>(message.release())};
}

XError Protocol_impl::execute_close() {
  XError error = send(Mysqlx::Session::Close());

  if (error) return error;

  error = recv_ok();

  return error;
}

std::unique_ptr<XQuery_result> Protocol_impl::recv_resultset() {
  return m_factory->create_result(shared_from_this(), m_query_instances.get(),
                                  m_context);
}

std::unique_ptr<XQuery_result> Protocol_impl::recv_resultset(
    XError *out_error) {
  if (m_context->m_global_error) {
    *out_error = m_context->m_global_error;

    return {};
  }

  auto result = recv_resultset();

  result->get_metadata(out_error);

  if (*out_error) return nullptr;

  return result;
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_with_resultset(
    const Client_message_type_id mid, const Message &msg, XError *out_error) {
  *out_error = send(mid, msg);

  if (*out_error) return {};

  return recv_resultset(out_error);
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_stmt(
    const Mysqlx::Sql::StmtExecute &m, XError *out_error) {
  return execute(m, out_error);
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_find(
    const Mysqlx::Crud::Find &m, XError *out_error) {
  return execute(m, out_error);
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_update(
    const Mysqlx::Crud::Update &m, XError *out_error) {
  return execute(m, out_error);
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_insert(
    const Mysqlx::Crud::Insert &m, XError *out_error) {
  return execute(m, out_error);
}

std::unique_ptr<XQuery_result> Protocol_impl::execute_delete(
    const Mysqlx::Crud::Delete &m, XError *out_error) {
  return execute(m, out_error);
}

XError Protocol_impl::authenticate_mysql41(const std::string &user,
                                           const std::string &pass,
                                           const std::string &db) {
  class Mysql41_continue_handler {
   public:
    explicit Mysql41_continue_handler(Protocol_impl *protocol)
        : m_protocol(protocol) {}

    std::string get_name() const { return "MYSQL41"; }

    XError operator()(
        const std::string &user, const std::string &pass, const std::string &db,
        const Mysqlx::Session::AuthenticateContinue &auth_continue) {
      std::string data;
      std::string password_hash;

      Mysqlx::Session::AuthenticateContinue auth_continue_response;

      if (pass.length()) {
        password_hash = password_hasher::scramble(
            auth_continue.auth_data().c_str(), pass.c_str());
        password_hash = password_hasher::get_password_from_salt(password_hash);

        if (password_hash.empty()) {
          return XError{CR_UNKNOWN_ERROR, ER_TEXT_HASHING_FUNCTION_FAILED};
        }
      }

      data.append(db).push_back('\0');    // authz
      data.append(user).push_back('\0');  // authc
      data.append(password_hash);         // pass
      auth_continue_response.set_auth_data(data);

      return m_protocol->send(auth_continue_response);
    }

   private:
    Protocol_impl *m_protocol;
  };

  return authenticate_challenge_response<Mysql41_continue_handler>(user, pass,
                                                                   db);
}

XError Protocol_impl::authenticate_sha256_memory(const std::string &user,
                                                 const std::string &pass,
                                                 const std::string &db) {
  class Sha256_memory_continue_handler {
   public:
    explicit Sha256_memory_continue_handler(Protocol_impl *protocol)
        : m_protocol(protocol) {}

    std::string get_name() const { return "SHA256_MEMORY"; }

    XError operator()(
        const std::string &user, const std::string &pass, const std::string &db,
        const Mysqlx::Session::AuthenticateContinue &auth_continue) {
      Mysqlx::Session::AuthenticateContinue auth_continue_response;

      auto nonce = auth_continue.auth_data();
      char sha256_scramble[SHA256_DIGEST_LENGTH] = {0};
      if (xcl::generate_sha256_scramble(
              reinterpret_cast<unsigned char *>(sha256_scramble),
              SHA256_DIGEST_LENGTH, pass.c_str(), pass.length(), nonce.c_str(),
              nonce.length()))
        return XError{CR_UNKNOWN_ERROR, ER_TEXT_HASHING_FUNCTION_FAILED};

      std::string scramble_hex(2 * SHA256_DIGEST_LENGTH + 1, '\0');
      password_hasher::octet2hex(&scramble_hex[0], &sha256_scramble[0],
                                 SHA256_DIGEST_LENGTH);
      scramble_hex
          .pop_back();  // Skip the additional \0 sign added by octet2hex

      std::string data;
      data.append(db).push_back('\0');
      data.append(user).push_back('\0');
      data.append(scramble_hex);
      auth_continue_response.set_auth_data(data);

      return m_protocol->send(auth_continue_response);
    }

   private:
    Protocol_impl *m_protocol;
  };

  return authenticate_challenge_response<Sha256_memory_continue_handler>(
      user, pass, db);
}

XError Protocol_impl::authenticate_plain(const std::string &user,
                                         const std::string &pass,
                                         const std::string &db) {
  XError error;

  {
    Mysqlx::Session::AuthenticateStart auth;

    auth.set_mech_name("PLAIN");
    std::string data;

    data.append(db).push_back('\0');    // authz
    data.append(user).push_back('\0');  // authc
    data.append(pass);                  // pass

    auth.set_auth_data(data);
    error = send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, auth);
  }

  if (error) return error;

  return recv_id(Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK);
}

XError Protocol_impl::send(const Header_message_type_id mid,
                           const uint8_t *buffer,
                           const std::size_t buffer_length) {
  if (m_context->m_global_error) return m_context->m_global_error;

  union {
    uint8_t header[5];  // Must be properly aligned
    longlong dummy;
  };
  /*
    Use dummy, otherwise g++ 4.4 reports: unused variable 'dummy'
    MY_ATTRIBUTE((unused)) did not work, so we must use it.
  */
  dummy = 0;

  uint32_t *buf_ptr = reinterpret_cast<uint32_t *>(header);
  *buf_ptr = static_cast<uint32>(buffer_length + 1);
#ifdef WORDS_BIGENDIAN
  std::swap(header[0], header[3]);
  std::swap(header[1], header[2]);
#endif
  header[4] = mid;

  if (buffer_length + 1 > std::numeric_limits<uint32>::max())
    return XError{CR_MALFORMED_PACKET, ER_TEXT_DATA_TOO_LARGE};

  XError error = m_sync_connection->write(header, 5);
  if (!error) {
    if (0 != buffer_length)
      error = m_sync_connection->write(buffer, buffer_length);
  }

  return error;
}

XError Protocol_impl::send(const Client_message_type_id mid,
                           const Message &msg) {
  if (m_context->m_global_error) return m_context->m_global_error;

  std::string msg_buffer;
  const std::uint8_t header_size = 5;
  const std::size_t msg_size = msg.ByteSize();
  msg_buffer.resize(msg_size + header_size);

  if (msg_size > std::numeric_limits<uint32>::max() - header_size)
    return XError{CR_MALFORMED_PACKET, ER_TEXT_DATA_TOO_LARGE};

  dispatch_send_message(mid, msg);

  if (!msg.SerializeToArray(&msg_buffer[0] + header_size, msg_size)) {
    return XError{CR_MALFORMED_PACKET, ER_TEXT_PB_SERIALIZATION_FAILED};
  }

  const auto msg_size_to_buffer = static_cast<std::uint32_t>(msg_size + 1);

  memcpy(&msg_buffer[0], &msg_size_to_buffer, sizeof(std::uint32_t));
#ifdef WORDS_BIGENDIAN
  std::swap(msg_buffer[0], msg_buffer[3]);
  std::swap(msg_buffer[1], msg_buffer[2]);
#endif
  msg_buffer[4] = mid;

  return m_sync_connection->write(
      reinterpret_cast<const std::uint8_t *>(msg_buffer.data()),
      msg_buffer.size());
}

XProtocol::Handler_id Protocol_impl::add_notice_handler(
    Notice_handler handler, const Handler_position position,
    const Handler_priority priority) {
  const auto id = m_last_handler_id++;
  const auto prio = static_cast<int>(priority);

  switch (position) {
    case Handler_position::Begin:
      m_notice_handlers.emplace_front(id, prio, handler);
      break;

    case Handler_position::End:
      m_notice_handlers.emplace_back(id, prio, handler);
      break;
  }

  return id;
}

XProtocol::Handler_id Protocol_impl::add_received_message_handler(
    Server_message_handler handler, const Handler_position position,
    const Handler_priority priority) {
  const auto id = m_last_handler_id++;
  const auto prio = static_cast<int>(priority);

  switch (position) {
    case Handler_position::Begin:
      m_message_received_handlers.emplace_front(id, prio, handler);
      break;

    case Handler_position::End:
      m_message_received_handlers.emplace_back(id, prio, handler);
      break;
  }

  return id;
}

XProtocol::Handler_id Protocol_impl::add_send_message_handler(
    Client_message_handler handler, const Handler_position position,
    const Handler_priority priority) {
  const auto id = m_last_handler_id++;
  const auto prio = static_cast<int>(priority);

  switch (position) {
    case Handler_position::Begin:
      m_message_send_handlers.emplace_front(id, prio, handler);
      break;

    case Handler_position::End:
      m_message_send_handlers.emplace_back(id, prio, handler);
      break;
  }

  return id;
}

void Protocol_impl::remove_notice_handler(const Handler_id id) {
  const auto handler = std::find_if(
      m_notice_handlers.begin(), m_notice_handlers.end(),
      [id](const Handler_with_id<Notice_handler> &handler) -> bool {
        return id == handler.m_id;
      });

  if (handler == m_notice_handlers.end()) return;

  m_notice_handlers.erase(handler);
}

void Protocol_impl::remove_received_message_handler(const Handler_id id) {
  const auto handler = std::find_if(
      m_message_received_handlers.begin(), m_message_received_handlers.end(),
      [id](const Server_handler_with_id &handler) -> bool {
        return id == handler.m_id;
      });

  if (handler == m_message_received_handlers.end()) return;

  m_message_received_handlers.erase(handler);
}

void Protocol_impl::remove_send_message_handler(const Handler_id id) {
  const auto handler = std::find_if(
      m_message_send_handlers.begin(), m_message_send_handlers.end(),
      [id](const Client_handler_with_id &handler) -> bool {
        return id == handler.m_id;
      });

  if (handler == m_message_send_handlers.end()) return;

  m_message_send_handlers.erase(handler);
}

Handler_result Protocol_impl::dispatch_notice(
    const Mysqlx::Notice::Frame &frame) {
  for (const auto &holder : m_notice_handlers) {
    const Handler_result result = holder.m_handler(
        this, frame.scope() == Mysqlx::Notice::Frame_Scope_GLOBAL,
        static_cast<Mysqlx::Notice::Frame::Type>(frame.type()),
        frame.has_payload() ? frame.payload().c_str() : nullptr,
        frame.has_payload() ? static_cast<uint32>(frame.payload().length())
                            : 0);

    if (Handler_result::Continue != result) return result;
  }

  return Handler_result::Continue;
}

Handler_result Protocol_impl::dispatch_received_message(
    const Server_message_type_id id, const Message &message) {
  for (const auto &holder : m_message_received_handlers) {
    const Handler_result result = holder.m_handler(this, id, message);

    if (Handler_result::Continue != result) return result;
  }

  return Handler_result::Continue;
}

void Protocol_impl::dispatch_send_message(const Client_message_type_id id,
                                          const Message &message) {
  for (const auto &holder : m_message_send_handlers) {
    holder.m_handler(this, id, message);
  }
}

XError Protocol_impl::recv_ok() { return recv_id(Mysqlx::ServerMessages::OK); }

XError Protocol_impl::recv_header(Header_message_type_id *out_mid,
                                  std::size_t *out_buffer_size) {
  XError error;

  union {
    uint8_t header_buffer[5];  // Must be properly aligned
    uint32_t payload_size;
  };

  *out_mid = 0;

  error = m_sync_connection->read(header_buffer, 5);

  if (error) {
    return error;
  }

#ifdef WORDS_BIGENDIAN
  std::swap(header_buffer[0], header_buffer[3]);
  std::swap(header_buffer[1], header_buffer[2]);
#endif

  *out_buffer_size = payload_size - 1;
  *out_mid = header_buffer[4];

  return {};
}

XError Protocol_impl::recv(Header_message_type_id *out_mid, uint8_t **buffer,
                           std::size_t *buffer_size) {
  std::unique_ptr<uint8_t[]> payload_buffer;
  std::size_t msglen = 0;
  XError error = recv_header(out_mid, &msglen);

  if (error) {
    return error;
  }

  if (*buffer && *buffer_size < msglen) {
    return XError{CR_X_RECEIVE_BUFFER_TO_SMALL,
                  ER_TEXT_RECEIVE_BUFFER_TO_SMALL};
  }

  if (0 < msglen) {
    uint8_t *payload = *buffer;

    if (nullptr == payload) {
      payload_buffer.reset(new uint8_t[msglen]);
      payload = payload_buffer.get();
    }

    error = m_sync_connection->read(payload, msglen);
  }

  if (error) {
    return error;
  }

  if (payload_buffer) {
    *buffer = payload_buffer.release();
  }

  *buffer_size = msglen;

  return {};
}

std::unique_ptr<XProtocol::Message> Protocol_impl::deserialize_received_message(
    const Header_message_type_id mid, const uint8_t *payload,
    const std::size_t payload_size, XError *out_error) {
  std::unique_ptr<Message> ret_val;

  switch (static_cast<Mysqlx::ServerMessages::Type>(mid)) {
    case Mysqlx::ServerMessages::OK:
      ret_val.reset(new Mysqlx::Ok());
      break;
    case Mysqlx::ServerMessages::ERROR:
      ret_val.reset(new Mysqlx::Error());
      break;
    case Mysqlx::ServerMessages::NOTICE:
      ret_val.reset(new Mysqlx::Notice::Frame());
      break;
    case Mysqlx::ServerMessages::CONN_CAPABILITIES:
      ret_val.reset(new Mysqlx::Connection::Capabilities());
      break;
    case Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE:
      ret_val.reset(new Mysqlx::Session::AuthenticateContinue());
      break;
    case Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK:
      ret_val.reset(new Mysqlx::Session::AuthenticateOk());
      break;
    case Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA:
      ret_val.reset(new Mysqlx::Resultset::ColumnMetaData());
      break;
    case Mysqlx::ServerMessages::RESULTSET_ROW:
      ret_val.reset(new Mysqlx::Resultset::Row());
      break;
    case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE:
      ret_val.reset(new Mysqlx::Resultset::FetchDone());
      break;
    case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS:
      ret_val.reset(new Mysqlx::Resultset::FetchDoneMoreResultsets());
      break;
    case Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK:
      ret_val.reset(new Mysqlx::Sql::StmtExecuteOk());
      break;
    case Mysqlx::ServerMessages::RESULTSET_FETCH_SUSPENDED:
      break;
    case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_OUT_PARAMS:
      ret_val.reset(new Mysqlx::Resultset::FetchDoneMoreOutParams());
      break;
  }

  if (!ret_val) {
    std::stringstream ss;
    ss << ERR_MSG_UNEXPECTED_MESSAGE;
    ss << mid;
    *out_error = XError(CR_MALFORMED_PACKET, ss.str());

    return {};
  }

  // Parses the received message
  ret_val->ParseFromArray(reinterpret_cast<const char *>(payload),
                          static_cast<int>(payload_size));

  if (!ret_val->IsInitialized()) {
    std::string err(ERR_MSG_MESSAGE_NOT_INITIALIZED);
    err += ret_val->InitializationErrorString();
    *out_error = XError(CR_MALFORMED_PACKET, err);

    return {};
  }

  return ret_val;
}

std::unique_ptr<XProtocol::Message> Protocol_impl::recv_single_message(
    Server_message_type_id *out_mid, XError *out_error) {
  if (m_context->m_global_error) {
    *out_error = m_context->m_global_error;
    return {};
  }

  *out_error = XError();

  while (true) {
    std::unique_ptr<Message> msg(recv_message_with_header(out_mid, out_error));

    if (*out_error) return {};

    const Handler_result result =
        dispatch_received_message(*out_mid, *msg.get());

    if (Handler_result::Consumed == result) continue;

    if (Handler_result::Error == result) {
      *out_error =
          XError{CR_X_INTERNAL_ABORTED, ER_TEXT_RECEIVE_HANDLER_FAILED};

      return {};
    }

    if (Mysqlx::ServerMessages::NOTICE == *out_mid) {
      auto frame = static_cast<Mysqlx::Notice::Frame *>(msg.get());
      const Handler_result notice_ext_handled = dispatch_notice(*frame);

      if (Handler_result::Consumed == notice_ext_handled) continue;

      if (Handler_result::Error == notice_ext_handled) {
        *out_error =
            XError{CR_X_INTERNAL_ABORTED, ER_TEXT_NOTICE_HANDLER_FAILED};

        return {};
      }
    }

    return msg;
  }
}

XError Protocol_impl::recv_id(
    const XProtocol::Server_message_type_id expected_id) {
  XError out_error;
  Server_message_type_id out_mid;
  std::unique_ptr<Message> msg(recv_single_message(&out_mid, &out_error));

  if (out_error) return out_error;

  if (Mysqlx::ServerMessages::ERROR == out_mid) {
    const ::Mysqlx::Error &error = *static_cast<Mysqlx::Error *>(msg.get());

    return details::make_xerror(error);
  }

  if (expected_id != out_mid) {
    std::stringstream ss;
    ss << "Unknown message received from server ";
    ss << out_mid;

    return XError{CR_MALFORMED_PACKET, ss.str()};
  }

  return {};
}

XProtocol::Message *Protocol_impl::recv_id(
    const XProtocol::Server_message_type_id expected_id, XError *out_error) {
  Server_message_type_id out_mid;
  std::unique_ptr<Message> msg(recv_single_message(&out_mid, out_error));

  if (*out_error) return nullptr;

  if (Mysqlx::ServerMessages::ERROR == out_mid) {
    const ::Mysqlx::Error &error = *static_cast<Mysqlx::Error *>(msg.get());

    *out_error = details::make_xerror(error);
    return nullptr;
  }

  if (expected_id != out_mid) {
    std::stringstream ss;
    ss << "Unknown message received from server ";
    ss << out_mid;

    *out_error = XError{CR_MALFORMED_PACKET, ss.str()};
    return nullptr;
  }

  return msg.release();
}

XProtocol::Message *Protocol_impl::recv_message_with_header(
    Server_message_type_id *mid, XError *out_error) {
  std::size_t payload_size = 0;
  Header_message_type_id header_mid;
  *out_error = recv_header(&header_mid, &payload_size);
  if (*out_error) return nullptr;

  std::unique_ptr<std::uint8_t[]> allocated_payload_buffer;
  std::uint8_t *payload = nullptr;
  if (payload_size > 0) {
    if (payload_size > m_static_recv_buffer.size()) {
      allocated_payload_buffer.reset(new uint8_t[payload_size]);
      payload = allocated_payload_buffer.get();
    } else {
      payload = &m_static_recv_buffer[0];
    }
    *out_error = m_sync_connection->read(payload, payload_size);
    if (*out_error) return nullptr;
  }

  *mid = static_cast<Server_message_type_id>(header_mid);

  return deserialize_received_message(header_mid, payload, payload_size,
                                      out_error)
      .release();
}

}  // namespace xcl
