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

#include "mrs/database/helper/object_query.h"
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

namespace {

mysqlrouter::sqlstring format_pk(const std::string &table_name,
                                 const std::string &column_name) {
  if (table_name.empty()) return mysqlrouter::sqlstring("!") << column_name;
  return mysqlrouter::sqlstring("!.!") << table_name << column_name;
}

void dump_table(std::shared_ptr<entry::Table> t, const std::string &indent) {
  if (auto bt = std::dynamic_pointer_cast<entry::BaseTable>(t)) {
    std::cout << indent << "- " << t->table << " (" << t->table_alias
              << "): BaseTable\n";
  } else if (auto jt = std::dynamic_pointer_cast<entry::JoinedTable>(t)) {
    std::cout << indent << "- " << t->table << " (" << t->table_alias
              << "): JoinedTable " << (jt->unnest ? "@unnest\n" : "\n");
  } else {
    std::cout << indent << "\t- " << t->table << ": TABLE (" << t->table_alias
              << ")\n";
  }
}

void dump_object(std::shared_ptr<entry::Object> object,
                 const std::string &indent = "") {
  std::cout << indent << "# " << object->name << "\n";
  std::cout << indent << "base tables:\n";
  for (const auto &t : object->base_tables) {
    dump_table(t, indent);
  }
  std::cout << indent << "fields:\n";
  for (const auto &f : object->fields) {
    if (auto rf = std::dynamic_pointer_cast<entry::ReferenceField>(f)) {
      if (rf->is_array())
        std::cout << indent << (f->enabled ? "+ " : "- ") << f->name
                  << ": ReferenceField[]\n";
      else
        std::cout << indent << (f->enabled ? "+ " : "- ") << f->name
                  << ": ReferenceField\n";
      dump_object(rf->nested_object, indent + "\t");
    } else if (auto df = std::dynamic_pointer_cast<entry::DataField>(f)) {
      std::cout << indent << (f->enabled ? "+ " : "- ") << f->name
                << ": DataField  ";
      std::cout << df->source->table.lock()->table << "." << df->source->name
                << "\n";
    } else {
      std::cout << indent << (f->enabled ? "+ " : "- ") << f->name
                << ": FIELD\n";
    }
  }
}

}  // namespace

mysqlrouter::sqlstring format_where_expr(
    std::shared_ptr<database::entry::Table> table,
    const std::string &table_name, const PrimaryKeyColumnValues &f) {
  mysqlrouter::sqlstring s;

  for (const auto &c : f) {
    auto type = table->get_column(c.first)->type;

    mysqlrouter::sqlstring col;
    if (table_name.empty() || type == entry::ColumnType::BINARY) {
      col = mysqlrouter::sqlstring("! = ?");
    } else {
      col = mysqlrouter::sqlstring("!.! = ?");
      col << table_name;
    }

    if (type == entry::ColumnType::BINARY) {
      col << (mysqlrouter::sqlstring{"cast(! as BINARY)"}
              << format_pk(table_name, c.first));
    } else {
      col << c.first;
    }

    col << c.second;

    s.append_preformatted_sep(" AND ", col);
  }

  return s;
}

mysqlrouter::sqlstring format_join_condition(const entry::JoinedTable &table) {
  mysqlrouter::sqlstring cond;

  assert(!table.column_mapping.empty());

  for (const auto &c : table.column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << c.first->table.lock()->table_alias << c.first->name
        << c.second->table.lock()->table_alias << c.second->name;
    cond.append_preformatted_sep(" AND ", tmp);
  }

  return cond;
}

mysqlrouter::sqlstring format_from_clause(const Tables &db_table,
                                          const Tables &db_join,
                                          bool is_table) {
  auto &base = db_table.front();

  if (is_table) {
    mysqlrouter::sqlstring from{"!.! as !"};
    from << base->schema << base->table << base->table_alias;

    for (size_t i = 1; i < db_table.size(); i++) {
      mysqlrouter::sqlstring join{" LEFT JOIN !.! as ! ON ?"};
      join << db_table[i]->schema << db_table[i]->table
           << db_table[i]->table_alias
           << format_join_condition(
                  *std::dynamic_pointer_cast<entry::JoinedTable>(db_table[i]));

      from.append_preformatted(join);
    }

    for (size_t i = 0; i < db_join.size(); i++) {
      mysqlrouter::sqlstring join{" LEFT JOIN !.! as ! ON ?"};
      join << db_join[i]->schema << db_join[i]->table << db_join[i]->table_alias
           << format_join_condition(
                  *std::dynamic_pointer_cast<entry::JoinedTable>(db_join[i]));

      from.append_preformatted(join);
    }

    return from;
  }

  return mysqlrouter::sqlstring{"!.!"} << base->schema << base->table;
}

mysqlrouter::sqlstring format_where_expr(
    std::shared_ptr<database::entry::Table> table,
    const PrimaryKeyColumnValues &f) {
  // If thr request generates JOINs, ten table_alias is required
  return format_where_expr(table, table->table_alias, f);
}

