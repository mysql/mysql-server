/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "mrs/database/query_rest_table.h"

#include <map>
#include <stdexcept>

#include "helper/json/text_to.h"
#include "helper/mysql_column.h"
#include "mrs/database/filter_object_generator.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

class ColumnDefinitionIterator
    : public mysqlrouter::sqlstring::CustomContainerIterator<
          std::vector<helper::Column>::const_iterator,
          ColumnDefinitionIterator> {
 public:
  using CustomContainerIterator::CustomContainerIterator;

  /**
   * Serialize the ColumnDefinition to string.
   *
   * This method returns `sqlstring` because streaming it to `sqlstring` doesn't
   * force quoting or escaping. Thus it allows to concat two SQLs.
   */
  mysqlrouter::sqlstring operator*() {
    mysqlrouter::sqlstring fmt{"?,!"};
    fmt << it_->name << it_->name;
    return fmt;
  }
};

void QueryRestTable::query_entries(
    MySQLSession *session, const std::vector<Column> &columns,
    const std::string &schema, const std::string &object, const uint32_t offset,
    const uint32_t limit, const std::string &url_route,
    const std::string &primary, const bool is_default_limit,
    const RowUserOwnership &row_user, UserId *user_id,
    const std::vector<RowGroupOwnership> &row_groups,
    const std::set<UniversalId> &user_groups, const std::string &q) {
  if (columns.empty()) throw std::invalid_argument("Empty column list.");
  build_query(columns, schema, object, offset, limit + 1, url_route, primary,
              row_user, user_id, row_groups, user_groups, q);

  serializer_.begin(offset, limit, is_default_limit, url_route);
  execute(session);
  serializer_.end();

  response = serializer_.get_result();
}

void QueryRestTable::on_metadata(unsigned number, MYSQL_FIELD *fields) {
  for (unsigned i = 0; i < number; ++i) columns_.emplace_back(&fields[i]);
}

void QueryRestTable::on_row(const Row &r) {
  serializer_.push_json_document(r[0]);
}

