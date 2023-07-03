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

#include "classic_auth_sha256_password.h"

#include "classic_frame.h"

// AuthSha256Password

std::optional<std::string> AuthSha256Password::scramble(
    [[maybe_unused]] std::string_view nonce, std::string_view pwd) {
  std::string s(pwd);

  s.push_back('\0');

  return s;
}

stdx::expected<size_t, std::error_code>
AuthSha256Password::send_public_key_request(
    Channel *dst_channel, ClassicProtocolState *dst_protocol) {
  return ClassicFrame::send_msg(
      dst_channel, dst_protocol,
      classic_protocol::borrowed::message::client::AuthMethodData{
          kPublicKeyRequest});
}

stdx::expected<size_t, std::error_code> AuthSha256Password::send_public_key(
    Channel *dst_channel, ClassicProtocolState *dst_protocol,
    const std::string &public_key) {
  return ClassicFrame::send_msg(
      dst_channel, dst_protocol,
      classic_protocol::borrowed::message::server::AuthMethodData{public_key});
}

stdx::expected<size_t, std::error_code>
AuthSha256Password::send_plaintext_password(Channel *dst_channel,
                                            ClassicProtocolState *dst_protocol,
                                            const std::string &password) {
  return ClassicFrame::send_msg(
      dst_channel, dst_protocol,
      classic_protocol::borrowed::message::client::AuthMethodData{password +
                                                                  '\0'});
}

stdx::expected<size_t, std::error_code>
AuthSha256Password::send_encrypted_password(Channel *dst_channel,
                                            ClassicProtocolState *dst_protocol,
                                            const std::string &encrypted) {
  return ClassicFrame::send_msg(
      dst_channel, dst_protocol,
      classic_protocol::borrowed::message::client::AuthMethodData{encrypted});
}

bool AuthSha256Password::is_public_key_request(const std::string_view &data) {
  return data == kPublicKeyRequest;
}

bool AuthSha256Password::is_public_key(const std::string_view &data) {
  return data.size() == 256;
}
