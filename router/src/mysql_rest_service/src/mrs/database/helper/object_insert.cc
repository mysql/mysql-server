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

#include "mrs/database/helper/object_insert.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "helper/json/to_sqlstring.h"
#include "mysql/harness/utility/string.h"

#include <iostream>

namespace mrs {
namespace database {

// Generates INSERT statements for POST operations
//
// for each field:
// - if it's a plain value, add to the INSERT
// - if it's a nested object, then:
//    - if it's an outgoing reference
//        - assign
//        - create nested and assign
//    - if it's an incoming 1:n reference
//        - create nested
//    - if it's an incoming n:m reference
//        - create a join row
//        - create a nested object and a join row

void JsonInsertBuilder::process(const rapidjson::Value &doc) {
  if (!doc.IsObject())
    throw std::invalid_argument("JSON data must be of type Object");

  // do some validations
  auto base_fields = get_base_table_fields();
  bool has_pk = false;
  for (const auto &field : base_fields) {
    if (field->db_is_primary) {
      has_pk = true;
      break;
    }
  }
  if (!has_pk)
    throw std::invalid_argument("Object metadata has no PRIMARY KEY columns");

  process_object(m_object, doc);
}

void JsonInsertBuilder::process_object(std::shared_ptr<entry::Object> object,
                                       const rapidjson::Value &doc) {
  std::map<std::string, TableRowData> rows;

  for (const auto &member : doc.GetObject()) {
    std::string member_name = member.name.GetString();
    auto field = get_field(*object, member_name);

    if (!field)
      throw std::invalid_argument("Unrecognized field '" + member_name +
                                  "' in JSON document");

    // XXX handle non-auto-inc columns

    on_table_field(*field, member.value, &rows);
  }

  for (auto &rit : rows) {
    m_rows.emplace_back(std::move(rit.second));
  }

  // XXX check multi-column FKs
  // XXX check if PK value is included or if it's auto-inc if not
}

mysqlrouter::sqlstring JsonInsertBuilder::insert() const {
  mysqlrouter::sqlstring sql{"INSERT INTO !.! (?) VALUES (?)"};

  for (const auto &row : m_rows) {
    if (std::dynamic_pointer_cast<entry::BaseTable>(row.source)) {
      sql << row.source->schema << row.source->table << row.columns
          << row.values;
      return sql;
    }
  }
  throw std::logic_error("Base table has no data");
}

std::vector<mysqlrouter::sqlstring> JsonInsertBuilder::additional_inserts(
    const PrimaryKeyColumnValues &base_primary_key) const {
  std::vector<mysqlrouter::sqlstring> l;

  for (const auto &row : m_rows) {
    if (auto join = std::dynamic_pointer_cast<entry::JoinedTable>(row.source)) {
      mysqlrouter::sqlstring sql{"INSERT INTO !.! (?) VALUES (?)"};

      // XXX handle objects that don't join directly to the base table
      auto columns = row.columns;
      auto values = row.values;

      for (const auto &c : join->column_mapping) {
        columns.append_preformatted_sep(", ", mysqlrouter::sqlstring("!")
                                                  << c.first);
        values.append_preformatted_sep(
            ", ", mysqlrouter::sqlstring("?") << base_primary_key.at(c.first));
      }

      sql << row.source->schema << row.source->table << columns << values;
      l.emplace_back(std::move(sql));
    }
  }

  return l;
}

std::string JsonInsertBuilder::column_for_last_insert_id() const {
  // if the PK of the base table has auto_inc columns, return its name
  auto base_table = get_base_table();
  auto fields = get_base_table_fields();

  for (const auto &c : fields) {
    if (c->db_is_primary && c->db_auto_inc) return c->db_name;
  }

  // XXX add check if ther's a PK column earlier on

  return "";
}

JsonInsertBuilder::PrimaryKeyColumnValues
JsonInsertBuilder::fixed_primary_key_values() const {
  return {};
}

std::shared_ptr<entry::ObjectField> JsonInsertBuilder::get_field(
    const entry::Object &object, const std::string &name) {
  for (const auto &f : object.fields) {
    if (f->name == name) return f;
  }
  return {};
}

void JsonInsertBuilder::on_table_field(
    const entry::ObjectField &field, const rapidjson::Value &value,
    std::map<std::string, TableRowData> *rows) {
  if (field.nested_object) {
    if (auto ref = std::dynamic_pointer_cast<entry::JoinedTable>(
            field.nested_object->base_tables.back());
        ref && ref->to_many) {
      if (!value.IsArray())
        throw std::invalid_argument(field.name + ": expected to be an Array");
      for (const auto &v : value.GetArray()) {
        process_object(field.nested_object, v);
      }
    } else {
      if (value.IsArray())
        throw std::invalid_argument(field.name +
                                    ": is an Array but wasn't expected to be");
      process_object(field.nested_object, value);
    }
  } else {
    // XXX handle columns that are part of a FK to the base table

    check_scalar_field_value_for_insert(field, value, "TODO");

    auto &row = (*rows)[field.source->table_alias];

    row.source = field.source;
    {
      mysqlrouter::sqlstring tmp("!");
      tmp << field.db_name;
      row.columns.append_preformatted_sep(", ", tmp);
    }
    {
      using namespace helper::json::sql;  // NOLINT(build/namespaces)

      mysqlrouter::sqlstring tmp("?");
      tmp << value;
      row.values.append_preformatted_sep(", ", tmp);
    }
  }
}

void JsonInsertBuilder::check_scalar_field_value_for_insert(
    const entry::ObjectField &field, const rapidjson::Value &value,
    const std::string &path) const {
  if (field.db_is_generated) {
    throw std::runtime_error(path + " is generated and cannot have a value");
  }
  if (field.db_not_null && value.IsNull()) {
    throw std::runtime_error(path + " cannot be NULL");
  }
}

std::vector<std::shared_ptr<entry::ObjectField>>
JsonInsertBuilder::get_base_table_fields() const {
  auto base_table = get_base_table();
  std::vector<std::shared_ptr<entry::ObjectField>> fields;

  for (const auto &f : m_object->fields) {
    if (f->source == base_table) {
      fields.push_back(f);
    }
  }
  return fields;
}

std::shared_ptr<entry::BaseTable> JsonInsertBuilder::get_base_table() const {
  return std::dynamic_pointer_cast<entry::BaseTable>(
      m_object->base_tables.back());
}

}  // namespace database
}  // namespace mrs
