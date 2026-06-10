/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "classic_connect.h"

#include <chrono>
#include <memory>
#include <system_error>

#include "basic_protocol_splicer.h"
#include "classic_connection_base.h"
#include "destination_error.h"
#include "mysql/harness/destination.h"
#include "mysql/harness/destination_endpoint.h"
#include "mysql/harness/destination_socket.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/poll.h"
#include "mysql/harness/net_ts/impl/resolver.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/resolver/resolver.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/utils.h"  // to_string
#include "processor.h"

IMPORT_LOG_FUNCTIONS()

stdx::expected<Processor::Result, std::error_code> ConnectProcessor::process() {
  switch (stage()) {
    case Stage::InitDestination:
      return init_destination();
    case Stage::Resolve:
      return resolve();
    case Stage::InitEndpoint:
      return init_endpoint();
    case Stage::FromPool:
      return from_pool();
    case Stage::NextEndpoint:
      return next_endpoint();
    case Stage::NextDestination:
      return next_destination();
    case Stage::InitConnect:
      return init_connect();
    case Stage::Connect:
      return connect();
    case Stage::ConnectFinish:
      return connect_finish();
    case Stage::Connected:
      return connected();
    case Stage::Error:
      return error();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

// get the socket-error from a connection.
//
// error   if getting socket error failed.
// success if error could be fetched
static stdx::expected<std::error_code, std::error_code> sock_error_code(
    MysqlRoutingClassicConnectionBase::ServerSideConnection &conn) {
  auto *tcp_conn = dynamic_cast<TcpConnection *>(conn.connection().get());

  net::socket_base::error sock_err;
  const auto getopt_res = tcp_conn->get_option(sock_err);
  if (!getopt_res) return stdx::unexpected(getopt_res.error());

  if (sock_err.value() != 0) {
    return std::error_code {
      sock_err.value(),
#if defined(_WIN32)
          std::system_category()
#else
          std::generic_category()
#endif
    };
  }

  return {};
}

/**
 * Skip destinations which don't match the current expected server-mode.
 */
static bool skip_destination(MysqlRoutingClassicConnectionBase *conn,
                             Destination *destination) {
  if (conn->context().access_mode() != routing::AccessMode::kAuto) return false;

  const auto conn_server_mode = conn->current_server_mode();
  const auto dest_server_mode = destination->server_mode();

  return ((conn_server_mode == mysqlrouter::ServerMode::ReadOnly &&
           dest_server_mode == mysqlrouter::ServerMode::ReadWrite) ||
          (conn_server_mode == mysqlrouter::ServerMode::ReadWrite &&
           dest_server_mode == mysqlrouter::ServerMode::ReadOnly));
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::init_destination() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::init_destination"));
  }

  trace_event_connect_ =
      trace_span(parent_event_, "mysql/from_pool_or_connect");

  const auto &destination_manager = connection()->destination_manager();
  bool dest_manager_started{true};

  if (!connection()->has_transient_error_at_connect()) {
    if (destination_manager->routing_guidelines_session_rand_used()) {
      connection()->set_routing_guidelines_session_rand();
    }
    const auto &session_info = connection()->get_session_info();

    dest_manager_started =
        destination_manager->init_destinations(session_info).has_value();

    if (dest_manager_started) {
      destination_ = destination_manager->get_next_destination(session_info);
    }
  } else {
    destination_ = destination_manager->get_last_used_destination();
  }

  if (!dest_manager_started || !destination_) {
    if (connect_errors_.empty()) {
      // no backends
      log_debug("init_destination(): the destinations list is empty");

      connect_errors_.emplace_back(
          "no destinations",
          make_error_code(DestinationsErrc::kNoDestinations));
    }
    stage(Stage::Error);
    return Result::Again;
  }

  // reset the error-code for this destination.
  destination_ec_.clear();

  all_quarantined_ = true;

  // adjust the current-server-mode depending if we have:
  //
  // - RW, RO
  // - only RW (multi-primary)
  // - only RO (replica of replicaset)
  if (connection()->context().access_mode() == routing::AccessMode::kAuto) {
    const bool has_read_only = destination_manager->has_read_only();
    const bool has_read_write = destination_manager->has_read_write();

    if (has_read_only && !has_read_write) {
      connection()->current_server_mode(mysqlrouter::ServerMode::ReadOnly);
    } else if (!has_read_only && has_read_write) {
      connection()->current_server_mode(mysqlrouter::ServerMode::ReadWrite);
    }
  }

  if (connection()->context().access_mode() == routing::AccessMode::kAuto) {
    if (skip_destination(connection(), destination_.get())) {
      connect_errors_.emplace_back(
          "connect(/* " + destination_->destination().str() + " */)",
          make_error_code(DestinationsErrc::kIgnored));

      destination_manager->connect_status(
          make_error_code(DestinationsErrc::kIgnored));

      stage(Stage::NextDestination);
      return Result::Again;
    }
  }

  if (is_destination_good(destination_->destination())) {
    stage(Stage::Resolve);
  } else {
    connect_errors_.emplace_back(
        "connect(/* " + destination_->destination().str() + " */)",
        make_error_code(DestinationsErrc::kQuarantined));

    stage(Stage::NextDestination);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ConnectProcessor::resolve() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::resolve"));
  }

  // must use current_server_mode() here as this may be a fallback round.
  const auto dest_id =
      connection()->current_server_mode() == mysqlrouter::ServerMode::ReadOnly
          ? connection()->read_only_destination_id()
          : connection()->read_write_destination_id();

  if (dest_id.has_value()) {
    // already connected before. Make sure the same endpoint is connected.
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::sticky: " + dest_id->str()));
    }

    if (dest_id != destination_->destination()) {
      destination_ec_ = make_error_code(std::errc::no_such_file_or_directory);
      connection()->destination_manager()->connect_status(destination_ec_);

      stage(Stage::NextDestination);
      return Result::Again;
    }
  }

  auto started = std::chrono::steady_clock::now();

  if (destination_->destination().is_tcp()) {
    auto tcp_dest = destination_->destination().as_tcp();

    const auto resolve_result = mysql_harness::resolver::resolve_host(
        tcp_dest.hostname(),
        mysql_harness::resolver::CachePolicy::UseIfPresent);

    if (!resolve_result) {
      auto ec = resolve_result.error();

      const auto resolve_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - started);
      connect_errors_.emplace_back(
          "resolve(" + tcp_dest.hostname() + ") failed after " +
              std::to_string(resolve_duration.count()) + "ms",
          ec);

      log_debug("resolve(%s,%d) failed: %s:%s",  //
                tcp_dest.hostname().c_str(), tcp_dest.port(),
                ec.category().name(), ec.message().c_str());

      destination_ec_ = ec;

      connection()->destination_manager()->connect_status(destination_ec_);

      // resolve(...) failed, move host:port to the quarantine to monitor the
      // solve to come back.

      auto &ctx = connection()->context();

      if (ctx.shared_quarantine().update(destination_->destination(), false)) {
        log_debug("[%s] add destination '%s' to quarantine",
                  ctx.get_name().c_str(),
                  destination_->destination().str().c_str());
      } else {
        // failed to connect, but not quarantined. Don't close the ports, yet.
        all_quarantined_ = false;
      }

      stage(Stage::NextDestination);
      return Result::Again;
    }

    endpoints_.clear();

    for (const auto &ep : resolve_result->addresses) {
      endpoints_.emplace_back(
          mysql_harness::DestinationEndpoint::TcpType(ep, tcp_dest.port()));
    }
  } else {
    endpoints_.clear();

    endpoints_.emplace_back(mysql_harness::DestinationEndpoint::LocalType(
        destination_->destination().as_local().path()));
  }

