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
#include <set>
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

std::vector<std::string> build_select_item(
    const std::string &base_table_name, const entry::ObjectField &field,
    const std::string &prefix, const ObjectFieldFilter &field_filter) {
  if (field.reference) {
    std::string where_sql = build_select_where(base_table_name, field);

    if (field.reference->reduce_to_field_id) {
      const auto &rfield = field.reference->reduced_to_field();

      std::vector<std::string> body =
          build_select_item(field.reference->table_alias, rfield,
                            prefix + field.name + ".", field_filter);

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

      std::string nprefix = prefix;
      if (!field.reference->unnest)
        nprefix += field.name;
      else if (!nprefix.empty())
        nprefix.resize(nprefix.length() - 1);

      for (const auto &f : field.reference->fields) {
        if (f->enabled) {
          std::string f_path =
              nprefix +
              (!f->reference || !f->reference->unnest ? "." + f->name : "");
          if (field_filter.is_included(f_path)) {
            auto fields = build_select_item(field.reference->table_alias, *f,
                                            nprefix + ".", field_filter);

            std::copy(fields.begin(), fields.end(),
                      std::back_inserter(field_list));
            if (f->reference && f->reference->to_many && f->reference->unnest) {
              joins_sql += build_select_join(field.reference->table_alias, *f);
            }
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

mysqlrouter::sqlstring build_sql_json_object(
    const entry::Object &object, const ObjectFieldFilter &field_filter) {
  std::vector<std::string> select_fields;
  for (const auto &f : object.fields) {
    if (f->enabled && field_filter.is_included(f->name)) {
      auto fields = build_select_item("t", *f, "", field_filter);

      std::copy(fields.begin(), fields.end(),
                std::back_inserter(select_fields));
    }
  }

  return mysqlrouter::sqlstring(
      mysql_harness::join(select_fields, ",\n\t").c_str());
}

namespace {
bool is_exclude_filter(const std::vector<std::string> &filter) {
  std::optional<bool> is_exclude;

  for (const auto &f : filter) {
    if (!f.empty()) {
      if (!is_exclude.has_value()) {
        is_exclude = f[0] == '!';
      } else {
        if (*is_exclude != (f[0] == '!')) {
          throw std::invalid_argument(
              "Filter must not mix inclusions and exclusions");
        }
      }
    }
  }
  return is_exclude.value_or(true);
}

void insert_parents(const std::string &f, std::set<std::string> *filter) {
  auto pos = f.rfind('.');
  if (pos != std::string::npos) {
    auto prefix = f.substr(0, pos);
    filter->insert(prefix);
    insert_parents(prefix, filter);
  }
}

}  // namespace

ObjectFieldFilter ObjectFieldFilter::from_url_filter(
    const entry::Object &, const std::vector<std::string> &filter) {
  ObjectFieldFilter object_filter;

  object_filter.m_exclusive = is_exclude_filter(filter);

  for (const auto &f : filter) {
    object_filter.m_filter.insert(f);
    // ensure parents of subfields are included too
    if (!object_filter.m_exclusive) insert_parents(f, &object_filter.m_filter);
  }

  return object_filter;
}

ObjectFieldFilter ObjectFieldFilter::from_object(const entry::Object &) {
  ObjectFieldFilter object_filter;

  // excludes nothing
  object_filter.m_exclusive = true;

  return object_filter;
}

bool ObjectFieldFilter::is_parent_included(const std::string &field) const {
  if (field.empty()) return false;

  auto last_part = field.rfind('.');
  if (last_part == std::string::npos) return false;

  // if parent is included, check if there are any field of the parent included
  auto prefix = field.substr(0, last_part);
  if (auto it = m_filter.find(prefix); it != m_filter.end()) {
    ++it;  // set iterator is sorted, so the next item is either something
           // unrelated or a subfield
    if (it != m_filter.end() && it->compare(0, prefix.length() + 1, field) == 0)
      return false;
    return true;
  } else {
    return is_parent_included(prefix);
  }
}

bool ObjectFieldFilter::is_included(const std::string &field) const {
  if (m_exclusive) {
    return m_filter.count(field) == 0;
  } else {
    if (m_filter.count(field) != 0) {
      return true;
    }
    if (is_parent_included(field)) {
      return true;
    }
    return false;
  }
}

size_t ObjectFieldFilter::num_included_fields() const {
  return m_filter.size();  // TODO XXX
}

std::string ObjectFieldFilter::get_first_included() const {
  return "";  // XXX
}

}  // namespace database
}  // namespace mrs
