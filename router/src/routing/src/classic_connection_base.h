/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_CONNECTION_BASE_INCLUDED
#define ROUTING_CLASSIC_CONNECTION_BASE_INCLUDED

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "channel.h"
#include "classic_prepared_statement.h"
#include "connection.h"  // MySQLRoutingConnectionBase
#include "mysql/harness/net_ts/executor.h"
#include "mysql/harness/net_ts/timer.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/classic_protocol_session_track.h"
#include "mysqlrouter/connection_pool.h"
#include "processor.h"
#include "sql_exec_context.h"
#include "trace_span.h"
#include "tracer.h"

/**
 * protocol state of a classic protocol connection.
 */
class ClassicProtocolState : public ProtocolStateBase {
 public:
  ClassicProtocolState() = default;

  ClassicProtocolState(
      classic_protocol::capabilities::value_type server_caps,
      classic_protocol::capabilities::value_type client_caps,
      std::optional<classic_protocol::message::server::Greeting>
          server_greeting,
      std::string username,  //
      std::string schema,    //
      std::string attributes)
      : server_caps_{server_caps},
        client_caps_{client_caps},
        server_greeting_{std::move(server_greeting)},
        username_{std::move(username)},
        schema_{std::move(schema)},
        sent_attributes_{std::move(attributes)} {}

  void server_capabilities(classic_protocol::capabilities::value_type caps) {
    server_caps_ = caps;
  }

  void client_capabilities(classic_protocol::capabilities::value_type caps) {
    client_caps_ = caps;
  }

  classic_protocol::capabilities::value_type client_capabilities() const {
    return client_caps_;
  }

  classic_protocol::capabilities::value_type server_capabilities() const {
    return server_caps_;
  }

  classic_protocol::capabilities::value_type shared_capabilities() const {
    return server_caps_ & client_caps_;
  }

  std::optional<classic_protocol::message::client::Greeting> client_greeting()
      const {
    return client_greeting_;
  }

  void client_greeting(
      std::optional<classic_protocol::message::client::Greeting> msg) {
    client_greeting_ = std::move(msg);
  }

  std::optional<classic_protocol::message::server::Greeting> server_greeting()
      const {
    return server_greeting_;
  }

  void server_greeting(
      std::optional<classic_protocol::message::server::Greeting> msg) {
    server_greeting_ = std::move(msg);
  }

  uint8_t &seq_id() { return seq_id_; }
  uint8_t seq_id() const { return seq_id_; }

  void seq_id(uint8_t id) { seq_id_ = id; }

  struct FrameInfo {
    uint8_t seq_id_;               //!< sequence id.
    size_t frame_size_;            //!< size of the whole frame.
    size_t forwarded_frame_size_;  //!< size of the whole frame that's already
                                   //!< forwarded.
  };

  std::optional<FrameInfo> &current_frame() { return current_frame_; }
  std::optional<uint8_t> &current_msg_type() { return msg_type_; }

  uint64_t columns_left{};
  uint32_t params_left{};

  [[nodiscard]] std::string auth_method_name() const {
    return auth_method_name_;
  }

  void auth_method_name(std::string name) {
    auth_method_name_ = std::move(name);
  }

  [[nodiscard]] std::string auth_method_data() const {
    return auth_method_data_;
  }

  void auth_method_data(std::string data) {
    auth_method_data_ = std::move(data);
  }

  std::string username() { return username_; }
  void username(std::string user) { username_ = std::move(user); }

  void password(std::optional<std::string> pw) { password_ = std::move(pw); }
  const std::optional<std::string> &password() const { return password_; }

  std::string schema() { return schema_; }
  void schema(std::string s) { schema_ = std::move(s); }

  // connection attributes there were received.
  std::string attributes() { return recv_attributes_; }
  void attributes(std::string attrs) { recv_attributes_ = std::move(attrs); }

  // connection attributes that were sent.
  std::string sent_attributes() { return sent_attributes_; }
  void sent_attributes(std::string attrs) {
    sent_attributes_ = std::move(attrs);
  }

  using PreparedStatements = std::unordered_map<uint32_t, PreparedStatement>;

  const PreparedStatements &prepared_statements() const {
    return prepared_stmts_;
  }
  PreparedStatements &prepared_statements() { return prepared_stmts_; }