#if 0
  std::cerr << __LINE__ << ": " << destination_->hostname() << "\n";
  for (auto const &ep : endpoints_) {
    std::cerr << __LINE__ << ": .. " << ep.endpoint() << "\n";
  }
#endif

  stage(Stage::InitEndpoint);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::init_endpoint() {
  // trace(Tracer::Event().stage("connect::init_endpoint"));

  endpoints_it_ = endpoints_.begin();

  stage(Stage::InitConnect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::init_connect() {
  // trace(Tracer::Event().stage("connect::init_connect"));

  // Connection sharing may be set explicitly by the routing guidelines route
  // setting
  connection()->context().connection_sharing(
      destination_->guidelines_route_info().connection_sharing_allowed);

  (void)connection()->server_conn().close();

  connection()->connect_error_code({});  // reset the connect-error-code.

  server_endpoint_ = *endpoints_it_;

  stage(Stage::FromPool);
  return Result::Again;
}

static stdx::expected<void, std::error_code> socket_is_alive(
    const ConnectionPool::ServerSideConnection &server_conn) {
  std::array<net::impl::poll::poll_fd, 1> fds{
      {{server_conn.connection()->native_handle(), POLLIN, 0}}};
  auto poll_res = net::impl::poll::poll(fds.data(), fds.size(),
                                        std::chrono::milliseconds(0));
  if (!poll_res) {
    if (poll_res.error() != std::errc::timed_out) {
      // shouldn't happen, but if it does, ignore the socket.
      return stdx::unexpected(poll_res.error());
    }

    return {};
  }

  // there is data -> Error packet -> server closed the connection.
  return stdx::unexpected(make_error_code(net::stream_errc::eof));
}

void ConnectProcessor::assign_server_side_connection_after_pool(
    ConnectionPool::ServerSideConnection server_conn) {
  connection()->server_conn() = std::move(server_conn);

  (void)connection()->server_conn().connection()->set_io_context(
      connection()->client_conn().connection()->io_ctx());

  // reset the seq-id of the server side as this is a new command.
  connection()->server_protocol().seq_id(0xff);

  if (connection()->expected_server_mode() ==
      mysqlrouter::ServerMode::Unavailable) {
    // before the first query, the server-mode is not set,
    // remember it now.
    connection()->expected_server_mode(destination_->server_mode());
  }

  // set destination-id to get the "trace_set_connection_attributes"
  // right.
  connection()->destination_id(destination_->destination());
  connection()->destination_endpoint(*endpoints_it_);

  connection()->server_address(connection()->server_conn().endpoint());

  // update the msg-tracer callback to the new connection.
  if (auto *ssl = connection()->server_conn().channel().ssl()) {
    SSL_set_msg_callback_arg(ssl, connection());
  }
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::from_pool() {
  auto &client_protocol = connection()->client_protocol();

  if (!client_protocol.client_greeting()) {
    // taking a connection from the pool requires that the client's greeting
    // must been received already.
    stage(Stage::Connect);
    return Result::Again;
  }

  trace_event_socket_from_pool_ =
      trace_span(trace_event_connect_, "mysql/from_pool");

  auto &pools = ConnectionPoolComponent::get_instance();

  if (auto pool = pools.get(ConnectionPoolComponent::default_pool_name())) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage(
          "connect::from_pool::try: " + endpoints_it_->str() +
          ", expected-transport-constraints: " +
          connection()->expected_server_transport_constraints().to_string()));
    }

    // preference order:
    //
    // 0. take a server-side connection that is still owned by us.
    // 1. take a server-side connection from the "pool".
    // 2. steal a server-side connection from another connection
    //    (from the "stash").

    // if the RW-node is used for Reads too, we may end up on the same node that
    // was just stashed.
    if (auto pop_res =
            pool->unstash_mine(server_endpoint_.str(), connection())) {
      if (!socket_is_alive(*pop_res)) {
        // take the next connection from pool, this one is dead.
        return Result::Again;
      }

      assign_server_side_connection_after_pool(std::move(*pop_res));

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("connect::from_stash_mine: " +
                                       server_endpoint_.str()));
      }

      if (auto *ev = trace_event_socket_from_pool_) {
        trace_set_connection_attributes(ev);
        trace_span_end(ev);
      }

      stage(Stage::Connected);
      return Result::Again;
    }

    // pop the first connection from the pool that matches our requirements
    //
    // - endpoint
    // - capabilities

    auto client_caps = client_protocol.shared_capabilities();

    client_caps
        // connection specific.
        .reset(classic_protocol::capabilities::pos::ssl)
        .reset(classic_protocol::capabilities::pos::query_attributes)
        .reset(classic_protocol::capabilities::pos::compress)
        .reset(classic_protocol::capabilities::pos::compress_zstd)
        .reset(classic_protocol::capabilities::pos::session_track)
        .reset(classic_protocol::capabilities::pos::
                   text_result_with_session_tracking)
        // session specific capabilities which can be recovered by
        // set_server_option()
        .reset(classic_protocol::capabilities::pos::multi_statements);

    auto connection_matcher =
        [client_caps,
         expected_transport_constraints =
             connection()->expected_server_transport_constraints()]  //
        (const auto &pooled_conn) {
          auto pooled_caps = pooled_conn.protocol().shared_capabilities();

          pooled_caps.reset(classic_protocol::capabilities::pos::ssl)
              .reset(classic_protocol::capabilities::pos::query_attributes)
              .reset(classic_protocol::capabilities::pos::compress)
              .reset(classic_protocol::capabilities::pos::compress_zstd)
              .reset(classic_protocol::capabilities::pos::session_track)
              .reset(classic_protocol::capabilities::pos::
                         text_result_with_session_tracking)
              .reset(classic_protocol::capabilities::pos::multi_statements);

          const bool has_ssl = pooled_conn.channel().ssl() != nullptr;
          const bool is_secure_transport = pooled_conn.is_secure_transport();
          const bool has_client_cert =
              has_ssl &&
              (SSL_get_certificate(pooled_conn.channel().ssl()) != nullptr);

          if (client_caps != pooled_caps) return false;

          using TC = TransportConstraints::Constraint;

          switch (expected_transport_constraints.constraint()) {
            case TC::kHasClientCert:
              if (!has_client_cert) return false;
              [[fallthrough]];
            case TC::kEncrypted:
              return has_ssl;
            case TC::kSecure:
              return is_secure_transport;
            case TC::kPlaintext:
              return !has_ssl;
          }
          return false;
        };

    // check the pool for a connection we can use.
    if (auto pool_res =
            pool->pop_if(server_endpoint_.str(), connection_matcher)) {
      if (!socket_is_alive(*pool_res)) {
        // take the next connection from pool, this one is dead.
        return Result::Again;
      }

      assign_server_side_connection_after_pool(std::move(*pool_res));

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("connect::from_pool: " +
                                       endpoints_it_->str()));
      }

      if (auto *ev = trace_event_socket_from_pool_) {
        trace_set_connection_attributes(ev);
        trace_span_end(ev);
      }

      stage(Stage::Connected);
      return Result::Again;
    }

    // no connection from the pool, try to steal a sharable one from another
    // connection.

    // if there is currently a transient connect error like max-connect-error,
    // ignore the sharing delay as the error may be caused by the
    // connection-pool keeping to many connections open.
    const bool ignore_sharing_delay =
        connection()->has_transient_error_at_connect();

    // try to steal a server-side connection from another connection.
    if (auto pop_res = pool->unstash_if(
            server_endpoint_.str(), connection_matcher, ignore_sharing_delay)) {
      if (!socket_is_alive(*pop_res)) {
        // take the next connection from pool, this one is dead.
        return Result::Again;
      }

      assign_server_side_connection_after_pool(std::move(*pop_res));

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage(
            "pool::unstashed::steal: fd=" +
            std::to_string(connection()->server_conn().native_handle()) + ", " +
            connection()->server_conn().endpoint()));
      }

      if (auto *ev = trace_event_socket_from_pool_) {
        trace_set_connection_attributes(ev);
        trace_span_end(ev);
      }

      stage(Stage::Connected);
      return Result::Again;
    }

    if (auto *ev = trace_event_socket_from_pool_) {
      ev->attrs.emplace_back("mysql.error_message", "no match");
      trace_span_end(ev, TraceEvent::StatusCode::kError);
    }
  } else {
    if (auto *ev = trace_event_socket_from_pool_) {
      ev->attrs.emplace_back("mysql.error_message", "no pool");
      trace_span_end(ev, TraceEvent::StatusCode::kError);
    }
  }

  stage(Stage::Connect);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ConnectProcessor::connect() {
  if (auto &tr = tracer()) {
    tr.trace(
        Tracer::Event().stage("connect::connect: " + server_endpoint_.str()));
  }

  trace_event_socket_connect_ =
      trace_span(trace_event_connect_, "mysql/connect");

  if (auto *ev = trace_event_socket_connect_) {
    // https://opentelemetry.io/docs/specs/semconv/attributes-registry/network/
    //
    // says:
    //
    // - net.peer.* is deprecated
    // - network.* is the stable name.
    //
    // keep the old names for now for existing names, and use the new names for
    // new stuff.
    if (endpoints_it_->is_tcp()) {
      auto tcp_ep = endpoints_it_->as_tcp();
      ev->attrs.emplace_back("net.peer.name", tcp_ep.address().to_string());
      ev->attrs.emplace_back("net.peer.port", std::to_string(tcp_ep.port()));
    } else {
      auto local_ep = endpoints_it_->as_local();
      ev->attrs.emplace_back("network.peer.address", local_ep.path());
    }
  }

#if 0
  if (log_level_is_handled(mysql_harness::logging::LogLevel::kDebug)) {
    log_debug("trying %s", mysqlrouter::to_string(server_endpoint_).c_str());
  }
#endif

  const int socket_flags {
#if defined(SOCK_NONBLOCK)
    // linux|freebsd|sol11.4 allows to set NONBLOCK as part of the socket()
    // call to save the extra syscall
    SOCK_NONBLOCK
#endif
  };

  auto server_sock =
      endpoints_it_->is_tcp()
          ? mysql_harness::DestinationSocket{mysql_harness::DestinationSocket::
                                                 TcpType(io_ctx_)}
          : mysql_harness::DestinationSocket{
                mysql_harness::DestinationSocket::LocalType(io_ctx_)};

  auto open_res = server_sock.open(server_endpoint_, socket_flags);
  if (!open_res) return stdx::unexpected(open_res.error());

  const auto non_block_res = server_sock.native_non_blocking(true);
  if (!non_block_res) return stdx::unexpected(non_block_res.error());

  if (server_sock.is_tcp()) {
    server_sock.set_option(net::ip::tcp::no_delay{true});
  }

#ifdef FUTURE_TASK_USE_SOURCE_ADDRESS
  /* set the source address to take a specific route.
   *
   *
   */

  // IP address of the interface we want to route-through.
  std::string src_addr_str;

  // src_addr_str = "192.168.178.78";

  if (!src_addr_str.empty()) {
    const auto src_addr_res = net::ip::make_address_v4(src_addr_str.c_str());
    if (!src_addr_res) return stdx::unexpected(src_addr_res.error());

#if defined(IP_BIND_ADDRESS_NO_PORT)
    // linux 4.2 introduced IP_BIND_ADDRESS_NO_PORT to delay assigning a
    // source-port until connect()
    net::socket_option::integer<IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT> sockopt;

    const auto setsockopt_res = server_sock.set_option(sockopt);
    if (!setsockopt_res) {
      // if the glibc supports IP_BIND_ADDRESS_NO_PORT, but the kernel
      // doesn't: ignore it.
      if (setsockopt_res.error() !=
          make_error_code(std::errc::invalid_argument)) {
        log_warning(
            "%d: setsockopt(IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT) "
            "failed: "
            "%s",
            __LINE__, setsockopt_res.error().message().c_str());
        return stdx::unexpected(setsockopt_res.error());
      }
    }
#endif

    const auto bind_res = server_sock.bind(net::ip::tcp::endpoint(
        src_addr_res.value_or(net::ip::address_v4{}), 0));
    if (!bind_res) return stdx::unexpected(bind_res.error());
  }
#endif

  connect_started_ = std::chrono::steady_clock::now();

  const auto connect_res = server_sock.connect(server_endpoint_);

  // don't assign the connection if disconnect is requested.
  //
  // assigning the connection would lead to a deadlock in start_acceptor()
  auto disconnected_requested =
      connection()->disconnect_request([this, &server_sock](bool req) {
        if (req) return true;

        if (server_sock.is_tcp()) {
          connection()->server_conn().assign_connection(
              std::make_unique<TcpConnection>(std::move(server_sock.as_tcp()),
                                              server_endpoint_.as_tcp()));
        } else {
          connection()->server_conn().assign_connection(
              std::make_unique<UnixDomainConnection>(
                  std::move(server_sock.as_local()),
                  server_endpoint_.as_local()));
        }

        return false;
      });
  if (disconnected_requested) {
    connection()->connect_error_code(
        make_error_code(std::errc::operation_canceled));

    connection()->completed();
    stage(Stage::Done);
    return Result::Again;
  }

  if (!connect_res) {
    const auto ec = connect_res.error();
    if (ec == make_error_condition(std::errc::operation_in_progress) ||
        ec == make_error_condition(std::errc::operation_would_block)) {
      // connect in progress, wait for completion.
      stage(Stage::ConnectFinish);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("connect::wait"));
      }

      auto &timer = connection()->connect_timer();

      timer.expires_after(
          connection()->context().get_destination_connect_timeout());

      timer.async_wait([this](std::error_code ec) {
        if (ec) return;

        if (auto &tr = tracer()) {
          tr.trace(Tracer::Event().stage("connect::timed_out"));
        }

        auto &server_conn = connection()->server_conn();

        connection()->connect_error_code(make_error_code(std::errc::timed_out));

        (void)server_conn.cancel();
      });

      connection()->server_conn().async_wait_error(
          [conn = connection()](std::error_code ec) {
            if (ec) return;

            auto &server_conn = conn->server_conn();

            auto sock_ec_res = sock_error_code(server_conn);
            if (!sock_ec_res) {
              conn->connect_error_code(sock_ec_res.error());
            } else {
              conn->connect_error_code(sock_ec_res.value());
            }

            // cancel all the other waiters
            (void)server_conn.cancel();
          });

      return Result::SendableToServer;
    } else {
      log_debug("connect(%s) failed: %s:%s", server_endpoint_.str().c_str(),
                connect_res.error().category().name(),
                connect_res.error().message().c_str());
      connection()->connect_error_code(ec);

      stage(Stage::ConnectFinish);
      return Result::Again;
    }
  }

  stage(Stage::Connected);
  return Result::Again;
}

