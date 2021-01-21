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

#ifndef ROUTING_BASIC_PROTOCOL_SPLICER_INCLUDED
#define ROUTING_BASIC_PROTOCOL_SPLICER_INCLUDED

#include <cstdint>  // size_t
#include <cstdio>
#include <functional>  // function
#include <string>

#ifdef _WIN32
// include winsock2.h before openssl/ssl.h
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <openssl/ssl.h>

#include "channel.h"
#include "ssl_mode.h"

enum class TlsContentType {
  kChangeCipherSpec = 0x14,
  kAlert,
  kHandshake,
  kApplication,
  kHeartbeat
};

inline std::string tls_content_type_to_string(TlsContentType v) {
  switch (v) {
    case TlsContentType::kChangeCipherSpec:
      return "change-cipher-spec";
    case TlsContentType::kAlert:
      return "alert";
    case TlsContentType::kHandshake:
      return "handshake";
    case TlsContentType::kApplication:
      return "application";
    case TlsContentType::kHeartbeat:
      return "heartbeat";
  }

  return "unknown-" + std::to_string(static_cast<int>(v));
}

class BasicSplicer {
 public:
  enum class State {
    SERVER_GREETING,
    CLIENT_GREETING,
    TLS_ACCEPT,
    TLS_CLIENT_GREETING,
    TLS_CLIENT_GREETING_RESPONSE,
    TLS_CONNECT,
    SPLICE_INIT,
    SPLICE,
    TLS_SHUTDOWN,
    FINISH,
    DONE,
    ERROR,
  };

  static const char *state_to_string(State st) {
    switch (st) {
      case State::SERVER_GREETING:
        return "server_greeting";
      case State::CLIENT_GREETING:
        return "client_greeting";
      case State::TLS_ACCEPT:
        return "tls_accept";
      case State::TLS_CLIENT_GREETING:
        return "tls_client_greeting";
      case State::TLS_CLIENT_GREETING_RESPONSE:
        return "tls_client_greeting_response";
      case State::TLS_CONNECT:
        return "tls_connect";
      case State::SPLICE_INIT:
        return "splice_init";
      case State::SPLICE:
        return "splice";
      case State::TLS_SHUTDOWN:
        return "tls_shutdown";
      case State::FINISH:
        return "finish";
      case State::DONE:
        return "done";
      case State::ERROR:
        return "error";
    }

    return "unknown";
  }

  BasicSplicer(
      SslMode source_ssl_mode, SslMode dest_ssl_mode,
      std::function<SSL_CTX *()> client_ssl_ctx_getter,
      std::function<SSL_CTX *()> server_ssl_ctx_getter,
      std::vector<std::pair<std::string, std::string>> session_attributes)
      : source_ssl_mode_{source_ssl_mode},
        dest_ssl_mode_{dest_ssl_mode},
        client_ssl_ctx_getter_{std::move(client_ssl_ctx_getter)},
        server_ssl_ctx_getter_{std::move(server_ssl_ctx_getter)},
        client_channel_{std::make_unique<Channel>()},
        server_channel_{std::make_unique<Channel>()},
        session_attributes_{std::move(session_attributes)} {}

  virtual ~BasicSplicer() = default;

  bool handshake_done() const { return handshake_done_; }
  void handshake_done(bool v) { handshake_done_ = v; }

  SslMode source_ssl_mode() const { return source_ssl_mode_; }
  SslMode dest_ssl_mode() const { return dest_ssl_mode_; }

  void state(State st) {
#if 0
    std::cerr << __LINE__ << ": " << state_to_string(state_) << " -> "
              << state_to_string(st) << std::endl;
#endif
    state_ = st;
  }

  State state() const { return state_; }

  Channel *client_channel() { return client_channel_.get(); }
  Channel *server_channel() { return server_channel_.get(); }

  const Channel *client_channel() const { return client_channel_.get(); }
  const Channel *server_channel() const { return server_channel_.get(); }

  bool client_waiting() const { return client_waiting_; }
  void client_waiting(bool waiting) { client_waiting_ = waiting; }

  bool server_waiting() const { return server_waiting_; }
  void server_waiting(bool waiting) { server_waiting_ = waiting; }

  std::vector<std::pair<std::string, std::string>> session_attributes() const {
    return session_attributes_;
  }

  /**
   * start the packet reception.
   *
   * @retval true wait for data from client
   * @retval false wait for data from server
   */
  virtual bool start() = 0;

  /**
   * handle the server message.
   *
   * - waits for the server greeting to be complete
   * - parses server-greeting message
   * - unsets compress capabilities
   * - tracks capabilities.
   *
   * @returns next state to process
   * @retval State::SERVER_GREETING wait for more data from server
   * @retval State::CLIENT_GREETING drain send-buffer to client and continue
   * with client-greeting.
   * @retval State::FINISH drain send-buffer to client and close connection.
   */
  virtual State server_greeting() = 0;

  /**
   * accept a TLS connection from the client_channel_.
   *
   * @return next State
   */
  State tls_accept();

  /**
   * establish a TLS connection to the server_channel_.
   *
   * @return next State
   */
  virtual State tls_connect() = 0;

  /**
   * shutdown a TLS connection.
   *
   * @return next State
   */
  State tls_shutdown();

