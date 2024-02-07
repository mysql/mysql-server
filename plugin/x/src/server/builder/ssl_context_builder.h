/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_SERVER_BUILDER_SSL_CONTEXT_BUILDER_H_
#define PLUGIN_X_SRC_SERVER_BUILDER_SSL_CONTEXT_BUILDER_H_

#include <memory>
#include <string>

#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/variables/ssl_config.h"

namespace xpl {

class Ssl_context_builder {
 public:
  Ssl_context_builder() = default;

  std::unique_ptr<iface::Ssl_context> get_result_context() const;

 private:
  struct Ssl_config_local {
    std::string m_ssl_key;
    std::string m_ssl_ca;
    std::string m_ssl_capath;
    std::string m_ssl_cert;
    std::string m_ssl_cipher;
    std::string m_ssl_crl;
    std::string m_ssl_crlpath;
    std::string m_ssl_tls_version;
    bool m_have_ssl = false;
  };

  xpl::Ssl_config choose_ssl_config(const bool mysqld_have_ssl,
                                    const xpl::Ssl_config &mysqld_ssl,
                                    const xpl::Ssl_config &mysqlx_ssl) const;
  Ssl_config_local get_mysqld_ssl_config() const;
  void setup_ssl_context(iface::Ssl_context *ssl_context) const;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVER_BUILDER_SSL_CONTEXT_BUILDER_H_