namespace {
std::string pretty_endpoint(const mysql_harness::DestinationEndpoint &ep,
                            const mysql_harness::Destination &dest) {
  if (dest.is_tcp()) {
    // if the hostname is an IP, return it directly.
    if (ep.as_tcp().address().to_string() == dest.as_tcp().hostname()) {
      return ep.str();
    }

    // ... otherwise append it.
    return ep.str() + " /* " + dest.as_tcp().hostname() + " */";
  }

  return dest.str();
}
}  // namespace

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::connect_finish() {
  auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - connect_started_);

  connection()->connect_timer().cancel();

  auto &server_conn = connection()->server_conn();

  // cancel all handlers.
  (void)server_conn.cancel();

  if (auto ec = connection()->connect_error_code()) {
    log_debug("connect(%s) failed: %s:%s", server_endpoint_.str().c_str(),
              ec.category().name(), ec.message().c_str());

    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("connect::connect_finish: " + ec.message()));
    }

    connect_errors_.emplace_back(
        "connect(" +
            pretty_endpoint(server_endpoint_, destination_->destination()) +
            ") failed after " + std::to_string(connect_duration.count()) + "ms",
        ec);

    destination_ec_ = ec;

    stage(Stage::NextEndpoint);
    return Result::Again;
  }

  auto sock_ec_res = sock_error_code(server_conn);
  if (!sock_ec_res) {
    auto ec = sock_ec_res.error();

    log_debug("connect(%s) failed: %s:%s", server_endpoint_.str().c_str(),
              ec.category().name(), ec.message().c_str());

    if (auto &tr = tracer()) {
      tr.trace(
          Tracer::Event().stage("connect::connect_finish: " + ec.message()));
    }

    connect_errors_.emplace_back(
        "connect(" +
            pretty_endpoint(server_endpoint_, destination_->destination()) +
            ")::getsockopt()",
        ec);

    destination_ec_ = ec;

    stage(Stage::NextEndpoint);
    return Result::Again;
  }

  auto sock_ec = *sock_ec_res;

  if (sock_ec != std::error_code{}) {
    log_debug("connect(%s) failed: %s:%s", server_endpoint_.str().c_str(),
              sock_ec.category().name(), sock_ec.message().c_str());

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::connect_finish: " +
                                     sock_ec.message()));
    }

    connect_errors_.emplace_back(
        "connect(" +
            pretty_endpoint(server_endpoint_, destination_->destination()) +
            ") failed after " + std::to_string(connect_duration.count()) + "ms",
        sock_ec);

    destination_ec_ = sock_ec;

    stage(Stage::NextEndpoint);
    return Result::Again;
  }

  if (auto *ev = trace_event_socket_connect_) {
    trace_span_end(ev);
  }

  stage(Stage::Connected);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::next_endpoint() {
  (void)connection()->server_conn().close();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::next_endpoint"));
  }

  if (auto *ev = trace_event_socket_connect_) {
    auto last_ec = connect_errors_.empty() ? std::error_code{}
                                           : connect_errors_.back().second;

    ev->attrs.emplace_back("mysql.error_message", last_ec.message());

    trace_span_end(ev);
  }

  std::advance(endpoints_it_, 1);

  if (endpoints_it_ != endpoints_.end()) {
    stage(Stage::InitConnect);
    return Result::Again;
  }

  // report back the connect status to the destination
  connection()->destination_manager()->connect_status(destination_ec_);

  if (destination_ec_) {
    auto &ctx = connection()->context();

    if (ctx.shared_quarantine().update(destination_->destination(), false)) {
      log_debug("[%s] Add destination '%s' to quarantine",
                ctx.get_name().c_str(),
                destination_->destination().str().c_str());
    } else {
      // failed to connect, but not quarantined. Don't close the ports, yet.
      all_quarantined_ = false;
    }
  }

  stage(Stage::NextDestination);
  return Result::Again;
}

