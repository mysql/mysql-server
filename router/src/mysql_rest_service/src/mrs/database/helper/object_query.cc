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

#include "mrs/database/helper/object_query.h"
#include <algorithm>
#include <string>
#include <vector>
#include "mysql/harness/utility/string.h"

namespace mrs {
namespace database {

namespace {
std::string table_with_alias(const entry::ObjectField &field) {
  return mysqlrouter::quote_identifier(field.reference->schema_name, '`') +
         "." +
         mysqlrouter::quote_identifier(field.reference->object_name, '`') +
         " " + field.reference->table_alias;
}

std::string build_select_where(const std::string &base_table_name,
                               const entry::ObjectField &field) {
  std::string where_sql;

  for (const auto &c : field.reference->column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << base_table_name << c.first << field.reference->table_alias
        << c.second;

    where_sql += tmp.str() + " AND ";
  }
  if (!where_sql.empty()) {
    where_sql.resize(where_sql.length() - strlen(" AND "));
  }

  return where_sql;
}

std::string build_select_join(const std::string &base_table_name,
                              const entry::ObjectField &field) {
  std::string join_sql = " LEFT JOIN " + table_with_alias(field) + " ON ";

  std::string cond_sql;
  for (const auto &c : field.reference->column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << base_table_name << c.first << field.reference->table_alias
        << c.second;

    cond_sql += tmp.str() + " AND ";
  }
  if (!cond_sql.empty()) {
    cond_sql.resize(cond_sql.length() - strlen(" AND "));
  }
  return join_sql + cond_sql;
}

std::vector<std::string> build_select_item(const std::string &base_table_name,
                                           const entry::ObjectField &field) {
  if (field.reference) {
    std::string where_sql = build_select_where(base_table_name, field);

    if (field.reference->reduce_to_field_id) {
      const auto &rfield = field.reference->reduced_to_field();

      std::vector<std::string> body =
          build_select_item(field.reference->table_alias, rfield);

      if (field.reference->to_many) {
        if (field.reference->unnest) {
          return {(mysqlrouter::sqlstring("?") << field.name).str(),
                  "(SELECT " + body[1] + " FROM " + table_with_alias(field) +
                      " WHERE " + where_sql + ")"};
        } else {
          return {(mysqlrouter::sqlstring("?") << field.name).str(),
                  "(SELECT JSON_ARRAYAGG(" + body[1] + ") FROM " +
                      table_with_alias(field) + " WHERE " + where_sql + ")"};
        }
      } else {
        return {(mysqlrouter::sqlstring("?") << field.name).str(),
                "(SELECT " + body[1] + " FROM " + table_with_alias(field) +
                    " WHERE " + where_sql + ")"};
      }

    } else {
      std::vector<std::string> field_list;
      std::string joins_sql;

      for (const auto &f : field.reference->fields) {
        if (f->enabled) {
          auto fields = build_select_item(field.reference->table_alias, *f);

          std::copy(fields.begin(), fields.end(),
                    std::back_inserter(field_list));
          if (f->reference && f->reference->to_many && f->reference->unnest) {
            joins_sql += build_select_join(field.reference->table_alias, *f);
          }
        }
      }
      if (field.reference->to_many) {
        if (field.reference->unnest) {
          return field_list;
        } else {
          std::string fields_sql = mysql_harness::join(field_list, ", ");

          return {(mysqlrouter::sqlstring("?") << field.name).str(),
                  "(SELECT JSON_ARRAYAGG(JSON_OBJECT(" + fields_sql +
                      ")) FROM " + table_with_alias(field) + " " + joins_sql +
                      " WHERE " + where_sql + ")"};
        }
      } else {
        std::string fields_sql = mysql_harness::join(field_list, ", ");

        return {(mysqlrouter::sqlstring("?") << field.name).str(),
                "(SELECT JSON_OBJECT(" + fields_sql + ") FROM " +
                    table_with_alias(field) + " " + joins_sql + " WHERE " +
                    where_sql + " LIMIT 1)"};
      }
    }
  } else {
    return {(mysqlrouter::sqlstring("?") << field.name).str(),
            mysqlrouter::quote_identifier(base_table_name, '`') + "." +
                mysqlrouter::quote_identifier(field.db_name, '`')};
  }
}
}  // namespace

mysqlrouter::sqlstring build_sql_json_object(const entry::Object &object) {
  std::vector<std::string> select_fields;

  for (const auto &f : object.fields) {
    if (f->enabled) {
      auto fields = build_select_item("t", *f);

      std::copy(fields.begin(), fields.end(),
                std::back_inserter(select_fields));
    }
  }

  return mysqlrouter::sqlstring(
      mysql_harness::join(select_fields, ",\n\t").c_str());
}

}  // namespace database
}  // namespace mrs
