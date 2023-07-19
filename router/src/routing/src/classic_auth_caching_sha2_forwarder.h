/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_AUTH_CACHING_SHA2_FORWARDER_INCLUDED
#define ROUTING_CLASSIC_AUTH_CACHING_SHA2_FORWARDER_INCLUDED

#include <string>
#include <string_view>
#include <system_error>

#include "classic_auth.h"
#include "classic_auth_caching_sha2.h"
#include "classic_connection_base.h"
#include "forwarding_processor.h"
#include "mysql/harness/stdx/expected.h"

class AuthCachingSha2Forwarder : public ForwardingProcessor {
 public:
  AuthCachingSha2Forwarder(MysqlRoutingClassicConnectionBase *conn,
                           std::string initial_server_auth_data,
                           bool client_requested_full_auth = false)
      : ForwardingProcessor(conn),
        initial_server_auth_data_{std::move(initial_server_auth_data)},
        client_requested_full_auth_{client_requested_full_auth},
        stage_{client_requested_full_auth_ ? Stage::Response : Stage::Init} {}

  enum class Stage {
    Init,

    ClientData,
    EncryptedPassword,
    PlaintextPassword,

    PublicKeyResponse,
    PublicKey,
    AuthData,

    Response,

    Error,
    Ok,

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  using Auth = AuthCachingSha2Password;

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> client_data();
  stdx::expected<Result, std::error_code> encrypted_password();
  stdx::expected<Result, std::error_code> plaintext_password();
  stdx::expected<Result, std::error_code> auth_data();
  stdx::expected<Result, std::error_code> public_key_response();
  stdx::expected<Result, std::error_code> public_key();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  stdx::expected<Result, std::error_code> send_password();

  std::string initial_server_auth_data_;

  // track if the plaintext password was requested from the client.
  bool client_requested_full_auth_{false};
  // track if the plaintext password was requested by the server.
  bool server_requested_full_auth_{false};

  Stage stage_;
};

#endif
