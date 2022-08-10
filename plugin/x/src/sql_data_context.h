/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_SQL_DATA_CONTEXT_H_
#define PLUGIN_X_SRC_SQL_DATA_CONTEXT_H_

#include <stdio.h>

#include <string>

#ifndef _WIN32
#include <netdb.h>
#endif

#include "my_hostname.h"
#include "mysql/service_command.h"
#include "mysql_com.h"

#include "plugin/x/src/buffering_command_delegate.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/io/connection_type.h"
#include "plugin/x/src/streaming_command_delegate.h"

// Use an internal MySQL server user
#define MYSQL_SESSION_USER "mysql.session"
#define MYSQLXSYS_HOST "localhost"
#define MYSQLXSYS_ACCOUNT "'" MYSQL_SESSION_USER "'@'" MYSQLXSYS_HOST "'"

namespace xpl {

typedef std::function<bool(const std::string &password_hash)>
    On_user_password_hash;
typedef Buffering_command_delegate::Field_value Field_value;
typedef Buffering_command_delegate::Row_data Row_data;
class Account_verification_handler;

class Sql_data_context : public iface::Sql_session {
 public:
  Sql_data_context()
      : m_mysql_session(nullptr),
        m_last_sql_errno(0),
        m_password_expired(false),
        m_using_password(false),
        m_pre_authenticate_event_fired(false) {}

  ~Sql_data_context() override;

  // The authentication process of the user connecting to the X Plugin.
  //
  // @param user                    User name.
  // @param host                    Host name.
  // @param ip                      IP value.
  // @param db                      Database name.
  // @param passwd                  User password.
  // @param account_verification    User authentication object.
  // @param allow_expired_passwords Ignore password
  //
  // @return Error_code instance that holds authentication result.
  ngs::Error_code authenticate(
      const char *user, const char *host, const char *ip, const char *db,
      const std::string &passwd,
      const iface::Authentication &account_verification,
      bool allow_expired_passwords) override;

  uint64_t mysql_session_id() const override;

  ngs::Error_code set_connection_type(const Connection_type type) override;
  bool is_killed() const override;
  bool password_expired() const override { return m_password_expired; }

  // Get data which are part of string printed by
  // CURRENT_USER() function
  std::string get_authenticated_user_name() const override;
  std::string get_authenticated_user_host() const override;
  bool has_authenticated_user_a_super_priv() const override;

  ngs::Error_code execute_kill_sql_session(uint64_t mysql_session_id) override;

  // can only be executed once authenticated
  ngs::Error_code execute(const char *sql, std::size_t sql_len,
                          iface::Resultset *rset) override;
  ngs::Error_code execute_sql(const char *sql, std::size_t sql_len,
                              iface::Resultset *rset) override;
  ngs::Error_code prepare_prep_stmt(const char *sql, std::size_t sql_len,
                                    iface::Resultset *rset) override;
  ngs::Error_code deallocate_prep_stmt(const uint32_t stmt_id,
                                       iface::Resultset *rset) override;
  ngs::Error_code execute_prep_stmt(const uint32_t stmt_id,
                                    const bool has_cursor,
                                    const PS_PARAM *parameters,
                                    const std::size_t parameters_count,
                                    iface::Resultset *rset) override;

  ngs::Error_code fetch_cursor(const uint32_t id, const uint32_t row_count,
                               iface::Resultset *rset) override;

  ngs::Error_code attach() override;
  ngs::Error_code detach() override;
  ngs::Error_code reset() override;
  bool is_sql_mode_set(const std::string &mode) override;

  ngs::Error_code init(const bool is_admin = false);
  ngs::Error_code init(const int client_port, const Connection_type type,
                       const bool is_admin = false);

  void deinit();

  MYSQL_THD get_thd() const;

  bool kill();
  bool is_acl_disabled();
  void switch_to_local_user(const std::string &username);
  static bool wait_api_ready(std::function<bool()> exiting);

 private:
  Sql_data_context(const Sql_data_context &) = delete;
  Sql_data_context &operator=(const Sql_data_context &) = delete;

  // The real authentication process implementation, without generation
  // of the audit events. For argument description see @ref authenticate.
  ngs::Error_code authenticate_internal(
      const char *user, const char *host, const char *ip, const char *db,
      const std::string &passwd,
      const iface::Authentication &account_verification,
      bool allow_expired_passwords);

  MYSQL_SESSION mysql_session() const { return m_mysql_session; }
  static bool is_api_ready();
  // Get data which are parts of the string printed by
  // USER() function
  std::string get_user_name() const;
  std::string get_host_or_ip() const;

  ngs::Error_code switch_to_user(const char *username, const char *hostname,
                                 const char *address, const char *db);

  static void default_completion_handler(void *ctx, unsigned int sql_errno,
                                         const char *err_msg);

  ngs::Error_code execute_server_command(const enum_server_command cmd,
                                         const COM_DATA &cmd_data,
                                         iface::Resultset *rset);

  // We need to keep pointers to std::string to guarantee that pointer
  // of the buffer holding the string changes. This is due to the security
  // context implementation, which do not update internal data, when
  // the pointer does not change.
  char m_username[USERNAME_LENGTH + 1];
  char m_hostname[HOSTNAME_LENGTH + 1];
  char m_address[NI_MAXHOST];
  char m_db[NAME_LEN + 1];

  MYSQL_SESSION m_mysql_session;

  int m_last_sql_errno;
  std::string m_last_sql_error;

  bool m_password_expired;

  // Flag indicating whether a password is used during the authentication
  // process.
  bool m_using_password;

  // Single X plugin user connection can be authenticated multiple times
  // using different connection methods. This flag assures that there is
  // only one MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE event generated.
  bool m_pre_authenticate_event_fired;
  // Last authentication process error code. The code is used to pass error
  // code to the generated MYSQL_AUDIT_CONNECTION_CONNECT event.
  ngs::Error_code m_authentication_code;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SQL_DATA_CONTEXT_H_
