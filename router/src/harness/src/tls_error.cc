/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
#include "mysql/harness/tls_error.h"

#include <array>
#include <system_error>

#include <openssl/err.h>
#include <openssl/ssl.h>

static const std::error_category &tls_cert_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "tls_cert"; }
    std::string message(int ev) const override {
      switch (static_cast<TlsCertErrc>(ev)) {
        case TlsCertErrc::kNoRSACert:
          return "no RSA Cert";
        case TlsCertErrc::kNotACertificate:
          return "not a certificate";
        case TlsCertErrc::kRSAKeySizeToSmall:
          return "key-size too small";
      }

      return "unknown";
    }
  };

  static category_impl instance;
  return instance;
}

std::error_code make_error_code(TlsCertErrc e) {
  return {static_cast<int>(e), tls_cert_category()};
}

static const std::error_category &tls_ssl_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "tls_ssl"; }
    std::string message(int ev) const override {
      switch (static_cast<TlsErrc>(ev)) {
        case TlsErrc::kWantRead:
          return "want read";
        case TlsErrc::kWantWrite:
          return "want write";
        case TlsErrc::kZeroReturn:
          return "zero return";
      }

      return "unknown";
    }
  };

  static category_impl instance;
  return instance;
}

std::error_code make_error_code(TlsErrc e) {
  return {static_cast<int>(e), tls_ssl_category()};
}

const std::error_category &tls_err_category() noexcept {
  class category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "tls_err"; }
    std::string message(int ev) const override {
      std::array<char, 120> buf;
      return ERR_error_string(ev, buf.data());
    }
  };

  static category_impl instance;
  return instance;
}

std::error_code make_tls_error() {
  return {static_cast<int>(ERR_get_error()), tls_err_category()};
}

std::error_code make_tls_ssl_error(const SSL *ssl, int res) {
  const auto ssl_err = SSL_get_error(ssl, res);

  switch (ssl_err) {
    case SSL_ERROR_NONE:
      return {};
    case SSL_ERROR_SYSCALL:
      return {errno, std::generic_category()};
    case SSL_ERROR_SSL:
      return make_tls_error();
    default:
      // all others a SSL_ERROR_* ...
      return {ssl_err, tls_ssl_category()};
  }
}
