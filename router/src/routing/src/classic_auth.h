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

#ifndef ROUTING_CLASSIC_AUTH_INCLUDED
#define ROUTING_CLASSIC_AUTH_INCLUDED

#include <memory>  // unique_ptr
#include <string_view>
#include <system_error>

#include <openssl/ssl.h>

#include "classic_connection.h"
#include "mysql/harness/stdx/expected.h"

template <class T>
struct OsslDeleter;

template <>
struct OsslDeleter<EVP_PKEY> {
  void operator()(EVP_PKEY *k) { EVP_PKEY_free(k); }
};

using EvpPkey = std::unique_ptr<EVP_PKEY, OsslDeleter<EVP_PKEY>>;

class AuthBase {
 public:
  static stdx::expected<std::string, std::error_code>
  public_key_from_ssl_ctx_as_pem(SSL_CTX *ssl_ctx);

  static stdx::expected<EvpPkey, std::error_code> public_key_from_pem(
      std::string_view pubkey);

  static stdx::expected<std::string, std::error_code> public_key_encrypt(
      std::string plaintext, EVP_PKEY *pkey);

  static stdx::expected<std::string, std::error_code> private_key_decrypt(
      std::string_view ciphertext, EVP_PKEY *priv);

  static stdx::expected<std::string, std::error_code> rsa_decrypt_password(
      SSL_CTX *ssl_ctx, std::string_view encrypted, std::string_view nonce);

  static stdx::expected<std::string, std::error_code> rsa_encrypt_password(
      const EvpPkey &pkey, std::string_view password, std::string_view nonce);
};

class AuthNativePassword {
 public:
  static constexpr const std::string_view kName{"mysql_native_password"};

  static std::optional<std::string> scramble(std::string_view nonce,
                                             std::string_view pwd);
};

class AuthCleartextPassword {
 public:
  static constexpr const std::string_view kName{"mysql_clear_password"};

  static std::optional<std::string> scramble(std::string_view nonce,
                                             std::string_view pwd);
};

// low-level routings for caching_sha2_password
class AuthCachingSha2Password : public AuthBase {
 public:
  static constexpr const std::string_view kName{"caching_sha2_password"};

  static constexpr const std::string_view kPublicKeyRequest{"\x02"};
  static constexpr const uint8_t kFastAuthDone{0x03};
  static constexpr const uint8_t kPerformFullAuth{0x04};

  static std::optional<std::string> scramble(std::string_view nonce,
                                             std::string_view pwd);

  static stdx::expected<size_t, std::error_code> send_public_key_request(
      Channel *dst_channel, ClassicProtocolState *dst_protocol);

  static stdx::expected<size_t, std::error_code> send_public_key(
      Channel *dst_channel, ClassicProtocolState *dst_protocol,
      const std::string &public_key);

  static stdx::expected<size_t, std::error_code>
  send_plaintext_password_request(Channel *dst_channel,
                                  ClassicProtocolState *dst_protocol);

  static stdx::expected<size_t, std::error_code> send_plaintext_password(
      Channel *dst_channel, ClassicProtocolState *dst_protocol,
      const std::string &password);

  static stdx::expected<size_t, std::error_code> send_encrypted_password(
      Channel *dst_channel, ClassicProtocolState *dst_protocol,
      const std::string &password);

  static bool is_public_key_request(const std::string_view &data);
  static bool is_public_key(const std::string_view &data);
};

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

#endif
