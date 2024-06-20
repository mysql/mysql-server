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

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_SSL_OPERATION_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_SSL_OPERATION_H_

#include <openssl/bio.h>

#include <utility>

#include "mysql/harness/tls_error.h"
#include "tls/details/flexible_buffer.h"
#include "tls/details/lower_layer_completion.h"
#include "tls/details/ssl_operation.h"

namespace net {
namespace tls {

inline const mutable_buffer *tls_buffer_sequence_begin(
    const mutable_buffer &b) noexcept {
  return std::addressof(b);
}

inline const mutable_buffer *tls_buffer_sequence_end(
    const mutable_buffer &b) noexcept {
  return std::addressof(b) + 1;
}

inline const const_buffer *tls_buffer_sequence_begin(
    const const_buffer &b) noexcept {
  return std::addressof(b);
}

inline const const_buffer *tls_buffer_sequence_end(
    const const_buffer &b) noexcept {
  return std::addressof(b) + 1;
}

template <class C>
inline auto tls_buffer_sequence_begin(const C &c) noexcept
    -> decltype(c.begin()) {
  return c.begin();
}

template <class C>
inline auto tls_buffer_sequence_end(const C &c) noexcept -> decltype(c.end()) {
  return c.end();
}

class AsyncAction {
 public:
  template <typename Layer, typename Handler>
  auto recv(Layer *layer, FlexibleInputBuffer &input, Handler &&handler) {
    return layer->async_receive(input, handler);
  }

  template <typename Layer, typename Handler>
  auto send(Layer *layer, FlexibleOutputBuffer &output, Handler &&handler) {
    return layer->async_send(output, handler);
  }
};

class SyncAction {
 public:
  struct Unexpected {};
  using Handler_result = stdx::expected<Operation::Result, Unexpected>;
  using Handler_arguments = stdx::expected<size_t, std::error_code>;

  template <typename Layer, typename Handler>
  auto recv(Layer *layer, FlexibleInputBuffer &input, Handler &&) {
    return read_result_ = layer->read_some(input);
  }

  template <typename Layer, typename Handler>
  auto send(Layer *layer, FlexibleOutputBuffer &output, Handler &&) {
    return write_result_ = layer->write_some(output);
  }

  template <typename Handler>
  Handler_result handle_write_result(Handler *handler) {
    auto result = write_result_;
    write_result_ = {};
    if (result.has_value()) {
      return handler->handle_write({}, result.value());
    }

    return handler->handle_write(result.error(), 0);
  }

  template <typename Handler>
  Handler_result handle_read_result(Handler *handler) {
    auto result = read_result_;
    read_result_ = {};
    if (result.has_value()) {
      return handler->handle_read({}, result.value());
    }

    return handler->handle_read(result.error(), 0);
  }

  Handler_arguments write_result_;
  Handler_arguments read_result_;
};

template <typename SslIO, typename BufferSequence, typename Token,
          typename TlsLayer, typename Action = AsyncAction>
class SslIoCompletionToken {
 public:
  using Token_result = std::decay_t<Token>;
  using Token_handler =
      std::conditional_t<std::is_same<Token, Token_result>::value,
                         Token_result &, Token_result>;

  template <typename UniToken>
  SslIoCompletionToken(TlsLayer &tls_layer, const BufferSequence &buffer,
                       UniToken &&token, Action action = Action())
      : tls_layer_{tls_layer},
        output_{tls_layer_.output_},
        input_{tls_layer_.input_},
        buffer_{buffer},
        token_{std::forward<UniToken>(token)},
        action_{action} {}

  SslIoCompletionToken(SslIoCompletionToken &&other)
      : number_bytes_transfered_{other.number_bytes_transfered_},
        tls_layer_{other.tls_layer_},
        output_{tls_layer_.output_},
        input_{tls_layer_.input_},
        buffer_{other.buffer_},
        token_{other.token_},
        action_{other.action_} {}

  SslIoCompletionToken(const SslIoCompletionToken &other)
      : number_bytes_transfered_{other.number_bytes_transfered_},
        tls_layer_{other.tls_layer_},
        output_{tls_layer_.output_},
        input_{tls_layer_.input_},
        buffer_{other.buffer_},
        token_{other.token_},
        action_{other.action_} {}

  Operation::Result handle_read(std::error_code ec, size_t size) {
    if (ec) {
      do_token(ec, 0);
      return Operation::Result::fatal;
    }

    input_.push(size);
    return do_read();
  }

  Operation::Result handle_write(std::error_code ec, size_t size) {
    if (ec) {
      do_token(ec, 0);
      return Operation::Result::fatal;
    }

    output_.pop(size);
    if (0 != net::buffer_size(output_)) {
      return do_write();
    }

    return do_it();
  }