  /**
   * process the Client Greeting packet from the client.
   *
   * - wait for for a full protocol frame
   * - decode client-greeting packet and decide how to proceed based on
   * capabilities and configuration
   *
   * ## client-side connection state
   *
   * ssl-cap::client
   * :  SSL capability the client sends to router
   *
   * ssl-cap::server
   * :  SSL capability the server sends to router
   *
   * ssl-mode::client
   * :  client_ssl_mode used by router
   *
   * ssl-mode::server
   * :  server_ssl_mode used by router
   *
   * | ssl-mode    | ssl-mode | ssl-cap | ssl-cap  | ssl    |
   * | client      | server   | client  | server   | client |
   * | ----------- | -------- | ------- | -------- | ------ |
   * | DISABLED    | any      | any     | any      | PLAIN  |
   * | PREFERRED   | any      | [ ]     | any      | PLAIN  |
   * | PREFERRED   | any      | [x]     | any      | SSL    |
   * | REQUIRED    | any      | [ ]     | any      | FAIL   |
   * | REQUIRED    | any      | [x]     | any      | SSL    |
   * | PASSTHROUGH | any      | [ ]     | any      | PLAIN  |
   * | PASSTHROUGH | any      | [x]     | [x]      | (SSL)  |
   *
   * PLAIN
   * :  client-side connection is plaintext
   *
   * FAIL
   * :  router fails connection with client
   *
   * SSL
   * :  encrypted, client-side TLS endpoint
   *
   * (SSL)
   * :  encrypted, no TLS endpoint
   *
   * ## server-side connection state
   *
   * | ssl-mode    | ssl-mode  | ssl-cap | ssl-cap | ssl    |
   * | client      | server    | client  | server  | server |
   * | ----------- | --------- | ------- | ------- | ------ |
   * | any         | DISABLED  | any     | any     | PLAIN  |
   * | any         | PREFERRED | any     | [ ]     | PLAIN  |
   * | any         | PREFERRED | any     | [x]     | SSL    |
   * | any         | REQUIRED  | any     | [ ]     | FAIL   |
   * | any         | REQUIRED  | any     | [x]     | SSL    |
   * | PASSTHROUGH | AS_CLIENT | [ ]     | any     | PLAIN  |
   * | PASSTHROUGH | AS_CLIENT | [x]     | [x]     | (SSL)  |
   * | other       | AS_CLIENT | [ ]     | any     | PLAIN  |
   * | other       | AS_CLIENT | [x]     | [ ]     | FAIL   |
   * | other       | AS_CLIENT | [x]     | [x]     | SSL    |
   *
   * PLAIN
   * :  client-side connection is plaintext
   *
   * FAIL
   * :  router fails connection with client
   *
   * SSL
   * :  encrypted, client-side TLS endpoint
   *
   * (SSL)
   * :  encrypted, no TLS endpoint
   *
   */
  virtual State client_greeting() = 0;

  /**
   * after tls-accept expect another client-greeting.
   */
  virtual State tls_client_greeting() = 0;

  virtual State tls_client_greeting_response() = 0;

  virtual State splice_to_server() = 0;

  virtual State splice_to_client() = 0;

  virtual stdx::expected<size_t, std::error_code> on_block_client_host(
      std::vector<uint8_t> &buf) = 0;

  template <bool to_server>
  State splice() {
    return to_server ? splice_to_server() : splice_to_client();
  }

  static stdx::expected<size_t, std::error_code> read_to_plain(
      Channel *src_channel, std::vector<uint8_t> &plain_buf);

  /**
   * move the contents of one buffer to the end of another.
   *
   * @param send_buf to move to
   * @param recv_buf to move from
   */
  template <class DstDynamicBuffer, class SrcDynamicBuffer>
  static void move_buffer(DstDynamicBuffer &&send_buf,
                          SrcDynamicBuffer &&recv_buf) {
    return move_buffer(send_buf, recv_buf, recv_buf.size());
  }

  /**
   * move parts of the contents of one buffer to the end of another.
   *
   * @param send_buf to move to
   * @param recv_buf to move from
   * @param to_transfer amount of data to transfer from recv_buf
   */
  template <class DstDynamicBuffer, class SrcDynamicBuffer>
  static void move_buffer(DstDynamicBuffer &&send_buf,
                          SrcDynamicBuffer &&recv_buf, size_t to_transfer) {
    auto orig_size = send_buf.size();

    send_buf.grow(to_transfer);
    auto transferred = net::buffer_copy(send_buf.data(orig_size, to_transfer),
                                        recv_buf.data(0, to_transfer));

    recv_buf.consume(transferred);
  }

 protected:
  SslMode source_ssl_mode_;
  SslMode dest_ssl_mode_;

  std::function<SSL_CTX *()> client_ssl_ctx_getter_;
  std::function<SSL_CTX *()> server_ssl_ctx_getter_;

  State state_{State::SERVER_GREETING};

  std::unique_ptr<Channel> client_channel_;
  std::unique_ptr<Channel> server_channel_;

  bool handshake_done_{false};

  bool client_waiting_{false};
  bool server_waiting_{false};

  std::vector<std::pair<std::string, std::string>> session_attributes_;
};

#endif
