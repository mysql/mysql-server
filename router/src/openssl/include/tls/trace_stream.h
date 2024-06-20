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

#ifndef ROUTER_SRC_OPENSSL_INCLUDE_TLS_TRACE_STREAM_H_
#define ROUTER_SRC_OPENSSL_INCLUDE_TLS_TRACE_STREAM_H_

#include <algorithm>
#include <functional>
#include <iomanip>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "mysql/harness/net_ts/buffer.h"
#ifndef USE_CUSTOM_HOLDER
#include "tls/mutex_static_holder.h"
#endif  // USE_CUSTOM_HOLDER
#include "tls/details/lower_layer_completion.h"

// In future instead on defining output-stream methods here and a mutex to
// access it, extract Console/terminal class.
struct Trace_stream_static_holder {};

template <typename NameType, typename LowerLevelStream>
class TraceStream : Mutex_static_holder<Trace_stream_static_holder> {
  // Mutex_static_holder<Trace_stream_static_holder> - makes mutex static
  // variable common for different instantiations of this template

 public:
  using native_handle_type = std::nullptr_t;
  using protocol_type = std::nullptr_t;
  using endpoint_type = typename LowerLevelStream::endpoint_type;
  using VectorOfBuffers = std::vector<net::mutable_buffer>;
  using VectorOfConstBuffers = std::vector<net::const_buffer>;

  TraceStream(TraceStream &&other)
      : recv_buffer_{other.recv_buffer_},
        send_buffer_{other.send_buffer_},
        lower_layer_{std::move(other.lower_layer_)},
        out_{other.out_} {
    print("ctor move");
  }

  template <typename... Args>
  TraceStream(Args &&...args)  // NOLINT(runtime/explicit)
      : lower_layer_{std::forward<Args>(args)...}, out_{NameType::get_out()} {
    print("ctor");
  }

  auto get_executor() { return lower_layer_.get_executor(); }

  LowerLevelStream &lower_layer() { return lower_layer_; }
  const LowerLevelStream &lower_layer() const { return lower_layer_; }

  template <class MutableBufferSequence>
  stdx::expected<size_t, std::error_code> read_some(
      const MutableBufferSequence &buffers) {
    return lower_layer_.read_some(buffers);
  }

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> write_some(
      const ConstBufferSequence &buffers) {
    return lower_layer_.write_some(buffers);
  }

  template <typename Buffer, typename Handler>
  void async_send(const Buffer &buffer, Handler &&handler) {
    copy(send_buffer_, buffer);
    print("async_send buffer-size: ", net::buffer_size(buffer));
    WrappedTraceStream ref(this);
    lower_layer_.async_send(send_buffer_, get_write_handler(ref, handler));
  }

  template <class HandshakeType, class CompletionToken>
  auto async_handshake(HandshakeType type, CompletionToken &&token) {
    print("async_handshake type: ", type);
    WrappedTraceStream ref(this);
    lower_layer_.async_handshake(type, get_handshake_handler(ref, token));
  }

  template <typename Buffer, typename Handler>
  void async_receive(const Buffer &buffer, Handler &&handler) {
    copy(recv_buffer_, buffer);
    print("async_receive buffer-size: ", net::buffer_size(buffer));
    WrappedTraceStream ref(this);
    lower_layer_.async_receive(recv_buffer_, get_read_handler(ref, handler));
  }

  void set_parent(const char *parent) { parent_ = parent; }

  auto cancel() {
    print("cancel");
    return lower_layer_.cancel();
  }

  auto close() {
    print("close");
    return lower_layer_.close();
  }

  void handle_read(std::error_code ec, size_t size) {
    print("handle_read error:", ec, ", size:", size);

    dump(recv_buffer_, size);
  }

  void handle_write(std::error_code ec, size_t size) {
    print("handle_write error:", ec, ", size:", size);

    dump(send_buffer_, size);
  }

  void handle_handshake(std::error_code ec, size_t size) {
    print("handle_handshake error:", ec, ", size:", size);
  }

  template <typename SettableSocketOption>
  stdx::expected<void, std::error_code> set_option(
      const SettableSocketOption &option) {
    return lower_layer_.set_option(option);
  }

  template <typename... Args>
  void print(const Args &...args) {
    std::unique_lock<std::mutex> l{mutex_};
    *out_ << "this:" << parent_ << ", thread:" << std::this_thread::get_id()
          << ", " << NameType::get_name() << ": ";
    print_single(args...);
    *out_ << std::endl;
    out_->flush();
  }

 protected:
  class WrappedTraceStream {
   public:
    WrappedTraceStream(TraceStream *parent = nullptr) : parent_{parent} {}
    WrappedTraceStream(const WrappedTraceStream &other)
        : parent_{other.parent_} {}
    WrappedTraceStream(WrappedTraceStream &&other) : parent_{other.parent_} {}

    void handle_read(std::error_code ec, size_t size) {
      if (parent_) parent_->handle_read(ec, size);
    }

    void handle_write(std::error_code ec, size_t size) {
      if (parent_) parent_->handle_write(ec, size);
    }

    void handle_handshake(std::error_code ec, size_t size) {
      if (parent_) parent_->handle_handshake(ec, size);
    }

