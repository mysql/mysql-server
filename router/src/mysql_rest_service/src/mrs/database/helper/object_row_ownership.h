
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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_ROW_OWNERSHIP_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_ROW_OWNERSHIP_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "helper/json/to_sqlstring.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/database/entry/object.h"
#include "mrs/interface/object.h"

namespace mrs {
namespace database {

class ObjectRowOwnership {
 public:
  using UserId = mrs::database::entry::AuthUser::UserId;
  using RowUserOwnership = mrs::database::entry::RowUserOwnership;
  using VectorOfRowGroupOwnership =
      interface::Object::VectorOfRowGroupOwnership;

  ObjectRowOwnership(std::shared_ptr<entry::Table> table = {},
                     const RowUserOwnership *user_ownership = nullptr,
                     const std::optional<UserId> &user_id = {},
                     const VectorOfRowGroupOwnership &row_groups = {},
                     const std::set<UniversalId> &user_groups = {})
      : m_table(table),
        m_owner_column_name(
            (user_ownership && user_ownership->user_ownership_enforced)
                ? user_ownership->user_ownership_column
                : ""),
        m_user_id(!user_id.has_value() ? std::optional<mysqlrouter::sqlstring>()
                                       : to_sqlstring(*user_id)),
        m_row_groups(row_groups),
        m_user_groups(user_groups) {
    assert(!enabled() || user_id.has_value());
  }

  ObjectRowOwnership(std::shared_ptr<entry::Table> table,
                     const std::string &column_name,
                     const mysqlrouter::sqlstring &user_id,
                     const VectorOfRowGroupOwnership &row_groups = {},
                     const std::set<UniversalId> &user_groups = {})
      : m_table(table),
        m_owner_column_name(column_name),
        m_user_id(user_id),
        m_row_groups(row_groups),
        m_user_groups(user_groups) {}

  const mysqlrouter::sqlstring &owner_user_id() const { return *m_user_id; }

  const std::string &owner_column_name() const { return m_owner_column_name; }

  bool enabled() const { return !m_owner_column_name.empty(); }

  bool is_owner_id(const entry::Table &table,
                   const entry::Column &column) const {
    return enabled() && m_table->schema == table.schema &&
           m_table->table == table.table &&
           m_owner_column_name == column.column_name;
  }

  const VectorOfRowGroupOwnership &row_groups() const { return m_row_groups; }

  const std::set<UniversalId> user_groups() const { return m_user_groups; }

  mysqlrouter::sqlstring owner_check_expr() const {
    return mysqlrouter::sqlstring("(! = ?)")
           << owner_column_name() << owner_user_id();
  }

  mysqlrouter::sqlstring owner_check_expr(const std::string &table_name) const {
    return mysqlrouter::sqlstring("(!.! = ?)")
           << table_name << owner_column_name() << owner_user_id();
  }

 private:
  std::shared_ptr<entry::Table> m_table;
  std::string m_owner_column_name;
  std::optional<mysqlrouter::sqlstring> m_user_id;
  const VectorOfRowGroupOwnership &m_row_groups;
  const std::set<UniversalId> &m_user_groups;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_ROW_OWNERSHIP_H_