bool ConnectProcessor::is_destination_good(
    const mysql_harness::Destination &dest) const {
  const auto &ctx = connection()->context();

  const auto is_quarantined = ctx.shared_quarantine().is_quarantined(dest);
  if (is_quarantined) {
    const auto &client_socket = connection()->client_conn();
    log_debug("[%s] fd=%d skip quarantined destination '%s'",
              ctx.get_name().c_str(), client_socket.native_handle(),
              dest.str().c_str());

    return false;
  }

  return true;
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::next_destination() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::next_destination"));
  }

  bool is_quarantined{false};
  bool is_skipped{false};
  const auto &session_info = connection()->get_session_info();
  const auto &destination_manager = connection()->destination_manager();
  do {
    destination_ = destination_manager->get_next_destination(session_info);
    if (destination_) {
      // for read-only connections, skip the writable destinations,
      // for read-write connections, skip the read-only destinations.
      is_skipped = skip_destination(connection(), destination_.get());
      if (is_skipped) {
        connect_errors_.emplace_back(
            "connect(/* " + destination_->destination().str() + " */)",
            make_error_code(DestinationsErrc::kIgnored));

        destination_manager->connect_status(
            make_error_code(DestinationsErrc::kIgnored));
      }

      is_quarantined = !is_destination_good(destination_->destination());
      if (is_quarantined) {
        connect_errors_.emplace_back(
            "connect(/* " + destination_->destination().str() + " */)",
            make_error_code(DestinationsErrc::kQuarantined));

        destination_manager->connect_status(
            make_error_code(DestinationsErrc::kQuarantined));
      }
    }
  } while (destination_ && (is_quarantined || is_skipped));

  if (destination_) {
    // next destination
    stage(Stage::Resolve);
    return Result::Again;
  } else if (destination_ec_ != make_error_condition(std::errc::timed_out) &&
             destination_ec_.category() != net::ip::resolver_category() &&
             destination_manager->refresh_destinations(session_info)) {
    // On member failure (connection refused, ...) wait for failover and use
    // the new primary.
    destination_ = destination_manager->get_next_destination(session_info);
    if (destination_) {
      stage(Stage::Resolve);
      return Result::Again;
    }
  }

  if (connection()->context().access_mode() == routing::AccessMode::kAuto &&
      connection()->expected_server_mode() ==
          mysqlrouter::ServerMode::ReadOnly &&
      connection()->current_server_mode() ==
          mysqlrouter::ServerMode::ReadOnly) {
    // if we want a RO connections but there are only primaries, take a
    // primary.
    connection()->current_server_mode(mysqlrouter::ServerMode::ReadWrite);
    stage(Stage::InitDestination);
    return Result::Again;
  }

  connect_errors_.emplace_back(
      "end of destinations",
      make_error_code(DestinationsErrc::kNoDestinations));

  // we couldn't connect to any of the destinations. Give up.
  stage(Stage::Error);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
