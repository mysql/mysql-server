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

#ifndef ROUTING_CLASSIC_AUTH_NATIVE_SENDER_INCLUDED
#define ROUTING_CLASSIC_AUTH_NATIVE_SENDER_INCLUDED

#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "classic_auth_native.h"
#include "classic_connection_base.h"
#include "mysql/harness/stdx/expected.h"
#include "processor.h"

class AuthNativeSender : public Processor {
 public:
  AuthNativeSender(MysqlRoutingClassicConnectionBase *conn,
                   std::string initial_server_auth_data, std::string password)
      : Processor(conn),
        initial_server_auth_data_{std::move(initial_server_auth_data)},
        password_{std::move(password)} {}

  enum class Stage {
    Init,

    Response,

    Error,
    Ok,

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  using Auth = AuthNativePassword;

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  Stage stage_{Stage::Init};

  std::string initial_server_auth_data_;
  std::string password_;
};

#endif
