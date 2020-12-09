/*
  Copyright (c) 2017, 2020, Oracle and/or its affiliates.

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

#include "mysql_server_mock.h"

#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>  // cout
#include <memory>    // shared_ptr
#include <mutex>
#include <stdexcept>  // runtime_error
#include <string>
#include <system_error>
#include <thread>
#include <utility>  // move

#include "classic_mock_session.h"
#include "common.h"  // rename_thread()
#include "duktape_statement_reader.h"
#include "mock_session.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/mpmc_queue.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"  // net::ip::tcp
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/tls_server_context.h"
#include "x_mock_session.h"
IMPORT_LOG_FUNCTIONS()

using namespace std::chrono_literals;
using namespace std::string_literals;

namespace server_mock {

static constexpr const size_t kWorkerThreadCount{8};

MySQLServerMock::MySQLServerMock(std::string expected_queries_file,
                                 std::string module_prefix,
                                 std::string bind_address, unsigned bind_port,
                                 std::string protocol_name, bool debug_mode,
                                 TlsServerContext &&tls_server_ctx,
                                 mysql_ssl_mode ssl_mode)
    : bind_address_(std::move(bind_address)),
      bind_port_{bind_port},
      debug_mode_{debug_mode},
      expected_queries_file_{std::move(expected_queries_file)},
      module_prefix_{std::move(module_prefix)},
      protocol_name_(std::move(protocol_name)),
      tls_server_ctx_{std::move(tls_server_ctx)},
      ssl_mode_{ssl_mode} {
  if (debug_mode_)
    std::cout << "\n\nExpected SQL queries come from file '"
              << expected_queries_file << "'\n\n"
              << std::flush;
}

// close all active connections
void MySQLServerMock::close_all_connections() {
  // interrupt all worker threads.
  shared_([](auto &shared) {
    for (size_t ndx = 0; ndx < kWorkerThreadCount; ndx++) {
      // either the thread is blocked on a poll() or a mpmc-pop()
      std::array<const char, 1> ping_byte = {'.'};
      shared.wakeup_sock_send_.send(net::buffer(ping_byte));
    }
  });
}

void MySQLServerMock::run(mysql_harness::PluginFuncEnv *env) {
  mysql_harness::rename_thread("SM Main");

  setup_service();
  mysql_harness::on_service_ready(env);
  handle_connections(env);
}

void MySQLServerMock::setup_service() {
  net::ip::tcp::resolver resolver(io_ctx_);

  auto resolve_res =
      resolver.resolve(bind_address_, std::to_string(bind_port_));
  if (!resolve_res) {
    throw std::system_error(resolve_res.error(),
                            "resolve(" + bind_address_ + ", " +
                                std::to_string(bind_port_) + ") failed");
  }

  auto &ainfo = *resolve_res.value().begin();

  net::ip::tcp::acceptor sock(io_ctx_);

  auto res = sock.open(ainfo.endpoint().protocol());
  if (!res) {
    throw std::system_error(res.error(), "socket.open() failed");
  }

  res = sock.set_option(net::socket_base::reuse_address{true});
  if (!res) {
    throw std::system_error(res.error(), "socket.set_option() failed");
  }

  res = sock.bind(ainfo.endpoint());
  if (!res) {
    throw std::system_error(res.error(), "socket.bind(" + bind_address_ + ":" +
                                             std::to_string(bind_port_) +
                                             ") failed");
  }

  res = sock.listen(kListenQueueSize);
  if (!res) {
    throw std::system_error(res.error(), "socket.listen() failed");
  }

  listener_ = std::move(sock);
}

class StatementReaderFactory {
 public:
  static StatementReaderBase *create(
      const std::string &filename, std::string &module_prefix,
      std::map<std::string, std::string> session_data,
      std::shared_ptr<MockServerGlobalScope> shared_globals) {
    if (filename.substr(filename.size() - 3) == ".js") {
      return new DuktapeStatementReader(filename, module_prefix, session_data,
                                        shared_globals);
    } else {
      throw std::runtime_error("can't create reader for " + filename);
    }
  }
};

// the MPMC queue creates a dummy Node<Work> which needs to be default
// constructable.
net::io_context dummy_io_ctx;

struct Work {
  net::ip::tcp::socket client_socket{dummy_io_ctx};
  std::string expected_queries_file;
  std::string module_prefix;
  bool debug_mode;

  net::impl::socket::native_handle_type wakeup_fd;
};

using socket_pair_protocol =
#if defined(_WIN32)
    net::ip::tcp
#else
    local::stream_protocol
#endif
    ;

stdx::expected<void, std::error_code> connect_pair(
    net::io_context &io_ctx, socket_pair_protocol::socket &sock1,
    socket_pair_protocol::socket &sock2) {
#if defined(_WIN32)
  auto sockpair_proto = socket_pair_protocol::v4();
  auto pair_res = net::impl::socket::socketpair(sockpair_proto.family(),
                                                sockpair_proto.type(),
                                                sockpair_proto.protocol());
  if (!pair_res) {
    return pair_res.get_unexpected();
  }

  auto assign_res = sock1.assign(sockpair_proto, pair_res.value().first);
  if (!assign_res) {
    return assign_res.get_unexpected();
  }
  assign_res = sock2.assign(sockpair_proto, pair_res.value().second);
  if (!assign_res) {
    return assign_res.get_unexpected();
  }

  return {};
#else
  return local::connect_pair(&io_ctx, sock1, sock2);
#endif
}

void MySQLServerMock::handle_connections(mysql_harness::PluginFuncEnv *env) {
  log_info("Starting to handle connections on port: %d", bind_port_);

  mysql_harness::WaitingMPMCQueue<Work> work_queue;

  socket_pair_protocol::socket sock2(io_ctx_);
  auto connect_pair_res = shared_([this, &sock2](auto &shared) {
    return connect_pair(io_ctx_, shared.wakeup_sock_send_, sock2);
  });

  if (!connect_pair_res) {
    log_error("%s", connect_pair_res.error().message().c_str());
    return;
  }

  auto connection_handler = [&]() -> void {
    mysql_harness::rename_thread("SM Worker");

    while (true) {
      auto work = work_queue.pop();

      // exit
      if (!work.client_socket.is_open()) break;

      ProtocolBase *protocol{};
      std::unique_ptr<MySQLXProtocol> x_protocol;
      std::unique_ptr<MySQLClassicProtocol> classic_protocol;
      if (protocol_name_ == "x") {
        x_protocol = std::make_unique<MySQLXProtocol>(
            std::move(work.client_socket), work.wakeup_fd, tls_server_ctx_);

        protocol = x_protocol.get();
      } else if (protocol_name_ == "classic") {
        classic_protocol = std::make_unique<MySQLClassicProtocol>(
            std::move(work.client_socket), work.wakeup_fd, tls_server_ctx_);
        protocol = classic_protocol.get();
      }

      try {
        std::unique_ptr<StatementReaderBase> statement_reader{
            StatementReaderFactory::create(
                work.expected_queries_file, work.module_prefix,
                // expose session data json-encoded string
                {
                    {"port", std::to_string(bind_port_)},
                    {"ssl_cipher", "\"\""},
                    {"mysqlx_ssl_cipher", "\"\""},
                },
                MySQLServerSharedGlobals::get())};

        std::unique_ptr<MySQLServerMockSession> session;
        const bool with_tls = ssl_mode_ != SSL_MODE_DISABLED;
        if (protocol_name_ == "classic") {
          session = std::make_unique<MySQLServerMockSessionClassic>(
              classic_protocol.get(), std::move(statement_reader),
              work.debug_mode, with_tls);
        } else if (protocol_name_ == "x") {
          session = std::make_unique<MySQLServerMockSessionX>(
              x_protocol.get(), std::move(statement_reader), work.debug_mode,
              with_tls);
        }

        try {
          session->run();
        } catch (const std::exception &e) {
          log_error("%s", e.what());
        }
      } catch (const std::exception &e) {
        if (protocol != nullptr) {
          protocol->send_error(1064, "reader error: "s + e.what());
        }
        // close the connection before Session took over.
        log_error("%s", e.what());
      }
    }
  };

  auto res = listener_.native_non_blocking(true);
  if (!res) {
    log_error("set socket non-blocking failed, ignoring: %s",
              res.error().message().c_str());
  }

  std::deque<std::thread> worker_threads;
  // open enough worker threads to handle the needs of the tests:
  //
  // e.g. routertest_component_rest_routing keeps 4 connections open
  // and tries to open another 3.
  for (size_t ndx = 0; ndx < kWorkerThreadCount; ndx++) {
    worker_threads.emplace_back(connection_handler);
  }

  while (is_running(env)) {
    std::array<pollfd, 1> fds = {{
        {listener_.native_handle(), POLLIN, 0},
    }};

    auto poll_res = net::impl::poll::poll(fds.data(), fds.size(), 10ms);
    if (!poll_res) {
      if (poll_res.error() == make_error_condition(std::errc::interrupted) ||
          poll_res.error() ==
              make_error_condition(std::errc::operation_would_block) ||
          poll_res.error() == make_error_condition(std::errc::timed_out)) {
        continue;
      } else {
        log_error("poll() failed with error: %s",
                  poll_res.error().message().c_str());
        // leave the loop
        break;
      }
    }

    if (fds[0].revents != 0) {
      while (true) {
        net::ip::tcp::endpoint client_ep;
        auto accept_res = listener_.accept(client_ep);
        if (!accept_res) {
          auto accept_ec = accept_res.error();

          // if we got interrupted at shutdown, just leave
          if (!is_running(env)) break;

          if (accept_ec == std::errc::resource_unavailable_try_again) break;
          if (accept_ec == std::errc::operation_would_block) break;

          if (accept_ec == std::errc::interrupted) continue;

          log_error("%s",
                    std::system_error(accept_ec, "accept() failed").what());
          return;
        }

        auto client_socket = std::move(accept_res.value());

        work_queue.push(Work{std::move(client_socket), expected_queries_file_,
                             module_prefix_, debug_mode_,
                             sock2.native_handle()});
      }
    }
  }

  close_all_connections();

  for (size_t ndx = 0; ndx < worker_threads.size(); ndx++) {
    work_queue.push(Work{net::ip::tcp::socket(io_ctx_), "", "", false,
                         sock2.native_handle()});
  }
  for (auto &thr : worker_threads) {
    thr.join();
  }
}

std::shared_ptr<MockServerGlobalScope>
    MySQLServerSharedGlobals::shared_globals_;

std::mutex MySQLServerSharedGlobals::mtx_;

}  // namespace server_mock
