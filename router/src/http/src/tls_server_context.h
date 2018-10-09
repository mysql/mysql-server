/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_TLS_SERVER_CONTEXT_INCLUDED
#define ROUTER_TLS_SERVER_CONTEXT_INCLUDED

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <openssl/ssl.h>  // SSL_CTX, SSL_CTX_free

#include "openssl_version.h"

class TlsServerContext {
 public:
  static constexpr std::array<const char *, 9> unacceptable_cipher_spec{
      "!aNULL", "!eNULL", "!EXPORT", "!MD5",  "!DES",
      "!RC2",   "!RC4",   "!PSK",    "!SSLv3"};
  TlsServerContext(const std::string &cert_chain_file,
                   const std::string &private_key_file,
                   const std::string &ciphers, const std::string &dh_params);

  void set_min_version();
  void load_key_and_cert(const std::string &cert_chain_file,
                         const std::string &private_key_file);
  void init_tmp_ecdh();
  void init_tmp_dh(const std::string &dh_params);
  void set_cipher_list(const std::string &ciphers);
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  std::vector<std::string> cipher_list() const;
#endif

  SSL_CTX *get() const { return ssl_ctx_.get(); }

 protected:
  std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> ssl_ctx_;
};

class Tls {
 public:
  static std::vector<std::string> get_default_ciphers();
};

#endif
