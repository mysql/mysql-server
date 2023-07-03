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

#ifndef PLUGIN_X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_
#define PLUGIN_X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_

#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "my_macros.h"  // NOLINT(build/include_subdir)

#include "plugin/x/client/mysqlxclient/xconnection.h"
#include "plugin/x/client/mysqlxclient/xsession.h"
#include "plugin/x/protocol/stream/compression/compression_algorithm_interface.h"
#include "plugin/x/src/helper/optional_value.h"
#include "plugin/x/tests/driver/formatters/console.h"

struct Connection_options {
  std::string socket;
  std::string host;
  std::string network_namespace;
  int port{0};

  std::string user;
  std::string password;
  std::string schema;

  std::string ssl_mode;
  std::string ssl_ca;
  std::string ssl_fips_mode;
  std::string ssl_ca_path;
  std::string ssl_cert;
  std::string ssl_cipher;
  std::string ssl_key;
  std::string allowed_tls;
  int64_t io_timeout{-1};
  int64_t session_connect_timeout{-1};
  bool dont_wait_for_disconnect{false};
  bool trace_protocol{false};
  bool trace_protocol_history{false};
  xcl::Internet_protocol ip_mode{xcl::Internet_protocol::V4};
  std::vector<std::string> auth_methods;
  bool compatible{false};
  std::vector<std::string> compression_algorithm{"DEFLATE_STREAM",
                                                 "LZ4_MESSAGE", "ZSTD_STREAM"};
  std::string compression_mode{"DISABLED"};
  bool compression_combine_mixed_messages{true};
  int64_t compression_max_combine_messages{0};
  xpl::Optional_value<int32_t> compression_level;

  bool is_ssl_set() const {
    return !ssl_ca.empty() || !ssl_ca_path.empty() || !ssl_cert.empty() ||
           !ssl_cipher.empty() || !ssl_key.empty();
  }
};

class Session_holder {
 private:
  using Frame_type = Mysqlx::Notice::Frame::Type;

 public:
  Session_holder(std::unique_ptr<xcl::XSession> session,
                 const Console &console_with_flow_history,
                 const Console &console, const Connection_options &options);

  protocol::Compression_algorithm_interface *get_algorithm();
  xcl::XSession *get_session();

  bool enable_compression(const xcl::Compression_algorithm algorithm,
                          const int64_t level);
  void clear_received_messages();
  bool try_get_number_of_received_messages(const std::string message_name,
                                           uint64_t *value) const;
  void remove_notice_handler();

  xcl::XError connect(const bool is_raw_connection);
  xcl::XError reconnect();

 private:
  xcl::XError setup_session();
  xcl::XError setup_connection();
  void setup_compression();
  void setup_ssl();
  void setup_other_options();
  void setup_msg_callbacks();

  xcl::Handler_result count_received_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Server_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result dump_notices(const xcl::XProtocol *protocol,
                                   const bool is_global, const Frame_type type,
                                   const char *data,
                                   const uint32_t data_length);

  void print_message_to_consoles(const std::string &direction,
                                 const xcl::XProtocol::Message &msg);

  std::shared_ptr<protocol::Compression_algorithm_interface> m_algorithm;
  xcl::XProtocol::Handler_id m_handler_id{-1};
  std::unique_ptr<xcl::XSession> m_session;
  std::map<std::string, uint64_t> m_received_msg_counters;
  const Console &m_console_with_flow_history;
  const Console &m_console;
  Connection_options m_options;
  bool m_is_raw_connection{false};
  bool m_enable_tracing_in_console;
};

#endif  // PLUGIN_X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_
