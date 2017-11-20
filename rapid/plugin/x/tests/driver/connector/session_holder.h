/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_
#define X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_

#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "formatters/console.h"
#include "my_macros.h"
#include "mysqlxclient/xconnection.h"
#include "mysqlxclient/xsession.h"


struct Connection_options {
  std::string socket;
  std::string host;
  int         port { 0 };

  std::string user;
  std::string password;
  std::string schema;

  std::string ssl_mode;
  std::string ssl_ca;
  std::string ssl_ca_path;
  std::string ssl_cert;
  std::string ssl_cipher;
  std::string ssl_key;
  std::string allowed_tls;
  int64_t     io_timeout { -1 };
  bool        dont_wait_for_disconnect { false };
  xcl::Internet_protocol ip_mode { xcl::Internet_protocol::V4 };

  bool is_ssl_set() const {
    return !ssl_ca.empty() ||
        !ssl_ca_path.empty() ||
        !ssl_cert.empty() ||
        !ssl_cipher.empty() ||
        !ssl_key.empty();
  }
};


class Session_holder {
 private:
  using Frame_type = Mysqlx::Notice::Frame::Type;

 public:
  Session_holder(std::unique_ptr<xcl::XSession> session,
                 const Console &console);

  xcl::XSession *get_session();

  bool try_get_number_of_received_messages(const std::string message_name,
                               uint64_t *value) const;
  xcl::XError setup_session(const Connection_options &options);
  xcl::XError setup_connection(const Connection_options &options);
  void setup_ssl(const Connection_options &options);
  void setup_msg_callbacks();

  void remove_notice_handler();

 private:
  xcl::Handler_result trace_send_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Client_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result trace_received_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Server_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result count_received_messages(
      xcl::XProtocol *protocol,
      const xcl::XProtocol::Server_message_type_id msg_id,
      const xcl::XProtocol::Message &msg);

  xcl::Handler_result dump_notices(
      const xcl::XProtocol *protocol,
      const bool is_global,
      const Frame_type type,
      const char *data,
      const uint32_t data_length);

  void print_message(
      const std::string &direction,
      const xcl::XProtocol::Message &msg);

  xcl::XProtocol::Handler_id      m_handler_id{-1};
  std::unique_ptr<xcl::XSession>  m_session;
  std::map<std::string, uint64_t> m_received_msg_counters;
  const Console                  &m_console;
};

#endif  // X_TESTS_DRIVER_CONNECTOR_SESSION_HOLDER_H_
