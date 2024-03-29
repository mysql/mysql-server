/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_XSSL_CONFIG_H_
#define PLUGIN_X_CLIENT_XSSL_CONFIG_H_

#include <cstring>
#include <string>

namespace xcl {

class Ssl_config {
 public:
  enum class Mode {
    Ssl_disabled,
    Ssl_preferred,
    Ssl_required,
    Ssl_verify_ca,
    Ssl_verify_identity
  };

  enum class Mode_ssl_fips {
    Ssl_fips_mode_off,
    Ssl_fips_mode_on,
    Ssl_fips_mode_strict,
  };

 public:
  Ssl_config() = default;

  Ssl_config(const std::string &ssl_key, const std::string &ssl_ca,
             const std::string &ssl_ca_path, const std::string &ssl_cert,
             const std::string &ssl_cipher, const std::string &ssl_crl,
             const std::string &ssl_crl_path,
             const std::string &ssl_tls_version, const Mode mode,
             const Mode_ssl_fips ssl_fips_mode)
      : m_key(ssl_key),
        m_ca(ssl_ca),
        m_ca_path(ssl_ca_path),
        m_cert(ssl_cert),
        m_cipher(ssl_cipher),
        m_crl(ssl_crl),
        m_crl_path(ssl_crl_path),
        m_tls_version(ssl_tls_version),
        m_mode(mode),
        m_ssl_fips_mode(ssl_fips_mode) {}

  bool is_configured() const { return Mode::Ssl_disabled != m_mode; }

  bool does_mode_requires_ssl() const {
    switch (m_mode) {
      case Mode::Ssl_required:   // fall-through
      case Mode::Ssl_verify_ca:  // fall-through
      case Mode::Ssl_verify_identity:
        return true;

      default:
        return false;
    }
  }

  bool does_mode_requires_ca() const {
    return Mode::Ssl_verify_ca == m_mode || Mode::Ssl_verify_identity == m_mode;
  }

  bool is_ca_configured() const {
    if (m_ca.empty() && m_ca_path.empty()) return false;

    return true;
  }

  std::string m_key;
  std::string m_ca;
  std::string m_ca_path;
  std::string m_cert;
  std::string m_cipher;
  std::string m_crl;
  std::string m_crl_path;
  std::string m_tls_version;

  Mode m_mode{Mode::Ssl_preferred};
  Mode_ssl_fips m_ssl_fips_mode{Mode_ssl_fips::Ssl_fips_mode_off};
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XSSL_CONFIG_H_
