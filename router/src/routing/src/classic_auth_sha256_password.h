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

#ifndef ROUTING_CLASSIC_AUTH_SHA256_PASSWORD_INCLUDED
#define ROUTING_CLASSIC_AUTH_SHA256_PASSWORD_INCLUDED

#include <memory>  // unique_ptr
#include <string_view>
#include <system_error>

#include <openssl/ssl.h>

#include "classic_auth.h"
#include "classic_connection.h"
#include "mysql/harness/stdx/expected.h"

// low-level routings for sha256_password
class AuthSha256Password : public AuthBase {
 public:
  static constexpr const std::string_view kName{"sha256_password"};

  static constexpr const std::string_view kEmptyPassword{"\x00", 1};
  static constexpr const std::string_view kPublicKeyRequest{"\x01"};

  static std::optional<std::string> scramble(std::string_view nonce,
                                             std::string_view pwd);

  static stdx::expected<size_t, std::error_code> send_public_key_request(
      Channel *dst_channel, ClassicProtocolState *dst_protocol);

  static stdx::expected<size_t, std::error_code> send_public_key(
      Channel *dst_channel, ClassicProtocolState *dst_protocol,
      const std::string &public_key);

  static stdx::expected<size_t, std::error_code> send_plaintext_password(
      Channel *dst_channel, ClassicProtocolState *dst_protocol,
      const std::string &password);

  static stdx::expected<size_t, std::error_code> send_encrypted_password(
      Channel *dst_channel, ClassicProtocolState *dst_protocol,
      const std::string &password);

  static bool is_public_key_request(const std::string_view &data);
  static bool is_public_key(const std::string_view &data);
};

class AuthSha256Sender : public Processor {
 public:
  AuthSha256Sender(MysqlRoutingClassicConnection *conn,
                   std::string initial_server_auth_data, std::string password)
      : Processor(conn),
        initial_server_auth_data_{std::move(initial_server_auth_data)},
        password_{std::move(password)} {}

  enum class Stage {
    Init,

    Response,

    PublicKey,

    Error,
    Ok,

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  using Auth = AuthSha256Password;

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> public_key();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  Stage stage_{Stage::Init};

  std::string initial_server_auth_data_;
  std::string password_;
};

// forwarder

class AuthSha256Forwarder : public Processor {
 public:
  AuthSha256Forwarder(MysqlRoutingClassicConnection *conn,
                      std::string initial_server_auth_data,
                      bool in_handshake = false)
      : Processor(conn),
        initial_server_auth_data_{std::move(initial_server_auth_data)},
        stage_{in_handshake ? Stage::Response : Stage::Init} {}

  enum class Stage {
    Init,

    ClientData,
    EncryptedPassword,

    PublicKeyResponse,
    PublicKey,

    Response,
    Error,
    Ok,

    Done,
  };

  stdx::expected<Result, std::error_code> process() override;

  void stage(Stage stage) { stage_ = stage; }
  [[nodiscard]] Stage stage() const { return stage_; }

 private:
  using Auth = AuthSha256Password;

  stdx::expected<Result, std::error_code> init();
  stdx::expected<Result, std::error_code> client_data();
  stdx::expected<Result, std::error_code> encrypted_password();
  stdx::expected<Result, std::error_code> response();
  stdx::expected<Result, std::error_code> public_key();
  stdx::expected<Result, std::error_code> public_key_response();
  stdx::expected<Result, std::error_code> error();
  stdx::expected<Result, std::error_code> ok();

  stdx::expected<Result, std::error_code> send_password();

  std::string initial_server_auth_data_;

  Stage stage_;
};

#endif
