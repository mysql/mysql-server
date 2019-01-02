/*
   Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "plugin/x/src/xpl_system_variables.h"

#include <stdlib.h>

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/psi/psi_base.h"
#include "mysql/service_mysql_alloc.h"

namespace xpl {

int Plugin_system_variables::max_connections;
unsigned int Plugin_system_variables::port;
unsigned int Plugin_system_variables::min_worker_threads;
unsigned int Plugin_system_variables::idle_worker_thread_timeout;
unsigned int Plugin_system_variables::max_allowed_packet;
unsigned int Plugin_system_variables::connect_timeout;
char *Plugin_system_variables::socket;
unsigned int Plugin_system_variables::port_open_timeout;
char *Plugin_system_variables::bind_address;
uint32_t Plugin_system_variables::m_interactive_timeout;
uint32_t Plugin_system_variables::m_document_id_unique_prefix;
bool Plugin_system_variables::m_enable_hello_notice;

Ssl_config Plugin_system_variables::ssl_config;

std::vector<Plugin_system_variables::Value_changed_callback>
    Plugin_system_variables::m_callbacks;

void Plugin_system_variables::clean_callbacks() { m_callbacks.clear(); }

void Plugin_system_variables::registry_callback(
    Value_changed_callback callback) {
  m_callbacks.push_back(callback);
}

const char *Plugin_system_variables::get_system_variable_impl(
    const char *cnf_option, const char *env_variable,
    const char *compile_option) {
  if (NULL != cnf_option) {
    return cnf_option;
  }

  const char *variable_from_env = env_variable ? getenv(env_variable) : NULL;

  if (NULL != variable_from_env) return variable_from_env;

  return compile_option;
}

void Plugin_system_variables::setup_system_variable_from_env_or_compile_opt(
    char *&cnf_option, const char *env_variable, const char *compile_option) {
  char *value_old = cnf_option;
  const char *result =
      get_system_variable_impl(cnf_option, env_variable, compile_option);

  if (NULL != result)
    cnf_option = my_strdup(PSI_NOT_INSTRUMENTED, const_cast<char *>(result),
                           MYF(MY_WME));
  else
    cnf_option = NULL;

  if (NULL != value_old) my_free(value_old);
}

Ssl_config::Ssl_config()
    : ssl_key(NULL),
      ssl_ca(NULL),
      ssl_capath(NULL),
      ssl_cert(NULL),
      ssl_cipher(NULL),
      ssl_crl(NULL),
      ssl_crlpath(NULL),
      m_null_char(0) {}

bool Ssl_config::is_configured() const {
  return has_value(ssl_key) || has_value(ssl_ca) || has_value(ssl_capath) ||
         has_value(ssl_cert) || has_value(ssl_cipher) || has_value(ssl_crl) ||
         has_value(ssl_crlpath);
}

bool Ssl_config::has_value(const char *ptr) const { return ptr && *ptr; }

}  // namespace xpl