ConnectProcessor::connected() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::connected"));
  }

  if (auto *ev = trace_event_connect_) {
    trace_span_end(ev);
  }

  // remember the destination and its server-mode for connection-sharing.
  if (connection()->expected_server_mode() ==
      mysqlrouter::ServerMode::Unavailable) {
    // before the first query, the server-mode is not set,
    // remember it now.
    connection()->expected_server_mode(destination_->server_mode());
  }

  connection()->destination_id(destination_->destination());
  connection()->destination_endpoint(*endpoints_it_);

  connection()->server_address(connection()->server_conn().endpoint());

  // mark destination as reachable.
  connection()->context().shared_quarantine().update(
      destination_->destination(), true);

  // Keep information about destination, we need it to determine which
  // connections are no longer allowed after guidelines update.
  connection()->set_destination(std::move(destination_));

  connection()->completed();
  // We are done, destination manager should know about that
  connection()->destination_manager()->connect_status({});

  // back to the caller.
  stage(Stage::Done);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> ConnectProcessor::error() {
  // close the socket if it is still open.
  (void)connection()->server_conn().close();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("connect::error"));
  }

  const auto last_ec = connect_errors_.back().second;

  connection()->connect_error_code(last_ec);

  {
    std::string msg;
    for (auto [err, ec] : connect_errors_) {
      if (!msg.empty()) {
        msg += ", ";
      }
      msg += err;
      msg += ": ";
      msg += ec.message();
    }

    log_error("[%s] connecting to backend(s) for client from %s failed: %s",
              connection()->context().get_name().c_str(),
              connection()->client_conn().endpoint().c_str(), msg.c_str());
  }

  // We are done for this connection, lets reset the connect status for new
  // incoming connections.
  connection()->destination_manager()->connect_status({});

  if (auto *ev = trace_event_connect_) {
    ev->attrs.emplace_back("mysql.error_message", last_ec.message());
    trace_span_end(ev);
  }

  if (last_ec == make_error_condition(std::errc::too_many_files_open) ||
      last_ec ==
          make_error_condition(std::errc::too_many_files_open_in_system)) {
    // release file-descriptors on the connection pool when out-of-fds is
    // noticed.
    //
    // don't retry as router may run into an infinite loop.
    ConnectionPoolComponent::get_instance().clear();
  } else if (!connection()->get_destination_id().has_value() &&
             all_quarantined_) {
    // fresh-connect == "destination-id is empty"

    // if there are no destinations for a fresh connect, close the
    // acceptor-ports
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("connect::error::all_down"));
    }
  }

  connection()->server_conn().protocol().handshake_state(
      ClassicProtocolState::HandshakeState::kConnected);
  connection()->authenticated(false);

  connection()->completed();
  stage(Stage::Done);

  on_error_({2003, "Can't connect to remote MySQL server", "HY000"});

  return Result::Again;
}
