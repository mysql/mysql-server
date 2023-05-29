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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_QUERY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_QUERY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>
#include "mrs/database/entry/object.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class ObjectFieldFilter {
 public:
  static ObjectFieldFilter from_url_filter(const entry::Object &object,
                                           std::vector<std::string> filter);
  static ObjectFieldFilter from_object(const entry::Object &object);

  bool is_included(const std::string &prefix, const std::string &field) const;

 private:
  std::set<std::string> m_filter;
  bool m_exclusive = true;

  bool is_parent_included(const std::string &prefix) const;
};

class JsonQueryBuilder {
 public:
  explicit JsonQueryBuilder(const ObjectFieldFilter &filter)
      : m_filter(filter) {}

  void process_object(std::shared_ptr<entry::Object> object) {
    process_object(object, "");
  }

  mysqlrouter::sqlstring query() const {
    mysqlrouter::sqlstring q{"SELECT JSON_OBJECT(?) FROM ?"};

    q << select_items() << from_clause();

    return q;
  }

  mysqlrouter::sqlstring get_reference_base_table_column(
      const std::string &column_name);

  const mysqlrouter::sqlstring &select_items() const { return m_select_items; }
  mysqlrouter::sqlstring from_clause() const;

 private:
  const ObjectFieldFilter &m_filter;
  std::string m_path_prefix;
  mysqlrouter::sqlstring m_select_items;
  std::vector<std::shared_ptr<entry::FieldSource>> m_base_tables;
  std::vector<std::shared_ptr<entry::FieldSource>> m_joined_tables;

  void process_object(std::shared_ptr<entry::Object> object,
                      const std::string &path_prefix);

  mysqlrouter::sqlstring subquery_value(
      const std::string &base_table_name) const;

  mysqlrouter::sqlstring subquery_object(
      const std::string &base_table_name) const;

  mysqlrouter::sqlstring subquery_object_array(
      const std::string &base_table_name) const;

  mysqlrouter::sqlstring subquery_array(
      const std::string &base_table_name) const;

  mysqlrouter::sqlstring make_subselect_where(
      const std::string &base_table_name,
      std::shared_ptr<entry::JoinedTable> ref) const;

  mysqlrouter::sqlstring make_subquery(
      std::shared_ptr<entry::FieldSource> base_table,
      const entry::ObjectField &field) const;

  void add_field(std::shared_ptr<entry::FieldSource> base_table,
                 const entry::ObjectField &field);

  void add_field_value(std::shared_ptr<entry::FieldSource> base_table,
                       const entry::ObjectField &field);

  void add_joined_table(std::shared_ptr<entry::FieldSource> table);

  mysqlrouter::sqlstring join_condition(const entry::FieldSource &base_table,
                                        const entry::JoinedTable &table) const;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_QUERY_H_
