/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <string>
#include <thread>
#include <vector>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/util.h>

#include "mysqlrouter/http_server_component.h"
#include "posix_re.h"

using harness_socket_t = evutil_socket_t;

void stop_eventloop(evutil_socket_t, short, void *cb_arg);

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
    PosixRE url_regex;
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
  HttpRequestThread()
      : ev_base(event_base_new(), &event_base_free),
        ev_http(evhttp_new(ev_base.get()), &evhttp_free),
        ev_shutdown_timer(event_new(ev_base.get(), -1, EV_PERSIST,
                                    stop_eventloop, ev_base.get()),
                          &event_free) {}

  harness_socket_t get_socket_fd() { return accept_fd_; }

  void accept_socket();
  void set_request_router(HttpRequestRouter &router);
  void wait_and_dispatch();

 protected:
  std::unique_ptr<event_base, decltype(&event_base_free)> ev_base;
  std::unique_ptr<evhttp, decltype(&evhttp_free)> ev_http;
  std::unique_ptr<event, decltype(&event_free)> ev_shutdown_timer;

  harness_socket_t accept_fd_{-1};
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
  void add_route(const std::string &url_regex,
                 std::unique_ptr<BaseRequestHandler> cb);
  void remove_route(const std::string &url_regex);

  HttpRequestRouter &request_router() { return request_router_; }

 protected:
  std::vector<HttpRequestThread> thread_contexts_;
  std::string address_;
  uint16_t port_;
  HttpRequestRouter request_router_;

  std::vector<std::thread> sys_threads_;
};

class HttpStaticFolderHandler : public BaseRequestHandler {
 public:
  explicit HttpStaticFolderHandler(std::string static_basedir,
                                   std::string require_realm)
      : static_basedir_(std::move(static_basedir)),
        require_realm_{std::move(require_realm)} {}

  void handle_request(HttpRequest &req) override;

 private:
  std::string static_basedir_;
  std::string require_realm_;
};

#endif
