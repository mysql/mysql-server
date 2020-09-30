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

#include "protocol_splicer.h"

#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <openssl/ssl.h>  // SSL_get_version

#include "mysql.h"
#include "mysql/harness/net_ts/buffer.h"  // net::dynamic_buffer
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/classic_protocol_codec_base.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "ssl_mode.h"

/**
 * log error-msg with error code and set the connection to its FINISH state.
 */
static BasicSplicer::State log_fatal_error_code(const char *msg,
                                                std::error_code ec) {
  log_warning("%s: %s (%s:%d)", msg, ec.message().c_str(), ec.category().name(),
              ec.value());

  return BasicSplicer::State::FINISH;
}

stdx::expected<size_t, std::error_code> BasicSplicer::read_to_plain(
    Channel *src_channel, std::vector<uint8_t> &plain_buf) {
  // const size_t kTlsRecordMaxPayloadSize = 16 * 1024;
  // const size_t kTlsRecordHeaderSize = 5;
  {
    const auto flush_res = src_channel->flush_from_recv_buf();
    if (!flush_res) {
      return flush_res.get_unexpected();
    } else {
#if 0
        std::cerr << __LINE__ << ": " << from << "::ssl->" << from << "::enc"
                  << ": " << flush_res.value() << std::endl;
#endif
    }
  }

  // decrypt from src-ssl into the ssl-plain-buf
  while (true) {
    auto dyn_buf = net::dynamic_buffer(plain_buf);

    // append to the plain buffer
    const auto read_res = src_channel->read(dyn_buf);
    if (read_res) {
#if defined(DEBUG_SSL)
      std::cerr << __LINE__ << ": " << from << "::ssl->" << from << "::dec"
                << ": " << read_res.value() << std::endl;
#endif
    } else {
#if 0
      std::cerr << __LINE__ << ": read.fail() " << read_res.error()
                << std::endl;
#endif
      // read from client failed.

      if (read_res.error() != TlsErrc::kWantRead &&
          read_res.error() !=
              make_error_code(std::errc::operation_would_block)) {
        return read_res.get_unexpected();
      }

      break;
    }
  }

  // if there is something to send to the source, write into its buffer.
  return src_channel->flush_to_send_buf();
}

BasicSplicer::State BasicSplicer::tls_accept() {
  // write socket data to SSL struct
  auto *channel = client_channel_.get();

  if (client_waiting() && !server_waiting()) {
    // received something from the server while expected data from the client.
    //
    // data is added to the server_channel()->recv_buffer(). Wait for more.
    //
    // The message will be handled later.

    server_channel()->want_recv(1);
    return state();
  }

#if 0
  log_debug("%d: >> %s", __LINE__, state_to_string(state()));
#endif
  {
    auto flush_res = channel->flush_from_recv_buf();
    if (!flush_res) {
      return log_fatal_error_code("tls_accept::recv::flush() failed",
                                  flush_res.error());
    }
  }

  if (!channel->tls_init_is_finished()) {
    const auto res = channel->tls_accept();

    // flush the TLS message to the send-buffer.
    {
      const auto flush_res = channel->flush_to_send_buf();
      if (!flush_res &&
          (flush_res.error() !=
           make_error_condition(std::errc::operation_would_block))) {
        return log_fatal_error_code("tls_accept::send::flush() failed",
                                    flush_res.error());
      }
    }

    if (!res) {
      if (res.error() == TlsErrc::kWantRead) {
        // once all is written, wait for the TLS header.
        channel->want_recv(1);

#if 0
        log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
        return state();
      } else {
        log_debug("TLS handshake failed: %s", res.error().message().c_str());

#if 0
        log_debug("%d: << %s", __LINE__, state_to_string(state()));
#endif
        return State::FINISH;
      }
    }
  }

#if 0
  log_debug("%d: << %s (tls accepted)", __LINE__, state_to_string(state()));
#endif
  return State::TLS_CLIENT_GREETING;
}

/**
 * send TLS shutdown alert.
 */
BasicSplicer::State BasicSplicer::tls_shutdown() {
  // send SSL_shutdown();
  //
  // needed to trigger session resumption
  if (server_channel()->tls_shutdown()) {
  }

  server_channel()->flush_to_send_buf();

  return State::FINISH;
}
