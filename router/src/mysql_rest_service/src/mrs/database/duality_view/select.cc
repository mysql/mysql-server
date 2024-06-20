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

#include "mrs/database/duality_view/select.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "helper/string/contains.h"
#include "mysql/harness/utility/string.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {
namespace dv {

namespace {

mysqlrouter::sqlstring format_pk(const std::string &table_name,
                                 const std::string &column_name) {
  if (table_name.empty()) return mysqlrouter::sqlstring("!") << column_name;
  return mysqlrouter::sqlstring("!.!") << table_name << column_name;
}
}  // namespace

mysqlrouter::sqlstring format_join_where_expr(const Table &table,
                                              const ForeignKeyReference &fk) {
  mysqlrouter::sqlstring cond;

  for (const auto &col : fk.column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << table.table_alias << col.first << fk.ref_table->table_alias
        << col.second;
    cond.append_preformatted_sep(" AND ", tmp);
  }

  return cond;
}

mysqlrouter::sqlstring format_where_expr(const Table &table,
                                         const PrimaryKeyColumnValues &f,
                                         bool omit_row_owner) {
  // If the request generates JOINs, then table_alias is required
  return format_where_expr(table, table.table_alias, f, omit_row_owner);
}

mysqlrouter::sqlstring format_key_names(const Table &table) {
  mysqlrouter::sqlstring s;
  for (const auto c : table.primary_key()) {
    mysqlrouter::sqlstring col;
    if (c->type == entry::ColumnType::BINARY)
      col = mysqlrouter::sqlstring("TO_BASE64(!.!)");
    else if (c->type == entry::ColumnType::GEOMETRY)
      col = mysqlrouter::sqlstring("ST_AsGeoJSON(!.!)");
    else
      col = mysqlrouter::sqlstring("!.!");
    col << table.table_alias << c->column_name;

    s.append_preformatted_sep(", ", col);
  }
  return s;
}

mysqlrouter::sqlstring format_key(const Table &table,
                                  const PrimaryKeyColumnValues &f) {
  mysqlrouter::sqlstring s;
  for (const auto &c : f) {
    auto column = table.get_column_or_throw(c.first);

    if (column->type == entry::ColumnType::BINARY)
      s.append_preformatted_sep(",", mysqlrouter::sqlstring("TO_BASE64(?)")
                                         << c.second);
    else if (column->type == entry::ColumnType::GEOMETRY)
      s.append_preformatted_sep(",", mysqlrouter::sqlstring("St_AsGeoJSON(?)")
                                         << c.second);
    else
      s.append_preformatted_sep(",", c.second);
  }
  return s;
}

mysqlrouter::sqlstring format_where_expr(const Table &table,
                                         const std::string &table_name,
                                         const PrimaryKeyColumnValues &f,
                                         bool omit_row_owner) {
  mysqlrouter::sqlstring s;

  for (const auto &c : f) {
    auto column = table.get_column_or_throw(c.first);
    auto type = column->type;

    if (omit_row_owner && column->is_row_owner) continue;

    mysqlrouter::sqlstring col;
    if (table_name.empty()) {
      col = mysqlrouter::sqlstring("! = ?");
    } else if (type == entry::ColumnType::BINARY) {
      col = mysqlrouter::sqlstring("cast(! as BINARY) = ?");
    } else {
      col = mysqlrouter::sqlstring("!.! = ?");
      col << table_name;
    }

    if (type == entry::ColumnType::BINARY) {
      col << (mysqlrouter::sqlstring{"!"}
              << format_pk(table_name, column->column_name));
    } else {
      col << column->column_name;
    }

    col << c.second;

    s.append_preformatted_sep(" AND ", col);
  }

  return s;
}

void JsonQueryBuilder::process_view(std::shared_ptr<entry::DualityView> view) {
  process_table({}, view, "");

  if (m_select_items.str().empty()) {
    throw std::runtime_error("Invalid duality view metadata");
  }
}

void JsonQueryBuilder::process_table(std::shared_ptr<Table> parent_table,
                                     std::shared_ptr<Table> table,
                                     const std::string &path_prefix) {
  parent_table_ = parent_table;
  table_ = table;
  m_path_prefix = path_prefix;

  for (const auto &field : table->fields) {
    if (auto column = std::dynamic_pointer_cast<Column>(field)) {
      add_column_field(*column);
    } else if (auto reference =
                   std::dynamic_pointer_cast<ForeignKeyReference>(field)) {
      add_reference_field(*reference);
    }
  }
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_object(
    const ForeignKeyReference &fk) const {
  mysqlrouter::sqlstring q{
      "COALESCE((SELECT JSON_OBJECT(?) FROM ? WHERE ? LIMIT 1"};

  q << select_items() << from_clause() << make_subselect_where(fk);

  if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

  q.append_preformatted("), JSON_OBJECT())");

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_object_array(
    const ForeignKeyReference &fk) const {
  mysqlrouter::sqlstring q{
      "COALESCE((SELECT JSON_ARRAYAGG(JSON_OBJECT(?)) FROM ? WHERE ?"};

  q << select_items() << from_clause() << make_subselect_where(fk);

  if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

  q.append_preformatted("), JSON_ARRAY())");

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::make_subselect_where(
    const ForeignKeyReference &ref) const {
  return format_join_where_expr(*parent_table_, ref);
}

mysqlrouter::sqlstring JsonQueryBuilder::make_subquery(
    const ForeignKeyReference &fk) const {
  JsonQueryBuilder subquery(filter_, row_owner_, for_update_);

  std::string path = m_path_prefix;
  if (!path.empty() && !fk.name.empty()) path.append(".");
  path.append(fk.name);

  subquery.process_table(table_, fk.ref_table, path);

  mysqlrouter::sqlstring subq("(");

  if (fk.to_many)
    subq.append_preformatted(subquery.subquery_object_array(fk));
  else
    subq.append_preformatted(subquery.subquery_object(fk));

  subq.append_preformatted(")");
  return subq;
}

static mysqlrouter::sqlstring get_field_format(entry::ColumnType type,
                                               bool value_only) {
  if (type == entry::ColumnType::BOOLEAN)
    return {value_only ? "!.! is true" : "?, !.! is true"};
  else if (type == entry::ColumnType::BINARY)
    return {value_only ? "TO_BASE64(!.!)" : "?, TO_BASE64(!.!)"};
  else if (type == entry::ColumnType::GEOMETRY)
    return {value_only ? "ST_AsGeoJSON(!.!)" : "?, ST_AsGeoJSON(!.!)"};
  return {value_only ? "!.!" : "?, !.!"};
}

static mysqlrouter::sqlstring get_field_format(entry::ColumnType type,
                                               const std::string &datatype,
                                               bool value_only,
                                               bool bigints_as_string) {
  if (bigints_as_string) {
    if (type == entry::ColumnType::INTEGER &&
        helper::icontains(datatype, "bigint"))
      return {value_only ? "CONVERT(!.!,CHAR)" : "?, CONVERT(!.!, CHAR)"};
    else if (type == entry::ColumnType::DOUBLE)
      return {value_only ? "CONVERT(!.!,CHAR)" : "?, CONVERT(!.!, CHAR)"};
  }

  return get_field_format(type, value_only);
}

void JsonQueryBuilder::add_column_field(const Column &column) {
  if (!column.enabled) return;

  auto item =
      get_field_format(column.type, column.datatype, false, bigins_as_string_);
  item << column.name << table_->table_alias << column.column_name;
  m_select_items.append_preformatted_sep(", ", item);
}

void JsonQueryBuilder::add_reference_field(const ForeignKeyReference &fk) {
  if (!fk.enabled) return;

  auto subquery = make_subquery(fk);
  auto item = mysqlrouter::sqlstring("?, ");
  item << fk.name;
  m_select_items.append_preformatted_sep(", ", item);
  m_select_items.append_preformatted(subquery);
}

mysqlrouter::sqlstring JsonQueryBuilder::from_clause() const {
  mysqlrouter::sqlstring from{"!.! as !"};
  from << table_->schema << table_->table << table_->table_alias;

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

void insert_parents(const std::string &f,
                    std::set<std::string, std::less<>> *filter) {
  auto pos = f.rfind('.');
  if (pos != std::string::npos) {
    auto prefix = f.substr(0, pos);
    filter->insert(prefix);
    insert_parents(prefix, filter);
  }
}

}  // namespace

ObjectFieldFilter ObjectFieldFilter::from_url_filter(
    const Table &, std::vector<std::string> filter) {
  ObjectFieldFilter object_filter;

  object_filter.m_exclusive = is_exclude_filter(filter);

  for (auto &f : filter) {
    if (object_filter.m_exclusive && !f.empty() && f[0] == '!') f.erase(0, 1);
    object_filter.filter_.insert(f);
    // ensure parents of subfields are included too
    if (!object_filter.m_exclusive) insert_parents(f, &object_filter.filter_);
  }
  return object_filter;
}

ObjectFieldFilter ObjectFieldFilter::from_object(const Table &) {
  ObjectFieldFilter object_filter;

  // excludes nothing
  object_filter.m_exclusive = true;

  return object_filter;
}

bool ObjectFieldFilter::is_parent_included(std::string_view field) const {
  if (field.empty()) return false;

  // if parent is included, check if there are any field of the parent included
  if (auto it = filter_.find(field); it != filter_.end()) {
    ++it;  // set iterator is sorted, so the next item is either something
           // unrelated or a subfield that shares the prefix
    if (it != filter_.end() && it->compare(0, field.length(), field) == 0 &&
        it->length() > field.length() && it->at(field.length()) == '.') {
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

bool ObjectFieldFilter::is_included(std::string_view prefix,
                                    std::string_view field) const {
  if (m_exclusive) {
    if (prefix.empty())
      return filter_.count(field) == 0;
    else
      return filter_.count(std::string(prefix).append(".").append(field)) == 0;
  } else {
    if (prefix.empty() && filter_.count(field))
      return true;
    else if (!prefix.empty() && field.empty()
                 ? filter_.count(prefix)
                 : filter_.count(std::string(prefix).append(".").append(field)))
      return true;
    if (is_parent_included(prefix)) {
      return true;
    }
    return false;
  }
}

bool ObjectFieldFilter::is_filter_configured() const {
  return filter_.size() != 0;
}

}  // namespace dv
}  // namespace database
}  // namespace mrs
