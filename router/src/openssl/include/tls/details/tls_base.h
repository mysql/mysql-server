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

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_TLS_BASE_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_TLS_BASE_H_

#include <memory>
#include <utility>

#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/tls_client_context.h"
#include "mysql/harness/tls_server_context.h"

#include "tls/details/flexible_buffer.h"

namespace net {
namespace tls {

template <typename LowerLayer>
class TlsBase {
 private:
  template <typename Resource, typename Result,
            Result (*free_resource)(Resource *)>
  class Free {
   public:
    void operator()(Resource *res) { free_resource(res); }
  };

  using SslPtr = std::unique_ptr<SSL, Free<SSL, void, SSL_free>>;
  using BioPtr = std::unique_ptr<BIO, Free<BIO, int, BIO_free>>;
  using socket_type = typename LowerLayer::native_handle_type;
  using protocol_type = typename LowerLayer::protocol_type;

  TlsBase(LowerLayer &&layer, TlsContext *tls_context)
      : lower_layer_(std::forward<LowerLayer>(layer)),
        tls_context_{tls_context} {
    ssl_.reset(SSL_new(tls_context_->get()));
    BIO *internal_bio;
    BIO *external_bio;
    BIO_new_bio_pair(&internal_bio, 0, &external_bio, 0);
    SSL_set_bio(ssl_.get(), internal_bio, internal_bio);
    network_bio_.reset(external_bio);
  }

 public:
  using LowerLayerType = LowerLayer;

  template <typename... Args>
  TlsBase(TlsServerContext *tls_context, Args &&... args)
      : TlsBase(LowerLayer{std::forward<Args>(args)...}, tls_context) {
    SSL_set_accept_state(ssl_.get());
  }

  template <typename... Args>
  TlsBase(TlsClientContext *tls_context, Args &&... args)
      : TlsBase(LowerLayer{std::forward<Args>(args)...}, tls_context) {
    SSL_set_connect_state(ssl_.get());
  }

  TlsBase(TlsBase &&other)
      : lower_layer_{std::move(other.lower_layer_)},
        tls_context_{other.tls_context_},
        ssl_{std::move(other.ssl_)},
        network_bio_{std::move(other.network_bio_)} {}

 protected:
  template <typename SslIO, typename BufferSequence, typename Token,
            typename TlsLayer, typename Action>
  friend class SslIoCompletionToken;

  constexpr static uint32_t k_tls_buffer_size = 32000;

  LowerLayer lower_layer_;
  TlsContext *tls_context_;
  SslPtr ssl_;
  BioPtr network_bio_;
  uint8_t output_buffer_[k_tls_buffer_size];
  uint8_t input_buffer_[k_tls_buffer_size];
  FlexibleOutputBuffer output_{net::buffer(output_buffer_)};
  FlexibleInputBuffer input_{net::buffer(input_buffer_)};
};

}  // namespace tls
}  // namespace net

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_TLS_BASE_H_
