/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include <chrono>
#include <iostream>  // cout
#include <memory>    // shared_ptr
#include <mutex>
#include <stdexcept>  // runtime_error
#include <string>
#include <system_error>
#include <utility>  // move

#include "classic_mock_session.h"
#include "duktape_statement_reader.h"
#include "mock_session.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"  // net::ip::tcp
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/tls_server_context.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/utils.h"  // to_string
#include "router/src/mock_server/src/statement_reader.h"
#include "scope_guard.h"
#include "x_mock_session.h"

IMPORT_LOG_FUNCTIONS()

using namespace std::chrono_literals;
using namespace std::string_literals;

namespace server_mock {

MySQLServerMock::MySQLServerMock(net::io_context &io_ctx,
                                 std::string expected_queries_file,
                                 std::vector<std::string> module_prefixes,
                                 std::string bind_address, unsigned bind_port,
                                 std::string protocol_name, bool debug_mode,
                                 TlsServerContext &&tls_server_ctx,
                                 mysql_ssl_mode ssl_mode)
    : bind_address_(std::move(bind_address)),
      bind_port_{bind_port},
      debug_mode_{debug_mode},
      io_ctx_{io_ctx},
      expected_queries_file_{std::move(expected_queries_file)},
      module_prefixes_{std::move(module_prefixes)},
      protocol_name_(std::move(protocol_name)),
      tls_server_ctx_{std::move(tls_server_ctx)},
      ssl_mode_{ssl_mode} {
  if (debug_mode_)
    std::cout << "\n\nExpected SQL queries come from file '"
              << expected_queries_file << "'\n\n"
              << std::flush;
}

void MySQLServerMock::close_all_connections() {
  client_sessions_([](auto &socks) {
    for (auto &conn : socks) {
      conn->cancel();
    }
  });
}

class Acceptor {
 public:
  using protocol_type = net::ip::tcp;

  Acceptor(net::io_context &io_ctx, std::string protocol_name,
           WaitableMonitor<std::list<std::unique_ptr<MySQLServerMockSession>>>
               &client_sessions,
           DuktapeStatementReaderFactory &&reader_maker,
           TlsServerContext &tls_server_ctx, bool with_tls)
      : io_ctx_{io_ctx},
        reader_maker_{std::move(reader_maker)},
        protocol_name_{std::move(protocol_name)},
        client_sessions_{client_sessions},
        tls_server_ctx_{tls_server_ctx},
        with_tls_{with_tls} {}

  ~Acceptor() { stop(); }

