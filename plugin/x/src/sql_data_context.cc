/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/sql_data_context.h"

#include <algorithm>
#include <cinttypes>
#include <sstream>
#include <string>

#include "mysql/components/my_service.h"
#include "mysql/components/services/mysql_admin_session.h"
#include "mysql/plugin.h"
#include "mysql/service_command.h"
#include "sql/debug_sync.h"

#include "plugin/x/src/helper/get_system_variable.h"
#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/mysql_variables.h"
#include "plugin/x/src/notices.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_user_require.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_resultset.h"

#include "my_systime.h"  // my_sleep() NOLINT(build/include_subdir)

namespace xpl {

namespace details {

class Admin_session_factory {
 public:
  MYSQL_SESSION create(srv_session_error_cb error_cb, void *context) {
    if (!m_admin_session.is_valid()) return nullptr;

    return m_admin_session->open(error_cb, context);
  }

 private:
  class Registry_holder {
   public:
    Registry_holder() : m_registry{mysql_plugin_registry_acquire()} {}
    ~Registry_holder() { mysql_plugin_registry_release(m_registry); }

    SERVICE_TYPE(registry) *const m_registry;
  };

  Registry_holder m_registry_holder;
  my_service<SERVICE_TYPE(mysql_admin_session)> m_admin_session{
      "mysql_admin_session", m_registry_holder.m_registry};
};

}  // namespace details

ngs::Error_code Sql_data_context::init(const int client_port,
                                       const Connection_type type,
                                       const bool is_admin) {
  ngs::Error_code error = init(is_admin);
  if (error) return error;

  if ((error = set_connection_type(type))) return error;

  if (0 != srv_session_info_set_client_port(m_mysql_session, client_port))
    return ngs::Error_code(ER_X_SESSION, "Could not set session client port");

  return ngs::Error_code();
}

ngs::Error_code Sql_data_context::init(const bool is_admin) {
  if (is_admin) {
    details::Admin_session_factory factory;
    m_mysql_session =
        factory.create(&Sql_data_context::default_completion_handler, this);

  } else {
    m_mysql_session =
        srv_session_open(&Sql_data_context::default_completion_handler, this);
  }

  log_debug(
      "sqlsession init: %p [%i]", m_mysql_session,
      m_mysql_session ? srv_session_info_get_session_id(m_mysql_session) : -1);
  if (!m_mysql_session) {
    if (ER_SERVER_ISNT_AVAILABLE == m_last_sql_errno)
      return ngs::Error_code(ER_SERVER_ISNT_AVAILABLE, "Server API not ready");

    if (ER_CON_COUNT_ERROR == m_last_sql_errno)
      return ngs::Error_code(m_last_sql_errno, m_last_sql_error);

    if (ER_CON_COUNT_ERROR == m_last_sql_errno)
      return ngs::Error_code(m_last_sql_errno, m_last_sql_error);

    log_error(ER_XPLUGIN_FAILED_TO_OPEN_INTERNAL_SESSION);
    return ngs::Error_code(ER_X_SESSION, "Could not open session");
  }

  m_pre_authenticate_event_fired = false;
  m_authentication_code = ngs::Error_code();

  return ngs::Error_code();
}

void Sql_data_context::deinit() {
  if (m_mysql_session) {
    if (m_pre_authenticate_event_fired && !attach()) {
      if (!m_authentication_code) {
        // In case of successfull login, connect event has been generated
        // earlier. Time for generating disconnect event.
        modules::Module_mysqlx::get_instance_services()->m_audit_api->emit(
            get_thd(), MYSQL_AUDIT_CONNECTION_DISCONNECT);
      } else {
        modules::Module_mysqlx::get_instance_services()
            ->m_audit_api->emit_with_errorcode(get_thd(),
                                               MYSQL_AUDIT_CONNECTION_CONNECT,
                                               m_authentication_code.error);
      }
    }

    m_pre_authenticate_event_fired = false;
    m_authentication_code = ngs::Error_code();

    srv_session_detach(m_mysql_session);

    log_debug("sqlsession deinit: %p [%i]", m_mysql_session,
              srv_session_info_get_session_id(m_mysql_session));

    srv_session_close(m_mysql_session);
    m_mysql_session = nullptr;
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(delete_current_thread)();

  PSI_thread *psi =
      PSI_THREAD_CALL(new_thread)(KEY_thread_x_worker, 0, nullptr, 0);
  PSI_THREAD_CALL(set_thread_os_id)(psi);
  PSI_THREAD_CALL(set_thread)(psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */
}  // namespace xpl

static void kill_completion_handler(void *, unsigned int sql_errno,
                                    const char *err_msg) {
  log_warning(ER_XPLUGIN_CLIENT_KILL_MSG, sql_errno, err_msg);
}

bool Sql_data_context::kill() {
  if (srv_session_server_is_available()) {
    log_debug("sqlsession init (for kill): %p [%i]", m_mysql_session,
              m_mysql_session ? srv_session_info_get_session_id(m_mysql_session)
                              : -1);
    MYSQL_SESSION session = srv_session_open(kill_completion_handler, nullptr);
    bool ok = false;
    if (session) {
      MYSQL_SECURITY_CONTEXT scontext;

      if (thd_get_security_context(srv_session_info_get_thd(session),
                                   &scontext)) {
        log_warning(ER_XPLUGIN_FAILED_TO_GET_SECURITY_CTX);
      } else {
        const char *user = MYSQL_SESSION_USER;
        const char *host = MYSQLXSYS_HOST;
        if (security_context_lookup(scontext, user, host, nullptr, nullptr)) {
          log_warning(ER_XPLUGIN_FAILED_TO_SWITCH_SECURITY_CTX, user);
        } else {
          COM_DATA data;
          Callback_command_delegate deleg;
          Query_string_builder qb;
          qb.put("KILL ").put(mysql_session_id());

          memset(&data, 0, sizeof(data));
          data.com_query.query = qb.get().c_str();
          data.com_query.length = static_cast<unsigned int>(qb.get().length());

          if (!command_service_run_command(
                  session, COM_QUERY, &data, mysqld::get_default_charset(),
                  deleg.callbacks(), deleg.representation(), &deleg)) {
            if (!deleg.get_error())
              ok = true;
            else
              log_debug("Kill client: %i %s", deleg.get_error().error,
                        deleg.get_error().message.c_str());
          }
        }
      }
      srv_session_close(session);
    }
    return ok;
  }
  return false;
}

ngs::Error_code Sql_data_context::set_connection_type(
    const Connection_type type) {
  enum_vio_type vio_type = Connection_type_helper::convert_type(type);

  if (NO_VIO_TYPE == vio_type)
    return ngs::Error(ER_X_SESSION, "Connection type not known. type=%" PRIu32,
                      static_cast<uint32_t>(type));

  if (0 != srv_session_info_set_connection_type(m_mysql_session, vio_type))
    return ngs::Error_code(ER_X_SESSION,
                           "Could not set session connection type");

  return ngs::Error_code();
}

bool Sql_data_context::wait_api_ready(std::function<bool()> exiting) {
  bool result = is_api_ready();

  while (!result && !exiting()) {
    my_sleep(250000);  // wait for 0.25s

    result = is_api_ready();
  }

  return result;
}

Sql_data_context::~Sql_data_context() {
  if (m_mysql_session)
    log_debug("sqlsession deinit~: %p [%i]", m_mysql_session,
              srv_session_info_get_session_id(m_mysql_session));
  if (m_mysql_session && srv_session_close(m_mysql_session))
    log_warning(ER_XPLUGIN_FAILED_TO_CLOSE_SQL_SESSION);
}

void Sql_data_context::switch_to_local_user(const std::string &user) {
  ngs::Error_code error =
      switch_to_user(user.c_str(), "localhost", nullptr, nullptr);
  if (error) throw error;
}

ngs::Error_code Sql_data_context::authenticate_internal(
    const char *user, const char *host, const char *ip, const char *db,
    const std::string &passwd,
    const iface::Authentication &account_verification,
    bool allow_expired_passwords) {
  std::string authenticated_user_name = get_authenticated_user_name();
  std::string authenticated_user_host = get_authenticated_user_host();

  ngs::Error_code error =
      switch_to_user(MYSQL_SESSION_USER, MYSQLXSYS_HOST, nullptr, nullptr);

  if (error) {
    const char *session_user = MYSQL_SESSION_USER;
    log_error(ER_XPLUGIN_FAILED_TO_SWITCH_CONTEXT, session_user);
    return error;
  }

  if (!is_acl_disabled()) {
    error = account_verification.authenticate_account(
        authenticated_user_name, authenticated_user_host, passwd);
  }

  ngs::Error_code switch_error = switch_to_user(user, host, ip, db);

  if (error.error == ER_MUST_CHANGE_PASSWORD_LOGIN) {
    m_password_expired = true;

    // password is expired, client doesn't support it and server wants us to
    // disconnect these users
    if (error.severity == ngs::Error_code::FATAL && !allow_expired_passwords)
      return error;
  } else {
    if (error) return error;
  }

  error = switch_error;

  if (!error) {
    if (db && *db) {
      COM_DATA data;

      data.com_init_db.db_name = db;
      data.com_init_db.length =
          static_cast<unsigned long>(strlen(db));  // NOLINT(runtime/int)

      Callback_command_delegate callback_delegate;
      if (command_service_run_command(
              m_mysql_session, COM_INIT_DB, &data,
              mysqld::get_default_charset(), callback_delegate.callbacks(),
              callback_delegate.representation(), &callback_delegate))
        return ngs::Error_code(ER_NO_DB_ERROR, "Could not set database");
      error = callback_delegate.get_error();
    }

    std::string user_name = get_user_name();
    std::string host_or_ip = get_host_or_ip();

#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_account)
    (user_name.c_str(), static_cast<int>(user_name.length()),
     host_or_ip.c_str(), static_cast<int>(host_or_ip.length()));
#endif  // HAVE_PSI_THREAD_INTERFACE

    return error;
  }

  log_error(ER_XPLUGIN_FAILED_TO_SWITCH_CONTEXT, user);

  return error;
}

ngs::Error_code Sql_data_context::authenticate(
    const char *user, const char *host, const char *ip, const char *db,
    const std::string &passwd,
    const iface::Authentication &account_verification,
    bool allow_expired_passwords) {
  m_password_expired = false;

  m_using_password = passwd.size() > 0;

  ngs::Error_code error = switch_to_user(user, host, ip, db);

  if (error) error = ngs::SQLError_access_denied();

  m_authentication_code = error;

  if (!m_pre_authenticate_event_fired) {
    attach();
    int audit_result =
        modules::Module_mysqlx::get_instance_services()->m_audit_api->emit(
            static_cast<void *>(get_thd()),
            MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE);
    detach();

    if (audit_result != 0)
      return ngs::Fatal(ER_AUDIT_API_ABORT, my_get_err_msg(ER_AUDIT_API_ABORT),
                        "MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE",
                        audit_result);

    m_pre_authenticate_event_fired = true;
  }

  if (error) return error;

  error = authenticate_internal(user, host, ip, db, passwd,
                                account_verification, allow_expired_passwords);

  m_authentication_code = error;

  if (!error) {
    attach();
    int audit_result =
        modules::Module_mysqlx::get_instance_services()->m_audit_api->emit(
            static_cast<void *>(get_thd()), MYSQL_AUDIT_CONNECTION_CONNECT);
    detach();

    if (audit_result != 0)
      m_authentication_code = error =
          ngs::Fatal(ER_AUDIT_API_ABORT, my_get_err_msg(ER_AUDIT_API_ABORT),
                     "MYSQL_AUDIT_CONNECTION_CONNECT", audit_result);
  }

  return error;
}

template <typename Result_type>
bool get_security_context_value(MYSQL_THD thd, const char *option,
                                Result_type *result) {
  MYSQL_SECURITY_CONTEXT scontext;

  if (thd_get_security_context(thd, &scontext)) return false;

  return false == security_context_get_option(scontext, option, result);
}

bool Sql_data_context::is_acl_disabled() {
  MYSQL_LEX_CSTRING value{"", 0};

  if (get_security_context_value(get_thd(), "priv_user", &value)) {
    return 0 != value.length && nullptr != strstr(value.str, "skip-grants ");
  }

  return false;
}

bool Sql_data_context::has_authenticated_user_a_super_priv() const {
  my_svc_bool value = 0;
  if (get_security_context_value(get_thd(), "privilege_super", &value))
    return value != 0;

  return false;
}

std::string Sql_data_context::get_user_name() const {
  MYSQL_LEX_CSTRING result{"", 0};

  if (get_security_context_value(get_thd(), "user", &result)) return result.str;

  return "";
}

std::string Sql_data_context::get_host_or_ip() const {
  MYSQL_LEX_CSTRING result{"", 0};

  if (get_security_context_value(get_thd(), "host_or_ip", &result))
    return result.str;

  return "";
}

std::string Sql_data_context::get_authenticated_user_name() const {
  MYSQL_LEX_CSTRING result{"", 0};

  if (get_security_context_value(get_thd(), "priv_user", &result))
    return result.str;

  return "";
}

std::string Sql_data_context::get_authenticated_user_host() const {
  MYSQL_LEX_CSTRING result{"", 0};

  if (get_security_context_value(get_thd(), "priv_host", &result))
    return result.str;

  return "";
}

ngs::Error_code Sql_data_context::switch_to_user(const char *username,
                                                 const char *hostname,
                                                 const char *address,
                                                 const char *db) {
  MYSQL_SECURITY_CONTEXT scontext;

  if (thd_get_security_context(get_thd(), &scontext))
    return ngs::Fatal(ER_X_SERVICE_ERROR,
                      "Error getting security context for session");

  attach();
  CONDITIONAL_SYNC_POINT("xpl_switch_to_user_enter");

  // security_context_lookup - doesn't make a copy of username, hostname, addres
  //                           or db thus we need to make a copy of them
  //                           and pass our pointers to security_context_lookup.
  //                           If we change pass different pointers than in last
  //                           invocation of `switch_to_user`, then it might
  //                           cause an invalid memory access while listing all
  //                           sessions. Thus we must use memory regions that
  //                           can hold values of maximum length.
  memset(m_username, 0, sizeof(m_username));
  memset(m_hostname, 0, sizeof(m_hostname));
  memset(m_address, 0, sizeof(m_address));
  memset(m_db, 0, sizeof(m_db));

  if (username) {
    strncpy(m_username, username, sizeof(m_username) - 1);
  }

  if (hostname) {
    strncpy(m_hostname, hostname, sizeof(m_hostname) - 1);
  }

  if (address) {
    strncpy(m_address, address, sizeof(m_address) - 1);
  }

  if (db) {
    strncpy(m_db, db, sizeof(m_db) - 1);
  }

  CONDITIONAL_SYNC_POINT("xpl_switch_to_user_execute");
  detach();

  log_debug("Switching security context to user %s@%s [%s]", username, hostname,
            address);

  if (security_context_lookup(scontext, m_username, m_hostname, m_address,
                              m_db)) {
    log_debug("Unable to switch security context to user %s@%s [%s]", username,
              hostname, address);
    return ngs::Fatal(ER_X_SERVICE_ERROR, "Unable to switch context to user %s",
                      username);
  }

  return ngs::Success();
}

ngs::Error_code Sql_data_context::execute_kill_sql_session(
    uint64_t mysql_session_id) {
  Query_string_builder qb;
  qb.put("KILL ").put(mysql_session_id);
  Empty_resultset rset;
  return execute(qb.get().data(), qb.get().length(), &rset);
}

void Sql_data_context::default_completion_handler(void *ctx,
                                                  unsigned int sql_errno,
                                                  const char *err_msg) {
  Sql_data_context *self = reinterpret_cast<Sql_data_context *>(ctx);
  self->m_last_sql_errno = sql_errno;
  self->m_last_sql_error = err_msg ? err_msg : "";
}

bool Sql_data_context::is_killed() const {
  const auto kill = thd_killed(get_thd());
  DBUG_LOG("debug", "is_killed:" << kill);

  if (0 == kill) return false;

  return ER_QUERY_INTERRUPTED != kill;
}

bool Sql_data_context::is_api_ready() {
  return 0 != srv_session_server_is_available();
}

uint64_t Sql_data_context::mysql_session_id() const {
  return srv_session_info_get_session_id(m_mysql_session);
}

MYSQL_THD Sql_data_context::get_thd() const {
  return srv_session_info_get_thd(m_mysql_session);
}

ngs::Error_code Sql_data_context::execute(const char *sql, std::size_t sql_len,
                                          iface::Resultset *rset) {
  const auto error = execute_sql(sql, sql_len, rset);
  if (error) return error;

  if (m_password_expired) {
    // if a SQL command succeeded while password is expired, it means the user
    // probably changed the password
    // we run a command to check just in case... (some commands are still
    // allowed in expired password mode)
    Empty_resultset rs;
    static const std::string cmd("select 1");
    if (!execute_sql(cmd.c_str(), cmd.length(), &rs))
      m_password_expired = false;
  }
  return ngs::Success();
}

ngs::Error_code Sql_data_context::execute_sql(const char *sql,
                                              std::size_t sql_len,
                                              iface::Resultset *rset) {
  COM_DATA data;
  memset(&data, 0, sizeof(data));
  data.com_query.query = sql;
  data.com_query.length = static_cast<unsigned int>(sql_len);
  return execute_server_command(COM_QUERY, data, rset);
}

ngs::Error_code Sql_data_context::fetch_cursor(const uint32_t statement_id,
                                               const uint32_t row_count,
                                               iface::Resultset *rset) {
  COM_DATA data;
  data.com_stmt_fetch.stmt_id = statement_id;
  data.com_stmt_fetch.num_rows = row_count;
  return execute_server_command(COM_STMT_FETCH, data, rset);
}

ngs::Error_code Sql_data_context::attach() {
  THD *previous_thd = nullptr;

  if (nullptr == m_mysql_session ||
      srv_session_attach(m_mysql_session, &previous_thd)) {
    return ngs::Error_code(ER_X_SERVICE_ERROR, "Internal error attaching");
  }

  return {};
}

ngs::Error_code Sql_data_context::detach() {
  if (nullptr == m_mysql_session || srv_session_detach(m_mysql_session)) {
    return ngs::Error_code(ER_X_SERVICE_ERROR, "Internal error when detaching");
  }

  return {};
}

ngs::Error_code Sql_data_context::prepare_prep_stmt(const char *sql,
                                                    std::size_t sql_len,
                                                    iface::Resultset *rset) {
  COM_DATA data;
  data.com_stmt_prepare = {sql, static_cast<unsigned>(sql_len)};
  return execute_server_command(COM_STMT_PREPARE, data, rset);
}

ngs::Error_code Sql_data_context::deallocate_prep_stmt(const uint32_t stmt_id,
                                                       iface::Resultset *rset) {
  COM_DATA data;
  data.com_stmt_close = {static_cast<unsigned>(stmt_id)};
  return execute_server_command(COM_STMT_CLOSE, data, rset);
}

ngs::Error_code Sql_data_context::execute_prep_stmt(
    const uint32_t stmt_id, const bool has_cursor, const PS_PARAM *parameters,
    const std::size_t parameters_count, iface::Resultset *rset) {
  COM_DATA cmd;
  cmd.com_stmt_execute = {
      static_cast<unsigned long>(stmt_id),     // NOLINT(runtime/int)
      static_cast<unsigned long>(has_cursor),  // NOLINT(runtime/int)
      const_cast<PS_PARAM *>(parameters),
      static_cast<unsigned long>(parameters_count),  // NOLINT(runtime/int)
      static_cast<unsigned char>(true)};             // NOLINT(runtime/int)

  return execute_server_command(COM_STMT_EXECUTE, cmd, rset);
}

ngs::Error_code Sql_data_context::reset() {
  COM_DATA data{};
  Empty_resultset rset;
  return execute_server_command(COM_RESET_CONNECTION, data, &rset);
}

ngs::Error_code Sql_data_context::execute_server_command(
    const enum_server_command cmd, const COM_DATA &cmd_data,
    iface::Resultset *rset) {
  ngs::Command_delegate &deleg = rset->get_callbacks();
  deleg.reset();
  const auto fail = command_service_run_command(
      m_mysql_session, cmd, &cmd_data, mysqld::get_default_charset(),
      deleg.callbacks(), deleg.representation(), &deleg);

  ngs::Error_code error = deleg.get_error();

  if (fail) {
    if (error) {
      log_debug("Error running server command: (%i:%s)", error.error,
                error.message.c_str());
      return error;
    }
    return ngs::Error(ER_X_SERVICE_ERROR, "Internal error executing command");
  }

  if (!error) return ngs::Success();

  log_debug("Error running server command: (%i:%s; killed: %i:%i)", error.error,
            error.message.c_str(), static_cast<int>(is_killed()),
            static_cast<int>(deleg.killed()));

  return is_killed() ? ngs::Fatal(error) : error;
}

bool Sql_data_context::is_sql_mode_set(const std::string &mode) {
  std::string modes;
  get_system_variable<std::string>(this, "sql_mode", &modes);
  std::istringstream str(modes);
  std::string m;
  while (std::getline(str, m, ','))
    if (m == mode) return true;
  return false;
}

}  // namespace xpl
