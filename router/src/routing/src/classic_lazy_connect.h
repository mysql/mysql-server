/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_LAZY_CONNECT_INCLUDED
#define ROUTING_CLASSIC_LAZY_CONNECT_INCLUDED

#include <system_error>

#include "processor.h"

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
class LazyConnector : public Processor {
 public:
  /**
   * create a lazy-connector.
   *
   * @param conn a connection handle
   * @param in_handshake if true, the client connection is in Greeting or
   * ChangeUser right now.
   * @param on_error function that's called if an error happened.
   *
   * If "in_handshake" the LazyConnector may ask the client for a
   * "auth-method-switch" or a "plaintext-password".
   */
  LazyConnector(
      MysqlRoutingClassicConnectionBase *conn, bool in_handshake,
      std::function<void(const classic_protocol::message::server::Error &err)>
          on_error)
      : Processor(conn),
        in_handshake_{in_handshake},
        on_error_(std::move(on_error)) {}

  enum class Stage {
    Connect,
    Connected,
    Authenticated,
    SetVars,
    SetVarsDone,
    SetServerOption,
    SetServerOptionDone,
    SetSchema,
    SetSchemaDone,
    FetchSysVars,
    FetchSysVarsDone,

    Done,
  };

  stdx::expected<Processor::Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  stdx::expected<Processor::Result, std::error_code> connect();
  stdx::expected<Processor::Result, std::error_code> connected();
  stdx::expected<Processor::Result, std::error_code> authenticated();
  stdx::expected<Processor::Result, std::error_code> set_vars();
  stdx::expected<Processor::Result, std::error_code> set_vars_done();
  stdx::expected<Processor::Result, std::error_code> set_server_option();
  stdx::expected<Processor::Result, std::error_code> set_server_option_done();
  stdx::expected<Processor::Result, std::error_code> set_schema();
  stdx::expected<Processor::Result, std::error_code> set_schema_done();
  stdx::expected<Processor::Result, std::error_code> fetch_sys_vars();
  stdx::expected<Processor::Result, std::error_code> fetch_sys_vars_done();

  Stage stage_{Stage::Connect};

  bool in_handshake_;

  std::function<void(const classic_protocol::message::server::Error &err)>
      on_error_;
};

#endif
