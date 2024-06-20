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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_SELECT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_SELECT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "mrs/database/entry/object.h"
#include "mrs/database/helper/object_row_ownership.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

// Mapping between column names to SQL values (strings must be 'quoted', binary
// values must be quoted as _binary'string')
using PrimaryKeyColumnValues = std::map<std::string, mysqlrouter::sqlstring>;

using ColumnValues = std::vector<mysqlrouter::sqlstring>;

namespace dv {

using Table = entry::Table;
using Field = entry::Field;
using Column = entry::Column;
using ForeignKeyReference = entry::ForeignKeyReference;

mysqlrouter::sqlstring format_key_names(const Table &table);

mysqlrouter::sqlstring format_key(const Table &table,
                                  const PrimaryKeyColumnValues &f);

mysqlrouter::sqlstring format_where_expr(const Table &table,
                                         const std::string &table_name,
                                         const PrimaryKeyColumnValues &f,
                                         bool omit_row_owner = false);

mysqlrouter::sqlstring format_where_expr(const Table &table,
                                         const PrimaryKeyColumnValues &f,
                                         bool omit_row_owner = false);

mysqlrouter::sqlstring format_join_where_expr(const Table &table,
                                              const ForeignKeyReference &fk);

class ObjectFieldFilter {
 public:
  static ObjectFieldFilter from_url_filter(const Table &table,
                                           std::vector<std::string> filter);
  static ObjectFieldFilter from_object(const Table &table);

  bool is_included(std::string_view prefix, std::string_view field) const;
  bool is_filter_configured() const;

 private:
  std::set<std::string, std::less<>> filter_;
  bool m_exclusive = true;

  bool is_parent_included(std::string_view prefix) const;
};

class JsonQueryBuilder {
 public:
  explicit JsonQueryBuilder(const ObjectFieldFilter &filter,
                            const ObjectRowOwnership &row_owner = {},
                            bool for_update = false,
                            bool for_bigins_as_string = false)
      : filter_(filter),
        row_owner_(row_owner),
        for_update_(for_update),
        bigins_as_string_{for_bigins_as_string} {}

  void process_view(std::shared_ptr<entry::DualityView> view);

  mysqlrouter::sqlstring query() const {
    mysqlrouter::sqlstring q{"SELECT JSON_OBJECT(?) FROM ?"};

    q << select_items() << from_clause();

    if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

    return q;
  }

  mysqlrouter::sqlstring query_one(const PrimaryKeyColumnValues &pk) const {
    mysqlrouter::sqlstring q{"SELECT ?.?(?)"};

    q << select_items() << from_clause() << format_where_expr(*table_, pk);

    if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

    return q;
  }

  const mysqlrouter::sqlstring &select_items() const { return m_select_items; }
  mysqlrouter::sqlstring from_clause() const;

 private:
  const ObjectFieldFilter &filter_;
  const ObjectRowOwnership &row_owner_;
  std::shared_ptr<Table> parent_table_;
  std::shared_ptr<Table> table_;
  std::string m_path_prefix;
  mysqlrouter::sqlstring m_select_items;
  bool for_update_ = false;
  bool bigins_as_string_ = false;

  void process_table(std::shared_ptr<Table> parent_table,
                     std::shared_ptr<Table> table,
                     const std::string &path_prefix);

  mysqlrouter::sqlstring subquery_object(const ForeignKeyReference &fk) const;
  mysqlrouter::sqlstring subquery_object_array(
      const ForeignKeyReference &fk) const;

  mysqlrouter::sqlstring make_subselect_where(
      const ForeignKeyReference &ref) const;

  mysqlrouter::sqlstring make_subquery(const ForeignKeyReference &ref) const;

  void add_column_field(const Column &column);
  void add_reference_field(const ForeignKeyReference &fk);
};

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_SELECT_H_
