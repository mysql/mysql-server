/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/connector/session_holder.h"

#include <array>

#include "my_dbug.h"         // NOLINT(build/include_subdir)
#include "template_utils.h"  // NOLINT(build/include_subdir)

#include "plugin/x/protocol/stream/compression/compression_algorithm_lz4.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zlib.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_zstd.h"
#include "plugin/x/tests/driver/connector/mysqlx_all_msgs.h"

namespace {

std::string get_ip_mode_to_text(const xcl::Internet_protocol ip) {
  switch (ip) {
    case xcl::Internet_protocol::V4:
      return "IP4";

    case xcl::Internet_protocol::V6:
      return "IP6";

    case xcl::Internet_protocol::Any:
    default:
      return "ANY";
  }
}

size_t get_message_size(const xcl::XProtocol::Message &msg) {
#if (defined(GOOGLE_PROTOBUF_VERSION) && GOOGLE_PROTOBUF_VERSION > 3000000)
  return msg.ByteSizeLong() + 1;
#else
  return msg.ByteSize() + 1;
#endif
}

template <typename Msg = xcl::XProtocol::Message>
void print_message(const Console &console, const std::string &direction,
                   const size_t size, const Msg &msg) {
  console.print(direction, size, " ", msg);
}

}  // namespace

const int k_maximum_size_of_message_in_flow_history = 1000;

Session_holder::Session_holder(std::unique_ptr<xcl::XSession> session,
                               const Console &console_with_flow_history,
                               const Console &console,
                               const Connection_options &options)
    : m_session(std::move(session)),
      m_console_with_flow_history(console_with_flow_history),
      m_console(console),
      m_options(options) {
  const auto env_variable_enable_tracing = getenv("MYSQLX_TRACE_CONNECTION");
  m_enable_tracing_in_console =
      m_options.trace_protocol ||
      (env_variable_enable_tracing && strlen(env_variable_enable_tracing));
}

bool Session_holder::enable_compression(
    const xcl::Compression_algorithm algorithm, const int64_t level) {
  m_algorithm.reset();

  get_session()->get_protocol().use_compression(algorithm, level);

  switch (algorithm) {
    case xcl::Compression_algorithm::k_lz4:
      m_algorithm.reset(new protocol::Compression_algorithm_lz4(level));
      break;

    case xcl::Compression_algorithm::k_deflate:
      m_algorithm.reset(new protocol::Compression_algorithm_zlib(level));
      break;

    case xcl::Compression_algorithm::k_zstd:
      m_algorithm.reset(new protocol::Compression_algorithm_zstd(level));
      break;

    default:
    case xcl::Compression_algorithm::k_none:
      return false;
  }

  return true;
}

protocol::Compression_algorithm_interface *Session_holder::get_algorithm() {
  return m_algorithm.get();
}

xcl::XSession *Session_holder::get_session() { return m_session.get(); }

xcl::XError Session_holder::connect(const bool is_raw_connection) {
  DBUG_TRACE;
  setup_compression();

  setup_ssl();
  setup_msg_callbacks();
  setup_other_options();

  m_is_raw_connection = is_raw_connection;

  return reconnect();
}

xcl::XError Session_holder::reconnect() {
  if (m_is_raw_connection) return setup_connection();

  return setup_session();
}

void Session_holder::clear_received_messages() {
  m_received_msg_counters.clear();
}

bool Session_holder::try_get_number_of_received_messages(
    const std::string message_name, uint64_t *value) const {
  assert(nullptr != value);
  if (!m_received_msg_counters.count(message_name)) return false;

  *value = m_received_msg_counters.at(message_name);

  return true;
}

xcl::XError Session_holder::setup_session() {
  DBUG_TRACE;
  xcl::XError error;

  if (m_options.socket.empty()) {
    error = m_session->connect(
        m_options.host.c_str(), m_options.port, m_options.user.c_str(),
        m_options.password.c_str(), m_options.schema.c_str());
  } else {
    error = m_session->connect(m_options.socket.c_str(), m_options.user.c_str(),
                               m_options.password.c_str(),
                               m_options.schema.c_str());
  }

  return error;
}

xcl::XError Session_holder::setup_connection() {
  xcl::XError error;
  auto &protocol = m_session->get_protocol();
  auto &connection = protocol.get_connection();

  if (connection.state().is_connected())
    return xcl::XError{CR_ALREADY_CONNECTED, "Already connected"};

  if (m_options.socket.empty()) {
    error = connection.connect(m_options.host.c_str(), m_options.port,
                               m_options.ip_mode);
  } else {
    error = connection.connect_to_localhost(m_options.socket.c_str());
  }

  return error;
}

