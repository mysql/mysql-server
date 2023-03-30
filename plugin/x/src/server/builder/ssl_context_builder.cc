/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/server/builder/ssl_context_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "plugin/x/src/config/config.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/ssl_context.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

Ssl_context_builder::Ssl_config_local
Ssl_context_builder::get_mysqld_ssl_config() const {
  Ssl_config_local result;
  result.m_ssl_tls_version =
      Plugin_system_variables::get_system_variable("tls_version");

  result.m_ssl_key = Plugin_system_variables::get_system_variable("ssl_key");
  result.m_ssl_ca = Plugin_system_variables::get_system_variable("ssl_ca");
  result.m_ssl_capath =
      Plugin_system_variables::get_system_variable("ssl_capath");
  result.m_ssl_cert = Plugin_system_variables::get_system_variable("ssl_cert");
  result.m_ssl_cipher =
      Plugin_system_variables::get_system_variable("ssl_cipher");
  result.m_ssl_crl = Plugin_system_variables::get_system_variable("ssl_crl");
  result.m_ssl_crlpath =
      Plugin_system_variables::get_system_variable("ssl_crlpath");
  result.m_have_ssl =
      Plugin_system_variables::get_system_variable("have_ssl") == "YES";

  return result;
}

xpl::Ssl_config Ssl_context_builder::choose_ssl_config(
    const bool mysqld_have_ssl, const xpl::Ssl_config &mysqld_ssl,
    const xpl::Ssl_config &mysqlx_ssl) const {
  if (mysqlx_ssl.is_configured()) {
    log_info(ER_XPLUGIN_USING_SSL_CONF_FROM_MYSQLX);
    return mysqlx_ssl;
  }

  if (mysqld_have_ssl) {
    log_info(ER_XPLUGIN_USING_SSL_CONF_FROM_SERVER);
    return mysqld_ssl;
  }

  log_info(ER_XPLUGIN_FAILED_TO_USE_SSL_CONF);

  return xpl::Ssl_config();
}

void Ssl_context_builder::setup_ssl_context(
    iface::Ssl_context *ssl_context) const {
  auto ssl_config_from_mysqld_local = get_mysqld_ssl_config();
  auto &ssl_config_from_plugin = xpl::Plugin_system_variables::m_ssl_config;
  xpl::Ssl_config ssl_config_from_mysqld;

  ssl_config_from_mysqld.m_ssl_key = &ssl_config_from_mysqld_local.m_ssl_key[0];
  ssl_config_from_mysqld.m_ssl_ca = &ssl_config_from_mysqld_local.m_ssl_ca[0];
  ssl_config_from_mysqld.m_ssl_capath =
      &ssl_config_from_mysqld_local.m_ssl_capath[0];
  ssl_config_from_mysqld.m_ssl_cert =
      &ssl_config_from_mysqld_local.m_ssl_cert[0];
  ssl_config_from_mysqld.m_ssl_cipher =
      &ssl_config_from_mysqld_local.m_ssl_cipher[0];
  ssl_config_from_mysqld.m_ssl_crl = &ssl_config_from_mysqld_local.m_ssl_crl[0];
  ssl_config_from_mysqld.m_ssl_crlpath =
      &ssl_config_from_mysqld_local.m_ssl_crlpath[0];

  auto choosen_ssl_config =
      choose_ssl_config(ssl_config_from_mysqld_local.m_have_ssl,
                        ssl_config_from_mysqld, ssl_config_from_plugin);
  const char *crl = choosen_ssl_config.m_ssl_crl;
  const char *crlpath = choosen_ssl_config.m_ssl_crlpath;

  const bool ssl_setup_result = ssl_context->setup(
      ssl_config_from_mysqld_local.m_ssl_tls_version.c_str(),
      choosen_ssl_config.m_ssl_key, choosen_ssl_config.m_ssl_ca,
      choosen_ssl_config.m_ssl_capath, choosen_ssl_config.m_ssl_cert,
      choosen_ssl_config.m_ssl_cipher, crl, crlpath);

  if (ssl_setup_result) {
    log_info(ER_XPLUGIN_USING_SSL_FOR_TLS_CONNECTION, "OpenSSL");
  } else {
    log_info(ER_XPLUGIN_REFERENCE_TO_SECURE_CONN_WITH_XPLUGIN);
  }
}

std::unique_ptr<iface::Ssl_context> Ssl_context_builder::get_result_context()
    const {
  std::unique_ptr<iface::Ssl_context> result = std::make_unique<Ssl_context>();

  setup_ssl_context(result.get());

  return result;
}

}  // namespace xpl
