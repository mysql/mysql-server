/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_TLS_STREAM_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_TLS_STREAM_H_

#include <errno.h>
#include <memory>
#include <utility>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/tls_error.h"

#include "tls/details/ssl_io_completion.h"
#include "tls/details/ssl_operation.h"
#include "tls/details/tls_base.h"

namespace net {
namespace tls {

enum HandshakeType { kClient, kServer };

template <typename LowerLayer>
class TlsStream : private TlsBase<LowerLayer> {
 public:
  using Parent = TlsBase<LowerLayer>;
  using LowerLayerType = typename Parent::LowerLayerType;
  using endpoint_type = typename LowerLayer::endpoint_type;
  using Io_result_type = stdx::expected<size_t, std::error_code>;

 public:
  // Import constructor
  using Parent::TlsBase;

  void set_parent(const char *) {}

  auto get_executor() { return lower_layer().get_executor(); }
  auto cancel() { return lower_layer().cancel(); }

  typename Parent::LowerLayerType &lower_layer() {
    return Parent::lower_layer_;
  }

  const typename Parent::LowerLayerType &lower_layer() const {
    return Parent::lower_layer_;
  }

  bool is_open() const { return lower_layer().is_open(); }

  auto connect(const endpoint_type &endpoint) {
    // The call might initialize SSL handshake.
    // Current implementation is sufficient.
    return lower_layer().connect(endpoint);
  }

  template <class CompletionToken>
  auto async_connect(const endpoint_type &endpoint, CompletionToken &&token) {
    // The call might initialize SSL handshake.
    // Current implementation is sufficient.
    lower_layer().async_connect(endpoint, std::forward<CompletionToken>(token));
  }

  template <class CompletionToken>
  auto async_handshake(HandshakeType type, CompletionToken &&token) {
    if (type == kServer) {
      assert(false && "Server handshake is not supported.");
      return;
    }

    SslIoCompletionToken<SslHandshakeClientOperation, net::mutable_buffer,
                         CompletionToken, Parent>
        io_token(*this, {}, token);

    io_token.do_it();
  }

  template <class MutableBufferSequence, class CompletionToken>
  auto async_receive(const MutableBufferSequence &buffers,
                     CompletionToken &&token) {
    SslIoCompletionToken<SslReadOperation, MutableBufferSequence,
                         CompletionToken, Parent>
        io_token(*this, buffers, token);

    io_token.do_it();
  }

  template <class ConstBufferSequence, class CompletionToken>
  auto async_send(const ConstBufferSequence &buffers,
                  CompletionToken &&user_token) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");

    SslIoCompletionToken<SslWriteOperation, ConstBufferSequence,
                         CompletionToken, Parent>
        io_token(*this, buffers, user_token);

    io_token.do_it();
  }

  template <typename ConstBufferSequence>
  Io_result_type write_some(const ConstBufferSequence &buffers) {
    static_assert(net::is_const_buffer_sequence<ConstBufferSequence>::value,
                  "");

    Io_result_type result;
    SyncAction sync_action;
    auto handle_write_done = [&result](std::error_code ec, size_t s) {
      if (ec)
        result = stdx::unexpected(ec);
      else
        result = s;
    };
    SslIoCompletionToken<SslWriteOperation, ConstBufferSequence,
                         decltype(handle_write_done), Parent, SyncAction &>
        it(*this, buffers, std::move(handle_write_done), sync_action);

    SyncAction::Handler_result handle_result{it.do_it()};

    while (handle_result) {
      switch (handle_result.value()) {
        case Operation::Result::want_read:
          handle_result = sync_action.handle_read_result(&it);
          break;

        case Operation::Result::want_write:
          handle_result = sync_action.handle_write_result(&it);
          break;

        default:
          handle_result = SyncAction::Handler_result{stdx::unexpect};
      }
    }

    return result;
  }

  template <typename MutableBufferSequence>
  Io_result_type read_some(const MutableBufferSequence &buffers) {
    Io_result_type result;
    size_t total{0};
    SyncAction sync_action;
    auto handle_read_done = [&result, &total](std::error_code ec, size_t s) {
      total += s;
      if (ec)
        result = stdx::unexpected(ec);
      else
        result = total;
    };
    SslIoCompletionToken<SslReadOperation, MutableBufferSequence,
                         decltype(handle_read_done), Parent, SyncAction &>
        it(*this, buffers, std::move(handle_read_done), sync_action);

    SyncAction::Handler_result handle_result{it.do_it()};

    while (handle_result) {
      switch (handle_result.value()) {
        case Operation::Result::want_read:
          handle_result = sync_action.handle_read_result(&it);
          break;

        case Operation::Result::want_write:
          handle_result = sync_action.handle_write_result(&it);
          break;

        default:
          handle_result = SyncAction::Handler_result{stdx::unexpect};
      }
    }

    return result;
  }

  template <typename SettableSocketOption>
  stdx::expected<void, std::error_code> set_option(
      const SettableSocketOption &option) {
    return lower_layer().set_option(option);
  }

  auto close() { return lower_layer().close(); }
  auto release() { return lower_layer().release(); }
  auto native_handle() { return lower_layer().native_handle(); }
};

}  // namespace tls
}  // namespace net

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_TLS_STREAM_H_
