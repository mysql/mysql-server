/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_QUERY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_QUERY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "mrs/database/entry/object.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

using PrimaryKeyColumnValues = std::map<std::string, mysqlrouter::sqlstring>;
using ColumnValues = std::vector<mysqlrouter::sqlstring>;
using Tables = std::vector<std::shared_ptr<entry::Table>>;

mysqlrouter::sqlstring format_from_clause(const Tables &table,
                                          const Tables &join,
                                          bool is_table = true);

mysqlrouter::sqlstring format_where_expr(
    std::shared_ptr<database::entry::Table> table,
    const std::string &table_name, const PrimaryKeyColumnValues &f);

mysqlrouter::sqlstring format_where_expr(
    std::shared_ptr<database::entry::Table> table,
    const PrimaryKeyColumnValues &f);

mysqlrouter::sqlstring format_key_names(
    std::shared_ptr<database::entry::Table> table);

mysqlrouter::sqlstring format_parameters(
    std::shared_ptr<database::entry::Object> object, const ColumnValues &f);

mysqlrouter::sqlstring format_key(std::shared_ptr<database::entry::Table> table,
                                  const PrimaryKeyColumnValues &f);

mysqlrouter::sqlstring format_column_mapping(
    const entry::JoinedTable::ColumnMapping &map);

mysqlrouter::sqlstring format_left_join(const entry::Table &table,
                                        const entry::JoinedTable &join);

class ObjectFieldFilter {
 public:
  static ObjectFieldFilter from_url_filter(const entry::Object &object,
                                           std::vector<std::string> filter);
  static ObjectFieldFilter from_object(const entry::Object &object);

  bool is_included(std::string_view prefix, std::string_view field) const;

 private:
  std::set<std::string, std::less<>> m_filter;
  bool m_exclusive = true;

  bool is_parent_included(std::string_view prefix) const;
};

class JsonQueryBuilder {
 public:
  explicit JsonQueryBuilder(const ObjectFieldFilter &filter,
                            bool for_update = false, bool for_checksum = false,
                            bool for_bigins_as_string = false)
      : m_filter(filter),
        for_update_(for_update),
        for_checksum_(for_checksum),
        bigins_as_string_{for_bigins_as_string} {}

  void process_object(std::shared_ptr<entry::Object> object);

  mysqlrouter::sqlstring query() const {
    mysqlrouter::sqlstring q{"SELECT JSON_OBJECT(?) FROM ?"};

    q << select_items() << from_clause();

    if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

    return q;
  }

  mysqlrouter::sqlstring query_one(const PrimaryKeyColumnValues &pk) const {
    mysqlrouter::sqlstring q{"SELECT ?.?(?)"};

    q << select_items() << from_clause()
      << format_where_expr(m_object->get_base_table(), pk);

    if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

    return q;
  }

  const mysqlrouter::sqlstring &select_items() const { return m_select_items; }
  mysqlrouter::sqlstring from_clause() const;

 private:
  const ObjectFieldFilter &m_filter;
  std::shared_ptr<entry::Object> m_object;
  std::string m_path_prefix;
  mysqlrouter::sqlstring m_select_items;
  Tables m_base_tables;
  Tables m_joined_tables;
  bool for_update_ = false;
  bool for_checksum_ = false;
  bool bigins_as_string_ = false;

  void process_object(std::shared_ptr<entry::Object> object,
                      const std::string &path_prefix, bool unnest_to_first);

  mysqlrouter::sqlstring subquery_value() const;

  mysqlrouter::sqlstring subquery_object() const;

  mysqlrouter::sqlstring subquery_object_array() const;

  mysqlrouter::sqlstring subquery_array() const;

  mysqlrouter::sqlstring make_subselect_where(
      std::shared_ptr<entry::JoinedTable> ref) const;

  mysqlrouter::sqlstring make_subquery(
      const entry::ReferenceField &field) const;

  void add_field(std::shared_ptr<entry::ObjectField> field);

  void add_field_value(std::shared_ptr<entry::ObjectField> field);

  void add_joined_table(std::shared_ptr<entry::Table> table);
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_QUERY_H_
