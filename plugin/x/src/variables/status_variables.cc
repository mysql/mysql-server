/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "plugin/x/src/variables/status_variables.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "plugin/x/generated/mysqlx_version.h"
#include "plugin/x/src/client.h"
#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/interface/ssl_context_options.h"
#include "plugin/x/src/interface/ssl_session_options.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/mysql_show_variable_wrapper.h"
#include "plugin/x/src/ngs/common_status_variables.h"
#include "plugin/x/src/ssl_session_options.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"

namespace xpl {

ngs::Server_properties Plugin_status_variables::m_properties;

namespace {

std::string get_property(const ngs::Server_property_ids id) {
  if (Plugin_status_variables::m_properties.empty()) return "";

  if (0 == Plugin_status_variables::m_properties.count(id))
    return ngs::PROPERTY_NOT_CONFIGURED;

  return Plugin_status_variables::m_properties.at(id);
}

std::string get_socket_file() {
  return get_property(ngs::Server_property_ids::k_unix_socket);
}

std::string get_tcp_port() {
  return get_property(ngs::Server_property_ids::k_tcp_port);
}

std::string get_tcp_bind_address() {
  return get_property(ngs::Server_property_ids::k_tcp_bind_address);
}

inline char *xpl_func_ptr(mysql_show_var_func callback) {
  return reinterpret_cast<char *>(callback);
}

template <typename ReturnType, ReturnType (xpl::Client::*method)() const>
int session_status_variable(THD *thd, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  auto server = modules::Module_mysqlx::get_instance_server();

  if (server.container()) {
    MUTEX_LOCK(lock, server->get_client_exit_mutex());
    auto client =
        std::dynamic_pointer_cast<xpl::Client>(server->get_client(thd));

    if (client) {
      ReturnType result = ((*client).*method)();
      mysqld::xpl_show_var(var).assign(result);
    }
  }
  return 0;
}

template <typename ReturnType,
          ReturnType (iface::Ssl_session_options::*method)() const>
int session_status_variable(THD *thd, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  auto server = modules::Module_mysqlx::get_instance_server();

  if (server.container()) {
    MUTEX_LOCK(lock, server->get_client_exit_mutex());
    auto client =
        std::dynamic_pointer_cast<xpl::Client>(server->get_client(thd));

    if (client) {
      ReturnType result =
          (Ssl_session_options(&client->connection()).*method)();
      mysqld::xpl_show_var(var).assign(result);
    }
  }
  return 0;
}

template <void (iface::Server::*method)(SHOW_VAR *)>
int global_status_variable(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  auto server = modules::Module_mysqlx::get_instance_server();

  if (server.container()) {
    (server.container()->*method)(var);
  }
  return 0;
}

template <typename ReturnType, ReturnType (*method)()>
int global_status_variable_custom_callback(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  ReturnType result = method();

  mysqld::xpl_show_var(var).assign(result);

  return 0;
}

template <typename ReturnType, xpl::Global_status_variables::Variable
                                   xpl::Global_status_variables::*variable>
int global_status_variable_server(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  ReturnType result =
      (xpl::Global_status_variables::instance().*variable).load();
  mysqld::xpl_show_var(var).assign(result);
  return 0;
}

template <typename ReturnType, ngs::Common_status_variables::Variable
                                   ngs::Common_status_variables::*variable>
int common_status_variable(THD *thd, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  auto server = modules::Module_mysqlx::get_instance_server();

  if (server.container()) {
    MUTEX_LOCK(lock, server->get_client_exit_mutex());
    auto client =
        std::dynamic_pointer_cast<xpl::Client>(server->get_client(thd));

    if (client) {
      // Status can be queried from different thread than client is bound to.
      // User can reset the session by sending SessionReset, to be secure for
      // released session pointer, the code needs to hold current session by
      // shared_ptr.
      auto client_session(client->session_shared_ptr());

      if (client_session) {
        auto &common_status = client_session->get_status_variables();
        ReturnType result = (common_status.*variable).load();
        mysqld::xpl_show_var(var).assign(result);
      }
      return 0;
    }
  }

  ngs::Common_status_variables &common_status =
      xpl::Global_status_variables::instance();
  ReturnType result = (common_status.*variable).load();
  mysqld::xpl_show_var(var).assign(result);

  return 0;
}

template <typename ReturnType,
          ReturnType (iface::Ssl_context_options::*method)()>
int global_status_variable(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_UNDEF;
  var->value = buff;

  auto server = modules::Module_mysqlx::get_instance_server();

  if (!server.container()) return 0;

  if (!server->ssl_context()) return 0;

  auto &context = server->ssl_context()->options();
  auto context_method = method;  // workaround on VC compiler internal error
  ReturnType result = (context.*context_method)();

  mysqld::xpl_show_var(var).assign(result);
  return 0;
}

}  // namespace

#define SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(NAME, METHOD)               \
  {                                                                        \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                   \
        xpl_func_ptr(common_status_variable<int64_t, &METHOD>), SHOW_FUNC, \
        SHOW_SCOPE_GLOBAL                                                  \
  }

#define GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(NAME, METHOD)            \
  {                                                                    \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                               \
        xpl_func_ptr(global_status_variable_server<int64_t, &METHOD>), \
        SHOW_FUNC, SHOW_SCOPE_GLOBAL                                   \
  }

#define SESSION_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)                \
  {                                                                      \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                 \
        xpl_func_ptr(session_status_variable<TYPE, &METHOD>), SHOW_FUNC, \
        SHOW_SCOPE_GLOBAL                                                \
  }

#define GLOBAL_SSL_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)            \
  {                                                                     \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                \
        xpl_func_ptr(global_status_variable<TYPE, &METHOD>), SHOW_FUNC, \
        SHOW_SCOPE_GLOBAL                                               \
  }

#define SESSION_SSL_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)            \
  {                                                                      \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                 \
        xpl_func_ptr(session_status_variable<TYPE, &METHOD>), SHOW_FUNC, \
        SHOW_SCOPE_GLOBAL                                                \
  }

#define SESSION_SSL_STATUS_VARIABLE_ENTRY_ARRAY(NAME, METHOD)      \
  {                                                                \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                           \
        xpl_func_ptr(session_status_variable<&METHOD>), SHOW_FUNC, \
        SHOW_SCOPE_GLOBAL                                          \
  }

#define GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)              \
  {                                                                          \
    MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                     \
        xpl_func_ptr(global_status_variable_custom_callback<TYPE, &METHOD>), \
        SHOW_FUNC, SHOW_SCOPE_GLOBAL                                         \
  }

struct SHOW_VAR Plugin_status_variables::m_plugin_status_variables[] = {
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_execute_sql", ngs::Common_status_variables::m_stmt_execute_sql),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_execute_xplugin",
        ngs::Common_status_variables::m_stmt_execute_xplugin),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_execute_mysqlx",
        ngs::Common_status_variables::m_stmt_execute_mysqlx),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_update", ngs::Common_status_variables::m_crud_update),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_delete", ngs::Common_status_variables::m_crud_delete),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_find", ngs::Common_status_variables::m_crud_find),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_insert", ngs::Common_status_variables::m_crud_insert),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_create_view", ngs::Common_status_variables::m_crud_create_view),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_modify_view", ngs::Common_status_variables::m_crud_modify_view),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "crud_drop_view", ngs::Common_status_variables::m_crud_drop_view),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "prep_prepare", ngs::Common_status_variables::m_prep_prepare),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "prep_execute", ngs::Common_status_variables::m_prep_execute),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "prep_deallocate", ngs::Common_status_variables::m_prep_deallocate),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "cursor_open", ngs::Common_status_variables::m_cursor_open),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "cursor_close", ngs::Common_status_variables::m_cursor_close),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "cursor_fetch", ngs::Common_status_variables::m_cursor_fetch),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "expect_open", ngs::Common_status_variables::m_expect_open),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "expect_close", ngs::Common_status_variables::m_expect_close),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_create_collection",
        ngs::Common_status_variables::m_stmt_create_collection),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_create_collection_index",
        ngs::Common_status_variables::m_stmt_create_collection_index),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_drop_collection",
        ngs::Common_status_variables::m_stmt_drop_collection),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_ensure_collection",
        ngs::Common_status_variables::m_stmt_ensure_collection),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_modify_collection_options",
        ngs::Common_status_variables::m_stmt_modify_collection_options),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_get_collection_options",
        ngs::Common_status_variables::m_stmt_get_collection_options),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_drop_collection_index",
        ngs::Common_status_variables::m_stmt_drop_collection_index),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_list_objects", ngs::Common_status_variables::m_stmt_list_objects),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_enable_notices",
        ngs::Common_status_variables::m_stmt_enable_notices),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_disable_notices",
        ngs::Common_status_variables::m_stmt_disable_notices),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_list_notices", ngs::Common_status_variables::m_stmt_list_notices),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_list_clients", ngs::Common_status_variables::m_stmt_list_clients),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_kill_client", ngs::Common_status_variables::m_stmt_kill_client),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "stmt_ping", ngs::Common_status_variables::m_stmt_ping),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "bytes_sent", ngs::Common_status_variables::m_bytes_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "bytes_received", ngs::Common_status_variables::m_bytes_received),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "bytes_sent_compressed_payload",
        ngs::Common_status_variables::m_bytes_sent_compressed_payload),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "bytes_sent_uncompressed_frame",
        ngs::Common_status_variables::m_bytes_sent_uncompressed_frame),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "bytes_received_compressed_payload",
        ngs::Common_status_variables::m_bytes_received_compressed_payload),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "bytes_received_uncompressed_frame",
        ngs::Common_status_variables::m_bytes_received_uncompressed_frame),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "errors_sent", ngs::Common_status_variables::m_errors_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "rows_sent", ngs::Common_status_variables::m_rows_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "messages_sent", ngs::Common_status_variables::m_messages_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "notice_warning_sent",
        ngs::Common_status_variables::m_notice_warning_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "notice_other_sent", ngs::Common_status_variables::m_notice_other_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "notice_global_sent",
        ngs::Common_status_variables::m_notice_global_sent),
    SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(
        "errors_unknown_message_type",
        ngs::Common_status_variables::m_errors_unknown_message_type),
    SESSION_STATUS_VARIABLE_ENTRY(
        "compression_algorithm", std::string,
        xpl::Client::get_status_compression_algorithm),
    SESSION_STATUS_VARIABLE_ENTRY("compression_level", std::string,
                                  xpl::Client::get_status_compression_level),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "sessions", xpl::Global_status_variables::m_sessions_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "sessions_closed",
        xpl::Global_status_variables::m_closed_sessions_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "sessions_fatal_error",
        xpl::Global_status_variables::m_sessions_fatal_errors_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "init_error", xpl::Global_status_variables::m_init_errors_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "sessions_accepted",
        xpl::Global_status_variables::m_accepted_sessions_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "sessions_rejected",
        xpl::Global_status_variables::m_rejected_sessions_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "sessions_killed",
        xpl::Global_status_variables::m_killed_sessions_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "connections_closed",
        xpl::Global_status_variables::m_closed_connections_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "connections_accepted",
        xpl::Global_status_variables::m_accepted_connections_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "connections_rejected",
        xpl::Global_status_variables::m_rejected_connections_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "connection_accept_errors",
        xpl::Global_status_variables::m_connection_accept_errors_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "connection_errors",
        xpl::Global_status_variables::m_connection_errors_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "worker_threads", xpl::Global_status_variables::m_worker_thread_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "worker_threads_active",
        xpl::Global_status_variables::m_active_worker_thread_count),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "aborted_clients", xpl::Global_status_variables::m_aborted_clients),
    GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(
        "notified_by_group_replication",
        xpl::Global_status_variables::m_notified_by_group_replication),

    SESSION_STATUS_VARIABLE_ENTRY("ssl_cipher_list", std::string,
                                  xpl::Client::get_status_ssl_cipher_list),
    SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_active", bool,
                                      iface::Ssl_session_options::active_tls),
    SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_cipher", std::string,
                                      iface::Ssl_session_options::ssl_cipher),
    SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_version", std::string,
                                      iface::Ssl_session_options::ssl_version),
    SESSION_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_verify_depth", int64_t,
        iface::Ssl_session_options::ssl_verify_depth),
    SESSION_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_verify_mode", int64_t,
        iface::Ssl_session_options::ssl_verify_mode),
    GLOBAL_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_ctx_verify_depth", int64_t,
        iface::Ssl_context_options::ssl_ctx_verify_depth),
    GLOBAL_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_ctx_verify_mode", int64_t,
        iface::Ssl_context_options::ssl_ctx_verify_mode),
    GLOBAL_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_finished_accepts", int64_t,
        iface::Ssl_context_options::ssl_sess_accept_good),
    GLOBAL_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_accepts", int64_t, iface::Ssl_context_options::ssl_sess_accept),
    GLOBAL_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_server_not_after", std::string,
        iface::Ssl_context_options::ssl_server_not_after),
    GLOBAL_SSL_STATUS_VARIABLE_ENTRY(
        "ssl_server_not_before", std::string,
        iface::Ssl_context_options::ssl_server_not_before),

    GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY("socket", std::string, get_socket_file),
    GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY("port", std::string, get_tcp_port),
    GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY("address", std::string,
                                        get_tcp_bind_address),

    {nullptr, nullptr, SHOW_BOOL, SHOW_SCOPE_GLOBAL}};

}  // namespace xpl
