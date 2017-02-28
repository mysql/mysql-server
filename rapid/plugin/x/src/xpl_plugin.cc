/*
   Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <my_config.h>

#include <mysql/plugin.h>
#include <mysql_version.h>

#include "xpl_session.h"
#include "xpl_system_variables.h"
#include "xpl_server.h"
#include "xpl_performance_schema.h"
#include "mysqlx_version.h"
#include "xpl_log.h"

#include <stdio.h>                            // Solaris header file bug.
#include <limits>

#define BYTE(X)  (X)
#define KBYTE(X) ((X) * 1024)
#define MBYTE(X) ((X) * 1024 * 1024)
#define GBYTE(X) ((X) * 1024 * 1024 * 1024)

namespace
{

typedef void (*Xpl_status_variable_get)(THD *, st_mysql_show_var *, char *);

char *xpl_func_ptr(Xpl_status_variable_get callback)
{
  union
  {
    char *ptr;
    Xpl_status_variable_get callback;
  } ptr_cast;

  ptr_cast.callback = callback;

  return ptr_cast.ptr;
}

void exit_hook()
{
  google::protobuf::ShutdownProtobufLibrary();
}

} // namespace


/*
  Start the plugin: start webservers

  SYNOPSIS
    xpl_plugin_init()
    p plugin handle

  RETURN
     0  success
     1  error
 */
int xpl_plugin_init(MYSQL_PLUGIN p)
{
  static bool atexit_installed = false;
  if (!atexit_installed)
  {
    atexit_installed = true;
    atexit(exit_hook);
  }

  xpl::Plugin_system_variables::clean_callbacks();

  xpl_init_performance_schema();

  return xpl::Server::main(p);
}

/*
  Shutdown the plugin: stop webservers

  SYNOPSIS
    xpl_plugin_deinit()
    p plugin handle

  RETURN
     0  success
     1  error
 */
int xpl_plugin_deinit(MYSQL_PLUGIN p)
{
  return xpl::Server::exit(p);
}


static struct st_mysql_daemon xpl_plugin_info ={
  MYSQL_DAEMON_INTERFACE_VERSION
};


static MYSQL_SYSVAR_UINT(port, xpl::Plugin_system_variables::port,
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
    "Port on which X Plugin is going to accept incoming connections.",
    NULL, NULL, MYSQLX_TCP_PORT, 1, std::numeric_limits<uint16>::max(), 0);

static MYSQL_SYSVAR_INT(max_connections, xpl::Plugin_system_variables::max_connections,
    PLUGIN_VAR_OPCMDARG,
    "Maximum number of concurrent X protocol connections. Actual number of connections is also affected by the general max_connections.",
    NULL, NULL, 100, 1, std::numeric_limits<unsigned short>::max(), 0);

static MYSQL_SYSVAR_UINT(min_worker_threads, xpl::Plugin_system_variables::min_worker_threads,
    PLUGIN_VAR_OPCMDARG,
    "Minimal number of worker threads.",
    NULL, &xpl::Plugin_system_variables::update_func<unsigned int>, 2U, 1, 100, 0);

static MYSQL_SYSVAR_UINT(idle_worker_thread_timeout, xpl::Plugin_system_variables::idle_worker_thread_timeout,
    PLUGIN_VAR_OPCMDARG,
    "Time after which an idle worker thread is terminated (in seconds).",
    NULL, &xpl::Plugin_system_variables::update_func<unsigned int>, 60, 0, 60 * 60, 0);

static MYSQL_SYSVAR_UINT(max_allowed_packet, xpl::Plugin_system_variables::max_allowed_packet,
    PLUGIN_VAR_OPCMDARG,
    "Size of largest message that client is going to handle.",
    NULL, &xpl::Plugin_system_variables::update_func<unsigned int>, MBYTE(1), BYTE(512), GBYTE(1), 0);

static MYSQL_SYSVAR_UINT(connect_timeout, xpl::Plugin_system_variables::connect_timeout,
    PLUGIN_VAR_OPCMDARG,
    "Maximum allowed waiting time for connection to setup a session (in seconds).",
    NULL, &xpl::Plugin_system_variables::update_func<unsigned int>, 30, 1, 1000000000, 0);

