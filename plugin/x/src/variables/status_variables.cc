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

// simple (no measurement attributes supported) common Variable metric
// callback
static void get_metric_simple_variable(void *measurement_context,
                                       measurement_delivery_callback_t delivery,
                                       void *delivery_context) {
  assert(measurement_context != nullptr);
  assert(delivery != nullptr);
  ngs::Common_status_variables::Variable &var =
      *(ngs::Common_status_variables::Variable *)measurement_context;
  const int64_t value = var.load();
  delivery->value_int64(delivery_context, value);
}

static void get_metric_ssl_sess_accept(void * /* measurement_context */,
                                       measurement_delivery_callback_t delivery,
                                       void *delivery_context) {
  assert(delivery != nullptr);
  auto server = modules::Module_mysqlx::get_instance_server();
  if (!server.container()) return;
  if (!server->ssl_context()) return;

  auto &context = server->ssl_context()->options();
  const int64_t value = context.ssl_sess_accept();
  delivery->value_int64(delivery_context, value);
}

static void get_metric_ssl_sess_accept_good(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  assert(delivery != nullptr);
  auto server = modules::Module_mysqlx::get_instance_server();
  if (!server.container()) return;
  if (!server->ssl_context()) return;

  auto &context = server->ssl_context()->options();
  const int64_t value = context.ssl_sess_accept_good();
  delivery->value_int64(delivery_context, value);
}

//
// Telemetry metric sources instrumented within the X plugin
// are being defined below.
//