  stdx::expected<void, std::error_code> init(std::string address,
                                             uint16_t port) {
    net::ip::tcp::resolver resolver(io_ctx_);

    auto resolve_res = resolver.resolve(address, std::to_string(port));
    if (!resolve_res) return resolve_res.get_unexpected();

    for (auto ainfo : resolve_res.value()) {
      net::ip::tcp::acceptor sock(io_ctx_);

      auto res = sock.open(ainfo.endpoint().protocol());
      if (!res) return res.get_unexpected();

      res = sock.set_option(net::socket_base::reuse_address{true});
      if (!res) return res.get_unexpected();

      res = sock.bind(ainfo.endpoint());
      if (!res) return res.get_unexpected();

      res = sock.listen(256);
      if (!res) return res.get_unexpected();

      sock_ = std::move(sock);

      return {};
    }

    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  void accepted(protocol_type::socket client_sock) {
    auto reader = reader_maker_();

    auto session_it = client_sessions_([&](auto &socks) {
      if (protocol_name_ == "classic") {
        socks.emplace_back(std::make_unique<MySQLServerMockSessionClassic>(
            MySQLClassicProtocol{std::move(client_sock), client_ep_,
                                 tls_server_ctx_},
            std::move(reader), false, with_tls_));
      } else {
        socks.emplace_back(std::make_unique<MySQLServerMockSessionX>(
            MySQLXProtocol{std::move(client_sock), client_ep_, tls_server_ctx_},
            std::move(reader), false, with_tls_));
      }
      return std::prev(socks.end());
    });

    auto &session = *session_it;
    session->disconnector([this, session_it]() mutable {
      client_sessions_.serialize_with_cv(
          [session_it](auto &sessions, auto &condvar) {
            // remove the connection from the connection container
            // which calls the destructor of the Connection
            sessions.erase(session_it);

            // notify the "wait for all sockets to shutdown"
            condvar.notify_one();
          });
    });

    net::defer(io_ctx_, [&session]() { session->run(); });

    // accept the next connection.
    async_run();
  }

  /**
   * accept connections asynchronously.
   *
   * runs until stopped().
   */
  void async_run() {
    if (stopped()) return;

    work_([](auto &work) { ++work; });

    sock_.async_accept(client_ep_, [this](std::error_code ec,
                                          protocol_type::socket client_sock) {
      Scope_guard guard([&]() {
        work_.serialize_with_cv([](auto &work, auto &cv) {
          // leaving acceptor.
          //
          // Notify the stop() which may wait for the work to become zero.
          --work;
          cv.notify_one();
        });
      });

      if (ec) {
        return;
      }

      client_sock.set_option(net::ip::tcp::no_delay{true});

      log_info("accepted from %s", mysqlrouter::to_string(client_ep_).c_str());

      this->accepted(std::move(client_sock));
    });
  }

  /**
   * check if acceptor is stopped.
   *
   * @returns if acceptor is stopped.
   */
  bool stopped() const {
    return (stopped_([](bool stopped) { return stopped; }));
  }

  /**
   * stop the acceptor.
   */
  void stop() {
    if (!stopped_now()) return;

    // close()s the listening socket and cancels possible async_wait() on the
    // socket
    sock_.close();

    // wait until all async callbacks finished.
    work_.wait([](auto work) { return work == 0; });
  }

 private:
  /**
   * mark the acceptor as stopped.
   *
   * @returns whether the acceptor was marked as stopped by this call.
   * @retval true marked acceptor as "stopped" _now_.
   * @retval false already stopped before.
   */
  bool stopped_now() {
    return stopped_([](bool &stopped) {
      // already stopped.
      if (stopped) return false;

      stopped = true;

      return true;
    });
  }

  net::io_context &io_ctx_;
  protocol_type::acceptor sock_{io_ctx_};

  DuktapeStatementReaderFactory reader_maker_;

  std::string protocol_name_;
  WaitableMonitor<std::list<std::unique_ptr<MySQLServerMockSession>>>
      &client_sessions_;
  protocol_type::endpoint client_ep_;

  TlsServerContext &tls_server_ctx_;

  bool with_tls_{false};

  Monitor<bool> stopped_{false};

  // initial work to not exit before stop() is called.
  //
  // tracks if async_accept is currently waiting.
  WaitableMonitor<int> work_{0};
};

void MySQLServerMock::run(mysql_harness::PluginFuncEnv *env) {
  Acceptor acceptor{io_ctx_,
                    protocol_name_,
                    client_sessions_,
                    DuktapeStatementReaderFactory{
                        expected_queries_file_,
                        module_prefixes_,
                        // expose session data as json-encoded string
                        {{"port", std::to_string(bind_port_)},
                         {"ssl_cipher", "\"\""},
                         {"mysqlx_ssl_cipher", "\"\""}},
                        MockServerComponent::get_instance().get_global_scope()},
                    tls_server_ctx_,
                    ssl_mode_ != SSL_MODE_DISABLED};

  auto res = acceptor.init(bind_address_, bind_port_);
  if (!res) {
    throw std::system_error(res.error(), "binding to " + bind_address_ + ":" +
                                             std::to_string(bind_port_) +
                                             " failed");
  }

  mysql_harness::on_service_ready(env);

  log_info("Starting to handle connections on port: %d", bind_port_);

  acceptor.async_run();

  mysql_harness::wait_for_stop(env, 0);

  // wait until acceptor stopped.
  acceptor.stop();

  close_all_connections();

  // wait until all connections are closed.
  client_sessions_.wait(
      [](const auto &sessions) -> bool { return sessions.empty(); });
}

}  // namespace server_mock
