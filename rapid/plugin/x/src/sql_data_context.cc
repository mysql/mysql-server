/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <algorithm>
#include "mysql/plugin.h"
#include "sql_data_context.h"
#include "sql_user_require.h"
#include "mysql/service_command.h"
#include "streaming_command_delegate.h"
#include "buffering_command_delegate.h"
#include "mysql_variables.h"
#include "query_string_builder.h"

#include "xpl_log.h"
#include "xpl_error.h"
#include "notices.h"

#include "user_verification_helper.h"


using namespace xpl;

ngs::Error_code Sql_data_context::init(const int client_port, const ngs::Connection_type type)
{
  ngs::Error_code error = init();
  if (error)
    return error;

  if ((error = set_connection_type(type)))
    return error;

  if (0 != srv_session_info_set_client_port(m_mysql_session, client_port))
    return ngs::Error_code(ER_X_SESSION, "Could not set session client port");

  return ngs::Error_code();
}


ngs::Error_code Sql_data_context::init()
{
  m_mysql_session = srv_session_open(&Sql_data_context::default_completion_handler, this);
  log_debug("sqlsession init: %p [%i]", m_mysql_session, m_mysql_session ? srv_session_info_get_session_id(m_mysql_session) : -1);
  if (!m_mysql_session)
  {
    if (ER_SERVER_ISNT_AVAILABLE == m_last_sql_errno)
      return ngs::Error_code(ER_SERVER_ISNT_AVAILABLE, "Server API not ready");
    log_error("Could not open internal MySQL session");
    return ngs::Error_code(ER_X_SESSION, "Could not open session");
  }
  return ngs::Error_code();
}