static PSI_metric_info_v1 xpl_metrics[] = {
    {"aborted_clients", "",
     "The number of clients that were disconnected because of an input or "
     "output error (Mysqlx_aborted_clients)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_aborted_clients},
    {"bytes_received", METRIC_UNIT_BYTES,
     "The total number of bytes received through the network, measured before "
     "decompression (Mysqlx_bytes_received)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_bytes_received},
    {"bytes_received_compressed_payload", METRIC_UNIT_BYTES,
     "The number of bytes received as compressed message payloads, measured "
     "before decompression (Mysqlx_bytes_received_compressed_payload)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance()
          .m_bytes_received_compressed_payload},
    {"bytes_received_uncompressed_frame", METRIC_UNIT_BYTES,
     "The number of bytes received as compressed message payloads, measured "
     "after decompression (Mysqlx_bytes_received_uncompressed_frame)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance()
          .m_bytes_received_uncompressed_frame},
    {"bytes_sent", METRIC_UNIT_BYTES,
     "The total number of bytes sent through the network (Mysqlx_bytes_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_bytes_sent},
    {"bytes_sent_compressed_payload", METRIC_UNIT_BYTES,
     "The number of bytes sent as compressed message payloads, measured after "
     "compression (Mysqlx_bytes_sent_compressed_payload)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_bytes_sent_compressed_payload},
    {"bytes_sent_uncompressed_frame", METRIC_UNIT_BYTES,
     "The number of bytes sent as compressed message payloads, measured before "
     "compression (Mysqlx_bytes_sent_uncompressed_frame)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_bytes_sent_uncompressed_frame},
    {"connection_accept_errors", "",
     "The number of connections which have caused accept errors "
     "(Mysqlx_connection_accept_errors)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance()
          .m_connection_accept_errors_count},
    {"connection_errors", "",
     "The number of connections which have caused errors "
     "(Mysqlx_connection_errors)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_connection_errors_count},
    {"connections_accepted", "",
     "The number of connections which have been accepted "
     "(Mysqlx_connections_accepted)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_accepted_connections_count},
    {"connections_closed", "",
     "The number of connections which have been closed "
     "(Mysqlx_connections_closed)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_closed_connections_count},
    {"connections_rejected", "",
     "The number of connections which have been rejected "
     "(Mysqlx_connections_rejected)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_rejected_connections_count},
    {"crud_create_view", "",
     "The number of create view requests received (Mysqlx_crud_create_view)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_create_view},
    {"crud_delete", "",
     "The number of delete requests received (Mysqlx_crud_delete)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_delete},
    {"crud_drop_view", "",
     "The number of drop view requests received (Mysqlx_crud_drop_view)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_drop_view},
    {"crud_find", "", "The number of find requests received (Mysqlx_crud_find)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_find},
    {"crud_insert", "",
     "The number of insert requests received (Mysqlx_crud_insert)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_insert},
    {"crud_modify_view", "",
     "The number of modify view requests received (Mysqlx_crud_modify_view)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_modify_view},
    {"crud_update", "",
     "The number of update requests received (Mysqlx_crud_update)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_crud_update},
    {"cursor_close", "",
     "The number of cursor-close messages received (Mysqlx_cursor_close)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_cursor_close},
    {"cursor_fetch", "",
     "The number of cursor-fetch messages received (Mysqlx_cursor_fetch)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_cursor_fetch},
    {"cursor_open", "",
     "The number of cursor-open messages received (Mysqlx_cursor_open)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_cursor_open},
    {"errors_sent", "",
     "The number of errors sent to clients (Mysqlx_errors_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_errors_sent},
    {"errors_unknown_message_type", "",
     "The number of unknown message types that have been received "
     "(Mysqlx_errors_unknown_message_type)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_errors_unknown_message_type},
    {"expect_close", "",
     "The number of expectation blocks closed (Mysqlx_expect_close)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_expect_close},
    {"expect_open", "",
     "The number of expectation blocks opened (Mysqlx_expect_open)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_expect_open},
    {"init_error", "",
     "The number of errors during initialisation (Mysqlx_init_error)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_init_errors_count},
    {"messages_sent", "",
     "The total number of messages of all types sent to clients "
     "(Mysqlx_messages_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_messages_sent},
    {"notice_global_sent", "",
     "The number of global notifications sent to clients "
     "(Mysqlx_notice_global_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_notice_global_sent},
    {"notice_other_sent", "",
     "The number of other types of notices sent back to clients "
     "(Mysqlx_notice_other_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_notice_other_sent},
    {"notice_warning_sent", "",
     "The number of warning notices sent back to clients "
     "(Mysqlx_notice_warning_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_notice_warning_sent},
    {"notified_by_group_replication", "",
     "Number of Group Replication notifications sent to clients "
     "(Mysqlx_notified_by_group_replication)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_notified_by_group_replication},
    {"prep_deallocate", "",
     "The number of prepared-statement-deallocate messages received "
     "(Mysqlx_prep_deallocate)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_prep_deallocate},
    {"prep_execute", "",
     "The number of prepared-statement-execute messages received "
     "(Mysqlx_prep_execute)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_prep_execute},
    {"prep_prepare", "",
     "The number of prepared-statement messages received (Mysqlx_prep_prepare)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_prep_prepare},
    {"rows_sent", "",
     "The number of rows sent back to clients (Mysqlx_rows_sent)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_rows_sent},
    {"sessions", "",
     "The number of sessions that have been opened (Mysqlx_sessions)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_sessions_count},
    {"sessions_accepted", "",
     "The number of session attempts which have been accepted "
     "(Mysqlx_sessions_accepted)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_accepted_sessions_count},
    {"sessions_closed", "",
     "The number of sessions that have been closed (Mysqlx_sessions_closed)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_closed_sessions_count},
    {"sessions_fatal_error", "",
     "The number of sessions that have closed with a fatal error "
     "(Mysqlx_sessions_fatal_error)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_sessions_fatal_errors_count},
    {"sessions_killed", "",
     "The number of sessions which have been killed (Mysqlx_sessions_killed)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_killed_sessions_count},
    {"sessions_rejected", "",
     "The number of session attempts which have been rejected "
     "(Mysqlx_sessions_rejected)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_rejected_sessions_count},
    {"ssl_accepts", "",
     "The number of accepted SSL connections (Mysqlx_ssl_accepts)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_ssl_sess_accept, nullptr},
    {"ssl_finished_accepts", "",
     "The number of successful SSL connections to the server "
     "(Mysqlx_ssl_finished_accepts)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_ssl_sess_accept_good, nullptr},
    {"worker_threads", "",
     "The number of worker threads available (Mysqlx_worker_threads)",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_worker_thread_count},
    {"worker_threads_active", "",
     "The number of worker threads currently used "
     "(Mysqlx_worker_threads_active)",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_active_worker_thread_count}};

static PSI_metric_info_v1 stmt_metrics[] = {
    {"create_collection", "",
     "The number of create collection statements received "
     "(Mysqlx_stmt_create_collection)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_create_collection},
    {"create_collection_index", "",
     "The number of create collection index statements received "
     "(Mysqlx_stmt_create_collection_index)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_create_collection_index},
    {"disable_notices", "",
     "The number of disable notice statements received "
     "(Mysqlx_stmt_disable_notices)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_disable_notices},
    {"drop_collection", "",
     "The number of drop collection statements received "
     "(Mysqlx_stmt_drop_collection)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_drop_collection},
    {"drop_collection_index", "",
     "The number of drop collection index statements received "
     "(Mysqlx_stmt_drop_collection_index)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_drop_collection_index},
    {"enable_notices", "",
     "The number of enable notice statements received "
     "(Mysqlx_stmt_enable_notices)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_enable_notices},
    {"ensure_collection", "",
     "The number of ensure collection statements received "
     "(Mysqlx_stmt_ensure_collection)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_ensure_collection},
    {"execute_mysqlx", "",
     "The number of StmtExecute messages received with namespace set to "
     "mysqlx (Mysqlx_stmt_execute_mysqlx)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_execute_mysqlx},
    {"execute_sql", "",
     "The number of StmtExecute requests received for the SQL namespace "
     "(Mysqlx_stmt_execute_sql)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_execute_sql},
    {"get_collection_options", "",
     "The number of get collection object statements received "
     "(Mysqlx_stmt_get_collection_options)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_get_collection_options},
    {"kill_client", "",
     "The number of kill client statements received (Mysqlx_stmt_kill_client)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_kill_client},
    {"list_clients", "",
     "The number of list client statements received (Mysqlx_stmt_list_clients)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_list_clients},
    {"list_notices", "",
     "The number of list notice statements received (Mysqlx_stmt_list_notices)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_list_notices},
    {"list_objects", "",
     "The number of list object statements received (Mysqlx_stmt_list_objects)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_list_objects},
    {"modify_collection_options", "",
     "The number of modify collection options statements received "
     "(Mysqlx_stmt_modify_collection_options)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance()
          .m_stmt_modify_collection_options},
    {"ping", "", "The number of ping statements received (Mysqlx_stmt_ping)",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_simple_variable,
     &xpl::Global_status_variables::instance().m_stmt_ping}};

PSI_meter_info_v1 Plugin_status_variables::m_xpl_meter[] = {
    {"mysql.x", "MySql X plugin metrics", 10, 0, 0, xpl_metrics,
     std::size(xpl_metrics)},
    {"mysql.x.stmt", "MySql X plugin statement statistics", 10, 0, 0,
     stmt_metrics, std::size(stmt_metrics)},
};

size_t Plugin_status_variables::get_meter_count() {
  return std::size(m_xpl_meter);
}

}  // namespace xpl
