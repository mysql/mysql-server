/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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
      else
      {
        const char *user = MYSQLXSYS_USER;
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

bool Sql_data_context::wait_api_ready(boost::function<bool()> exiting)
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

  my_free(m_user);
  my_free(m_host);
  my_free(m_ip);
  my_free(m_db);
}


void Sql_data_context::switch_to_local_user(const std::string &user)
{
  ngs::Error_code error = switch_to_user(user.c_str(), "localhost", NULL, NULL);
  if (error)
    throw error;
}


ngs::Error_code Sql_data_context::query_user(const char *user, const char *host, const char *ip,
                                             On_user_password_hash &hash_verification_cb,
                                             ngs::IOptions_session_ptr &options_session, const ngs::Connection_type type)
{
  COM_DATA data;

  User_verification_helper user_verification(hash_verification_cb, m_buffering_delegate.get_field_types(), ip, options_session, type);

  std::string query = user_verification.get_sql(user, host);

  data.com_query.query = (char*)query.c_str();
  data.com_query.length = static_cast<unsigned int>(query.length());

  log_debug("login query: %s", data.com_query.query);
  if (command_service_run_command(m_mysql_session, COM_QUERY, &data, mysqld::get_charset_utf8mb4_general_ci(),
                                  m_buffering_delegate.callbacks(), CS_TEXT_REPRESENTATION, &m_buffering_delegate))
  {
    return ngs::Error_code(ER_X_SERVICE_ERROR, "Error executing internal query");
  }

  ngs::Error_code error = m_buffering_delegate.get_error();
  if (error)
  {
    log_debug("Error %i occurred while executing query: %s", error.error, error.message.c_str());
    return error;
  }

  Buffering_command_delegate::Resultset &result_set = m_buffering_delegate.resultset();

  try
  {
    if (result_set.end() == std::find_if(result_set.begin(), result_set.end(), user_verification))
    {
      return ngs::Error_code(ER_NO_SUCH_USER, "Invalid user or password");
    }
  }
  catch (ngs::Error_code &e)
  {
    return e;
  }
  return ngs::Error_code();
}


ngs::Error_code Sql_data_context::authenticate(const char *user, const char *host, const char *ip,
                                               const char *db, On_user_password_hash password_hash_cb,
                                               bool allow_expired_passwords, ngs::IOptions_session_ptr &options_session, const ngs::Connection_type type)
{
  ngs::Error_code error = switch_to_user(MYSQLXSYS_USER, MYSQLXSYS_HOST, NULL, NULL);
  if (error)
  {
    log_error("Unable to switch context to user %s", MYSQLXSYS_USER);
    throw error;
  }

  if (!is_acl_disabled())
  {
    error = query_user(user, host, ip, password_hash_cb, options_session, type);
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
    if (m_db && *m_db)
    {
      COM_DATA data;

      data.com_init_db.db_name = m_db;
      data.com_init_db.length = static_cast<unsigned long>(strlen(m_db));

      m_callback_delegate.reset();
      if (command_service_run_command(m_mysql_session, COM_INIT_DB, &data, mysqld::get_charset_utf8mb4_general_ci(),
                                      m_callback_delegate.callbacks(), m_callback_delegate.representation(), &m_callback_delegate))
        return ngs::Error_code(ER_NO_DB_ERROR, "Could not set database");
      error = m_callback_delegate.get_error();
    }

    return ngs::Error_code();
  }

  log_error("Unable to switch context to user %s", user);

  return error;
}

bool Sql_data_context::is_acl_disabled()
{
  MYSQL_SECURITY_CONTEXT scontext;

  if (thd_get_security_context(get_thd(), &scontext))
    return false;

  MYSQL_LEX_CSTRING value;
  if (false != security_context_get_option(scontext, "priv_user", &value))
    return false;

  return 0 != value.length && NULL != strstr(value.str, "skip-grants ");
}

ngs::Error_code Sql_data_context::switch_to_user(const char *username, const char *hostname, const char *address,  const char *db)
{
  MYSQL_SECURITY_CONTEXT scontext;

  // switch security context
  my_free(m_user);
  m_user = my_strdup(PSI_NOT_INSTRUMENTED, username, 0);
  my_free(m_host);
  if (hostname)
    m_host = my_strdup(PSI_NOT_INSTRUMENTED, hostname, 0);
  else
    m_host = NULL;
  my_free(m_ip);
  if (address)
    m_ip = my_strdup(PSI_NOT_INSTRUMENTED, address, 0);
  else
    m_ip = NULL;
  my_free(m_db);
  if (db)
    m_db = my_strdup(PSI_NOT_INSTRUMENTED, db, 0);
  else
    m_db = NULL;

  m_is_super = false;
  m_auth_ok = false;

  if (thd_get_security_context(get_thd(), &scontext))
    return ngs::Fatal(ER_X_SERVICE_ERROR, "Error getting security context for session");

  log_debug("Switching security context to user %s@%s [%s]", m_user, m_host, m_ip);
  if (security_context_lookup(scontext, m_user, m_host, m_ip, m_db))
  {
    return ngs::Fatal(ER_X_SERVICE_ERROR, "Unable to switch context to user %s", m_user);
  }

  m_auth_ok = true;
  {
    my_svc_bool value = 0;
    if (false == security_context_get_option(scontext, "privilege_super", &value))
      m_is_super = value != 0;
  }

  return ngs::Success();
}


ngs::Error_code Sql_data_context::execute_kill_sql_session(uint64_t mysql_session_id)
{
  Query_string_builder qb;
  qb.put("KILL ").put(mysql_session_id);
  Sql_data_context::Result_info r_info;

  return execute_sql_no_result(qb.get(), r_info);
}


ngs::Error_code Sql_data_context::execute_sql(Command_delegate &deleg,
                                              const char *sql, size_t length, Sql_data_context::Result_info &r_info)
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


ngs::Error_code Sql_data_context::execute_sql_no_result(const std::string &sql, Sql_data_context::Result_info &r_info)
{
  m_callback_delegate.set_callbacks(Callback_command_delegate::Start_row_callback(),
                                    Callback_command_delegate::End_row_callback());
  return execute_sql(m_callback_delegate, sql.data(), sql.length(), r_info);
}


ngs::Error_code Sql_data_context::execute_sql_and_collect_results(const std::string &sql,
                                                                  std::vector<Command_delegate::Field_type> &r_types,
                                                                  Buffering_command_delegate::Resultset &r_rows,
                                                                  Result_info &r_info)
{
  ngs::Error_code error = execute_sql(m_buffering_delegate, sql.data(), sql.length(), r_info);
  if (!error)
  {
    r_types = m_buffering_delegate.get_field_types();
    r_rows = m_buffering_delegate.resultset();
  }
  return error;
}

ngs::Error_code Sql_data_context::execute_sql_and_process_results(const std::string &sql,
                                                                  const Callback_command_delegate::Start_row_callback &start_row,
                                                                  const Callback_command_delegate::End_row_callback &end_row,
                                                                  Sql_data_context::Result_info &r_info)
{
  m_callback_delegate.set_callbacks(start_row, end_row);
  return execute_sql(m_callback_delegate, sql.data(), sql.length(), r_info);
}


ngs::Error_code Sql_data_context::execute_sql_and_stream_results(const std::string &sql,
                                                                 bool compact_metadata, Result_info &r_info)
{
  m_streaming_delegate.set_compact_metadata(compact_metadata);
  return execute_sql(m_streaming_delegate, sql.data(), sql.length(), r_info);
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