  classic_protocol::status::value_type status_flags() const {
    return status_flags_;
  }

  void status_flags(classic_protocol::status::value_type val) {
    status_flags_ = val;
  }

  /**
   * trace the events of the commands.
   *
   * - enabled by ROUTER SET trace = 1
   * - disabled by ROUTER SET trace = 0, change-user or reset-connection.
   *
   * @retval true if 'ROUTER SET trace' is '1'
   * @retval false if 'ROUTER SET trace' is '0'
   */
  bool trace_commands() const { return trace_commands_; }
  void trace_commands(bool val) { trace_commands_ = val; }

  // executed GTIDs for this connection.
  void gtid_executed(const std::string &gtid_execed) {
    gtid_executed_ = gtid_execed;
  }
  std::string gtid_executed() const { return gtid_executed_; }

  void wait_for_my_writes(bool v) { wait_for_my_writes_ = v; }
  bool wait_for_my_writes() const { return wait_for_my_writes_; }

  std::chrono::seconds wait_for_my_writes_timeout() const {
    return wait_for_my_writes_timeout_;
  }
  void wait_for_my_writes_timeout(std::chrono::seconds timeout) {
    wait_for_my_writes_timeout_ = timeout;
  }

  enum class AccessMode {
    ReadWrite,
    ReadOnly,
  };

  std::optional<AccessMode> access_mode() const { return access_mode_; }
  void access_mode(std::optional<AccessMode> v) { access_mode_ = v; }

 private:
  classic_protocol::capabilities::value_type server_caps_{};
  classic_protocol::capabilities::value_type client_caps_{};

  std::optional<classic_protocol::message::client::Greeting> client_greeting_{};
  std::optional<classic_protocol::message::server::Greeting> server_greeting_{};

  std::optional<FrameInfo> current_frame_{};
  std::optional<uint8_t> msg_type_{};

  uint8_t seq_id_{255};  // next use will increment to 0

  std::string username_;
  std::optional<std::string> password_;
  std::string schema_;
  std::string recv_attributes_;
  std::string sent_attributes_;

  std::string auth_method_name_;
  std::string auth_method_data_;

  PreparedStatements prepared_stmts_;

  // status flags of the last statement.
  classic_protocol::status::value_type status_flags_{};

  // if commands shall be traced.
  bool trace_commands_{false};

  std::string gtid_executed_;

  bool wait_for_my_writes_{routing::kDefaultWaitForMyWrites};
  std::chrono::seconds wait_for_my_writes_timeout_{
      routing::kDefaultWaitForMyWritesTimeout};

  std::optional<AccessMode> access_mode_{};
};