   private:
    TraceStream *parent_;
  };

  template <typename Vector>
  void dump(Vector &data, size_t s) {
    std::unique_lock<std::mutex> l{mutex_};
    auto it = data.begin();
    size_t offset = 0;

    while (it != data.end() && s) {
      auto data_on_page = std::min(s, it->size());
      const char *data_ptr = (const char *)it->data();
      auto to_print = data_on_page;

      while (to_print) {
        auto line = std::min(to_print, 16UL);
        *out_ << "this:" << parent_ << " " << std::hex << std::setfill('0')
              << std::setw(8) << offset << " | ";
        for (size_t i = 0; i < line; ++i) {
          *out_ << " 0x" << std::setw(2) << (int)((const uint8_t *)data_ptr)[i];
        }

        *out_ << std::string((16 - line) * 5, ' ');

        *out_ << " - ";
        for (size_t i = 0; i < line; ++i) {
          *out_ << (iscntrl(data_ptr[i]) ? '?' : data_ptr[i]);
        }

        *out_ << std::dec << std::setw(1) << std::endl;

        to_print -= line;
        data_ptr += line;
        s -= line;
        offset += line;
      }
    }

    out_->flush();
  }

  template <typename Vector, typename SrcBuffers>
  void copy(Vector &dst, const SrcBuffers &src) {
    dst.resize(0);
    auto it = net::buffer_sequence_begin(src);
    auto end = net::buffer_sequence_end(src);
    while (it != end) {
      dst.emplace_back(it->data(), it->size());
      ++it;
    }
  }

  void print_single() {}

  template <typename Arg, typename... Args>
  void print_single(const Arg &arg, const Args... args) {
    *out_ << arg;
    print_single(args...);
  }

  template <typename WriteToken, typename StandardToken>
  net::tls::LowerLayerWriteCompletionToken<WriteToken, StandardToken>
  get_write_handler(WriteToken &write_token, StandardToken &std_token) {
    using Write_token_result = std::decay_t<WriteToken>;
    using Write_token_handler =
        std::conditional_t<std::is_same<WriteToken, Write_token_result>::value,
                           Write_token_result &, Write_token_result>;

    using Standard_token_result = std::decay_t<StandardToken>;
    using Standard_token_handler = std::conditional_t<
        std::is_same<StandardToken, Standard_token_result>::value,
        Standard_token_result &, Standard_token_result>;

    return net::tls::LowerLayerWriteCompletionToken<WriteToken, StandardToken>(
        std::forward<Write_token_handler>(write_token),
        std::forward<Standard_token_handler>(std_token));
  }

  template <typename ReadToken, typename StandardToken>
  net::tls::LowerLayerReadCompletionToken<ReadToken, StandardToken>
  get_read_handler(ReadToken &read_token, StandardToken &std_token) {
    using Read_token_result = std::decay_t<ReadToken>;
    using Read_token_handler =
        std::conditional_t<std::is_same<ReadToken, Read_token_result>::value,
                           Read_token_result &, Read_token_result>;

    using Standard_token_result = std::decay_t<StandardToken>;
    using Standard_token_handler = std::conditional_t<
        std::is_same<StandardToken, Standard_token_result>::value,
        Standard_token_result &, Standard_token_result>;
    return net::tls::LowerLayerReadCompletionToken<ReadToken, StandardToken>(
        std::forward<Read_token_handler>(read_token),
        std::forward<Standard_token_handler>(std_token));
  }

  template <typename HandshakeToken, typename StandardToken>
  net::tls::LowerLayerHandshakeCompletionToken<HandshakeToken, StandardToken>
  get_handshake_handler(HandshakeToken &handshake_token,
                        StandardToken &std_token) {
    using Handshake_token_result = std::decay_t<HandshakeToken>;
    using Handshake_token_handler = std::conditional_t<
        std::is_same<HandshakeToken, Handshake_token_result>::value,
        Handshake_token_result &, Handshake_token_result>;

    using Standard_token_result = std::decay_t<StandardToken>;
    using Standard_token_handler = std::conditional_t<
        std::is_same<StandardToken, Standard_token_result>::value,
        Standard_token_result &, Standard_token_result>;
    return net::tls::LowerLayerHandshakeCompletionToken<HandshakeToken,
                                                        StandardToken>(
        std::forward<Handshake_token_handler>(handshake_token),
        std::forward<Standard_token_handler>(std_token));
  }

  VectorOfBuffers recv_buffer_;
  VectorOfConstBuffers send_buffer_;
  LowerLevelStream lower_layer_;
  std::ostream *out_;
  std::string parent_;
};

template <typename NameType, typename LowerLevelStream>
class TlsTraceStream : public TraceStream<NameType, LowerLevelStream> {
 public:
  using This = TraceStream<NameType, LowerLevelStream>;
  using This::TraceStream;
  using LowerLayerType = typename LowerLevelStream::LowerLayerType;

  LowerLayerType &lower_layer() { return this->lower_layer_.lower_layer(); }
  const LowerLayerType &lower_layer() const {
    return this->lower_layer_.lower_layer();
  }
};

#endif  // ROUTER_SRC_OPENSSL_INCLUDE_TLS_TRACE_STREAM_H_
