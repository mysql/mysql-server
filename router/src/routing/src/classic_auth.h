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

#ifndef ROUTING_CLASSIC_AUTH_INCLUDED
#define ROUTING_CLASSIC_AUTH_INCLUDED

#include <memory>  // unique_ptr
#include <string_view>
#include <system_error>

#include <openssl/ssl.h>

#include "classic_connection_base.h"
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

  static std::string_view strip_trailing_null(std::string_view s);

  static bool connection_has_public_key(
      MysqlRoutingClassicConnectionBase *connection);
};

#endif
