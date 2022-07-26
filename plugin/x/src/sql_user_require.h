/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_SQL_USER_REQUIRE_H_
#define PLUGIN_X_SRC_SQL_USER_REQUIRE_H_

#include <string>

#include "plugin/x/src/interface/ssl_session_options.h"
#include "plugin/x/src/sql_data_context.h"

namespace xpl {

class Sql_user_require {
 public:
  std::string ssl_type;
  std::string ssl_cipher;
  std::string ssl_x509_issuer;
  std::string ssl_x509_subject;

  ngs::Error_code validate(const iface::Ssl_session_options &options) const;

 private:
  const static std::string SSL_TYPE_NONE;
  const static std::string SSL_TYPE_SSL;
  const static std::string SSL_TYPE_X509;
  const static std::string SSL_TYPE_SPECIFIC;

  ngs::Error_code check_ssl(const iface::Ssl_session_options &options) const;
  ngs::Error_code check_x509(const iface::Ssl_session_options &options) const;
  ngs::Error_code check_specific(
      const iface::Ssl_session_options &options) const;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SQL_USER_REQUIRE_H_
