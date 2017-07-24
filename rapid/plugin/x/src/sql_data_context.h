/*
 * Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.
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

#ifndef XPL_SQL_DATA_CONTEXT_H_
#define XPL_SQL_DATA_CONTEXT_H_

#include "ngs_common/connection_type.h"
#include "ngs/protocol_encoder.h"

#include "mysql/service_my_snprintf.h"
#include "mysql/service_command.h"

#include "buffering_command_delegate.h"
#include "streaming_command_delegate.h"

// Use an internal MySQL server user
#define MYSQL_SESSION_USER "mysql.session"
#define MYSQLXSYS_HOST "localhost"
#define MYSQLXSYS_ACCOUNT "'" MYSQL_SESSION_USER "'@'" MYSQLXSYS_HOST "'"


namespace ngs
{
  class IOptions_session;
  typedef ngs::shared_ptr<IOptions_session> IOptions_session_ptr;
  class Protocol_encoder;
}  // namespace ngs


namespace xpl
{

typedef ngs::function<bool (const std::string &password_hash)> On_user_password_hash;
typedef Buffering_command_delegate::Field_value Field_value;
typedef Buffering_command_delegate::Row_data    Row_data;


class Sql_data_context
{
public:
  struct Result_info
  {
    uint64_t affected_rows;
    uint64_t last_insert_id;
    uint32_t num_warnings;
    std::string message;
    uint32_t server_status;

    Result_info()
    : affected_rows(0),
      last_insert_id(0),
      num_warnings(0),
      server_status(0)
    {}
  };

  Sql_data_context(ngs::Protocol_encoder *proto, const bool query_without_authentication = false)
  : m_proto(proto), m_mysql_session(NULL),
    m_streaming_delegate(m_proto),
    m_last_sql_errno(0),
    m_auth_ok(false),
    m_query_without_authentication(query_without_authentication),
    m_password_expired(false)
  {}

  virtual ~Sql_data_context();

  ngs::Error_code init();
  ngs::Error_code init(const int client_port, const ngs::Connection_type type);
  void            deinit();

  virtual ngs::Error_code authenticate(const char *user, const char *host, const char *ip, const char *db, On_user_password_hash password_hash_cb, bool allow_expired_passwords, ngs::IOptions_session_ptr &options_session, const ngs::Connection_type type);

  ngs::Protocol_encoder &proto() { return *m_proto; }
  MYSQL_SESSION mysql_session() const { return m_mysql_session; }

  uint64_t mysql_session_id() const;
  MYSQL_THD get_thd() const;

  void detach();

  ngs::Error_code set_connection_type(const ngs::Connection_type type);
  bool kill();
  bool is_killed();
  bool is_acl_disabled();
  bool is_api_ready();

  bool wait_api_ready(ngs::function<bool()> exiting);
  bool password_expired() const { return m_password_expired; }

  // Get data which are parts of the string printed by
  // USER() function
  std::string get_user_name() const;
  std::string get_host_or_ip() const;

  // Get data which are part of string printed by
  // CURRENT_USER() function
  std::string get_authenticated_user_name() const;
  std::string get_authenticated_user_host() const;
  bool        has_authenticated_user_a_super_priv() const;

  void switch_to_local_user(const std::string &username);

  ngs::Error_code execute_kill_sql_session(uint64_t mysql_session_id);

  // can only be executed once authenticated
  virtual ngs::Error_code execute_sql_no_result(const char *sql, std::size_t sql_len, Result_info &r_info);
  virtual ngs::Error_code execute_sql_and_collect_results(const char *sql, std::size_t sql_len,
                                                          std::vector<Command_delegate::Field_type> &r_types,
                                                          Buffering_command_delegate::Resultset &r_rows,
                                                          Result_info &r_info);
  virtual ngs::Error_code execute_sql_and_process_results(const char *sql, std::size_t sql_len,
                                                          const Callback_command_delegate::Start_row_callback &start_row,
                                                          const Callback_command_delegate::End_row_callback &end_row,
                                                          Result_info &r_info);
  virtual ngs::Error_code execute_sql_and_stream_results(const char *sql, std::size_t sql_len,
                                                         bool compact_metadata, Result_info &r_info);

private:
  Sql_data_context(const Sql_data_context &);
  Sql_data_context &operator=(const Sql_data_context &);

  ngs::Error_code execute_sql(Command_delegate &deleg, const char *sql, size_t length, Result_info &r_info);

  ngs::Error_code switch_to_user(const char *username, const char *hostname, const char *address, const char *db);

  static void default_completion_handler(void *ctx, unsigned int sql_errno, const char *err_msg);

  std::string m_username;
  std::string m_hostname;
  std::string m_address;
  std::string m_db;

  ngs::Protocol_encoder *m_proto;
  MYSQL_SESSION          m_mysql_session;

  Callback_command_delegate m_callback_delegate;
  Buffering_command_delegate m_buffering_delegate;
  Streaming_command_delegate m_streaming_delegate;

  int m_last_sql_errno;
  std::string m_last_sql_error;

  bool m_auth_ok;
  bool m_query_without_authentication;
  bool m_password_expired;
};

} // namespace xpl

#undef MYSQL_CLIENT

#endif // XPL_SQL_DATA_CONTEXT_H_
