/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PLUGIN_X_SRC_VARIABLES_SSL_CONFIG_H_
#define PLUGIN_X_SRC_VARIABLES_SSL_CONFIG_H_

namespace xpl {

struct Ssl_config {
  Ssl_config()
      : m_ssl_key(nullptr),
        m_ssl_ca(nullptr),
        m_ssl_capath(nullptr),
        m_ssl_cert(nullptr),
        m_ssl_cipher(nullptr),
        m_ssl_crl(nullptr),
        m_ssl_crlpath(nullptr),
        m_null_char(0) {}

  bool is_configured() const {
    return has_value(m_ssl_key) || has_value(m_ssl_ca) ||
           has_value(m_ssl_capath) || has_value(m_ssl_cert) ||
           has_value(m_ssl_cipher) || has_value(m_ssl_crl) ||
           has_value(m_ssl_crlpath);
  }

 public:
  char *m_ssl_key = nullptr;
  char *m_ssl_ca = nullptr;
  char *m_ssl_capath = nullptr;
  char *m_ssl_cert = nullptr;
  char *m_ssl_cipher = nullptr;
  char *m_ssl_crl = nullptr;
  char *m_ssl_crlpath = nullptr;

 private:
  static bool has_value(const char *ptr) { return ptr && *ptr; }

  char m_null_char;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_VARIABLES_SSL_CONFIG_H_
