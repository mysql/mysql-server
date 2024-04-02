/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_H_

#include <optional>

#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class Query {
 public:
  using MySQLSession = mysqlrouter::MySQLSession;
  using Row = MySQLSession::Row;
  using ResultRow = MySQLSession::ResultRow;

 public:
  virtual ~Query() = default;

  virtual void execute(MySQLSession *session);
  virtual void query(MySQLSession *session, const std::string &q);
  virtual void prepare_and_execute(MySQLSession *session, const std::string &q,
                                   std::vector<enum_field_types> pt);

  std::unique_ptr<MySQLSession::ResultRow> query_one(MySQLSession *session);
  std::unique_ptr<MySQLSession::ResultRow> query_one(MySQLSession *session,
                                                     const std::string &q);

  virtual void on_row(const ResultRow &r);
  virtual void on_metadata(unsigned number, MYSQL_FIELD *fields);

  mysqlrouter::sqlstring query_;
  std::optional<std::string> sqlstate_;
  MYSQL_FIELD *metadata_{nullptr};
  unsigned no_od_metadata_{0};
};

class QueryLog : public Query {
 public:
  void query(MySQLSession *session, const std::string &q) override;
  void prepare_and_execute(MySQLSession *session, const std::string &q,
                           std::vector<enum_field_types> pt) override;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_H_
