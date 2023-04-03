/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mrs/database/query_rest_table_single_row.h"
#include <stdexcept>
#include "mrs/database/helper/object_query.h"

namespace helper {

/**
 * Convert custom type to string.
 *
 * This function was create for `sqlstring`, which uses `std::to_string`
 * function for conversion, still when no function is found then it uses
 * ADL to lookup for custom/application types.
 *
 * Additionally the function is marked as static, in case when other
 * compilation unit would like to define its own conversion `way`.
 */
/*static std::string to_string(const Column &cd) {
  mysqlrouter::sqlstring fmt{"?, !"};
  fmt << cd.name << cd.name;
  return fmt.str();
}*/

}  // namespace helper

namespace mrs {
namespace database {

void QueryRestTableSingleRow::query_entries(
    MySQLSession *session, std::shared_ptr<database::entry::Object> object,
    const ObjectFieldFilter &field_filter,
    [[maybe_unused]] const std::string &primary_key,
    const mysqlrouter::sqlstring &pri_value, const std::string &url_route) {
  response = "";
  items = 0;
  build_query(object, field_filter, primary_key, pri_value, url_route);

  execute(session);
}

void QueryRestTableSingleRow::query_last_inserted(
    MySQLSession *session, std::shared_ptr<database::entry::Object> object,
    const ObjectFieldFilter &field_filter, const std::string &primary_key,
    const std::string &url_route) {
  response = "";
  items = 0;
  build_query_last_inserted(object, field_filter, primary_key, url_route);

  execute(session);
}

void QueryRestTableSingleRow::on_row(const Row &r) {
  if (!response.empty())
    throw std::runtime_error(
        "Querying single row, from a table. Received multiple.");
  response.append(r[0]);
  ++items;
}

void QueryRestTableSingleRow::build_query(
    std::shared_ptr<database::entry::Object> object,
    const ObjectFieldFilter &field_filter, const std::string &primary_key,
    const mysqlrouter::sqlstring &pri_value, const std::string &url_route) {
  query_ =
      "SELECT JSON_OBJECT(?,'links', JSON_ARRAY(JSON_OBJECT('rel', 'self', "
      "'href', CONCAT(?,'/',?)))) FROM ? WHERE !=?;";

  JsonQueryBuilder qb(field_filter);
  qb.process_object(object);

  query_ << qb.select_items() << url_route << pri_value << qb.from_clause()
         << primary_key << pri_value;
}

void QueryRestTableSingleRow::build_query_last_inserted(
    std::shared_ptr<database::entry::Object> object,
    const ObjectFieldFilter &field_filter, const std::string &primary_key,
    const std::string &url_route) {
  query_ =
      "SELECT JSON_OBJECT(?,'links', JSON_ARRAY(JSON_OBJECT('rel', 'self', "
      "'href', CONCAT(?,'/',!)))) FROM ? WHERE !=!;";

  JsonQueryBuilder qb(field_filter);
  qb.process_object(object);

  static mysqlrouter::sqlstring last_inserted{"LAST_INSERT_ID()"};
  query_ << qb.select_items() << url_route << last_inserted << qb.from_clause()
         << primary_key << last_inserted;
}

}  // namespace database
}  // namespace mrs
