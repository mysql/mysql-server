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

#ifndef ROUTING_CLASSIC_AUTH_CACHING_SHA2_INCLUDED
#define ROUTING_CLASSIC_AUTH_CACHING_SHA2_INCLUDED

#include <optional>
#include <string_view>
#include <system_error>

#include "basic_protocol_splicer.h"  // TlsSwitchableConnection
#include "classic_auth.h"
#include "mysql/harness/stdx/expected.h"

// low-level routings for caching_sha2_password
class AuthCachingSha2Password : public AuthBase {
 public:
  static constexpr const size_t kNonceLength{20};

  static constexpr const std::string_view kName{"caching_sha2_password"};

  static constexpr const std::string_view kPublicKeyRequest{"\x02"};
  static constexpr const uint8_t kFastAuthDone{0x03};
  static constexpr const uint8_t kPerformFullAuth{0x04};

  static std::optional<std::string> scramble(std::string_view nonce,
                                             std::string_view pwd);

  static stdx::expected<size_t, std::error_code> send_public_key_request(
      Channel &dst_channel, ClassicProtocolState &dst_protocol);

  template <class Proto>
  static stdx::expected<size_t, std::error_code> send_public_key_request(
      TlsSwitchableConnection<Proto> &conn) {
    return send_public_key_request(conn.channel(), conn.protocol());
  }

  static stdx::expected<size_t, std::error_code> send_public_key(
      Channel &dst_channel, ClassicProtocolState &dst_protocol,
      const std::string &public_key);

  template <class Proto>
  static stdx::expected<size_t, std::error_code> send_public_key(
      TlsSwitchableConnection<Proto> &conn, const std::string &public_key) {
    return send_public_key(conn.channel(), conn.protocol(), public_key);
  }

  static stdx::expected<size_t, std::error_code>
  send_plaintext_password_request(Channel &dst_channel,
                                  ClassicProtocolState &dst_protocol);

  static stdx::expected<size_t, std::error_code> send_plaintext_password(
      Channel &dst_channel, ClassicProtocolState &dst_protocol,
      const std::string &password);

  template <class Proto>
  static stdx::expected<size_t, std::error_code> send_plaintext_password(
      TlsSwitchableConnection<Proto> &conn, const std::string &password) {
    return send_plaintext_password(conn.channel(), conn.protocol(), password);
  }

  static stdx::expected<size_t, std::error_code> send_encrypted_password(
      Channel &dst_channel, ClassicProtocolState &dst_protocol,
      const std::string &password);

  template <class Proto>
  static stdx::expected<size_t, std::error_code> send_encrypted_password(
      TlsSwitchableConnection<Proto> &conn, const std::string &password) {
    return send_encrypted_password(conn.channel(), conn.protocol(), password);
  }

  static bool is_public_key_request(const std::string_view &data);
  static bool is_public_key(const std::string_view &data);
};

#endif
