/* Copyright (c) 2021, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

/*
  In case of kerberos authentication we need to fill user name as kerberos user
  name if it is empty. We need to fill user name inside mysql->user, clients
  uses my_strdup directly to create new string. To use MySQL alloc functions we
  need to include "/mysql/service_mysql_alloc.h". Inside service_mysql_alloc.h
  there is #define which forces all dynamic plugins to use MySQL malloc function
  via services. Client side plugin cannot use any services as of now.   Code
  check in service_mysql_alloc.h #ifdef MYSQL_DYNAMIC_PLUGIN #define my_strdup
  mysql_malloc_service->my_strdup #else extern char *my_strdup(PSI_memory_key
  key, const char *from, myf_t flags); #endif Client authentication plugin
  defines MYSQL_DYNAMIC_PLUGIN. And this forces to use always my_strdup via
  services. To use native direct my_strdup, we need to undefine
  MYSQL_DYNAMIC_PLUGIN. And again define MYSQL_DYNAMIC_PLUGIN once correct
  my_strdup are declared. service_mysql_alloc.h should provide proper fix like
  Instead of #ifdef MYSQL_DYNAMIC_PLUGIN
  #ifdef  MYSQL_DYNAMIC_PLUGIN  &&   ! MYSQL_CLIENT_PLUGIN
*/
#undef MYSQL_DYNAMIC_PLUGIN
#include <mysql/service_mysql_alloc.h>
#define MYSQL_DYNAMIC_PLUGIN

#include "auth_kerberos_client_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mysql.h>
#include <mysql/client_plugin.h>
#include <sql_common.h>
#include "auth_kerberos_client_io.h"
#include "gssapi_utility.h"
#include "log_client.h"

Logger_client *g_logger_client{nullptr};

Kerberos_plugin_client::Kerberos_plugin_client(MYSQL_PLUGIN_VIO *vio,
                                               MYSQL *mysql)
    : m_vio{vio}, m_mysql{mysql} {}

/*
  This method try to get kerberos TGT if user name and password are not
  empty. If method fails, We should not start kerberos authentication
  process. Otherwise kerberos authentication may consume existing TGT and
  authentication process will start.
*/
bool Kerberos_plugin_client::obtain_store_credentials() {
  if (!m_kerberos_client) {
    m_kerberos_client = std::unique_ptr<Kerberos_client>(
        new Kerberos_client(m_user_principal_name.c_str(), m_password.c_str()));
  }
  log_client_dbg("Obtaining TGT TGS tickets from kerberos.");
  if (!m_kerberos_client->obtain_store_credentials()) {
    log_client_error(
        "Plug-in has failed to obtained kerberos TGT, authentication process "
        "will be aborted. Please provide valid configuration, user name and "
        "password.");
    return false;
  } else {
    return true;
  }
}

Kerberos_plugin_client::~Kerberos_plugin_client() {}

void Kerberos_plugin_client::set_mysql_account_name(
    std::string mysql_account_name) {
  std::string cc_user_name;
  std::stringstream log_client_stream;

  if (mysql_account_name.empty()) {
    Kerberos_client kerberos_client{m_user_principal_name.c_str(),
                                    m_password.c_str()};
    log_client_dbg("Getting user name from Kerberos credential cache.");
    kerberos_client.get_upn(cc_user_name);
  } else {
    log_client_dbg("MySQL user account name is not empty.");
    return;
  }
  if (!cc_user_name.empty()) {
    log_client_dbg(
        "Setting MySQL account name using Kerberos credential cache default "
        "UPN.");
    /*
      MySQL clients/lib uses my_free, my_strdup my_* string function for string
      management. We also need to use same methods as these are used/free inside
      libmysqlclient
    */
    if (m_mysql->user) {
      my_free(m_mysql->user);
      m_mysql->user = nullptr;
    }
    size_t pos = std::string::npos;
    /* Remove realm */
    if ((pos = cc_user_name.find("@")) != std::string::npos) {
      log_client_dbg("Trimming realm from upn.");
      cc_user_name.erase(pos, cc_user_name.length() - pos + 1);
    }
    m_mysql->user =
        my_strdup(PSI_NOT_INSTRUMENTED, cc_user_name.c_str(), MYF(MY_WME));
    log_client_stream.str("");
    log_client_stream << "Setting MySQL account name as: "
                      << cc_user_name.c_str();
    log_client_dbg(log_client_stream.str());
  } else {
    log_client_dbg(
        "Kerberos credential cache default UPN empty, Setting MySQL account "
        "name from OS name.");
  }
}

