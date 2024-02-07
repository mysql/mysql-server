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

#ifndef ROUTING_CLASSIC_AUTH_CLEARTEXT_FORWARDER_INCLUDED
#define ROUTING_CLASSIC_AUTH_CLEARTEXT_FORWARDER_INCLUDED

#include <string>
#include <string_view>
#include <system_error>

#include "classic_auth.h"
#include "classic_auth_cleartext.h"
#include "classic_connection_base.h"
#include "forwarding_processor.h"
#include "mysql/harness/stdx/expected.h"

class AuthCleartextForwarder : public ForwardingProcessor {
 public:
  AuthCleartextForwarder(MysqlRoutingClassicConnectionBase *conn,
                         std::string initial_server_auth_data,
                         bool in_handshake = false)
      : ForwardingProcessor(conn),
        initial_server_auth_data_{std::move(initial_server_auth_data)},
        stage_{in_handshake ? Stage::Response : Stage::Init} {}

  enum class Stage {
    Init,

    ClientData,
    Response,

    Error,
    Ok,

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  using Auth = AuthCleartextPassword;

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> client_data();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  std::string initial_server_auth_data_;

  Stage stage_{Stage::Init};
};

#endif