static MYSQL_SYSVAR_STR(ssl_key, xpl::Plugin_system_variables::ssl_config.ssl_key,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
      "X509 key in PEM format.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(ssl_ca, xpl::Plugin_system_variables::ssl_config.ssl_ca,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
      "CA file in PEM format.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(ssl_capath, xpl::Plugin_system_variables::ssl_config.ssl_capath,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
      "CA directory.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(ssl_cert, xpl::Plugin_system_variables::ssl_config.ssl_cert,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
      "X509 cert in PEM format.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(ssl_cipher, xpl::Plugin_system_variables::ssl_config.ssl_cipher,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
      "SSL cipher to use.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(ssl_crl, xpl::Plugin_system_variables::ssl_config.ssl_crl,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_MEMALLOC,
      "Certificate revocation list.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(ssl_crlpath, xpl::Plugin_system_variables::ssl_config.ssl_crlpath,
      PLUGIN_VAR_READONLY,
      "Certificate revocation list path.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(socket, xpl::Plugin_system_variables::socket,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
      "X Plugin's unix socket for local connection.", NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(bind_address, xpl::Plugin_system_variables::bind_address,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
      "Address to which X Plugin should bind the TCP socket.", NULL, NULL, "*");

static MYSQL_SYSVAR_UINT(port_open_timeout, xpl::Plugin_system_variables::port_open_timeout,
      PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG ,
      "How long X Plugin is going to retry binding of server socket (in case of failure)",
      NULL, &xpl::Plugin_system_variables::update_func<unsigned int>, 0, 0, 120, 0);

static struct st_mysql_sys_var* xpl_plugin_system_variables[]= {
  MYSQL_SYSVAR(port),
  MYSQL_SYSVAR(max_connections),
  MYSQL_SYSVAR(min_worker_threads),
  MYSQL_SYSVAR(idle_worker_thread_timeout),
  MYSQL_SYSVAR(max_allowed_packet),
  MYSQL_SYSVAR(connect_timeout),
  MYSQL_SYSVAR(ssl_key),
  MYSQL_SYSVAR(ssl_ca),
  MYSQL_SYSVAR(ssl_capath),
  MYSQL_SYSVAR(ssl_cert),
  MYSQL_SYSVAR(ssl_cipher),
  MYSQL_SYSVAR(ssl_crl),
  MYSQL_SYSVAR(ssl_crlpath),
  MYSQL_SYSVAR(socket),
  MYSQL_SYSVAR(bind_address),
  MYSQL_SYSVAR(port_open_timeout),
  NULL
};

#define SESSION_STATUS_VARIABLE_ENTRY_LONGLONG(NAME, METHOD)                 \
    { MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                   \
      xpl_func_ptr(xpl::Server::common_status_variable<long long, &METHOD>), \
      SHOW_FUNC,                                                             \
      SHOW_SCOPE_GLOBAL }

#define GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG(NAME, METHOD)                         \
    { MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                          \
      xpl_func_ptr(xpl::Server::global_status_variable_server<long long, &METHOD>), \
      SHOW_FUNC,                                                                    \
      SHOW_SCOPE_GLOBAL }

#define GLOBAL_SSL_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)            \
    { MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                              \
      xpl_func_ptr(xpl::Server::global_status_variable<TYPE, &METHOD>), \
      SHOW_FUNC,                                                        \
      SHOW_SCOPE_GLOBAL }

#define SESSION_SSL_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)            \
    { MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                               \
      xpl_func_ptr(xpl::Server::session_status_variable<TYPE, &METHOD>), \
      SHOW_FUNC,                                                         \
      SHOW_SCOPE_GLOBAL }

#define SESSION_SSL_STATUS_VARIABLE_ENTRY_ARRAY(NAME, METHOD)      \
    { MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                         \
      xpl_func_ptr(xpl::Server::session_status_variable<&METHOD>), \
      SHOW_FUNC,                                                   \
      SHOW_SCOPE_GLOBAL }

#define GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY(NAME, TYPE, METHOD)                            \
    { MYSQLX_STATUS_VARIABLE_PREFIX(NAME),                                                 \
      xpl_func_ptr(xpl::Server::global_status_variable_server_with_return<TYPE, &METHOD>), \
      SHOW_FUNC,                                                                           \
      SHOW_SCOPE_GLOBAL }

static struct st_mysql_show_var xpl_plugin_status[]=
{
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_execute_sql",        xpl::Common_status_variables::m_stmt_execute_sql),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_execute_xplugin",    xpl::Common_status_variables::m_stmt_execute_xplugin),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_execute_mysqlx",     xpl::Common_status_variables::m_stmt_execute_mysqlx),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_update",             xpl::Common_status_variables::m_crud_update),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_delete",             xpl::Common_status_variables::m_crud_delete),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_find",               xpl::Common_status_variables::m_crud_find),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_insert",             xpl::Common_status_variables::m_crud_insert),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_create_view",        xpl::Common_status_variables::m_crud_create_view),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_modify_view",        xpl::Common_status_variables::m_crud_modify_view),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("crud_drop_view",          xpl::Common_status_variables::m_crud_drop_view),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("expect_open",             xpl::Common_status_variables::m_expect_open),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("expect_close",            xpl::Common_status_variables::m_expect_close),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_create_collection",       xpl::Common_status_variables::m_stmt_create_collection),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_create_collection_index", xpl::Common_status_variables::m_stmt_create_collection_index),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_drop_collection",         xpl::Common_status_variables::m_stmt_drop_collection),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_ensure_collection",       xpl::Common_status_variables::m_stmt_ensure_collection),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_drop_collection_index",   xpl::Common_status_variables::m_stmt_drop_collection_index),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_list_objects",       xpl::Common_status_variables::m_stmt_list_objects),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_enable_notices",     xpl::Common_status_variables::m_stmt_enable_notices),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_disable_notices",    xpl::Common_status_variables::m_stmt_disable_notices),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_list_notices",       xpl::Common_status_variables::m_stmt_list_notices),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_list_clients",       xpl::Common_status_variables::m_stmt_list_clients),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_kill_client",        xpl::Common_status_variables::m_stmt_kill_client),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("stmt_ping",               xpl::Common_status_variables::m_stmt_ping),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("bytes_sent",              xpl::Common_status_variables::m_bytes_sent),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("bytes_received",          xpl::Common_status_variables::m_bytes_received),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("errors_sent",             xpl::Common_status_variables::m_errors_sent),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("rows_sent",               xpl::Common_status_variables::m_rows_sent),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("notice_warning_sent",     xpl::Common_status_variables::m_notice_warning_sent),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("notice_other_sent",       xpl::Common_status_variables::m_notice_other_sent),
  SESSION_STATUS_VARIABLE_ENTRY_LONGLONG("errors_unknown_message_type",    xpl::Common_status_variables::m_errors_unknown_message_type),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("sessions",                 xpl::Global_status_variables::m_sessions_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("sessions_closed",          xpl::Global_status_variables::m_closed_sessions_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("sessions_fatal_error",     xpl::Global_status_variables::m_sessions_fatal_errors_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("init_error",               xpl::Global_status_variables::m_init_errors_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("sessions_accepted",        xpl::Global_status_variables::m_accepted_sessions_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("sessions_rejected",        xpl::Global_status_variables::m_rejected_sessions_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("sessions_killed",          xpl::Global_status_variables::m_killed_sessions_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("connections_closed",       xpl::Global_status_variables::m_closed_connections_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("connections_accepted",     xpl::Global_status_variables::m_accepted_connections_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("connections_rejected",     xpl::Global_status_variables::m_rejected_connections_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("connection_accept_errors", xpl::Global_status_variables::m_connection_accept_errors_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("connection_errors",        xpl::Global_status_variables::m_connection_errors_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("worker_threads",           xpl::Global_status_variables::m_worker_thread_count),
  GLOBAL_STATUS_VARIABLE_ENTRY_LONGLONG("worker_threads_active",    xpl::Global_status_variables::m_active_worker_thread_count),

  SESSION_SSL_STATUS_VARIABLE_ENTRY_ARRAY("ssl_cipher_list", xpl::Client::get_status_ssl_cipher_list),
  SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_active",       bool,        ngs::IOptions_session::active_tls),
  SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_cipher",       std::string, ngs::IOptions_session::ssl_cipher),
  SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_version",      std::string, ngs::IOptions_session::ssl_version),
  SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_verify_depth", long,        ngs::IOptions_session::ssl_verify_depth),
  SESSION_SSL_STATUS_VARIABLE_ENTRY("ssl_verify_mode",  long,        ngs::IOptions_session::ssl_verify_mode),
  GLOBAL_SSL_STATUS_VARIABLE_ENTRY("ssl_ctx_verify_depth",  long,        ngs::IOptions_context::ssl_ctx_verify_depth),
  GLOBAL_SSL_STATUS_VARIABLE_ENTRY("ssl_ctx_verify_mode",   long,        ngs::IOptions_context::ssl_ctx_verify_mode),
  GLOBAL_SSL_STATUS_VARIABLE_ENTRY("ssl_finished_accepts",  long,        ngs::IOptions_context::ssl_sess_accept_good),
  GLOBAL_SSL_STATUS_VARIABLE_ENTRY("ssl_accepts",           long,        ngs::IOptions_context::ssl_sess_accept),
  GLOBAL_SSL_STATUS_VARIABLE_ENTRY("ssl_server_not_after",  std::string, ngs::IOptions_context::ssl_server_not_after),
  GLOBAL_SSL_STATUS_VARIABLE_ENTRY("ssl_server_not_before", std::string, ngs::IOptions_context::ssl_server_not_before),

  GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY("socket",  std::string, xpl::Server::get_socket_file),
  GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY("port",    std::string, xpl::Server::get_tcp_port),
  GLOBAL_CUSTOM_STATUS_VARIABLE_ENTRY("address", std::string, xpl::Server::get_tcp_bind_address),

  { NULL, NULL, SHOW_BOOL, SHOW_SCOPE_GLOBAL}
};


mysql_declare_plugin(xpl)
{
  MYSQL_DAEMON_PLUGIN,
  &xpl_plugin_info,
  MYSQLX_PLUGIN_NAME,
  "Oracle Corp",
  "X Plugin for MySQL",
  PLUGIN_LICENSE_GPL,
  xpl_plugin_init,              /* init       */
  xpl_plugin_deinit,            /* deinit     */
  MYSQLX_PLUGIN_VERSION,        /* version    */
  xpl_plugin_status,            /* status var */
  xpl_plugin_system_variables,  /* system var */
  NULL,                         /* options    */
  0                             /* flags      */
}
mysql_declare_plugin_end;
