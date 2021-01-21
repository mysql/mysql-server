/*
  Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_PROTOCOL_SPLICER_INCLUDED
#define ROUTING_CLASSIC_PROTOCOL_SPLICER_INCLUDED

#include "basic_protocol_splicer.h"

#include <functional>
#include <memory>  // unique_ptr
#include <string>
#include <utility>  // pair
#include <vector>

class ClassicProtocolSplicer : public BasicSplicer {
 public:
  ClassicProtocolSplicer(
      SslMode source_ssl_mode, SslMode dest_ssl_mode,
      std::function<SSL_CTX *()> client_ssl_ctx_getter,
      std::function<SSL_CTX *()> server_ssl_ctx_getter,
      std::vector<std::pair<std::string, std::string>> session_attributes)
      : BasicSplicer(
            source_ssl_mode, dest_ssl_mode, std::move(client_ssl_ctx_getter),
            std::move(server_ssl_ctx_getter), std::move(session_attributes)),
        client_protocol_{std::make_unique<ClassicProtocolState>()},
        server_protocol_{std::make_unique<ClassicProtocolState>()} {}

  bool start() override {
    server_channel()->want_recv(4);

    // read packets from server first.
    return false;
  }

  State server_greeting() override;
  State client_greeting() override;
  State tls_connect() override;
  State tls_client_greeting() override;
  State tls_client_greeting_response() override;

  static stdx::expected<size_t, std::error_code> encode_error_packet(
      std::vector<uint8_t> &error_frame, const uint8_t seq_id,
      const classic_protocol::capabilities::value_type caps,
      const uint16_t error_code, const std::string &msg,
      const std::string &sql_state = "HY000");

  stdx::expected<size_t, std::error_code> encode_error_packet(
      std::vector<uint8_t> &error_frame, const uint16_t error_code,
      const std::string &msg, const std::string &sql_state = "HY000");

  stdx::expected<size_t, std::error_code> on_block_client_host(
      std::vector<uint8_t> &buf) override;
  /**
   * forward bytes from one client to server.
   */
  State splice_to_server() override {
    return splice_int(client_channel(), client_protocol(), server_channel(),
                      server_protocol());
  }

  /**
   * forward bytes from one server to client.
   */
  State splice_to_client() override {
    return splice_int(server_channel(), server_protocol(), client_channel(),
                      client_protocol());
  }

  ClassicProtocolState *client_protocol() { return client_protocol_.get(); }
  ClassicProtocolState *server_protocol() { return server_protocol_.get(); }

  const ClassicProtocolState *client_protocol() const {
    return client_protocol_.get();
  }
  const ClassicProtocolState *server_protocol() const {
    return server_protocol_.get();
  }

 private:
  State splice_int(Channel *src_channel, ClassicProtocolState *src_protocol,
                   Channel *dst_channel, ClassicProtocolState *dst_protocol);

  std::unique_ptr<ClassicProtocolState> client_protocol_;
  std::unique_ptr<ClassicProtocolState> server_protocol_;
};

#endif
