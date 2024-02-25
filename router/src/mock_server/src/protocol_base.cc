/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "statement_reader.h"

#include <openssl/ssl.h>
#include <string>
#include <vector>

#include "mysql/harness/tls_error.h"

namespace server_mock {

ProtocolBase::ProtocolBase(ProtocolBase::socket_type client_sock,
                           ProtocolBase::endpoint_type client_ep,
                           TlsServerContext &tls_ctx)
    : client_socket_{std::move(client_sock)},
      client_ep_{std::move(client_ep)},
      tls_ctx_{tls_ctx} {
  // if it doesn't work, no problem.
  //
  client_socket_.set_option(net::ip::tcp::no_delay{true});
  client_socket_.native_non_blocking(true);
}

stdx::expected<size_t, std::error_code> ProtocolBase::write_ssl(
    const net::const_buffer &buf) {
  const auto res = SSL_write(ssl_.get(), buf.data(), buf.size());

  if (res <= 0) {
    return stdx::make_unexpected(make_tls_ssl_error(ssl_.get(), res));
  } else {
    return res;
  }
}

stdx::expected<size_t, std::error_code> ProtocolBase::read_ssl(
    const net::mutable_buffer &buf) {
  const auto res = SSL_read(ssl_.get(), buf.data(), buf.size());

  if (res <= 0) {
    auto ec = make_tls_ssl_error(ssl_.get(), res);

    // if ec.code() == 0, then we had EOF
    return stdx::make_unexpected(ec ? ec
                                    : make_error_code(net::stream_errc::eof));
  } else {
    return res;
  }
}

stdx::expected<size_t, std::error_code> ProtocolBase::avail_ssl() {
  const auto res = SSL_pending(ssl_.get());

  if (res <= 0) {
    return stdx::make_unexpected(make_tls_ssl_error(ssl_.get(), res));
  } else {
    return res;
  }
}

bool ProtocolBase::authenticate(const std::string &auth_method_name,
                                const std::string &auth_method_data,
                                const std::string &password,
                                const std::vector<uint8_t> &auth_response) {
  if (auth_method_name == CachingSha2Password::name) {
    auto scramble_res =
        CachingSha2Password::scramble(auth_method_data, password);
    return scramble_res && (scramble_res.value() == auth_response);
  } else if (auth_method_name == MySQLNativePassword::name) {
    auto scramble_res =
        MySQLNativePassword::scramble(auth_method_data, password);
    return scramble_res && (scramble_res.value() == auth_response);
  } else if (auth_method_name == ClearTextPassword::name) {
    auto scramble_res = ClearTextPassword::scramble(auth_method_data, password);
    return scramble_res && (scramble_res.value() == auth_response);
  } else {
    // there is also
    // - old_password (3.23, 4.0)
    // - sha256_password (5.6, ...)
    // - windows_authentication (5.6, ...)
    return false;
  }
}

void ProtocolBase::init_tls() {
  ssl_.reset(SSL_new(tls_ctx_.get()));

  if (recv_buffer_.empty()) {
    // if the recv-buffer is empty, attach the socket-handle to the SSL
    // connection.
    SSL_set_fd(ssl_.get(), client_socket_.native_handle());
  } else {
    // if recv-buffer has data, pass its content to a memory-BIO and switch to
    // the FD in tls_accept() once it is empty.
    auto r_mem_bio = BIO_new(BIO_s_mem());

    auto res = BIO_write(r_mem_bio, recv_buffer_.data(), recv_buffer_.size());
    if (res != static_cast<int>(recv_buffer_.size())) {
      // this should never fail.
      std::terminate();
    }

    recv_buffer_.clear();
    SSL_set_bio(
        ssl_.get(), r_mem_bio,
        BIO_new_socket(client_socket_.native_handle(), 0 /* close_flag */));
  }
}

void ProtocolBase::cancel() {
  client_socket_.cancel();
  exec_timer_.cancel();
}

stdx::expected<void, std::error_code> ProtocolBase::tls_accept() {
  auto ssl = ssl_.get();
  auto rbio = SSL_get_rbio(ssl);

  stdx::expected<void, std::error_code> result{};
  const auto accept_res = SSL_accept(ssl);
  if (accept_res != 1) {
    result = stdx::make_unexpected(make_tls_ssl_error(ssl, accept_res));
  }

  // if the initial memory bio is processed, switch to the fd for more data.
  if (BIO_method_type(rbio) == BIO_TYPE_MEM && BIO_ctrl_pending(rbio) == 0) {
    // we could use SSL_set_rfd as we only change read BIO here but in older
    // OpenSSL version it seems to be bogus and invalidates our existing write
    // BIO as a side effect
    SSL_set_fd(ssl, client_socket_.native_handle());
  }

  return result;
}
}  // namespace server_mock