void Kerberos_plugin_client::set_upn_info(std::string name, std::string pwd) {
  /*
    For some cases MySQL user name will be empty as user name is supposed to
    come from kerberos TGT. Get kerberos user name from TGT and assign this as
    MySQL user name.
  */
  m_password = pwd;
  if (!name.empty()) {
    create_upn(name);
  }
}

void Kerberos_plugin_client::create_upn(std::string account_name) {
  if (!m_as_user_relam.empty()) {
    m_user_principal_name = account_name + "@" + m_as_user_relam;
  }
}

bool Kerberos_plugin_client::authenticate() {
  Gssapi_client gssapi_client{m_service_principal, m_vio};
  if (gssapi_client.authenticate()) {
    return CR_OK;
  } else {
    return CR_ERROR;
  }
}

bool Kerberos_plugin_client::read_spn_realm_from_server() {
  Kerberos_client_io m_io{m_vio};
  return m_io.read_spn_realm_from_server(m_service_principal, m_as_user_relam);
}

#ifdef __clang__
// Clang UBSAN false positive?
// Call to function through pointer to incorrect function type
static int kerberos_authenticate(MYSQL_PLUGIN_VIO *vio,
                                 MYSQL *mysql) SUPPRESS_UBSAN;
static int initialize_plugin(char *, size_t, int, va_list) SUPPRESS_UBSAN;
static int deinitialize_plugin() SUPPRESS_UBSAN;
#endif  // __clang__

static int kerberos_authenticate(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql) {
  std::stringstream log_client_stream;
  Kerberos_plugin_client client{vio, mysql};
  client.set_mysql_account_name(mysql->user);
  if (!client.read_spn_realm_from_server()) {
    log_client_info(
        "kerberos_authenticate: Failed to read service principal from MySQL "
        "server.");
    /* Callee has already logged the messages. */
    return CR_ERROR;
  }
  client.set_upn_info(mysql->user, mysql->passwd);
  if (!client.obtain_store_credentials()) {
    log_client_error(
        "kerberos_authenticate: Kerberos obtain store credentials failed. ");
    return CR_ERROR;
  }
  if (client.authenticate()) {
    log_client_stream.str("");
    log_client_stream << "Kerberos authentication has succeeded for the user: "
                      << mysql->user;
    log_client_info(log_client_stream.str().c_str());
    return CR_OK;
  } else {
    log_client_stream.str("");
    log_client_stream << "Kerberos authentication has failed for the user: "
                      << mysql->user;
    log_client_error(log_client_stream.str().c_str());
    return CR_ERROR;
  }
}

static int initialize_plugin(char *, size_t, int, va_list) {
  g_logger_client = new Logger_client();
  const char *opt = getenv("AUTHENTICATION_KERBEROS_CLIENT_LOG");
  int opt_val = opt ? atoi(opt) : 0;
  if (opt && opt_val > 0 && opt_val < 6) {
    g_logger_client->set_log_level(static_cast<log_client_level>(opt_val));
  }
  return 0;
}

static int deinitialize_plugin() {
  delete g_logger_client;
  g_logger_client = nullptr;
  return 0;
}

mysql_declare_client_plugin(AUTHENTICATION) "authentication_kerberos_client",
    MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE, "Kerberos Client Authentication Plugin",
    {0, 1, 0}, "PROPRIETARY", nullptr, initialize_plugin, deinitialize_plugin,
    nullptr, kerberos_authenticate, nullptr mysql_end_client_plugin;
