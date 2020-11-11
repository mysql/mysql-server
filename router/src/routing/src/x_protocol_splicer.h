/*
  Copyright (c) 2020, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_X_PROTOCOL_SPLICER_INCLUDED
#define ROUTING_X_PROTOCOL_SPLICER_INCLUDED

#include "basic_protocol_splicer.h"

#include <functional>
#include <memory>  // unique_ptr
#include <string>
#include <utility>  // pair
#include <vector>

class XProtocolSplicer : public BasicSplicer {
 public:
  XProtocolSplicer(
      SslMode source_ssl_mode, SslMode dest_ssl_mode,
      std::function<SSL_CTX *()> client_ssl_ctx_getter,
      std::function<SSL_CTX *()> server_ssl_ctx_getter,
      std::vector<std::pair<std::string, std::string>> session_attributes)
      : BasicSplicer(
            source_ssl_mode, dest_ssl_mode, std::move(client_ssl_ctx_getter),
            std::move(server_ssl_ctx_getter), std::move(session_attributes)),
        client_xprotocol_{std::make_unique<XProtocolState>()},
        server_xprotocol_{std::make_unique<XProtocolState>()} {}

  bool start() override {
    state(State::SPLICE_INIT);
    client_channel()->want_recv(4);

    // read packets from client first.
    return true;
  }

  State server_greeting() override { return State::ERROR; }
  State client_greeting() override { return State::ERROR; }

  State tls_connect() override;
  State tls_client_greeting() override;
  State tls_client_greeting_response() override;

  static stdx::expected<size_t, std::error_code> encode_error_packet(
      std::vector<uint8_t> &error_frame, uint16_t error_code,
      const std::string &msg, const std::string &sql_state = "HY000");

  stdx::expected<size_t, std::error_code> on_block_client_host(
      std::vector<uint8_t> &buf) override;

  /**
   * forward bytes from client to server.
   */
  State splice_to_server() override {
    return xproto_splice_int(client_channel(), client_xprotocol(),
                             server_channel(), server_xprotocol());
  }

  /**
   * forward bytes from server to client.
   */
  State splice_to_client() override {
    return xproto_splice_int(server_channel(), server_xprotocol(),
                             client_channel(), client_xprotocol());
  }

  XProtocolState *client_xprotocol() { return client_xprotocol_.get(); }
  XProtocolState *server_xprotocol() { return server_xprotocol_.get(); }

  const XProtocolState *client_xprotocol() const {
    return client_xprotocol_.get();
  }
  const XProtocolState *server_xprotocol() const {
    return server_xprotocol_.get();
  }

 private:
  State xproto_splice_int(Channel *src_channel, XProtocolState *src_protocol,
                          Channel *dst_channel, XProtocolState *dst_protocol);

  std::unique_ptr<XProtocolState> client_xprotocol_;
  std::unique_ptr<XProtocolState> server_xprotocol_;

  bool is_switch_to_tls_{false};
  bool tls_handshake_tried_{false};
  bool tls_connect_sent_{false};

  std::vector<uint8_t> xproto_client_msg_type_{};
};

#endif
