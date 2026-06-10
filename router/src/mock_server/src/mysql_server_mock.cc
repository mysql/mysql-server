/*
  Copyright (c) 2017, 2026, Oracle and/or its affiliates.

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

#include "mysql_server_mock.h"

#include <functional>
#include <iostream>  // cout
#include <memory>    // shared_ptr
#include <string>
#include <system_error>
#include <utility>  // move

#include "classic_mock_session.h"
#include "duktape_statement_reader.h"
#include "mock_session.h"
#include "mysql/harness/destination.h"
#include "mysql/harness/destination_acceptor.h"
#include "mysql/harness/destination_endpoint.h"
#include "mysql/harness/destination_socket.h"
#include "mysql/harness/logging/logger.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket_constants.h"
#include "mysql/harness/net_ts/internet.h"  // net::ip::tcp
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"  // local::stream_protocol
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/monitor.h"
#include "mysql/harness/tls_server_context.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/mock_server_component.h"
#include "mysqlrouter/utils.h"  // to_string
#include "scope_guard.h"
#include "statement_reader.h"
#include "x_mock_session.h"

using namespace std::string_literals;

namespace server_mock {

MySQLServerMock::MySQLServerMock(net::io_context &io_ctx,
                                 std::string expected_queries_file,
                                 std::vector<std::string> module_prefixes,
                                 mysql_harness::Destination bind_destination,
                                 std::string protocol_name, bool debug_mode,
                                 TlsServerContext &&tls_server_ctx,
                                 mysql_ssl_mode ssl_mode)
    : bind_destination_(std::move(bind_destination)),
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
      conn->terminate();
    }
  });
}

namespace {
stdx::expected<mysql_harness::DestinationEndpoint, std::error_code>
make_destination_endpoint(net::io_context &io_ctx,
                          mysql_harness::Destination dest) {
  if (dest.is_tcp()) {
    auto tcp_dest = dest.as_tcp();
    net::ip::tcp::resolver resolver(io_ctx);

    auto resolve_res =
        resolver.resolve(tcp_dest.hostname(), std::to_string(tcp_dest.port()),
                         net::ip::resolver_base::passive);
    if (!resolve_res) return stdx::unexpected(resolve_res.error());

    for (const auto &ainfo : resolve_res.value()) {
      return mysql_harness::DestinationEndpoint(ainfo.endpoint());
    }

    return stdx::unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  return mysql_harness::DestinationEndpoint{
      mysql_harness::DestinationEndpoint::LocalType{dest.as_local().path()}};
}
}  // namespace

class Acceptor {
 public:
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

  ~Acceptor() {
    stop();

    if (at_destruct_) at_destruct_();
  }

  stdx::expected<void, std::error_code> init(
      const mysql_harness::Destination &dest) {
    mysql_harness::DestinationAcceptor sock(io_ctx_);

    auto ep_res = make_destination_endpoint(io_ctx_, dest);
    if (!ep_res) return stdx::unexpected(ep_res.error());
    const auto &ep = *ep_res;

    auto res = sock.open(ep);
    if (!res) return stdx::unexpected(res.error());

    res = sock.native_non_blocking(true);
    if (!res) return stdx::unexpected(res.error());

    if (ep.is_tcp()) {
      res = sock.set_option(net::socket_base::reuse_address{true});
      if (!res) return stdx::unexpected(res.error());
    }

    res = sock.bind(ep);
    if (!res) return stdx::unexpected(res.error());

    res = sock.listen(256);
    if (!res) return stdx::unexpected(res.error());

    if (ep.is_local()) {
      at_destruct_ = [path = ep.as_local().path()]() { unlink(path.c_str()); };
    }

    sock_ = std::move(sock);

    return {};
  }

  void accepted(mysql_harness::DestinationSocket client_sock) {
    auto reader = reader_maker_();

    auto session_it = client_sessions_([&](auto &socks) {
      if (protocol_name_ == "classic") {
        socks.emplace_back(std::make_unique<MySQLServerMockSessionClassic>(
            std::move(client_sock), client_ep_, tls_server_ctx_,
            std::move(reader), false, with_tls_));
      } else {
        socks.emplace_back(std::make_unique<MySQLServerMockSessionX>(
            std::move(client_sock), client_ep_, tls_server_ctx_,
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

    sock_.async_accept(
        client_ep_, [this](std::error_code ec,
                           mysql_harness::DestinationSocket client_sock) {
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

          if (client_sock.is_tcp()) {
            client_sock.set_option(net::ip::tcp::no_delay{true});
          }

          logger_.info(
              [this]() { return "accepted from " + client_ep_.str(); });

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
  mysql_harness::DestinationAcceptor sock_{io_ctx_};

  DuktapeStatementReaderFactory reader_maker_;

  std::string protocol_name_;
  WaitableMonitor<std::list<std::unique_ptr<MySQLServerMockSession>>>
      &client_sessions_;
  mysql_harness::DestinationEndpoint client_ep_;

  TlsServerContext &tls_server_ctx_;

  bool with_tls_{false};

  Monitor<bool> stopped_{false};

  // initial work to not exit before stop() is called.
  //
  // tracks if async_accept is currently waiting.
  WaitableMonitor<int> work_{0};

  mysql_harness::logging::DomainLogger logger_;

  std::function<void()> at_destruct_;
};

void MySQLServerMock::run(mysql_harness::PluginFuncEnv *env) {
  const auto &dest = bind_destination_;

  Acceptor acceptor{
      io_ctx_,
      protocol_name_,
      client_sessions_,
      DuktapeStatementReaderFactory{
          expected_queries_file_,
          module_prefixes_,
          // expose session data as json-encoded string
          {{"port",
            [&]() {
              return std::to_string(dest.is_tcp() ? dest.as_tcp().port() : 0);
            }},
           {"ssl_cipher", []() { return "\"\""s; }},
           {"mysqlx_ssl_cipher", []() { return "\"\""s; }},
           {"ssl_session_cache_hits",
            [this]() {
              return std::to_string(tls_server_ctx_.session_cache_hits());
            }}},
          MockServerComponent::get_instance().get_global_scope()},
      tls_server_ctx_,
      ssl_mode_ != SSL_MODE_DISABLED};

  auto res = acceptor.init(dest);
  if (!res) {
    throw std::system_error(res.error(),
                            "binding to " + dest.str() + " failed");
  }

  mysql_harness::on_service_ready(env);

  mysql_harness::logging::DomainLogger().info([this, dest]() {
    return mysql_harness::utility::string_format(
        "Starting to handle %s connections on %s",  //
        protocol_name_.c_str(), dest.str().c_str());
  });

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
