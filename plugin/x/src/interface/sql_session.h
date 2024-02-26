/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SQL_SESSION_H_
#define PLUGIN_X_SRC_INTERFACE_SQL_SESSION_H_

#include <string>

#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/resultset.h"
#include "plugin/x/src/io/connection_type.h"
#include "plugin/x/src/ngs/command_delegate.h"

struct PS_PARAM;

namespace xpl {
namespace iface {

class Sql_session {
 public:
  virtual ~Sql_session() = default;

  virtual ngs::Error_code set_connection_type(
      const xpl::Connection_type type) = 0;
  virtual ngs::Error_code execute_kill_sql_session(
      uint64_t mysql_session_id) = 0;
  virtual bool is_killed() const = 0;
  virtual bool password_expired() const = 0;
  virtual std::string get_authenticated_user_name() const = 0;
  virtual std::string get_authenticated_user_host() const = 0;
  virtual bool has_authenticated_user_a_super_priv() const = 0;
  virtual uint64_t mysql_session_id() const = 0;
  virtual ngs::Error_code authenticate(
      const char *user, const char *host, const char *ip, const char *db,
      const std::string &passwd, const Authentication &account_verification,
      bool allow_expired_passwords) = 0;
  virtual ngs::Error_code execute(const char *sql, std::size_t sql_len,
                                  Resultset *rset) = 0;
  virtual ngs::Error_code execute_sql(const char *sql, std::size_t sql_len,
                                      Resultset *rset) = 0;
  virtual ngs::Error_code fetch_cursor(const uint32_t id,
                                       const uint32_t row_count,
                                       Resultset *rset) = 0;
  virtual ngs::Error_code prepare_prep_stmt(const char *sql,
                                            std::size_t sql_len,
                                            Resultset *rset) = 0;
  virtual ngs::Error_code deallocate_prep_stmt(const uint32_t id,
                                               Resultset *rset) = 0;
  virtual ngs::Error_code execute_prep_stmt(const uint32_t stmt_id,
                                            const bool has_cursor,
                                            const PS_PARAM *parameters,
                                            const std::size_t parameter_count,
                                            Resultset *rset) = 0;
  virtual ngs::Error_code attach() = 0;
  virtual ngs::Error_code detach() = 0;
  virtual ngs::Error_code reset() = 0;
  virtual bool is_sql_mode_set(const std::string &mode) = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SQL_SESSION_H_
