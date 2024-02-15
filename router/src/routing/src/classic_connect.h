/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_CONNECT_INCLUDED
#define ROUTING_CLASSIC_CONNECT_INCLUDED

#include <chrono>
#include "classic_connection_base.h"
#include "destination.h"  // RouteDestination
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysqlrouter/destination.h"
#include "processor.h"

class ConnectProcessor : public Processor {
 public:
  ConnectProcessor(
      MysqlRoutingClassicConnectionBase *conn,
      std::function<void(const classic_protocol::message::server::Error &err)>
          on_error,
      TraceEvent *parent_event)
      : Processor(conn),
        io_ctx_{conn->client_conn().connection()->io_ctx()},
        destinations_{conn->current_destinations()},
        destinations_it_{destinations_.begin()},
        on_error_(std::move(on_error)),
        parent_event_(parent_event) {}

  using server_protocol_type = net::ip::tcp;

  enum class Stage {
    InitDestination,
    Resolve,
    InitEndpoint,
    NextEndpoint,
    NextDestination,
    InitConnect,
    FromPool,
    Connect,
    ConnectFinish,
    Connected,

    Error,
    Done,
  };

  stdx::expected<Processor::Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

  bool is_destination_good(const std::string &hostname, uint16_t port) const;

 private:
  stdx::expected<Processor::Result, std::error_code> init_destination();
  stdx::expected<Processor::Result, std::error_code> resolve();
  stdx::expected<Processor::Result, std::error_code> init_endpoint();
  stdx::expected<Processor::Result, std::error_code> next_endpoint();
  stdx::expected<Processor::Result, std::error_code> next_destination();
  stdx::expected<Processor::Result, std::error_code> init_connect();
  stdx::expected<Processor::Result, std::error_code> from_pool();
  stdx::expected<Processor::Result, std::error_code> connect();
  stdx::expected<Processor::Result, std::error_code> connect_finish();
  stdx::expected<Processor::Result, std::error_code> connected();
  stdx::expected<Processor::Result, std::error_code> error();

  void assign_server_side_connection_after_pool(
      ConnectionPool::ServerSideConnection server_conn);

  Stage stage_{Stage::InitDestination};

  net::io_context &io_ctx_;

  net::ip::tcp::resolver resolver_{io_ctx_};
  server_protocol_type::endpoint server_endpoint_;

  Destinations &destinations_;
  Destinations::iterator destinations_it_;
  net::ip::tcp::resolver::results_type endpoints_;
  net::ip::tcp::resolver::results_type::iterator endpoints_it_;

  bool all_quarantined_{false};
  std::error_code destination_ec_;

  // stack of errors.
  std::vector<std::pair<std::string, std::error_code>> connect_errors_;

  std::function<void(const classic_protocol::message::server::Error &err)>
      on_error_;

  std::chrono::steady_clock::time_point connect_started_;

  TraceEvent *parent_event_;
  TraceEvent *trace_event_connect_{nullptr};
  TraceEvent *trace_event_socket_connect_{nullptr};
  TraceEvent *trace_event_socket_from_pool_{nullptr};
};

#endif