void Session_holder::setup_other_options() {
  DBUG_TRACE;
  using Mysqlx_option = xcl::XSession::Mysqlx_option;
  const auto text_ip_mode = get_ip_mode_to_text(m_options.ip_mode);

  if (m_options.compatible) {
    m_session->set_mysql_option(Mysqlx_option::Authentication_method,
                                "FALLBACK");
  }

  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Hostname_resolve_to,
                              text_ip_mode);

  if (!m_options.auth_methods.empty())
    m_session->set_mysql_option(Mysqlx_option::Authentication_method,
                                m_options.auth_methods);
}

void Session_holder::setup_compression() {
  DBUG_TRACE;
  using Mysqlx_option = xcl::XSession::Mysqlx_option;

  if (m_options.compression_mode.empty()) return;

  xcl::XError error;
  error = m_session->set_mysql_option(
      Mysqlx_option::Compression_negotiation_mode, m_options.compression_mode);
  if (error)
    throw xcl::XError{CR_X_UNSUPPORTED_OPTION_VALUE,
                      "Unsupported value for \"compression-mode\" option"};

  error = m_session->set_mysql_option(Mysqlx_option::Compression_algorithms,
                                      m_options.compression_algorithm);
  if (error)
    throw xcl::XError{CR_X_UNSUPPORTED_OPTION_VALUE,
                      "Unsupported value for \"compression-algorithm\" option"};

  error = m_session->set_mysql_option(
      Mysqlx_option::Compression_combine_mixed_messages,
      m_options.compression_combine_mixed_messages);
  if (error)
    throw xcl::XError{
        CR_X_UNSUPPORTED_OPTION_VALUE,
        "Unsupported value for \"compression-combine-mixed-messages\" option"};

  error = m_session->set_mysql_option(
      Mysqlx_option::Compression_max_combine_messages,
      m_options.compression_max_combine_messages);
  if (error)
    throw xcl::XError{
        CR_X_UNSUPPORTED_OPTION_VALUE,
        "Unsupported value for \"compression-max-combine-messages\" option"};

  if (m_options.compression_level.has_value()) {
    error = m_session->set_mysql_option(
        Mysqlx_option::Compression_level_server,
        static_cast<int64_t>(m_options.compression_level.value()));
    if (error)
      throw xcl::XError{CR_X_UNSUPPORTED_OPTION_VALUE,
                        "Unsupported value for \"compression-level\" option"};

    error = m_session->set_mysql_option(
        Mysqlx_option::Compression_level_client,
        static_cast<int64_t>(m_options.compression_level.value()));
    if (error)
      throw xcl::XError{CR_X_UNSUPPORTED_OPTION_VALUE,
                        "Unsupported value for \"compression-level\" option"};
  }
}

void Session_holder::setup_ssl() {
  DBUG_TRACE;

  auto error = m_session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Ssl_fips_mode,
      m_options.ssl_fips_mode.empty() ? "off" : m_options.ssl_fips_mode);

  if (error) throw error;

  auto ssl_mode = m_options.ssl_mode;
  if (ssl_mode.empty()) {
    if (m_options.is_ssl_set())
      ssl_mode = "REQUIRED";
    else
      ssl_mode = "DISABLED";
  }

  error = m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_mode,
                                      ssl_mode);

  if (error) throw error;

  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_ca,
                              m_options.ssl_ca);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_ca_path,
                              m_options.ssl_ca_path);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_cert,
                              m_options.ssl_cert);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_cipher,
                              m_options.ssl_cipher);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Ssl_key,
                              m_options.ssl_key);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Allowed_tls,
                              m_options.allowed_tls);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Read_timeout,
                              m_options.io_timeout);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Write_timeout,
                              m_options.io_timeout);
  m_session->set_mysql_option(
      xcl::XSession::Mysqlx_option::Session_connect_timeout,
      m_options.session_connect_timeout);
  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Network_namespace,
                              m_options.network_namespace);
}