void Sql_data_context::deinit()
{
  if (m_mysql_session)
  {
    srv_session_detach(m_mysql_session);

    log_debug("sqlsession deinit: %p [%i]", m_mysql_session, srv_session_info_get_session_id(m_mysql_session));
    srv_session_close(m_mysql_session);
    m_mysql_session = NULL;
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(delete_current_thread)();

  PSI_thread *psi= PSI_THREAD_CALL(new_thread) (KEY_thread_x_worker, NULL, 0);
  PSI_THREAD_CALL(set_thread_os_id)(psi);
  PSI_THREAD_CALL(set_thread)(psi);
#endif /* HAVE_PSI_THREAD_INTERFACE */
}


static void kill_completion_handler(void *ctx, unsigned int sql_errno, const char *err_msg)
{
  log_warning("Kill client: %i %s", sql_errno, err_msg);
}


bool Sql_data_context::kill()
{
  if (srv_session_server_is_available())
  {
    log_debug("sqlsession init (for kill): %p [%i]", m_mysql_session, m_mysql_session ? srv_session_info_get_session_id(m_mysql_session) : -1);
    MYSQL_SESSION session = srv_session_open(kill_completion_handler, NULL);
    bool ok = false;
    if (session)
    {
      MYSQL_SECURITY_CONTEXT scontext;

      if (thd_get_security_context(srv_session_info_get_thd(session), &scontext))
        log_warning("Could not get security context for session");
      else {
        const char *user = MYSQL_SESSION_USER;
        const char *host = MYSQLXSYS_HOST;
        if (security_context_lookup(scontext, user, host, NULL, NULL))
          log_warning("Unable to switch security context to root");
        else
        {
          COM_DATA data;
          Callback_command_delegate deleg;
          Query_string_builder qb;
          qb.put("KILL ").put(mysql_session_id());

          data.com_query.query = (char*)qb.get().c_str();
          data.com_query.length = static_cast<unsigned int>(qb.get().length());

          if (!command_service_run_command(session, COM_QUERY, &data,
                                           mysqld::get_charset_utf8mb4_general_ci(), deleg.callbacks(),
                                           deleg.representation(), &deleg))
          {
            if (!deleg.get_error())
              ok = true;
            else
              log_info("Kill client: %i %s", deleg.get_error().error, deleg.get_error().message.c_str());
          }
        }
      }
      srv_session_close(session);
    }
    return ok;
  }
  return false;
}


ngs::Error_code Sql_data_context::set_connection_type(const ngs::Connection_type type)
{
  enum_vio_type vio_type = ngs::Connection_type_helper::convert_type(type);

  if (NO_VIO_TYPE == vio_type)
    return ngs::Error(ER_X_SESSION, "Connection type not known. type=%i", (int)type);

  if (0 != srv_session_info_set_connection_type(m_mysql_session, vio_type))
    return ngs::Error_code(ER_X_SESSION, "Could not set session connection type");

  return ngs::Error_code();
}

bool Sql_data_context::wait_api_ready(ngs::function<bool()> exiting)
{
  bool result = is_api_ready();

  while (!result && !exiting())
  {
    my_sleep(250000); // wait for 0.25s

    result = is_api_ready();
  }

  return result;
}


void Sql_data_context::detach()
{
  if (m_mysql_session)
    srv_session_detach(m_mysql_session);
}


Sql_data_context::~Sql_data_context()
{
  if (m_mysql_session)
    log_debug("sqlsession deinit~: %p [%i]", m_mysql_session, srv_session_info_get_session_id(m_mysql_session));
  if (m_mysql_session && srv_session_close(m_mysql_session))
    log_warning("Error closing SQL session");
}


void Sql_data_context::switch_to_local_user(const std::string &user)
{
  ngs::Error_code error = switch_to_user(user.c_str(), "localhost", NULL, NULL);
  if (error)
    throw error;
}


ngs::Error_code Sql_data_context::authenticate(const char *user, const char *host, const char *ip,
                                               const char *db, On_user_password_hash password_hash_cb,
                                               bool allow_expired_passwords, ngs::IOptions_session_ptr &options_session, const ngs::Connection_type type)
{
  ngs::Error_code error = switch_to_user(user, host, ip, db);

  if (error)
  {
    return ngs::Error(ER_NO_SUCH_USER, "Invalid user or password");
  }

  std::string authenticated_user_name = get_authenticated_user_name();
  std::string authenticated_user_host = get_authenticated_user_host();

  error = switch_to_user(MYSQL_SESSION_USER, MYSQLXSYS_HOST, NULL, NULL);

  if (error) {
    log_error("Unable to switch context to user %s", MYSQL_SESSION_USER);
    return error;
  }

  if (!is_acl_disabled())
  {
    User_verification_helper user_verification(password_hash_cb, options_session, type);

    error = user_verification.verify_mysql_account(
        *this,
        authenticated_user_name,
        authenticated_user_host);
  }

  if (error.error == ER_MUST_CHANGE_PASSWORD_LOGIN)
  {
    m_password_expired = true;

    // password is expired, client doesn't support it and server wants us to disconnect these users
    if (error.severity == ngs::Error_code::FATAL && !allow_expired_passwords)
      return error;

    // if client supports expired password mode, then pwd expired is not fatal
    // we send a notice and move on
    notices::send_account_expired(proto());
  }
  else if (error)
    return error;

  error = switch_to_user(user, host, ip, db);

  if (!error)
  {
    if (db && *db)
    {
      COM_DATA data;

      data.com_init_db.db_name = db;
      data.com_init_db.length = static_cast<unsigned long>(strlen(db));

      m_callback_delegate.reset();

      if (command_service_run_command(m_mysql_session, COM_INIT_DB, &data, mysqld::get_charset_utf8mb4_general_ci(),
                                      m_callback_delegate.callbacks(), m_callback_delegate.representation(), &m_callback_delegate))
        return ngs::Error_code(ER_NO_DB_ERROR, "Could not set database");
      error = m_callback_delegate.get_error();
    }

    std::string user_name = get_user_name();
    std::string host_or_ip = get_host_or_ip();

#ifdef HAVE_PSI_THREAD_INTERFACE
    PSI_THREAD_CALL(set_thread_account) (
        user_name.c_str(), user_name.length(),
        host_or_ip.c_str(), host_or_ip.length());
#endif // HAVE_PSI_THREAD_INTERFACE

    return error;
  }

  log_error("Unable to switch context to user %s", user);

  return error;
}

template<typename Result_type>
bool get_security_context_value(MYSQL_THD thd, const char *option, Result_type &result)
{
  MYSQL_SECURITY_CONTEXT scontext;

  if (thd_get_security_context(thd, &scontext))
    return false;

  return FALSE == security_context_get_option(scontext, option, &result);
}

bool Sql_data_context::is_acl_disabled()
{
  MYSQL_LEX_CSTRING value;

  if (get_security_context_value(get_thd(), "priv_user", value))
  {
    return 0 != value.length &&
           NULL != strstr(value.str, "skip-grants ");
  }

  return false;
}

bool Sql_data_context::has_authenticated_user_a_super_priv() const
{
  my_svc_bool value = 0;
  if (get_security_context_value(get_thd(), "privilege_super", value))
    return value != 0;

  return false;
}

std::string Sql_data_context::get_user_name() const
{
  MYSQL_LEX_CSTRING result;

  if (get_security_context_value(get_thd(), "user", result))
    return result.str;

  return "";
}

std::string Sql_data_context::get_host_or_ip() const
{
  MYSQL_LEX_CSTRING result;

  if (get_security_context_value(get_thd(), "host_or_ip", result))
    return result.str;

  return "";
}

std::string Sql_data_context::get_authenticated_user_name() const
{
  MYSQL_LEX_CSTRING result;

  if (get_security_context_value(get_thd(), "priv_user", result))
    return result.str;

  return "";
}

std::string Sql_data_context::get_authenticated_user_host() const
{
  MYSQL_LEX_CSTRING result;

  if (get_security_context_value(get_thd(), "priv_host", result))
    return result.str;

  return "";
}

ngs::Error_code Sql_data_context::switch_to_user(
    const char *username,
    const char *hostname,
    const char *address,
    const char *db)
{
  MYSQL_SECURITY_CONTEXT scontext;
  m_auth_ok = false;

  if (thd_get_security_context(get_thd(), &scontext))
    return ngs::Fatal(ER_X_SERVICE_ERROR, "Error getting security context for session");

  // security_context_lookup - doesn't make a copy of username, hostname, addres or db
  //                           thus we need to make a copy of them and pass our pointers
  //                           to security_context_lookup
  m_username = username ? username : "";
  m_hostname = hostname ? hostname : "";
  m_address = address ? address : "";
  m_db = db ? db : "";

  log_debug("Switching security context to user %s@%s [%s]", username, hostname, address);
  if (security_context_lookup(scontext, m_username.c_str(), m_hostname.c_str(), m_address.c_str(), m_db.c_str()))
  {
    return ngs::Fatal(ER_X_SERVICE_ERROR, "Unable to switch context to user %s", username);
  }

  m_auth_ok = true;

  return ngs::Success();
}


ngs::Error_code Sql_data_context::execute_kill_sql_session(uint64_t mysql_session_id)
{
  Query_string_builder qb;
  qb.put("KILL ").put(mysql_session_id);
  Sql_data_context::Result_info r_info;

  return execute_sql_no_result(qb.get().data(), qb.get().length(), r_info);
}


ngs::Error_code Sql_data_context::execute_sql(
    Command_delegate &deleg,
    const char *sql,
    size_t length,
    Sql_data_context::Result_info &r_info)
{
  if (!m_auth_ok && !m_query_without_authentication)
    throw std::logic_error("Attempt to execute query in non-authenticated session");

  COM_DATA data;

  data.com_query.query = sql;
  data.com_query.length = static_cast<unsigned int>(length);

  deleg.reset();

  if (command_service_run_command(m_mysql_session, COM_QUERY, &data, mysqld::get_charset_utf8mb4_general_ci(),
                                  deleg.callbacks(), deleg.representation(), &deleg))
  {
    log_debug("Error running command: %s (%i %s)", sql, m_last_sql_errno, m_last_sql_error.c_str());
    return ngs::Error_code(ER_X_SERVICE_ERROR, "Internal error executing query");
  }

  if (m_password_expired && !deleg.get_error())
  {
    // if a SQL command succeeded while password is expired, it means the user probably changed the password
    // we run a command to check just in case... (some commands are still allowed in expired password mode)
    Callback_command_delegate d;
    data.com_query.query = "select 1";
    data.com_query.length = static_cast<unsigned int>(strlen(data.com_query.query));
    if (false == command_service_run_command(m_mysql_session, COM_QUERY, &data, mysqld::get_charset_utf8mb4_general_ci(),
                                             d.callbacks(), d.representation(), &d) && !d.get_error())
    {
      m_password_expired = false;
    }
  }

  if (is_killed())
    throw ngs::Fatal(ER_QUERY_INTERRUPTED, "Query execution was interrupted");

  r_info.last_insert_id = deleg.last_insert_id();
  r_info.num_warnings = deleg.statement_warn_count();
  r_info.affected_rows = deleg.affected_rows();
  r_info.message = deleg.message();
  r_info.server_status = deleg.server_status();

  return deleg.get_error();
}


ngs::Error_code Sql_data_context::execute_sql_no_result(const char *sql, std::size_t sql_len,
                                                        Sql_data_context::Result_info &r_info)
{
  m_callback_delegate.set_callbacks(Callback_command_delegate::Start_row_callback(),
                                    Callback_command_delegate::End_row_callback());
  return execute_sql(m_callback_delegate, sql, sql_len, r_info);
}


ngs::Error_code Sql_data_context::execute_sql_and_collect_results(const char *sql, std::size_t sql_len,
                                                                  std::vector<Command_delegate::Field_type> &r_types,
                                                                  Buffering_command_delegate::Resultset &r_rows,
                                                                  Result_info &r_info)
{
  ngs::Error_code error = execute_sql(m_buffering_delegate, sql, sql_len, r_info);
  if (!error)
  {
    r_types = m_buffering_delegate.get_field_types();
    r_rows = m_buffering_delegate.resultset();
  }
  return error;
}

ngs::Error_code Sql_data_context::execute_sql_and_process_results(const char *sql, std::size_t sql_len,
                                                                  const Callback_command_delegate::Start_row_callback &start_row,
                                                                  const Callback_command_delegate::End_row_callback &end_row,
                                                                  Sql_data_context::Result_info &r_info)
{
  m_callback_delegate.set_callbacks(start_row, end_row);
  return execute_sql(m_callback_delegate, sql, sql_len, r_info);
}


ngs::Error_code Sql_data_context::execute_sql_and_stream_results(const char *sql, std::size_t sql_len,
                                                                 bool compact_metadata, Result_info &r_info)
{
  m_streaming_delegate.set_compact_metadata(compact_metadata);
  return execute_sql(m_streaming_delegate, sql, sql_len, r_info);
}


void Sql_data_context::default_completion_handler(void *ctx, unsigned int sql_errno, const char *err_msg)
{
  Sql_data_context *self = (Sql_data_context*)ctx;
  self->m_last_sql_errno = sql_errno;
  self->m_last_sql_error = err_msg ? err_msg : "";
}


bool Sql_data_context::is_killed()
{
  return srv_session_info_killed(m_mysql_session);
}


bool Sql_data_context::is_api_ready()
{
  return 0 != srv_session_server_is_available();
}


uint64_t Sql_data_context::mysql_session_id() const
{
  return srv_session_info_get_session_id(m_mysql_session);
}


MYSQL_THD Sql_data_context::get_thd() const
{
  return srv_session_info_get_thd(m_mysql_session);
}
