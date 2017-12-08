/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef NGS_SQL_SESSION_INTERFACE_H_
#define NGS_SQL_SESSION_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/resultset_interface.h"
#include "plugin/x/ngs/include/ngs/protocol_encoder.h"
#include "plugin/x/ngs/include/ngs_common/connection_type.h"


namespace ngs {

class Sql_session_interface {
 public:

  virtual ~Sql_session_interface() {}

  virtual Error_code set_connection_type(const Connection_type type) = 0;
  virtual Error_code execute_kill_sql_session(uint64_t mysql_session_id) = 0;
  virtual bool is_killed() const = 0;
  virtual bool password_expired() const = 0;
  virtual std::string get_authenticated_user_name() const = 0;
  virtual std::string get_authenticated_user_host() const = 0;
  virtual bool has_authenticated_user_a_super_priv() const = 0;
  virtual uint64_t mysql_session_id() const = 0;
  virtual Error_code authenticate(
      const char *user, const char *host, const char *ip, const char *db,
      const std::string &passwd,
      const Authentication_interface &account_verification,
      bool allow_expired_passwords) = 0;
  virtual Error_code execute(const char *sql, std::size_t sql_len,
                             Resultset_interface *rset) = 0;
};

}  // namespace ngs

#endif  // NGS_SQL_SESSION_INTERFACE_H_
