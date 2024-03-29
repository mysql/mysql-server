/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/sql_service/sql_service_interface.h"

#include <stddef.h>

#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_admin_session.h>
#include <mysqld_error.h>
#include "lex_string.h"
#include "my_dbug.h"
#include "my_systime.h"  // my_sleep()

static SERVICE_TYPE_NO_CONST(mysql_admin_session) * admin_session_factory;

/* Sql_service_interface constructor */
Sql_service_interface::Sql_service_interface(enum cs_text_or_binary cs_txt_bin,
                                             const CHARSET_INFO *charset)
    : m_plugin(nullptr), m_txt_or_bin(cs_txt_bin), m_charset(charset) {}

Sql_service_interface::~Sql_service_interface() {
  /* close server session */
  if (m_session) srv_session_close(m_session);

  /* if thread was initialized deinitialized it */
  if (m_plugin) srv_session_deinit_thread();
}

static void srv_session_error_handler(void *, unsigned int sql_errno,
                                      const char *err_msg) {
  switch (sql_errno) {
    case ER_CON_COUNT_ERROR:
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_SQL_SERVICE_MAX_CONN_ERROR_FROM_SERVER,
                   sql_errno);
      break;
    default:
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SQL_SERVICE_SERVER_ERROR_ON_CONN,
                   sql_errno, err_msg);
  }
}

int Sql_service_interface::open_session() {
  DBUG_TRACE;

  m_session = nullptr;
  /* open a server session after server is in operating state */
  if (!wait_for_session_server(SESSION_WAIT_TIMEOUT)) {
    m_session = admin_session_factory->open(srv_session_error_handler, nullptr);
    if (m_session == nullptr) return 1; /* purecov: inspected */
  } else {
    return 1; /* purecov: inspected */
  }

  if (configure_session()) {
    srv_session_close(m_session);
    m_session = nullptr;
    return 1;
  }

  return 0;
}

int Sql_service_interface::open_thread_session(void *plugin_ptr) {
  assert(plugin_ptr != nullptr);

  m_session = nullptr;
  /* open a server session after server is in operating state */
  if (!wait_for_session_server(SESSION_WAIT_TIMEOUT)) {
    /* initialize new thread to be used with server session */
    if (srv_session_init_thread(plugin_ptr)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_SQL_SERVICE_FAILED_TO_INIT_SESSION_THREAD);
      return 1;
      /* purecov: end */
    }

    m_session = admin_session_factory->open(srv_session_error_handler, nullptr);
    if (m_session == nullptr) {
      srv_session_deinit_thread();
      return 1;
    }
  } else {
    return 1; /* purecov: inspected */
  }

  if (configure_session()) {
    srv_session_close(m_session);
    m_session = nullptr;
    srv_session_deinit_thread();
    return 1;
  }

  m_plugin = plugin_ptr;
  return 0;
}

long Sql_service_interface::configure_session() {
  DBUG_TRACE;
  assert(m_session != nullptr);

  return execute_query("SET SESSION group_replication_consistency= EVENTUAL;");
}

long Sql_service_interface::execute_internal(
    Sql_resultset *rset, enum cs_text_or_binary cs_txt_bin,
    const CHARSET_INFO *cs_charset, COM_DATA cmd,
    enum enum_server_command cmd_type) {
  DBUG_TRACE;
  long err = 0;

  if (!m_session) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_SQL_SERVICE_COMM_SESSION_NOT_INITIALIZED,
                 cmd.com_query.query);
    return -1;
    /* purecov: end */
  }

  if (is_session_killed(m_session)) {
    /* purecov: begin inspected */
    LogPluginErr(INFORMATION_LEVEL,
                 ER_GRP_RPL_SQL_SERVICE_SERVER_SESSION_KILLED,
                 cmd.com_query.query);
    return -1;
    /* purecov: end */
  }

  Sql_service_context_base *ctx = new Sql_service_context(rset);

  /* execute sql command */
  if (command_service_run_command(
          m_session, cmd_type, &cmd, cs_charset,
          &Sql_service_context_base::sql_service_callbacks, cs_txt_bin, ctx)) {
    /* purecov: begin inspected */
    err = rset->sql_errno();

    if (err != 0) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SQL_SERVICE_FAILED_TO_RUN_SQL_QUERY,
                   cmd.com_query.query, rset->err_msg().c_str(),
                   rset->sql_errno());
    } else {
      if (is_session_killed(m_session) && rset->get_killed_status()) {
        LogPluginErr(INFORMATION_LEVEL,
                     ER_GRP_RPL_SQL_SERVICE_SERVER_SESSION_KILLED,
                     cmd.com_query.query);
        err = -1;
      } else {
        /* sql_errno is empty and session is alive */
        err = -2;
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_SQL_SERVICE_SERVER_INTERNAL_FAILURE,
                     cmd.com_query.query);
      }
    }

    delete ctx;
    return err;
    /* purecov: end */
  }

  err = rset->sql_errno();
  delete ctx;
  return err;
}