mysqlrouter::sqlstring format_key_names(
    std::shared_ptr<database::entry::Table> table) {
  mysqlrouter::sqlstring s;
  for (const auto &c : table->primary_key()) {
    mysqlrouter::sqlstring col;
    if (c->type == entry::ColumnType::BINARY)
      col = mysqlrouter::sqlstring("TO_BASE64(!.!)");
    else if (c->type == entry::ColumnType::GEOMETRY)
      col = mysqlrouter::sqlstring("ST_AsGeoJSON(!.!)");
    else
      col = mysqlrouter::sqlstring("!.!");
    col << table->table_alias << c->name;

    s.append_preformatted_sep(", ", col);
  }
  return s;
}

mysqlrouter::sqlstring format_key(std::shared_ptr<database::entry::Table> table,
                                  const PrimaryKeyColumnValues &f) {
  mysqlrouter::sqlstring s;
  for (const auto &c : f) {
    if (table->get_column(c.first)->type == entry::ColumnType::BINARY)
      s.append_preformatted_sep(",", mysqlrouter::sqlstring("TO_BASE64(?)")
                                         << c.second);
    else if (table->get_column(c.first)->type == entry::ColumnType::GEOMETRY)
      s.append_preformatted_sep(",", mysqlrouter::sqlstring("St_AsGeoJSON(?)")
                                         << c.second);
    else
      s.append_preformatted_sep(",", c.second);
  }
  return s;
}

mysqlrouter::sqlstring format_parameters(
    std::shared_ptr<database::entry::Object> object,
    const ColumnValues &values) {
  using namespace std::string_literals;
  mysqlrouter::sqlstring s;
  if (object->base_tables.empty()) return {};

  auto vit = values.begin();
  auto &columns = object->base_tables[0]->columns;
  for (const auto &c : columns) {
    if (vit == values.end()) {
      throw std::runtime_error("Parameter not set:"s + c->name);
    }

    if (c->type == entry::ColumnType::BINARY) {
      s.append_preformatted_sep(",", mysqlrouter::sqlstring("TO_BASE64(?)")
                                         << *vit);
    } else if (c->type == entry::ColumnType::GEOMETRY) {
      s.append_preformatted_sep(
          ",", mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?)") << *vit);
    } else {
      s.append_preformatted_sep(",", *vit);
    }

    ++vit;
  }

  return s;
}

mysqlrouter::sqlstring format_column_mapping(
    const entry::JoinedTable::ColumnMapping &map) {
  mysqlrouter::sqlstring s;

  for (const auto &c : map) {
    s.append_preformatted_sep(
        " AND ", mysqlrouter::sqlstring("!.! = !.!")
                     << c.first->table.lock()->table_alias << c.first->name
                     << c.second->table.lock()->table_alias << c.second->name);
  }

  return s;
}

mysqlrouter::sqlstring format_left_join(const entry::Table &table,
                                        const entry::JoinedTable &join) {
  mysqlrouter::sqlstring s("LEFT JOIN !.! as ! ON ?");

  s << table.schema << table.table << table.table_alias
    << format_column_mapping(join.column_mapping);

  return s;
}

void JsonQueryBuilder::process_object(std::shared_ptr<entry::Object> object) {
  // dump_object(object);

  m_object = object;
  process_object(object, "", false);

  if (m_select_items.str().empty()) {
    throw std::runtime_error("Invalid object metadata");
  }
}

