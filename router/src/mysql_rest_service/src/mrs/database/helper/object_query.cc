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
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "helper/string/contains.h"
#include "mysql/harness/utility/string.h"

#include <iostream>

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

void JsonQueryBuilder::process_object(std::shared_ptr<entry::Object> object,
                                      const std::string &path_prefix) {
  m_base_tables = object->base_tables;
  m_path_prefix = path_prefix;

  if (auto jtable =
          std::dynamic_pointer_cast<entry::JoinedTable>(m_base_tables.back());
      jtable && jtable->reduce_to_field) {
    assert(object->fields.empty() ||
           (object->fields.size() == 1 &&
            object->fields[0] == jtable->reduce_to_field));
    add_field_value(jtable, *jtable->reduce_to_field);
    return;
  }

  for (const auto &f : object->fields) {
    if (!f.get()) {
      log_debug("While building field list, detected field not initialized.");
      continue;
    }
    add_field(m_base_tables.back(), *f);
  }
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_value(
    const std::string &base_table_name) const {
  mysqlrouter::sqlstring q{"(SELECT ? FROM ? WHERE ? LIMIT 1)"};

  q << select_items() << from_clause()
    << make_subselect_where(base_table_name,
                            std::dynamic_pointer_cast<entry::JoinedTable>(
                                m_base_tables.front()));

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_object(
    const std::string &base_table_name) const {
  mysqlrouter::sqlstring q{"(SELECT JSON_OBJECT(?) FROM ? WHERE ? LIMIT 1)"};

  q << select_items() << from_clause()
    << make_subselect_where(base_table_name,
                            std::dynamic_pointer_cast<entry::JoinedTable>(
                                m_base_tables.front()));

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_object_array(
    const std::string &base_table_name) const {
  mysqlrouter::sqlstring q{
      "(SELECT JSON_ARRAYAGG(JSON_OBJECT(?)) FROM ? WHERE ?)"};

  q << select_items() << from_clause()
    << make_subselect_where(base_table_name,
                            std::dynamic_pointer_cast<entry::JoinedTable>(
                                m_base_tables.front()));

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_array(
    const std::string &base_table_name) const {
  mysqlrouter::sqlstring q{"(SELECT JSON_ARRAYAGG(?) FROM ? WHERE ?)"};

  q << select_items() << from_clause()
    << make_subselect_where(base_table_name,
                            std::dynamic_pointer_cast<entry::JoinedTable>(
                                m_base_tables.front()));

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::make_subselect_where(
    const std::string &base_table_name,
    std::shared_ptr<entry::JoinedTable> ref) const {
  mysqlrouter::sqlstring where_sql;

  for (const auto &c : ref->column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << base_table_name << c.first << ref->table_alias << c.second;
    where_sql.append_preformatted_sep(" AND ", tmp);
  }

  return where_sql;
}

mysqlrouter::sqlstring JsonQueryBuilder::make_subquery(
    std::shared_ptr<entry::FieldSource> base_table,
    const entry::ObjectField &field) const {
  JsonQueryBuilder subquery(m_filter);

  subquery.process_object(
      field.nested_object,
      m_path_prefix.empty() ? field.name : m_path_prefix + "." + field.name);

  auto nested_table = field.nested_object->base_tables.back();

  auto nested_join =
      std::dynamic_pointer_cast<entry::JoinedTable>(nested_table);
  if (nested_join->to_many) {
    if (nested_join->reduce_to_field)
      return subquery.subquery_array(base_table->table_alias);
    else
      return subquery.subquery_object_array(base_table->table_alias);
  } else {
    if (nested_join->reduce_to_field)
      return subquery.subquery_value(base_table->table_alias);
    else
      return subquery.subquery_object(base_table->table_alias);
  }
}

static mysqlrouter::sqlstring get_field_format(const std::string &data_type,
                                               bool value_only) {
  if (helper::starts_with(data_type, "bit")) {
    if (data_type.length() == 3 ||
        helper::contains(data_type.c_str() + 3, "(1)"))
      return {value_only ? "!.! is true" : "?, !.! is true"};
    return {value_only ? "TO_BASE64(!.!)" : "?, TO_BASE64(!.!)"};
  }
  if (helper::starts_with(data_type, "binary") ||
      helper::starts_with(data_type, "blob"))
    return {value_only ? "TO_BASE64(!.!)" : "?, TO_BASE64(!.!)"};

  return {value_only ? "!.!" : "?, !.!"};
}

void JsonQueryBuilder::add_field(std::shared_ptr<entry::FieldSource> base_table,
                                 const entry::ObjectField &field) {
  if (!m_filter.is_included(m_path_prefix, field.name)) return;

  if (!field.enabled) return;

  if (field.nested_object) {
    auto item = mysqlrouter::sqlstring("?, ");
    item << field.name;
    m_select_items.append_preformatted_sep(", ", item);
    m_select_items.append_preformatted(make_subquery(base_table, field));
  } else {
    auto item = get_field_format(field.db_datatype, false);
    item << field.name << field.source->table_alias << field.db_name;
    m_select_items.append_preformatted_sep(", ", item);

    add_joined_table(field.source);
  }
}

void JsonQueryBuilder::add_field_value(
    std::shared_ptr<entry::FieldSource> base_table,
    const entry::ObjectField &field) {
  if (field.nested_object) {
    m_select_items.append_preformatted_sep(", ",
                                           make_subquery(base_table, field));
  } else {
    auto item = get_field_format(field.db_datatype, true);
    item << field.source->table_alias << field.db_name;
    m_select_items.append_preformatted_sep(", ", item);

    add_joined_table(field.source);
  }
}

void JsonQueryBuilder::add_joined_table(
    std::shared_ptr<entry::FieldSource> table) {
  if (std::find(m_base_tables.begin(), m_base_tables.end(), table) !=
      m_base_tables.end())
    return;

  if (std::find(m_joined_tables.begin(), m_joined_tables.end(), table) ==
      m_joined_tables.end())
    m_joined_tables.push_back(table);
}

mysqlrouter::sqlstring JsonQueryBuilder::join_condition(
    const entry::FieldSource &base_table,
    const entry::JoinedTable &table) const {
  mysqlrouter::sqlstring cond;

  for (const auto &c : table.column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << base_table.table_alias << c.first << table.table_alias << c.second;
    cond.append_preformatted_sep(" AND ", tmp);
  }

  return cond;
}

mysqlrouter::sqlstring JsonQueryBuilder::get_reference_base_table_column(
    const std::string &column_name) {
  mysqlrouter::sqlstring ref{"!.!"};
  return ref << m_base_tables.front()->table_alias << column_name;
}

mysqlrouter::sqlstring JsonQueryBuilder::from_clause() const {
  mysqlrouter::sqlstring from{"!.! as !"};
  from << m_base_tables.front()->schema << m_base_tables.front()->table
       << m_base_tables.front()->table_alias;

  for (size_t i = 1; i < m_base_tables.size(); i++) {
    mysqlrouter::sqlstring join{" LEFT JOIN !.! as ! ON ?"};
    join << m_base_tables[i]->schema << m_base_tables[i]->table
         << m_base_tables[i]->table_alias
         << join_condition(*m_base_tables[i - 1],
                           *std::dynamic_pointer_cast<entry::JoinedTable>(
                               m_base_tables[i]));

    from.append_preformatted(join);
  }

  for (size_t i = 0; i < m_joined_tables.size(); i++) {
    mysqlrouter::sqlstring join{" LEFT JOIN !.! as ! ON ?"};
    join << m_joined_tables[i]->schema << m_joined_tables[i]->table
         << m_joined_tables[i]->table_alias
         << join_condition(*m_base_tables.back(),
                           *std::dynamic_pointer_cast<entry::JoinedTable>(
                               m_joined_tables[i]));

    from.append_preformatted(join);
  }

  return from;
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
    const entry::Object &, std::vector<std::string> filter) {
  ObjectFieldFilter object_filter;

  object_filter.m_exclusive = is_exclude_filter(filter);

  for (auto &f : filter) {
    // XXX check if the field is valid

    if (object_filter.m_exclusive && !f.empty() && f[0] == '!') f.erase(0, 1);
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

  // if parent is included, check if there are any field of the parent included
  if (auto it = m_filter.find(field); it != m_filter.end()) {
    ++it;  // set iterator is sorted, so the next item is either something
           // unrelated or a subfield that shares the prefix
    if (it != m_filter.end() &&
        it->compare(0, field.length() + 1, field + ".") == 0) {
      return false;
    }
    return true;
  } else {
    auto last_part = field.rfind('.');
    if (last_part == std::string::npos) return false;
    auto prefix = field.substr(0, last_part);
    return is_parent_included(prefix);
  }
}

bool ObjectFieldFilter::is_included(const std::string &prefix,
                                    const std::string &field) const {
  if (m_exclusive) {
    return m_filter.count(prefix.empty() ? field : prefix + "." + field) == 0;
  } else {
    if (m_filter.count(prefix.empty() ? field : prefix + "." + field) != 0) {
      return true;
    }
    if (is_parent_included(prefix)) {
      return true;
    }
    return false;
  }
}

}  // namespace database
}  // namespace mrs