class MysqlRoutingClassicConnectionBase
    : public MySQLRoutingConnectionBase,
      public std::enable_shared_from_this<MysqlRoutingClassicConnectionBase> {
 protected:
  // constructor
  //
  // use ::create() instead.
  MysqlRoutingClassicConnectionBase(
      MySQLRoutingContext &context, RouteDestination *route_destination,
      std::unique_ptr<ConnectionBase> client_connection,
      std::unique_ptr<RoutingConnectionBase> client_routing_connection,
      std::function<void(MySQLRoutingConnectionBase *)> remove_callback)
      : MySQLRoutingConnectionBase{context, std::move(remove_callback)},
        route_destination_{route_destination},
        destinations_{route_destination_ != nullptr
                          ? route_destination_->destinations()
                          : Destinations{}},
        socket_splicer_{std::make_unique<ProtocolSplicerBase>(
            TlsSwitchableConnection{std::move(client_connection),
                                    std::move(client_routing_connection),
                                    context.source_ssl_mode(),
                                    std::make_unique<ClassicProtocolState>()},
            TlsSwitchableConnection{nullptr, nullptr, context.dest_ssl_mode(),
                                    std::make_unique<ClassicProtocolState>()})},
        read_timer_{socket_splicer()->client_conn().connection()->io_ctx()},
        connect_timer_{socket_splicer()->client_conn().connection()->io_ctx()},
        wait_for_my_writes_{context.wait_for_my_writes()},
        wait_for_my_writes_timeout_{context.wait_for_my_writes_timeout()} {}

 public:
  // create a new shared_ptr<ThisClass>
  //
  template <typename... Args>
  [[nodiscard]] static std::shared_ptr<MysqlRoutingClassicConnectionBase>
  create(
      // clang-format off
      Args &&... args) {
    // clang-format on

    // can't use make_unique<> here as the constructor is private.
    return std::shared_ptr<MysqlRoutingClassicConnectionBase>(
        new MysqlRoutingClassicConnectionBase(std::forward<Args>(args)...));
  }

  // get a shared-ptr that refers the same 'this'
  std::shared_ptr<MysqlRoutingClassicConnectionBase> getptr() {
    return shared_from_this();
  }

  static stdx::expected<size_t, std::error_code> encode_error_packet(
      std::vector<uint8_t> &error_frame, const uint8_t seq_id,
      const classic_protocol::capabilities::value_type caps,
      const uint16_t error_code, const std::string &msg,
      const std::string &sql_state);

  void on_handshake_received();
  void on_handshake_aborted();

  SslMode source_ssl_mode() const {
    return this->socket_splicer()->source_ssl_mode();
  }

  SslMode dest_ssl_mode() const {
    return this->socket_splicer()->dest_ssl_mode();
  }

  net::impl::socket::native_handle_type get_client_fd() const override {
    return socket_splicer()->client_conn().native_handle();
  }

  std::string get_client_address() const override {
    return socket_splicer()->client_conn().endpoint();
  }

  std::string get_server_address() const override {
    return socket_splicer()->server_conn().endpoint();
  }

  void disconnect() override;

  virtual void async_run() {}

  void send_server_failed(std::error_code ec, bool call_finish = true);

  void recv_server_failed(std::error_code ec, bool call_finish = true);

  void send_client_failed(std::error_code ec, bool call_finish = true);

  void recv_client_failed(std::error_code ec, bool call_finish = true);

  void server_socket_failed(std::error_code ec, bool call_finish = true);

  virtual void client_socket_failed(std::error_code ec,
                                    bool call_finish = true);

  // resume
  //
  // A Processor may suspend by returning Result::Suspend. When woken,
  // typically using an async timer, the Processor calls resume() to execute
  // the next loop() iteration. This allows waiting asynchronously for a
  // condition other than async io.
  void resume() { call_next_function(Function::kLoop); }

 protected:
  enum class Function {
    kLoop,

    kFinish,
  };

  void call_next_function(Function next) {
    switch (next) {
      case Function::kFinish:
        return finish();

      case Function::kLoop:
        return loop();
    }
  }

 private:
  // a stack of processors
  //
  // take the last processor until its done.
  //
  // Flow -> Greeting | Command
  //   Greeting -> Connect -> Server::Greeting
  //     Server::Greeting -> Server::Greeting::Greeting |
  //     Server::Greeting::Error Server::Greeting::Error -> Error::Fatal
  //     Server::Greeting::Greeting -> Client::Greeting
  //     Client::Greeting -> TlsConnect | Server::Greeting::Response
  //     TlsConnect -> Client::Greeting::Full | Error::Fatal
  //     Client::Greeting::Full -> Server::Ok | Auth::Switch | Server::Error
  //     Auth::Switch -> ...
  //       Auth
  //     Server::Ok -> Command
  //   Command ->
  //
  std::vector<std::unique_ptr<BasicProcessor>> processors_;

 public:
  void push_processor(std::unique_ptr<BasicProcessor> processor) {
    return processors_.push_back(std::move(processor));
  }

  void pop_processor() { processors_.pop_back(); }

  stdx::expected<void, std::error_code> track_session_changes(
      net::const_buffer session_trackers,
      classic_protocol::capabilities::value_type caps,
      bool ignore_some_state_changed = false);

  /**
   * reset the connection's settings to the initial-values.
   */
  void reset_to_initial();

 private:
  void trace_and_call_function(Tracer::Event::Direction dir,
                               std::string_view stage, Function func);

  void async_send_client(Function next);

  void async_recv_client(Function next);

  void async_send_server(Function next);

  void async_recv_server(Function next);

  void async_recv_both(Function next);

  void async_send_client_and_finish();

  void async_wait_client_closed();

  void async_wait_send_server(Function next);

 private:
  // the client didn't send a Greeting before closing the connection.
  //
  // Generate a Greeting to be sent to the server, to ensure the router's IP
  // isn't blocked due to the server's max_connect_errors.
  void server_side_client_greeting();

  // main processing loop
  void loop();

  // after a QUIT, we should wait until the client closed the connection.

  // called when the connection should be closed.
  //
  // called multiple times (once per "active_work_").
  void finish();

  // final state.
  //
  // removes the connection from the connection-container.
  virtual void done();

 public:
  ClassicProtocolState *client_protocol() {
    return dynamic_cast<ClassicProtocolState *>(
        socket_splicer()->client_conn().protocol());
  }

  const ClassicProtocolState *client_protocol() const {
    return dynamic_cast<const ClassicProtocolState *>(
        socket_splicer()->client_conn().protocol());
  }

  ClassicProtocolState *server_protocol() {
    return dynamic_cast<ClassicProtocolState *>(
        socket_splicer()->server_conn().protocol());
  }

  const ClassicProtocolState *server_protocol() const {
    return dynamic_cast<const ClassicProtocolState *>(
        socket_splicer()->server_conn().protocol());
  }

  const ProtocolSplicerBase *socket_splicer() const {
    return socket_splicer_.get();
  }

  ProtocolSplicerBase *socket_splicer() { return socket_splicer_.get(); }

  std::string get_destination_id() const override {
    return expected_server_mode() == mysqlrouter::ServerMode::ReadOnly
               ? read_only_destination_id()
               : read_write_destination_id();
  }

  void destination_id(const std::string &id) {
    expected_server_mode() == mysqlrouter::ServerMode::ReadOnly
        ? read_only_destination_id(id)
        : read_write_destination_id(id);
  }

  std::string read_only_destination_id() const override {
    return ro_destination_id_;
  }
  void read_only_destination_id(const std::string &destination_id) {
    ro_destination_id_ = destination_id;
  }

  std::string read_write_destination_id() const override {
    return rw_destination_id_;
  }
  void read_write_destination_id(const std::string &destination_id) {
    rw_destination_id_ = destination_id;
  }

  /**
   * check if the connection is authenticated.
   *
   * 'true' after the initial handshake and change-user finished with "ok".
   * 'false' at connection start and after change-user is started.
   *
   * @retval true if the connection is authenticated.
   * @return false otherwise
   */
  bool authenticated() const { return authenticated_; }
  void authenticated(bool v) { authenticated_ = v; }

  /**
   * check if connection sharing is possible.
   *
   * - the configuration enabled it
   */
  bool connection_sharing_possible() const;

  /**
   * check if connection sharing is allowed.
   *
   * - connection sharing is possible.
   * - no active transaction
   * - no SET TRANSACTION
   */
  bool connection_sharing_allowed() const;

  /**
   * reset the connection-sharing state.
   *
   * - after COM_RESET_CONNECTION::ok
   * - after COM_CHANGE_USER::ok
   */
  void connection_sharing_allowed_reset();

  /**
   * @return a string representing the reason why sharing is blocked.
   */
  std::string connection_sharing_blocked_by() const;

 private:
  int active_work_{0};

  bool authenticated_{false};

  bool client_greeting_sent_{false};

 public:
  bool client_greeting_sent() const { return client_greeting_sent_; }
  void client_greeting_sent(bool sent) { client_greeting_sent_ = sent; }

  /**
   * if the router is sending the initial server-greeting.
   *
   * if true, the router sends the initial greeting to the client,
   * if false, the server is sending the initial greeting and router is forward
   * it.
   */
  bool greeting_from_router() const {
    return !((source_ssl_mode() == SslMode::kPassthrough) ||
             (source_ssl_mode() == SslMode::kPreferred &&
              dest_ssl_mode() == SslMode::kAsClient));
  }

  /// set if the server-connection requires TLS
  void requires_tls(bool v) { requires_tls_ = v; }

  /// get if the server-connection requires TLS
  bool requires_tls() const { return requires_tls_; }

  /// set if the server-connection requires a client cert
  void requires_client_cert(bool v) { requires_client_cert_ = v; }

  /// get if the server-connection requires a client cert
  bool requires_client_cert() const { return requires_client_cert_; }

  void some_state_changed(bool v) { some_state_changed_ = v; }

  void expected_server_mode(mysqlrouter::ServerMode v) {
    expected_server_mode_ = v;
  }
  mysqlrouter::ServerMode expected_server_mode() const {
    return expected_server_mode_;
  }

  void wait_for_my_writes(bool v) { wait_for_my_writes_ = v; }
  bool wait_for_my_writes() const { return wait_for_my_writes_; }

  void gtid_at_least_executed(const std::string &gtid) {
    gtid_at_least_executed_ = gtid;
  }
  std::string gtid_at_least_executed() const { return gtid_at_least_executed_; }

  std::chrono::seconds wait_for_my_writes_timeout() const {
    return wait_for_my_writes_timeout_;
  }
  void wait_for_my_writes_timeout(std::chrono::seconds timeout) {
    wait_for_my_writes_timeout_ = timeout;
  }

  RouteDestination *destinations() { return route_destination_; }
  Destinations &current_destinations() { return destinations_; }

  void collation_connection_maybe_dirty(bool val) {
    collation_connection_maybe_dirty_ = val;
  }

  bool collation_connection_maybe_dirty() const {
    return collation_connection_maybe_dirty_;
  }

  std::optional<classic_protocol::session_track::TransactionCharacteristics>
  trx_characteristics() const {
    return trx_characteristics_;
  }

  std::optional<classic_protocol::session_track::TransactionState> trx_state()
      const {
    return trx_state_;
  }

 private:
  RouteDestination *route_destination_;
  Destinations destinations_;

  std::unique_ptr<ProtocolSplicerBase> socket_splicer_;

  std::string rw_destination_id_;  // read-write destination-id
  std::string ro_destination_id_;  // read-only destination-id

  /**
   * client side handshake isn't finished yet.
   */
  bool in_handshake_{true};

  std::optional<classic_protocol::session_track::TransactionState> trx_state_;
  std::optional<classic_protocol::session_track::TransactionCharacteristics>
      trx_characteristics_;
  bool some_state_changed_{false};

  bool collation_connection_maybe_dirty_{false};

  bool requires_tls_{true};

  bool requires_client_cert_{false};

 public:
  ExecutionContext &execution_context() { return exec_ctx_; }
  const ExecutionContext &execution_context() const { return exec_ctx_; }

 private:
  ExecutionContext exec_ctx_;

 public:
  void trace(Tracer::Event e) { tracer_.trace(e); }

  Tracer &tracer() { return tracer_; }

 private:
  Tracer tracer_{false};

 public:
  net::steady_timer &read_timer() { return read_timer_; }
  net::steady_timer &connect_timer() { return connect_timer_; }

  void connect_error_code(const std::error_code &ec) { connect_ec_ = ec; }
  std::error_code connect_error_code() const { return connect_ec_; }

  void diagnostic_area_changed(bool diagnostic_area_changed) {
    diagnostic_area_changed_ = diagnostic_area_changed;
  }
  bool diagnostic_area_changed() const { return diagnostic_area_changed_; }

  const TraceSpan &events() const { return events_; }
  TraceSpan &events() { return events_; }

  enum class FromEither {
    None,
    Started,
    RecvedFromClient,
    RecvedFromServer,
  };

  void recv_from_either(FromEither v) { recv_from_either_ = v; }

  FromEither recv_from_either() const { return recv_from_either_; }

 private:
  net::steady_timer read_timer_;
  net::steady_timer connect_timer_;

  std::error_code connect_ec_{};

  bool diagnostic_area_changed_{};

  FromEither recv_from_either_{FromEither::None};

  // events for router.trace.
  TraceSpan events_;

  // where to target the server-connections if access_mode is kAuto
  //
  // - Unavailable -> any destination (at connect)
  // - ReadOnly    -> a read-only destination (if available)
  // - ReadWrite   -> a read-write destination (if available)
  mysqlrouter::ServerMode expected_server_mode_{
      mysqlrouter::ServerMode::Unavailable};

  // wait for 'gtid_at_least_executed_' with switch to a read-only destination?
  bool wait_for_my_writes_;

  // GTID to wait for. May be overwritten by client with query attributes.
  std::string gtid_at_least_executed_;

  // timeout for read your own writes. Setable with query attributes.
  std::chrono::seconds wait_for_my_writes_timeout_;
};

#endif
