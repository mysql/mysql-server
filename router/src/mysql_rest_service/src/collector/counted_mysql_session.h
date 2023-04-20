/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_COLLECTOR_MYSQL_SQL_SESSION_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_COLLECTOR_MYSQL_SQL_SESSION_H_

#include "mysqlrouter/mysql_session.h"

namespace collector {

class CountedMySQLSession : public mysqlrouter::MySQLSession {
 public:
  CountedMySQLSession();
  ~CountedMySQLSession() override;

  void change_user(const std::string &user, const std::string &password,
                   const std::string &db) override;
  void reset() override;
  uint64_t prepare(const std::string &query) override;
  void prepare_execute(uint64_t ps_id, std::vector<enum_field_types> pt,
                       const RowProcessor &processor,
                       const FieldValidator &validator) override;
  void prepare_remove(uint64_t ps_id) override;

  void execute(
      const std::string &query) override;  // throws Error, std::logic_error

  void query(const std::string &query, const RowProcessor &processor,
             const FieldValidator &validator)
      override;  // throws Error, std::logic_error
  std::unique_ptr<MySQLSession::ResultRow> query_one(
      const std::string &query,
      const FieldValidator &validator) override;  // throws Error
  std::unique_ptr<MySQLSession::ResultRow> query_one(
      const std::string &query) override;  // throws Error
};

}  // namespace collector

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_COLLECTOR_MYSQL_SQL_SESSION_H_
