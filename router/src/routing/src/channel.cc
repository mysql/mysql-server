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

#include "channel.h"

#include <cassert>

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include "mysql/harness/net_ts/buffer.h"
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

  if (res == 0) {
    // shutdown not finished yet, flush the alert to the send-buffer.
    flush_to_send_buf();
  }

  return {res == 1};
}

stdx::expected<size_t, std::error_code> Channel::write_plain(
    const net::const_buffer &b) {
  // append to write_buffer
  auto dyn_buf = net::dynamic_buffer(send_plain_buffer());

  auto orig_size = dyn_buf.size();
  dyn_buf.grow(b.size());

  return net::buffer_copy(dyn_buf.data(orig_size, b.size()), b);
}

stdx::expected<size_t, std::error_code> Channel::read_plain(
    const net::mutable_buffer &b) {
  if (ssl_) {
    const auto res = SSL_read(ssl_.get(), b.data(), b.size());
    if (res <= 0) {
      return stdx::make_unexpected(make_tls_ssl_error(ssl_.get(), res));
    }

    return res;
  }

  if (recv_view_.empty()) {
    return stdx::make_unexpected(
        make_error_code(std::errc::operation_would_block));
  }

  auto transferred = net::buffer_copy(
      b, net::buffer(recv_view_.first(std::min(b.size(), recv_view_.size()))));

  consume_raw(transferred);

  return transferred;
}

stdx::expected<size_t, std::error_code> Channel::flush_from_recv_buf() {
  if (ssl_) {
    auto &recv_buf = recv_buffer();

    view_discard_raw();

    auto rbio = SSL_get_rbio(ssl_.get());

    size_t transferred{};

    auto dyn_buf = net::dynamic_buffer(recv_buf);
    while (dyn_buf.size() != 0) {
      const auto orig_size = dyn_buf.size();

      auto buf = dyn_buf.data(0, orig_size);

      const auto bio_res = BIO_write(rbio, buf.data(), buf.size());
      if (bio_res < 0) {
        if (transferred != 0) return transferred;

        return stdx::make_unexpected(
            make_error_code(std::errc::operation_would_block));
      }

      dyn_buf.consume(bio_res);

      transferred += bio_res;

      // recv-buffer changed, update the view.
      view_sync_raw();
    }

    return transferred;
  } else {
    return recv_buffer().size();
  }
}

stdx::expected<size_t, std::error_code> Channel::flush_to_send_buf() {
  // if this is non-ssl channel, no bytes get copied from send_plain_buffer() to
  // send_buffer()
  if (!ssl_) return 0;

  //
  // if there is plaintext data, encrypt it ...
  //

  if (!this->send_plain_buffer_.empty()) {
    // encode the plaintext data
    auto &buf = this->send_plain_buffer_;

    const auto spn = net::buffer(buf);

    const auto res = SSL_write(ssl_.get(), spn.data(), spn.size());
    if (res <= 0) {
      return stdx::make_unexpected(make_tls_ssl_error(ssl_.get(), res));
    }

    // remove the data that has been sent.
    net::dynamic_buffer(buf).consume(res);
  }

  //
  // ... and if there is encrypted data, move it to the socket's send-buffer.
  //

  auto *wbio = SSL_get_wbio(ssl_.get());

  size_t transferred{};

  // check if there is encrypted data waiting.
  while (const auto pending = BIO_pending(wbio)) {
    auto dyn_buf = net::dynamic_buffer(this->send_buffer());
    const auto orig_size = dyn_buf.size();
    const auto grow_size = pending;

    // append encrypted data to the send-buffer.
    dyn_buf.grow(grow_size);
    auto buf = dyn_buf.data(orig_size, grow_size);

    const auto bio_res = BIO_read(wbio, buf.data(), buf.size());
    if (bio_res < 0) {
      dyn_buf.shrink(grow_size);

      if (!BIO_should_retry(wbio)) {
        return stdx::make_unexpected(
            make_error_code(std::errc::invalid_argument));
      }

      if (transferred != 0) return transferred;

      return stdx::make_unexpected(
          make_error_code(std::errc::operation_would_block));
    }

    assert(bio_res != 0);

    dyn_buf.shrink(grow_size - bio_res);

    transferred += bio_res;
  }

  return transferred;
}

stdx::expected<size_t, std::error_code> Channel::read_to_plain(size_t sz) {
  if (!ssl_) {
    // as the connection is plaintext, use the recv-buffer directly.
    return std::min(sz, recv_view_.size());
  }

  {
    const auto flush_res = flush_from_recv_buf();
    if (!flush_res) return stdx::make_unexpected(flush_res.error());
  }

  auto &plain_buf = recv_plain_buffer_;

  // consume all data that was consumed via the view already.
  view_discard_plain();

  size_t bytes_read{};
  // decrypt from src-ssl into the ssl-plain-buf
  while (sz > 0) {
    auto dyn_buf = net::dynamic_buffer(plain_buf);

    // append to the plain buffer
    const auto read_res = read(dyn_buf, sz);
    if (read_res) {
      const size_t transferred = read_res.value();

      sz -= transferred;
      bytes_read += transferred;

      view_sync_plain();
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

Channel::Ssl Channel::release_ssl() {
  if (ssl_) SSL_set_info_callback(ssl_.get(), nullptr);

  return std::exchange(ssl_, {});
}

Channel::recv_buffer_type &Channel::send_plain_buffer() {
  return ssl_ ? send_plain_buffer_ : send_buffer_;
}

const Channel::recv_view_type &Channel::recv_view() const { return recv_view_; }

const Channel::recv_view_type &Channel::recv_plain_view() const {
  return ssl_ ? recv_plain_view_ : recv_view_;
}

void Channel::consume_raw(size_t count) {
  assert(count <= recv_view_.size());

  recv_view_ = {recv_view_.data() + count, recv_view_.size() - count};
}

void Channel::consume_plain(size_t count) {
  if (ssl_) {
    assert(count <= recv_plain_view_.size());

    recv_plain_view_ = {recv_plain_view_.data() + count,
                        recv_plain_view_.size() - count};
  } else {
    consume_raw(count);
  }
}

void Channel::view_discard_raw() {
  net::dynamic_buffer(recv_buffer_)
      .consume(recv_buffer_.size() - recv_view_.size());
}

void Channel::view_discard_plain() {
  if (ssl_) {
    net::dynamic_buffer(recv_plain_buffer_)
        .consume(recv_plain_buffer_.size() - recv_plain_view_.size());
  } else {
    view_discard_raw();
  }
}

void Channel::view_sync_raw() { recv_view_ = recv_buffer_; }

void Channel::view_sync_plain() { recv_plain_view_ = recv_plain_buffer_; }

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