  Operation::Result do_it() {
    auto it = tls_buffer_sequence_begin(buffer_);
    auto it_next = it;
    auto end = tls_buffer_sequence_end(buffer_);

    ++it_next;
    size_t page_begin = 0;
    size_t page_end = it->size();

    while (it != end) {
      const bool is_last = it_next == end;
      if (!is_last && number_bytes_transfered_ >= page_end) {
        it = it_next++;
        page_begin = page_end;
        if (it != end) page_end += it->size();
        continue;
      }

      auto page_offset = number_bytes_transfered_ - page_begin;
      size_t number_of_bytes = 0;
      auto result =
          SslIO::op(tls_layer_.network_bio_.get(), tls_layer_.ssl_.get(),
                    cast_and_increment<uint8_t>(it->data(), page_offset),
                    it->size() - page_offset, &number_of_bytes);

      number_bytes_transfered_ += number_of_bytes;

      debug_print("do_it - ", (SslIO::is_read_operation() ? "read" : "write"),
                  " - result:", result,
                  " - number_bytes_transfered_:", number_bytes_transfered_);
      switch (result) {
        case Operation::Result::fatal:
          do_token(make_tls_error(), 0);
          return result;

        case Operation::Result::close:
          do_token(std::make_error_code(std::errc::broken_pipe), 0);
          return result;

        case Operation::Result::ok:
          do_token({}, number_bytes_transfered_);
          return result;

        case Operation::Result::want_read: {
          if (number_bytes_transfered_ && SslIO::is_read_operation()) {
            do_token({}, number_bytes_transfered_);
            return Operation::Result::ok;
          }
          return do_read();
        }

        case Operation::Result::want_write:
          return do_write();
      }
    }
    do_token({}, number_bytes_transfered_);
    return Operation::Result::ok;
  }

  template <typename HandlerToken>
  LowerLayerWriteCompletionToken<HandlerToken> get_write_handler(
      HandlerToken &&token) {
    return LowerLayerWriteCompletionToken<HandlerToken>(
        std::forward<HandlerToken>(token), NOP_token());
  }

  template <typename HandlerToken>
  LowerLayerReadCompletionToken<HandlerToken, NOP_token> get_read_handler(
      HandlerToken &&token) {
    return LowerLayerReadCompletionToken<HandlerToken, NOP_token>(
        std::forward<HandlerToken>(token), NOP_token());
  }

  void do_token(const std::error_code &ec, const size_t no_of_bytes) {
    token_(ec, no_of_bytes);
  }

  int bio_read_ex(size_t *out_readbytes) {
    auto bio = tls_layer_.network_bio_.get();
    *out_readbytes = 0;
#if OPENSSL_VERSION_NUMBER >= NET_TLS_USE_BACKWARD_COMPATIBLE_OPENSSL
    auto result = BIO_read_ex(bio, output_.data_free(), output_.size_free(),
                              out_readbytes);
#else
    auto result = BIO_read(bio, output_.data_free(), output_.size_free());
    if (result > 0) *out_readbytes = result;
#endif

    return result;
  }

  int bio_write_ex(size_t *out_written) {
    auto bio = tls_layer_.network_bio_.get();
    *out_written = 0;
#if OPENSSL_VERSION_NUMBER >= NET_TLS_USE_BACKWARD_COMPATIBLE_OPENSSL
    auto result =
        BIO_write_ex(bio, input_.data_used(), input_.size_used(), out_written);
#else
    auto result = BIO_write(bio, input_.data_used(), input_.size_used());
    if (result > 0) *out_written = result;
#endif

    return result;
  }

  Operation::Result do_write() {
    debug_print("do_write - ", (SslIO::is_read_operation() ? "read" : "write"));

    if (0 == net::buffer_size(output_)) {
      size_t readbytes;
      bio_read_ex(&readbytes);
      output_.push(readbytes);
    }

    action_.send(&tls_layer_.lower_layer_, output_,
                 get_write_handler(std::move(*this)));

    return Operation::Result::want_write;
  }

  Operation::Result do_read() {
    debug_print("do_read - ", (SslIO::is_read_operation() ? "read" : "write"));
    if (0 == input_.size_used()) {
      action_.recv(&tls_layer_.lower_layer_, input_,
                   get_read_handler(std::move(*this)));
      return Operation::Result::want_read;
    }

    size_t written;
    bio_write_ex(&written);
    input_.pop(written);
    return do_it();
  }

  template <typename... Parameters>
  void debug_print([[maybe_unused]] Parameters &&...parameters) const {
    //    (std::cout << ... << std::forward<Parameters>(parameters));
    //    std::cout << std::endl;
  }

  template <typename Type>
  static const Type *cast_and_increment(const void *ptr, int value) {
    return static_cast<const Type *>(ptr) + value;
  }

  template <typename Type>
  static Type *cast_and_increment(void *ptr, int value) {
    return static_cast<Type *>(ptr) + value;
  }

  size_t number_bytes_transfered_{0};
  TlsLayer &tls_layer_;
  FlexibleOutputBuffer &output_;
  FlexibleInputBuffer &input_;
  const BufferSequence buffer_;
  Token token_;
  Action action_;
};

}  // namespace tls
}  // namespace net

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_DETAILS_SSL_OPERATION_H_
