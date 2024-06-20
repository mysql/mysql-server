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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_CONNECTION_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_CONNECTION_H_

#include <algorithm>
#include <atomic>
#include <bitset>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "cno/core.h"
#include "http/base/connection_interface.h"
#include "http/base/connection_status_callbacks.h"
#include "http/base/details/owned_buffer.h"
#include "http/base/http_time.h"
#include "http/base/method.h"
#include "http/base/status_code.h"
#include "http/cno/buffer_sequence.h"
#include "http/cno/callback_init.h"
#include "http/cno/cno_interface.h"
#include "http/cno/error_code.h"
#include "http/cno/string.h"

#include "mysql/harness/net_ts/internet.h"

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {

namespace impl {

inline void set_socket_parent(net::ip::tcp::socket *, const char *) {
  // Do nothing, net::ip::tcp::socket, is missing the custom
  // `set_parent` method.
}

inline net::ip::tcp::socket *get_socket(net::ip::tcp::socket *s) { return s; }
inline const net::ip::tcp::socket *get_socket(const net::ip::tcp::socket *s) {
  return s;
}

template <typename T>
net::ip::tcp::socket *get_socket(T *s) {
  return get_socket(&s->lower_layer());
}

template <typename T>
auto *get_socket1(T *s) {
  return &s->lower_layer();
}

template <typename T>
const net::ip::tcp::socket *get_socket(const T *s) {
  return get_socket(&s->lower_layer());
}

template <typename T>
void set_socket_parent(T *s, const char *parent) {
  s->set_parent(parent);

  set_socket_parent(get_socket1(s), parent);
}

}  // namespace impl

enum Pending {
  k_pending_none = 0,
  k_pending_closing = 1 << 1,
  k_pending_reading = 1 << 2,
  k_pending_writing = 1 << 3
};

template <typename IOLayer>
class Connection : public base::ConnectionInterface, public cno::CnoInterface {
 public:
  using This = Connection<IOLayer>;
  using ConnectionStatusCallbacks = http::base::ConnectionStatusCallbacks<This>;
  using Methods = base::method::Bitset;
  using Headers = http::base::Headers;
  using owned_buffer = http::base::details::owned_buffer;
  using ref_buffers = http::base::details::ref_buffers<std::list<owned_buffer>>;
  using IO = IOLayer;

 public:
  Connection(IOLayer s, base::method::Bitset *allowed_method,
             ConnectionStatusCallbacks *connection_handler,
             CNO_CONNECTION_KIND kind, CNO_HTTP_VERSION version)
      : socket_(std::move(s)),
        allowed_method_(allowed_method),
        connection_handler_{connection_handler} {
    std::stringstream ss;
    ss << "HTTP-" << this;

    socket_.set_option(net::ip::tcp::no_delay{true});
    impl::set_socket_parent(&socket_, ss.str().c_str());
    cno_init(&cno_, kind);
    cno::callback_init(&cno_, this);
    output_buffers_.emplace_back(4096);
    cno_begin(&cno_, version);
  }

  ~Connection() override {
    cno_fini(&cno_);
    socket_.close();
  }

 public:  // ConnectionInterface implementation
  bool send(const uint32_t *stream_id_ptr, const int status_code,
            const std::string &method, const std::string &path,
            const Headers &headers, const IOBuffer &data) override {
    cno_message_t message;
    std::vector<cno_header_t> cno_header(headers.size(), cno_header_t());
    const bool only_header = 0 == data.length();

    auto output = cno_header.data();

    for (const auto &entry : headers) {
      output->name.size = entry.first.length();
      output->name.data = entry.first.c_str();

      output->value.size = entry.second.length();
      output->value.data = entry.second.c_str();
      ++output;
    }

    message.code = status_code;
    message.headers = cno_header.data();
    message.headers_len = cno_header.size();
    message.path.data = path.c_str();
    message.path.size = path.length();
    message.method.data = method.c_str();
    message.method.size = method.length();

    uint32_t stream_id =
        stream_id_ptr ? *stream_id_ptr : cno_next_stream(&cno_);
    if (CNO_OK != cno_write_head(&cno_, stream_id, &message, only_header)) {
      return false;
    }

    if (!only_header) {
      return CNO_OK == cno_write_data(&cno_, stream_id, data.get().c_str(),
                                      data.length(), true);
    }

    return true;
  }

  std::string get_peer_address() const override {
    auto *s = impl::get_socket(&socket_);
    if (auto e = s->remote_endpoint()) {
      return e->address().to_string();
    }

    return {};
  }

  uint16_t get_peer_port() const override {
    auto *s = impl::get_socket(&socket_);
    if (auto e = s->remote_endpoint()) {
      return e->port();
    }

    return 0;
  }

  IOLayer &get_socket() { return socket_; }

  void start() override { do_net_recv(); }

 protected:
  void do_net_send() {
    socket_.async_send(ref_buffers(output_buffers_),
                       [this](std::error_code error, auto size) {
                         switch (on_net_send(error, size)) {
                           case k_pending_none:
                             break;

                           case k_pending_reading:
                             break;

                           case k_pending_writing:
                             do_net_send();
                             break;

                           case k_pending_closing:
                             if (connection_handler_)
                               connection_handler_->on_connection_close(this);
                             break;
                         }
                       });
  }

  void do_net_recv() {
    socket_.async_receive(
        input_mutable_buffer_, [this](std::error_code error, auto size) {
          switch (on_net_receive(error, size)) {
            case k_pending_reading:
              do_net_recv();
              break;

            case k_pending_writing:
              break;

            case k_pending_none:
              break;

            case k_pending_closing:
              if (connection_handler_)
                connection_handler_->on_connection_close(this);
              break;
          }
        });
  }

  Pending on_net_receive(const std::error_code &ec,
                         std::size_t bytes_transferred) {
    if (!running_) {
      return stop_running() ? k_pending_writing : k_pending_closing;
    }

    if (ec) {
      stop_running();
      processed_request_ = false;
      output_pending_ = false;

      connection_handler_->on_connection_io_error(this, ec);
      return k_pending_closing;
    }

    const int result = cno_consume(
        &cno_, reinterpret_cast<char *>(input_buffer_), bytes_transferred);

    if (result < 0) {
      processed_request_ = false;
      output_pending_ = false;
      stop_running();
      auto ec = make_error_code(cno_error());
      connection_handler_->on_connection_io_error(this, ec);
      return k_pending_closing;
    }

    if (!keep_alive_) {
      return stop_running() ? k_pending_writing : k_pending_closing;
    }

    if (!running_) return k_pending_closing;
    if (suspend_) return k_pending_none;

    if (processed_request_) {
      if (!output_pending_) return k_pending_closing;
      return k_pending_none;
    }

    return k_pending_reading;
  }

  Pending on_net_send(const std::error_code &ec, size_t size) {
    bool has_more = true;
    bool should_close = false;
    {
      std::unique_lock<std::mutex> lock(output_buffer_mutex_);

      if (!ec) {
        while (size) {
          auto &page = output_buffers_.front();
          auto size_on_page = std::min(page.size(), size);
          page += size_on_page;
          size -= size_on_page;

          if (page.empty()) {
            if (1 == output_buffers_.size()) {
              page.reset();
            } else {
              output_buffers_.pop_front();
            }
          }
        }
      }

      if (0 == output_buffers_.front().size()) {
        has_more = false;
        processed_request_ = false;
        output_pending_ = false;

        if (!running_) {
          should_close = true;
        }
      }
    }

    if (ec) {
      stop_running();
      processed_request_ = false;
      output_pending_ = false;

      connection_handler_->on_connection_io_error(this, ec);
      return k_pending_closing;
    }
    if (has_more) return k_pending_writing;

    on_output_buffer_empty();

    if (should_close) return k_pending_closing;
    if (suspend_) return k_pending_none;

    return k_pending_reading;
  }

  void resume() { suspend_ = false; }
  void suspend() { suspend_ = true; }

  /**
   * Mark the connection that it should stop running
   *
   * @returns information if the object may be delete
   * @retval 'false' Connection object can be removed immediately
   * @retval 'true'  Connection object must wait until IO is finished.
   */
  bool stop_running() {
    std::unique_lock<std::mutex> lock(output_buffer_mutex_);
    running_ = false;

    return output_pending_;
  }

 protected:
  virtual void on_output_buffer_empty() {}

 protected:  // CnoInterface implementation
  int on_cno_writev(const cno_buffer_t *buffer, size_t count) override {
    bool was_first = false;
    {
      std::unique_lock<std::mutex> lock(output_buffer_mutex_);
      cno::BufferSequence<> buffers{buffer, count};

      bool expected = false;
      if (impl::get_socket(&socket_)->is_open())
        was_first = output_pending_.compare_exchange_weak(expected, true);
      auto source_it = buffers.begin();

      while (source_it != buffers.end()) {
        // The constructor fills the output buffer with single page
        // and all algorithms that clear not used pages, leave at
        // last one page.
        // thus we do not need to check if there are no pages.
        auto &obuffer = output_buffers_.back();

        if (0 == source_it->size()) {
          ++source_it;
          continue;
        }

        if (0 == obuffer.space_left()) {
          output_buffers_.emplace_back(4096);
          continue;
        }

        (*source_it) += obuffer.write(
            static_cast<const uint8_t *>(source_it->data()), source_it->size());
      }
    }

    if (was_first) {
      do_net_send();
    }

    return 0;
  }

  int on_cno_message_tail([[maybe_unused]] const uint32_t session_id,
                          [[maybe_unused]] const cno_tail_t *tail) override {
    processed_request_ = true;
    return 0;
  }

  int on_cno_stream_start([[maybe_unused]] const uint32_t id) override {
    return 0;
  }

  int on_cno_close() override {
    keep_alive_ = false;
    return 0;
  }

 protected:
  bool keep_alive_{true};
  IOLayer socket_;
  Methods *allowed_method_;
  cno_connection_t cno_;

  uint8_t input_buffer_[512];
  net::mutable_buffer input_mutable_buffer_{net::buffer(input_buffer_)};

  std::mutex output_buffer_mutex_;
  std::list<owned_buffer> output_buffers_;

  std::atomic<bool> processed_request_{false};
  std::atomic<bool> output_pending_{false};
  std::atomic<bool> running_{true};
  std::atomic<bool> suspend_{false};

  ConnectionStatusCallbacks *connection_handler_;
};

}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_CONNECTION_H_
