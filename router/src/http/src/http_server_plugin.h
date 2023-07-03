/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_HTTP_SERVER_PLUGIN_INCLUDED
#define MYSQLROUTER_HTTP_SERVER_PLUGIN_INCLUDED

#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysqlrouter/http_common.h"
#include "mysqlrouter/http_server_component.h"

class HttpRequestRouter {
 public:
  void append(const std::string &url_regex_str,
              std::unique_ptr<BaseRequestHandler> cb);
  void remove(const std::string &url_regex_str);

  // if no routes are specified, return 404
  void route_default(HttpRequest &req);

  void set_default_route(std::unique_ptr<BaseRequestHandler> cb);
  void clear_default_route();
  void route(HttpRequest req);

  void require_realm(const std::string &realm) { require_realm_ = realm; }

 private:
  struct RouterData {
    std::string url_regex_str;
    std::regex url_regex;
    std::unique_ptr<BaseRequestHandler> handler;
  };
  std::vector<RouterData> request_handlers_;

  std::unique_ptr<BaseRequestHandler> default_route_;
  std::string require_realm_;

  std::mutex route_mtx_;
};

/**
 * base class of all http request handler threads
 *
 * - HttpRequestMainThread opens the socket, and accepts and handles connections
 * - HttpRequestWorkerThread accepts and handles connections, using the socket
 *   listened by the main-thread
 *
 * As all threads can accept in parallel this may lead to a thundering herd
 * problem and quite likely it is better to let only one thread accept() and
 * push the socket handling into async-deque and let all workers steal from the
 * queue
 */
class HttpRequestThread {
 public:
  HttpRequestThread() {
    // enable all methods to allow the higher layers to handle them
    //
    // CONNECT, TRACE and OPTIONS are disabled by default if not explicitly
    // enabled.
    event_http_.set_allowed_http_methods(
        HttpMethod::Bitset().set(/*all types*/));
  }

  HttpRequestThread(HttpRequestThread &&object)
      : event_base_(std::move(object.event_base_)),
        event_http_(std::move(object.event_http_)),
        accept_fd_(object.accept_fd_) /* just copy */,
        initialized_(object.is_initalized()) /* just copy */ {}

  using native_handle_type = EventBaseSocket;

  native_handle_type get_socket_fd() { return accept_fd_; }

  void accept_socket();
  void set_request_router(HttpRequestRouter &router);
  void wait_and_dispatch();

 public:  // Thread safe methods
  void break_dispatching();
  void wait_until_ready();

 protected:
  static void on_event_loop_ready(native_handle_type, short, void *);

  bool is_initalized() const;
  void initialization_finished();

  EventBase event_base_;
  EventHttp event_http_{&event_base_};
  native_handle_type accept_fd_{kEventBaseInvalidSocket};
  WaitableMonitor<bool> initialized_{false};
};

class HttpServer {
 public:
  HttpServer(const char *address, uint16_t port)
      : address_(address), port_(port) {}

  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(const HttpServer &) = delete;

  HttpServer(HttpServer &&) = delete;
  HttpServer &operator=(HttpServer &&) = delete;

  void join_all();

  virtual ~HttpServer() { join_all(); }

  virtual void start(size_t max_threads);
  void stop();

  void add_route(const std::string &url_regex,
                 std::unique_ptr<BaseRequestHandler> cb);
  void remove_route(const std::string &url_regex);

  HttpRequestRouter &request_router() { return request_router_; }

 protected:
  std::vector<HttpRequestThread> thread_contexts_;
  std::string address_;
  uint16_t port_;
  HttpRequestRouter request_router_;

  net::io_context io_ctx_;
  net::ip::tcp::acceptor listen_sock_{io_ctx_};

  std::vector<std::thread> sys_threads_;
};

#endif
