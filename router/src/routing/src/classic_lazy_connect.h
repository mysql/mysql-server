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

#ifndef ROUTING_CLASSIC_LAZY_CONNECT_INCLUDED
#define ROUTING_CLASSIC_LAZY_CONNECT_INCLUDED

#include <system_error>

#include "forwarding_processor.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "router_require.h"

/**
 * attach a server connection and initialize it.
 *
 * - if a server-connection is attached to the client connection, leave.
 * - otherwise,
 *   - if a connection can be taken from the pool, take it.
 *   - otherwise, connect to the server and authenticate.
 * - set tracking session-vars
 * - set the client's schema, if it differs from the server-connection's.
 *
 * Precondition:
 *
 * - the client's cleartext password must be known.
 */
class LazyConnector : public ForwardingProcessor {
 public:
  /**
   * create a lazy-connector.
   *
   * @param conn a connection handle
   * @param in_handshake if true, the client connection is in Greeting or
   * ChangeUser right now.
   * @param on_error function that's called if an error happened.
   * @param parent_event parent event for the tracer
   *
   * If "in_handshake" the LazyConnector may ask the client for a
   * "auth-method-switch" or a "plaintext-password".
   */
  LazyConnector(
      MysqlRoutingClassicConnectionBase *conn, bool in_handshake,
      std::function<void(const classic_protocol::message::server::Error &err)>
          on_error,
      TraceEvent *parent_event)
      : ForwardingProcessor(conn),
        in_handshake_{in_handshake},
        on_error_(std::move(on_error)),
        parent_event_(parent_event) {}

  enum class Stage {
    FromStash,
    Connect,
    Connected,
    Authenticated,
    FetchUserAttrs,
    FetchUserAttrsDone,
    SendAuthOk,
    SetVars,
    SetVarsDone,
    SetServerOption,
    SetServerOptionDone,
    SetSchema,
    SetSchemaDone,
    FetchSysVars,
    FetchSysVarsDone,
    WaitGtidExecuted,
    WaitGtidExecutedDone,
    SetTrxCharacteristics,
    SetTrxCharacteristicsDone,

    PoolOrClose,
    FallbackToWrite,

    Done,
  };

  stdx::expected<Processor::Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

  struct RequiredConnectionAttributes {
    std::optional<bool> ssl;
    std::optional<bool> x509;
    std::optional<std::string> issuer;
    std::optional<std::string> subject;
  };

  void failed(
      const std::optional<classic_protocol::message::server::Error> &err) {
    failed_ = err;
  }

  std::optional<classic_protocol::message::server::Error> failed() const {
    return failed_;
  }

 private:
  stdx::expected<Processor::Result, std::error_code> from_stash();
  stdx::expected<Processor::Result, std::error_code> connect();
  stdx::expected<Processor::Result, std::error_code> connected();
  stdx::expected<Processor::Result, std::error_code> authenticated();
  stdx::expected<Processor::Result, std::error_code> fetch_user_attrs();
  stdx::expected<Processor::Result, std::error_code> fetch_user_attrs_done();
  stdx::expected<Processor::Result, std::error_code> send_auth_ok();
  stdx::expected<Processor::Result, std::error_code> set_vars();
  stdx::expected<Processor::Result, std::error_code> set_vars_done();
  stdx::expected<Processor::Result, std::error_code> set_server_option();
  stdx::expected<Processor::Result, std::error_code> set_server_option_done();
  stdx::expected<Processor::Result, std::error_code> set_schema();
  stdx::expected<Processor::Result, std::error_code> set_schema_done();
  stdx::expected<Processor::Result, std::error_code> fetch_sys_vars();
  stdx::expected<Processor::Result, std::error_code> fetch_sys_vars_done();
  stdx::expected<Processor::Result, std::error_code> wait_gtid_executed();
  stdx::expected<Processor::Result, std::error_code> wait_gtid_executed_done();
  stdx::expected<Processor::Result, std::error_code> set_trx_characteristics();
  stdx::expected<Processor::Result, std::error_code>
  set_trx_characteristics_done();

  stdx::expected<Processor::Result, std::error_code> pool_or_close();
  stdx::expected<Processor::Result, std::error_code> fallback_to_write();

  Stage stage_{Stage::FromStash};

  bool in_handshake_;

  RouterRequireFetcher::Result required_connection_attributes_fetcher_result_;

  std::function<void(const classic_protocol::message::server::Error &err)>
      on_error_;

  bool retry_connect_{false};     // set on transient failure
  bool already_fallback_{false};  // set in fallback_to_write()

  // start timepoint to calculate the connect-retry-timeout.
  std::chrono::steady_clock::time_point started_{
      std::chrono::steady_clock::now()};

  std::optional<classic_protocol::message::server::Error> failed_;

  std::string trx_stmt_;

  TraceEvent *parent_event_{};
  TraceEvent *trace_event_connect_{};
  TraceEvent *trace_event_authenticate_{};
  TraceEvent *trace_event_set_vars_{};
  TraceEvent *trace_event_fetch_sys_vars_{};
  TraceEvent *trace_event_set_schema_{};
  TraceEvent *trace_event_wait_gtid_executed_{};
  TraceEvent *trace_event_check_read_only_{};
  TraceEvent *trace_event_fallback_to_write_{};
  TraceEvent *trace_event_set_trx_characteristics_{};
};

#endif