long Sql_service_interface::execute_query(std::string sql_string) {
  DBUG_TRACE;
  assert(sql_string.length() <= UINT_MAX);
  COM_DATA cmd;
  Sql_resultset rset;

  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = sql_string.c_str();
  cmd.com_query.length = static_cast<unsigned int>(sql_string.length());

  long err = execute_internal(&rset, m_txt_or_bin, m_charset, cmd, COM_QUERY);

  return err;
}

long Sql_service_interface::execute_query(std::string sql_string,
                                          Sql_resultset *rset,
                                          enum cs_text_or_binary cs_txt_or_bin,
                                          const CHARSET_INFO *cs_charset) {
  DBUG_TRACE;
  assert(sql_string.length() <= UINT_MAX);
  COM_DATA cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.com_query.query = sql_string.c_str();
  cmd.com_query.length = static_cast<unsigned int>(sql_string.length());

  long err = execute_internal(rset, cs_txt_or_bin, cs_charset, cmd, COM_QUERY);

  return err;
}

long Sql_service_interface::execute(COM_DATA cmd,
                                    enum enum_server_command cmd_type,
                                    Sql_resultset *rset,
                                    enum cs_text_or_binary cs_txt_or_bin,
                                    const CHARSET_INFO *cs_charset) {
  DBUG_TRACE;

  long err = execute_internal(rset, cs_txt_or_bin, cs_charset, cmd, cmd_type);

  return err;
}

int Sql_service_interface::wait_for_session_server(ulong total_timeout) {
  int number_of_tries = 0;
  ulong wait_retry_sleep = total_timeout * 1000000 / MAX_NUMBER_RETRIES;
  int err = 0;

  while (!srv_session_server_is_available()) {
    /* purecov: begin inspected */
    if (number_of_tries >= MAX_NUMBER_RETRIES) {
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_SQL_SERVICE_RETRIES_EXCEEDED_ON_SESSION_STATE);

      err = 1;
      break;
    } else {
      /*
        If we have more tries.
        Then sleep before new attempts are made.
      */
      my_sleep(wait_retry_sleep);
      ++number_of_tries;
    }
    /* purecov: end */
  }

  return err;
}

int Sql_service_interface::set_session_user(const char *user) {
  MYSQL_SECURITY_CONTEXT sc;
  if (thd_get_security_context(srv_session_info_get_thd(m_session), &sc)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_SQL_SERVICE_FAILED_TO_FETCH_SECURITY_CTX);
    return 1;
    /* purecov: end */
  }
  if (security_context_lookup(sc, user, "localhost", nullptr, nullptr)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_SQL_SERVICE_SERVER_ACCESS_DENIED_FOR_USER, user);
    return 1;
    /* purecov: end */
  }
  return 0;
}

bool Sql_service_interface::is_acl_disabled() {
  MYSQL_SECURITY_CONTEXT scontext;

  if (thd_get_security_context(srv_session_info_get_thd(m_session), &scontext))
    return false; /* purecov: inspected */

  MYSQL_LEX_CSTRING value;
  if (false != security_context_get_option(scontext, "priv_user", &value))
    return false; /* purecov: inspected */

  return 0 != value.length && nullptr != strstr(value.str, "skip-grants ");
}

bool sql_service_interface_init() {
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  my_h_service hadmin;
  if (!plugin_registry) return true;

  if (plugin_registry->acquire("mysql_admin_session", &hadmin)) {
    mysql_plugin_registry_release(plugin_registry);
    admin_session_factory = nullptr;
    return true;
  }

  admin_session_factory =
      reinterpret_cast<SERVICE_TYPE_NO_CONST(mysql_admin_session) *>(hadmin);
  mysql_plugin_registry_release(plugin_registry);
  return false;
}

bool sql_service_interface_deinit() {
  if (admin_session_factory) {
    SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
    if (!plugin_registry) return true;
    my_h_service hadmin;

    hadmin = reinterpret_cast<my_h_service>(admin_session_factory);
    plugin_registry->release(hadmin);
    admin_session_factory = nullptr;

    mysql_plugin_registry_release(plugin_registry);
  }
  return false;
}
