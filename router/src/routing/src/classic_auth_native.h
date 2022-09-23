/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTING_CLASSIC_AUTH_NATIVE_INCLUDED
#define ROUTING_CLASSIC_AUTH_NATIVE_INCLUDED

#include <memory>  // unique_ptr
#include <optional>
#include <string_view>
#include <system_error>

#include <openssl/ssl.h>

#include "classic_connection.h"
#include "mysql/harness/stdx/expected.h"
#include "processor.h"

class AuthNativePassword {
 public:
  static constexpr const std::string_view kName{"mysql_native_password"};

  static std::optional<std::string> scramble(std::string_view nonce,
                                             std::string_view pwd);
};

class AuthNativeSender : public Processor {
 public:
  AuthNativeSender(MysqlRoutingClassicConnection *conn,
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

class AuthNativeForwarder : public Processor {
 public:
  AuthNativeForwarder(MysqlRoutingClassicConnection *conn,
                      std::string initial_server_auth_data,
                      bool in_handshake = false)
      : Processor(conn),
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
  using Auth = AuthNativePassword;

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> client_data();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  std::string initial_server_auth_data_;

  Stage stage_;
};

#endif