const mysqlrouter::sqlstring &QueryRestTable::build_where(
    const RowUserOwnership &row_user, UserId *user_id,
    const std::vector<RowGroupOwnership> &row_groups,
    const std::set<UniversalId> &user_groups) {
  using MatchLevel = entry::RowGroupOwnership::MatchLevel;
  static std::map<MatchLevel, mysqlrouter::sqlstring> operations{
      {MatchLevel::kHigher, ">"},
      {MatchLevel::kEqualOrHigher, ">="},
      {MatchLevel::kEqual, "="},
      {MatchLevel::kLowerOrEqual, "<="},
      {MatchLevel::kLower, "<"}};

  static std::string empty;
  auto &user_ownership_column =
      row_user.user_ownership_enforced ? row_user.user_ownership_column : empty;

  if (user_ownership_column.empty() && row_groups.empty())
    return mysqlrouter::sqlstring::empty;

  where_.reset("WHERE !");
  if (row_groups.empty()) {
    if (!user_id) {
      where_.reset("WHERE ! is NULL");

      where_ << user_ownership_column;
      return where_;
    }

    where_.reset(
        "WHERE (! IN (WITH RECURSIVE cte As ("
        "SELECT a.id as id FROM mysql_rest_service_metadata.auth_user a WHERE "
        "a.id = ? "
        "UNION ALL "
        "SELECT h.auth_user_id as id FROM "
        "mysql_rest_service_metadata.user_hierarchy as h "
        "JOIN cte c ON c.id=h.reporting_to_user_id"
        ") SELECT * FROM cte) OR ! is NULL)");

    where_ << user_ownership_column << to_sqlstring(*user_id)
           << user_ownership_column;

    return where_;
  }

  if (user_ownership_column.empty()) {
    std::string query{"WHERE ("};
    for (size_t i = 0; i < row_groups.size(); ++i) {
      query +=
          " ! in (WITH RECURSIVE cte AS("
          "SELECT user_group_id, parent_group_id, level FROM "
          "mysql_rest_service_metadata.user_group_hierarchy WHERE "
          "user_group_id in (?) AND group_hierarchy_type_id=? "
          "UNION ALL "
          "SELECT p.user_group_id, p.parent_group_id, p.level "
          "FROM mysql_rest_service_metadata.user_group_hierarchy as p JOIN "
          "cte on p.user_group_id = cte.parent_group_id AND "
          "group_hierarchy_type_id=? ) "
          "SELECT parent_group_id FROM cte  WHERE level ! ? ) OR ";
    }

    query += "(";
    for (size_t i = 0; i < row_groups.size(); ++i) {
      query += i != 0 ? "AND ! is NULL " : "! is NULL ";
    }
    query += ")) ";

    where_.reset(query.c_str());

    for (auto &group : row_groups) {
      where_ << group.row_group_ownership_column << user_groups
             << group.hierarhy_id << group.hierarhy_id
             << operations[group.match] << group.level;
    }

    for (auto &group : row_groups) {
      where_ << group.row_group_ownership_column;
    }

    return where_;
  }

  std::string query{
      "WHERE (! IN (WITH RECURSIVE cte As ("
      "SELECT a.id as id FROM mysql_rest_service_metadata.auth_user a WHERE"
      "a.id = ? "
      "UNION ALL "
      "SELECT h.auth_user_id as id FROM "
      "mysql_rest_service_metadata.user_hierarchy as h "
      "JOIN cte c ON c.id=h.reporting_to_user_id"
      ") SELECT * FROM cte) OR "};

  for (size_t i = 0; i < row_groups.size(); ++i) {
    query +=
        " ! in (WITH RECURSIVE cte AS("
        "SELECT user_group_id, parent_group_id, level FROM "
        "mysql_rest_service_metadata.user_group_hierarchy WHERE "
        "user_group_id in (?) AND group_hierarchy_type_id=? "
        "UNION ALL "
        "SELECT p.user_group_id, p.parent_group_id, p.level "
        "FROM mysql_rest_service_metadata.user_group_hierarchy as p JOIN "
        "cte on p.user_group_id = cte.parent_group_id AND "
        "group_hierarchy_type_id=? ) "
        "SELECT parent_group_id FROM cte  WHERE level ! ? ) OR ";
  }

  query += "( ! is NULL ";
  for (size_t i = 0; i < row_groups.size(); ++i) {
    query += "AND ! is NULL ";
  }
  query += ")) ";
  where_.reset(query.c_str());
  where_ << user_ownership_column << to_sqlstring(*user_id);

  for (auto &group : row_groups) {
    where_ << group.row_group_ownership_column << user_groups
           << group.hierarhy_id << group.hierarhy_id << operations[group.match]
           << group.level;
  }

  where_ << user_ownership_column;
  for (auto &group : row_groups) {
    where_ << group.row_group_ownership_column;
  }

  return where_;
}

void extend_where(mysqlrouter::sqlstring &where, const std::string &query) {
  using namespace std::literals::string_literals;
  if (query.empty()) return;
  FilterObjectGenerator fog;
  fog.parse(helper::json::text_to_document(query));
  auto result = fog.get_result();
  if (result.empty()) return;

  bool is_empty = where.str().empty();

  mysqlrouter::sqlstring r{"? ? ?"};
  r << where << mysqlrouter::sqlstring(is_empty ? "WHERE" : "AND")
    << mysqlrouter::sqlstring(result.c_str());
  where = r;
}

void QueryRestTable::build_query(
    const std::vector<Column> &columns, const std::string &schema,
    const std::string &object, const uint32_t offset, const uint32_t limit,
    const std::string &url, const std::string &primary,
    const RowUserOwnership &row_user, UserId *user_id,
    const std::vector<RowGroupOwnership> &row_groups,
    const std::set<UniversalId> &user_groups, const std::string &query) {
  auto where = build_where(row_user, user_id, row_groups, user_groups);
  extend_where(where, query);
  query_ = {"SELECT JSON_OBJECT(?, ?) FROM !.! ? LIMIT ?,?"};
  query_ << ColumnDefinitionIterator::from_container(columns);

  if (primary.empty()) {
    static mysqlrouter::sqlstring empty_links{"'links', JSON_ARRAY()"};
    query_ << empty_links;
  } else {
    mysqlrouter::sqlstring fmt{
        "'links', "
        "JSON_ARRAY(JSON_OBJECT('rel','self','href',CONCAT(?,'/',!)))"};
    fmt << url << primary;
    query_ << fmt;
  }

  query_ << schema << object << where << offset << limit;
}

}  // namespace database
}  // namespace mrs