void JsonQueryBuilder::process_object(std::shared_ptr<entry::Object> object,
                                      const std::string &path_prefix,
                                      bool unnest_to_first) {
  m_base_tables = object->base_tables;
  m_path_prefix = path_prefix;

  if (unnest_to_first) {
    for (const auto &f : object->fields) {
      if (f->enabled) {
        add_field_value(f);
        break;
      }
    }
  } else {
    for (const auto &f : object->fields) {
      add_field(f);
    }
  }
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_value() const {
  mysqlrouter::sqlstring q{"SELECT ? FROM ? WHERE ? LIMIT 1"};

  q << select_items() << from_clause()
    << make_subselect_where(std::dynamic_pointer_cast<entry::JoinedTable>(
           m_base_tables.front()));

  if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_object() const {
  mysqlrouter::sqlstring q{"SELECT JSON_OBJECT(?) FROM ? WHERE ? LIMIT 1"};

  q << select_items() << from_clause()
    << make_subselect_where(std::dynamic_pointer_cast<entry::JoinedTable>(
           m_base_tables.front()));

  if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_object_array() const {
  mysqlrouter::sqlstring q{
      "SELECT JSON_ARRAYAGG(JSON_OBJECT(?)) FROM ? WHERE ?"};

  q << select_items() << from_clause()
    << make_subselect_where(std::dynamic_pointer_cast<entry::JoinedTable>(
           m_base_tables.front()));

  if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::subquery_array() const {
  mysqlrouter::sqlstring q{"SELECT JSON_ARRAYAGG(?) FROM ? WHERE ?"};

  q << select_items() << from_clause()
    << make_subselect_where(std::dynamic_pointer_cast<entry::JoinedTable>(
           m_base_tables.front()));

  if (for_update_) q.append_preformatted(" FOR UPDATE NOWAIT");

  return q;
}

mysqlrouter::sqlstring JsonQueryBuilder::make_subselect_where(
    std::shared_ptr<entry::JoinedTable> ref) const {
  mysqlrouter::sqlstring where_sql;

  for (const auto &c : ref->column_mapping) {
    auto tmp = mysqlrouter::sqlstring("!.! = !.!");
    tmp << c.first->table.lock()->table_alias << c.first->name
        << c.second->table.lock()->table_alias << c.second->name;
    where_sql.append_preformatted_sep(" AND ", tmp);
  }

  return where_sql;
}

mysqlrouter::sqlstring JsonQueryBuilder::make_subquery(
    const entry::ReferenceField &field) const {
  JsonQueryBuilder subquery(m_filter, for_update_, for_checksum_);

  auto nested_table = field.nested_object->base_tables.front();

  auto nested_join =
      std::dynamic_pointer_cast<entry::JoinedTable>(nested_table);

  subquery.process_object(
      field.nested_object,
      m_path_prefix.empty() ? field.name : m_path_prefix + "." + field.name,
      nested_join->unnest);

  mysqlrouter::sqlstring subq("(");

  if (nested_join->unnest) {
    // only unnesting of 1:n (aka reduceto) is expected here, since other cases
    // will be unnested into DataFields instead of ReferenceFields
    assert(nested_join->to_many);
    subq.append_preformatted(subquery.subquery_array());
  } else {
    if (nested_join->to_many)
      subq.append_preformatted(subquery.subquery_object_array());
    else
      subq.append_preformatted(subquery.subquery_object());
  }

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

void JsonQueryBuilder::add_field(std::shared_ptr<entry::ObjectField> field) {
  if (!for_checksum_) {
    if (!m_filter.is_included(m_path_prefix, field->name)) return;
    if (!field->enabled) return;
  } else {
    if (field->no_check) return;
  }

  if (auto rfield = std::dynamic_pointer_cast<entry::ReferenceField>(field)) {
    log_debug("rfield->name:%s", rfield->name.c_str());
    auto subquery = make_subquery(*rfield);
    auto item = mysqlrouter::sqlstring("?, ");
    item << rfield->name;
    m_select_items.append_preformatted_sep(", ", item);
    m_select_items.append_preformatted(subquery);
  } else if (auto dfield = std::dynamic_pointer_cast<entry::DataField>(field)) {
    log_debug("dfield->name:%s", dfield->name.c_str());
    auto item = get_field_format(dfield->source->type, dfield->source->datatype,
                                 false, bigins_as_string_);
    item << dfield->name << dfield->source->table.lock()->table_alias
         << dfield->source->name;
    m_select_items.append_preformatted_sep(", ", item);

    add_joined_table(dfield->source->table.lock());
  }
}

void JsonQueryBuilder::add_field_value(
    std::shared_ptr<entry::ObjectField> field) {
  if (for_checksum_ && field->no_check) return;

  if (auto rfield = std::dynamic_pointer_cast<entry::ReferenceField>(field)) {
    auto subquery = make_subquery(*rfield);
    m_select_items.append_preformatted_sep(", ", subquery);
  } else if (auto dfield = std::dynamic_pointer_cast<entry::DataField>(field)) {
    log_debug("dfield->name:%s", dfield->name.c_str());
    auto item = get_field_format(dfield->source->type, dfield->source->datatype,
                                 true, bigins_as_string_);
    item << dfield->source->table.lock()->table_alias << dfield->source->name;
    m_select_items.append_preformatted_sep(", ", item);

    add_joined_table(dfield->source->table.lock());
  }
}

void JsonQueryBuilder::add_joined_table(std::shared_ptr<entry::Table> table) {
  if (std::find(m_base_tables.begin(), m_base_tables.end(), table) !=
      m_base_tables.end())
    return;

  if (std::find(m_joined_tables.begin(), m_joined_tables.end(), table) ==
      m_joined_tables.end())
    m_joined_tables.push_back(table);
}

mysqlrouter::sqlstring JsonQueryBuilder::from_clause() const {
  return format_from_clause(m_base_tables, m_joined_tables);
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
    const entry::Object &, std::vector<std::string> filter) {
  ObjectFieldFilter object_filter;

  object_filter.m_exclusive = is_exclude_filter(filter);

  for (auto &f : filter) {
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

bool ObjectFieldFilter::is_parent_included(std::string_view field) const {
  if (field.empty()) return false;

  // if parent is included, check if there are any field of the parent included
  if (auto it = m_filter.find(field); it != m_filter.end()) {
    ++it;  // set iterator is sorted, so the next item is either something
           // unrelated or a subfield that shares the prefix
    if (it != m_filter.end() && it->compare(0, field.length(), field) == 0 &&
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
      return m_filter.count(field) == 0;
    else
      return m_filter.count(std::string(prefix).append(".").append(field)) == 0;
  } else {
    if (prefix.empty() && m_filter.count(field))
      return true;
    else if (!prefix.empty() &&
             m_filter.count(std::string(prefix).append(".").append(field)))
      return true;
    if (is_parent_included(prefix)) {
      return true;
    }
    return false;
  }
}

}  // namespace database
}  // namespace mrs
