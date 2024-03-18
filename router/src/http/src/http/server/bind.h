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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_SERVER_BIND_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_SERVER_BIND_H_

#include <cstdint>
#include <string>
#include <utility>

#include "mysqlrouter/http_server_lib_export.h"

#include "helper/wait_variable.h"
#include "mysql/harness/net_ts/internet.h"

namespace http {
namespace server {

class HTTP_SERVER_LIB_EXPORT Bind {
 public:
  using io_context = net::io_context;
  using resolver = net::ip::tcp::resolver;
  using acceptor = net::ip::tcp::acceptor;
  using socket_type = acceptor::socket_type;
  using endpoint = acceptor::endpoint_type;

  Bind(io_context *io_context, const std::string &address, const uint16_t port);

  static bool is_not_fatal(const std::error_code &error) {
    return error == std::errc::resource_unavailable_try_again ||
           error == std::errc::interrupted;
  }

  template <typename Callback>
  void start_accepting_loop(Callback callback) {
    context_->get_executor().post(
        [this, callback]() {
          if (sync_state_.exchange(State::kInitializing, State::kRunning)) {
            on_new_socket_callback(callback);
          }
        },
        nullptr);
  }

  void stop_accepting_loop() {
    sync_state_.change([this](auto &value) {
      switch (value) {
        case State::kInitializing:
          value = State::kTerminated;
          break;
        case State::kRunning:
          value = State::kStopping;
          break;
        default:
          break;
      }
      socket_.cancel();
    });

    sync_state_.wait(State::kTerminated);
  }

  endpoint local_endpoint() { return socket_.local_endpoint().value(); }

 private:
  template <typename Callback>
  void on_new_socket_callback(Callback callback) {
    socket_.async_accept(
        [this, callback](const std::error_code &error, socket_type socket) {
          if (!error || is_not_fatal(error)) {
            if (sync_state_.is(State::kRunning,
                               [this, &error, &socket, &callback]() {
                                 if (!error) callback(std::move(socket));
                                 on_new_socket_callback(callback);
                               })) {
              return;
            }
          }
          sync_state_.set(State::kTerminated);
        });
  }

  enum class State { kInitializing, kRunning, kStopping, kTerminated };

  io_context *context_;
  resolver resolver_{*context_};
  acceptor socket_{*context_};
  WaitableVariable<State> sync_state_{State::kInitializing};
};

}  // namespace server
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_SERVER_BIND_H_
