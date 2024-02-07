/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include <string>

#include "mysql/harness/net_ts.h"

class Pump {
  class Connector;

 public:
  Pump(net::io_context &io_ctx,
       const net::ip::tcp::resolver::results_type &resolved)
      : io_ctx_{io_ctx}, conn_{io_ctx_}, connector_(conn_, resolved) {}

  void operator()() { connector_({}); }

 private:
  class Connector {
    enum class State {
      init,
      connect_inprogress,
      connect_finished,
      write,
      shutdown_send,
      wait_shutdown_recv,
      error,
      close
    };

   public:
    Connector(net::ip::tcp::socket &conn,
              const net::ip::tcp::resolver::results_type &resolved)
        : conn_(conn),
          endpoint_cur_{resolved.begin()},
          endpoint_end_{resolved.end()} {}

    stdx::expected<void, std::error_code> connect(
        const net::ip::tcp::endpoint &ep) {
#ifdef SOCK_NONBLOCK
      int open_flags = SOCK_NONBLOCK;
#else
      int open_flags = 0;
#endif

      conn_.open(ep.protocol(), open_flags);
#ifdef TCP_FASTOPEN_CONNECT
      conn_.set_option(net::ip::tcp::fast_open_connect(true));
#endif
      if (!conn_.native_non_blocking()) conn_.native_non_blocking(true);

      auto res = conn_.connect(ep);
      return res;
    }

    stdx::expected<void, std::error_code> connect_continue() {
      net::socket_base::error so_err;
      auto res = conn_.get_option(so_err);
      if (!res) return res;

      if (so_err.value() == 0) return {};

      return stdx::unexpected(
          std::error_code(net::impl::socket::make_error_code(so_err.value())));
    }

    stdx::expected<size_t, std::error_code> write_all() {
      return net::write(conn_, write_buffer_);
    }

    void operator()(std::error_code /* ec */) {
      bool is_final{false};

      while (!is_final) {
        switch (state_) {
          case State::init: {
            if (endpoint_cur_ == endpoint_end_) {
              // out of endpoint
              state_ = State::error;
              break;
            }

            auto res = connect(endpoint_cur_->endpoint());
            if (res) {
              state_ = State::connect_finished;
              break;
            } else if (res.error() == make_error_condition(
                                          std::errc::operation_in_progress) ||
                       res.error() == make_error_condition(
                                          std::errc::operation_would_block)) {
              state_ = State::connect_inprogress;

              conn_.async_wait(net::socket_base::wait_write, std::move(*this));
              return;
            } else if (res.error() == std::errc::connection_refused) {
              // check ec for ECONNREFUSED
              conn_.close();

              endpoint_cur_ = std::next(endpoint_cur_);

              state_ = State::init;
            } else {
              state_ = State::error;
            }
            break;
          }
          case State::connect_inprogress: {
            auto res = connect_continue();

            if (res) {
              state_ = State::connect_finished;
            } else if (res.error() ==
                       make_error_condition(std::errc::connection_refused)) {
              // check ec for ECONNREFUSED
              conn_.close();
              endpoint_cur_ = std::next(endpoint_cur_);

              state_ = State::init;
            } else {
              state_ = State::error;
            }

            break;
          }
          case State::connect_finished:
            write_buffer_storage_ = std::string(1024 * 1024, 'a');
            write_buffer_ = net::buffer(write_buffer_storage_);

            state_ = State::write;
            break;
          case State::write: {
            auto res = write_all();

            if (!res) {
              if (res.error() ==
                  make_error_condition(std::errc::operation_would_block)) {
                conn_.async_wait(net::socket_base::wait_write,
                                 std::move(*this));
                return;
              } else {
                state_ = State::error;
                break;
              }
            } else if (res.value() == write_buffer_storage_.size()) {
              state_ = State::shutdown_send;
              break;
            }

            break;
          }
          case State::shutdown_send:
            conn_.shutdown(net::socket_base::shutdown_send);

            state_ = State::wait_shutdown_recv;
            conn_.async_wait(net::socket_base::wait_write, std::move(*this));
            return;
          case State::wait_shutdown_recv:
            state_ = State::close;
            break;
          case State::error:
            state_ = State::close;
            break;
          case State::close:
            conn_.close();
            is_final = true;
            break;
        }
      }
    }

   private:
    net::ip::tcp::socket &conn_;

    net::ip::tcp::resolver::results_type::iterator endpoint_cur_;
    net::ip::tcp::resolver::results_type::iterator endpoint_end_;

    State state_{State::init};
    std::string write_buffer_storage_;
    net::mutable_buffer write_buffer_;
  };

 private:
  net::io_context &io_ctx_;
  net::ip::tcp::socket conn_;

  Connector connector_;
};

class PumpManager {
 public:
  PumpManager(net::io_context &io_ctx) : io_ctx_{io_ctx} {}

  void connect_all(net::ip::tcp::resolver::results_type &&resolved,
                   size_t num = 1) {
    for (size_t ndx = 0; ndx < num; ++ndx) {
      pumps_.emplace_back(io_ctx_, resolved);

      pumps_.back()();
    }
  }

  stdx::expected<net::ip::tcp::resolver::results_type, std::error_code> resolve(
      const std::string &hostname, const std::string &service) {
    net::ip::tcp::resolver resolver(io_ctx_);

    return resolver.resolve(hostname, service);
  }

 private:
  std::list<Pump> pumps_;

  net::io_context &io_ctx_;
};

int main() {
  net::impl::socket::init();

  net::io_context io_ctx;

  PumpManager mgr(io_ctx);
  auto resolve_res = mgr.resolve("localhost", "3307");
  if (!resolve_res) return EXIT_FAILURE;

  mgr.connect_all(std::move(resolve_res.value()), 1);

  io_ctx.run();

  return EXIT_SUCCESS;
}
