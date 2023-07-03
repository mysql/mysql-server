/*
Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "channel.h"

#include <openssl/ssl.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"

stdx::expected<void, std::error_code> Channel::tls_accept() {
  auto ssl = ssl_.get();

  const auto res = SSL_accept(ssl);
  if (res != 1) {
    return stdx::make_unexpected(make_tls_ssl_error(ssl, res));
  }

  return {};
}

stdx::expected<void, std::error_code> Channel::tls_connect() {
  auto ssl = ssl_.get();

  const auto res = SSL_connect(ssl);
  if (res != 1) {
    return stdx::make_unexpected(make_tls_ssl_error(ssl, res));
  }

  return {};
}

bool Channel::tls_init_is_finished() {
  return SSL_is_init_finished(ssl_.get());
}

stdx::expected<bool, std::error_code> Channel::tls_shutdown() {
  auto ssl = ssl_.get();

  const auto res = SSL_shutdown(ssl);
  if (res < 0) {
    return stdx::make_unexpected(make_tls_ssl_error(ssl, res));
  }

  return {res == 1};
}

stdx::expected<size_t, std::error_code> Channel::write_plain(
    const net::const_buffer &b) {
  if (ssl_) {
    const auto res = SSL_write(ssl_.get(), b.data(), b.size());
    if (res <= 0) {
      return stdx::make_unexpected(make_tls_ssl_error(ssl_.get(), res));
    }

    return res;
  } else {
    // append to write_buffer
    auto dyn_buf = net::dynamic_buffer(this->send_buffer());

    auto orig_size = dyn_buf.size();
    dyn_buf.grow(b.size());

    return net::buffer_copy(dyn_buf.data(orig_size, b.size()), b);
  }
}

stdx::expected<size_t, std::error_code> Channel::read_encrypted(
    const net::mutable_buffer &b) {
  if (ssl_) {
    auto wbio = SSL_get_wbio(ssl_.get());

    const auto res = BIO_read(wbio, b.data(), b.size());
    if (res < 0) {
      if (BIO_should_retry(wbio)) {
        return stdx::make_unexpected(
            make_error_code(std::errc::operation_would_block));
      } else {
        return stdx::make_unexpected(
            make_error_code(std::errc::invalid_argument));
      }
    }

    return res;
  } else {
    // read from send_buffer
    auto dyn_buf = net::dynamic_buffer(this->send_buffer());

    auto orig_size = dyn_buf.size();
    dyn_buf.grow(b.size());

    net::buffer_copy(dyn_buf.data(orig_size, b.size()), b);

    return b.size();
  }
}

stdx::expected<size_t, std::error_code> Channel::write_encrypted(
    const net::const_buffer &b) {
  if (ssl_) {
    auto rbio = SSL_get_rbio(ssl_.get());

    const auto res = BIO_write(rbio, b.data(), b.size());
    if (res < 0) {
      return stdx::make_unexpected(
          make_error_code(std::errc::operation_would_block));
    }

    return res;
  } else {
    // append to recv-buffer
    auto dyn_buf = net::dynamic_buffer(this->recv_buffer());

    auto orig_size = dyn_buf.size();
    dyn_buf.grow(b.size());

    return net::buffer_copy(dyn_buf.data(orig_size, b.size()), b);
  }
}

stdx::expected<size_t, std::error_code> Channel::read_plain(
    const net::mutable_buffer &b) {
  if (ssl_) {
    const auto res = SSL_read(ssl_.get(), b.data(), b.size());
    if (res <= 0) {
      return stdx::make_unexpected(make_tls_ssl_error(ssl_.get(), res));
    }

    return res;
  } else {
    if (recv_buffer().empty()) {
      return stdx::make_unexpected(
          make_error_code(std::errc::operation_would_block));
    }

    auto dyn_buf = net::dynamic_buffer(recv_buffer());

    auto transferred = net::buffer_copy(b, dyn_buf.data(0, b.size()));

    dyn_buf.consume(transferred);

    return transferred;
  }
}

stdx::expected<size_t, std::error_code> Channel::flush_from_recv_buf() {
  if (ssl_) {
    auto &recv_buf = recv_buffer();

    size_t transferred{};

    auto dyn_buf = net::dynamic_buffer(recv_buf);
    while (dyn_buf.size() != 0) {
      const auto orig_size = dyn_buf.size();
      const auto res = write_encrypted(dyn_buf.data(0, orig_size));

      if (!res) {
        if (res.error() == std::errc::operation_would_block &&
            transferred != 0) {
          return transferred;
        }
        return res;
      }
      dyn_buf.consume(res.value());

      transferred += res.value();
    }

    return transferred;
  } else {
    return recv_buffer().size();
  }
}

stdx::expected<size_t, std::error_code> Channel::flush_to_send_buf() {
  if (ssl_) {
    auto dyn_buf = net::dynamic_buffer(this->send_buffer());

    size_t transferred{};

    while (true) {
      const auto orig_size = dyn_buf.size();
      const auto grow_size = 16 * 1024;  // a TLS frame

      dyn_buf.grow(grow_size);

      const auto res = read_encrypted(dyn_buf.data(orig_size, grow_size));
      if (!res) {
        dyn_buf.shrink(grow_size);
        if ((res.error() ==
             make_error_condition(std::errc::operation_would_block)) &&
            transferred != 0) {
          return transferred;
        }
        return res;
      }
      dyn_buf.shrink(grow_size - res.value());

      transferred += res.value();
    }
  } else {
    return this->send_buffer().size();
  }
}

stdx::expected<size_t, std::error_code> Channel::read_to_plain(size_t sz) {
  auto &plain_buf = recv_plain_buffer();

  {
    const auto flush_res = flush_from_recv_buf();
    if (!flush_res) {
      return flush_res.get_unexpected();
    } else {
#if defined(DEBUG_SSL)
      std::cerr << __LINE__ << ": " << from << "::ssl->" << from << "::enc"
                << ": " << flush_res.value() << std::endl;
#endif
    }
  }

  size_t bytes_read{};
  // decrypt from src-ssl into the ssl-plain-buf
  while (sz > 0) {
    auto dyn_buf = net::dynamic_buffer(plain_buf);

    // append to the plain buffer
    const auto read_res = read(dyn_buf, sz);
    if (read_res) {
#if defined(DEBUG_SSL)
      std::cerr << __LINE__ << ": " << from << "::ssl->" << from << "::dec"
                << ": " << read_res.value() << std::endl;
#endif

      const size_t transferred = read_res.value();

      sz -= transferred;
      bytes_read += transferred;
    } else {
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
  flush_to_send_buf();

  return bytes_read;
}

#if 0
size_t Channel::plain_pending() const {
  if (ssl_) {
    auto bio = SSL_get_wbio(ssl_.get());
    return BIO_pending(bio);
  } else {
    return recv_buffer().size();
  }
}
#endif
