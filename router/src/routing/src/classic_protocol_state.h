/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_PROTOCOL_STATE_INCLUDED
#define ROUTING_CLASSIC_PROTOCOL_STATE_INCLUDED

#include <chrono>
#include <optional>

#include "classic_prepared_statement.h"

/**
 * protocol state of a classic protocol connection.
 */
class ClassicProtocolState {
 public:
  enum class HandshakeState {
    kConnected,
    kServerGreeting,
    kClientGreeting,
    kFinished,
  };

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
  const std::optional<FrameInfo> &current_frame() const {
    return current_frame_;
  }

  std::optional<uint8_t> &current_msg_type() { return msg_type_; }
  const std::optional<uint8_t> &current_msg_type() const { return msg_type_; }

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

  HandshakeState handshake_state() const { return handshake_state_; }

  void handshake_state(HandshakeState state) { handshake_state_ = state; }

#if 0
  classic_protocol::status::value_type status_flags() const {
    return status_flags_;
  }

  void status_flags(classic_protocol::status::value_type val) {
    status_flags_ = val;
  }
#endif

 private:
  classic_protocol::capabilities::value_type server_caps_{};
  classic_protocol::capabilities::value_type client_caps_{};

  std::optional<classic_protocol::message::client::Greeting> client_greeting_{};
  std::optional<classic_protocol::message::server::Greeting> server_greeting_{};

  std::optional<FrameInfo> current_frame_{};
  std::optional<uint8_t> msg_type_{};

  uint8_t seq_id_{255};  // next use will increment to 0

  std::string username_;
  std::string schema_;
  std::string recv_attributes_;
  std::string sent_attributes_;

  std::string auth_method_name_;
  std::string auth_method_data_;

  // status flags of the last statement.
  classic_protocol::status::value_type status_flags_{};

  HandshakeState handshake_state_{HandshakeState::kConnected};
};

class ClientSideClassicProtocolState : public ClassicProtocolState {
 public:
  using ClassicProtocolState::ClassicProtocolState;

  void password(std::optional<std::string> pw) { password_ = std::move(pw); }
  const std::optional<std::string> &password() const { return password_; }

  classic_protocol::status::value_type status_flags() const {
    return status_flags_;
  }

  void status_flags(classic_protocol::status::value_type val) {
    status_flags_ = val;
  }

  using PreparedStatements = std::unordered_map<uint32_t, PreparedStatement>;

  const PreparedStatements &prepared_statements() const {
    return prepared_stmts_;
  }
  PreparedStatements &prepared_statements() { return prepared_stmts_; }

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
  std::optional<std::string> password_;

  // status flags of the last statement.
  classic_protocol::status::value_type status_flags_{};

  PreparedStatements prepared_stmts_;

  // if commands shall be traced.
  bool trace_commands_{false};

  std::string gtid_executed_;

  bool wait_for_my_writes_{true};
  std::chrono::seconds wait_for_my_writes_timeout_{1};

  std::optional<AccessMode> access_mode_{};
};

class ServerSideClassicProtocolState : public ClassicProtocolState {
 public:
  using ClassicProtocolState::ClassicProtocolState;
};

#endif