void Session_holder::setup_msg_callbacks() {
  auto &protocol = m_session->get_protocol();

  m_session->set_mysql_option(xcl::XSession::Mysqlx_option::Consume_all_notices,
                              false);

  m_handler_id = protocol.add_notice_handler(
      [this](const xcl::XProtocol *protocol, const bool is_global,
             const Frame_type type, const char *data,
             const uint32_t data_length) -> xcl::Handler_result {
        return dump_notices(protocol, is_global, type, data, data_length);
      });

  protocol.add_received_message_handler(
      [this](xcl::XProtocol *protocol,
             const xcl::XProtocol::Server_message_type_id msg_id,
             const xcl::XProtocol::Message &msg) -> xcl::Handler_result {
        return count_received_messages(protocol, msg_id, msg);
      });

  protocol.add_received_message_handler(
      [this](xcl::XProtocol *protocol,
             const xcl::XProtocol::Server_message_type_id msg_id,
             const xcl::XProtocol::Message &msg) -> xcl::Handler_result {
        print_message_to_consoles("<<<< RECEIVE ", msg);

        DBUG_LOG("debug", "log message "
                              << "recv: " << msg);
        return xcl::Handler_result::Continue;
      });

  protocol.add_send_message_handler(
      [this](xcl::XProtocol *protocol,
             const xcl::XProtocol::Client_message_type_id msg_id,
             const xcl::XProtocol::Message &msg) -> xcl::Handler_result {
        print_message_to_consoles(">>>> SEND ", msg);

        DBUG_LOG("debug", "log message "
                              << "send: " << msg);
        /** None of processed messages should be filtered out*/
        return xcl::Handler_result::Continue;
      });
}

void Session_holder::remove_notice_handler() {
  if (m_handler_id >= 0) {
    m_session->get_protocol().remove_notice_handler(m_handler_id);
  }
}

xcl::Handler_result Session_holder::count_received_messages(
    xcl::XProtocol *protocol,
    const xcl::XProtocol::Server_message_type_id msg_id,
    const xcl::XProtocol::Message &msg) {
  const auto protobuf_message_name = msg.GetDescriptor()->full_name();
  const auto server_message_name =
      Mysqlx::ServerMessages::descriptor()->full_name();
  const bool is_empty_message = (protobuf_message_name == server_message_name);
  const std::string &msg_name = !is_empty_message
                                    ? msg.GetDescriptor()->full_name()
                                    : server_msgs_by_id[msg_id].second;

  ++m_received_msg_counters[msg_name];

  if (msg_name != Mysqlx::Notice::Frame::descriptor()->full_name())
    return xcl::Handler_result::Continue;

  static const std::array<std::string, 5> k_notice_type_id = {
      Mysqlx::Notice::Warning::descriptor()->full_name(),
      Mysqlx::Notice::SessionVariableChanged::descriptor()->full_name(),
      Mysqlx::Notice::SessionStateChanged::descriptor()->full_name(),
      Mysqlx::Notice::GroupReplicationStateChanged::descriptor()->full_name(),
      Mysqlx::Notice::ServerHello::descriptor()->full_name(),
  };

  const auto notice_type =
      static_cast<const Mysqlx::Notice::Frame *>(&msg)->type() - 1u;
  if (notice_type < k_notice_type_id.size())
    ++m_received_msg_counters[k_notice_type_id[notice_type]];

  /** None of processed messages should be filtered out*/
  return xcl::Handler_result::Continue;
}

xcl::Handler_result Session_holder::dump_notices(const xcl::XProtocol *protocol,
                                                 const bool is_global,
                                                 const Frame_type type,
                                                 const char *data,
                                                 const uint32_t data_length) {
  if (type == Frame_type::Frame_Type_SESSION_STATE_CHANGED) {
    Mysqlx::Notice::SessionStateChanged change;

    change.ParseFromArray(data, data_length);

    if (!change.IsInitialized()) {
      m_console.print_error("Invalid notice received from server ",
                            change.InitializationErrorString(), '\n');
    } else {
      if (change.param() ==
          Mysqlx::Notice::SessionStateChanged::ACCOUNT_EXPIRED) {
        m_console.print("NOTICE: Account password expired\n");

        return xcl::Handler_result::Consumed;
      }
    }
  }

  return xcl::Handler_result::Continue;
}

void Session_holder::print_message_to_consoles(
    const std::string &direction, const xcl::XProtocol::Message &msg) {
  const auto message_size = get_message_size(msg);
  const auto is_msg_too_big =
      message_size > k_maximum_size_of_message_in_flow_history;

  if (m_enable_tracing_in_console) {
    if (is_msg_too_big)
      print_message(m_console, direction + " BIG ", message_size,
                    msg.GetTypeName());
    else
      print_message(m_console, direction, message_size, msg);
  }

  if (is_msg_too_big)
    print_message(m_console_with_flow_history, direction + " BIG ",
                  message_size, msg.GetTypeName());
  else
    print_message(m_console_with_flow_history, direction, message_size, msg);
}
