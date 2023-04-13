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
#include "mrs/http/error.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/string.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace helper::json::sql;  // NOLINT(build/namespaces)

mysqlrouter::sqlstring join_sqlstrings(
    const std::vector<mysqlrouter::sqlstring> &strings,
    const std::string &sep) {
  mysqlrouter::sqlstring str;
  for (const auto &s : strings) {
    str.append_preformatted_sep(sep, s);
  }
  return str;
}

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

void JsonInsertBuilder::process(const rapidjson::Document &doc) {
  assert(doc.IsObject());

  // do some validations
  auto base_fields = get_base_table_fields();
  for (const auto &field : base_fields) {
    if (field->db_is_primary) {
      m_pk_field = field;
      break;
    }
  }
  if (!m_pk_field) {
    log_error(
        "Object for table '%s' has no PRIMARY KEY defined in MRS metadata",
        m_object->base_tables.front()->table.c_str());
    throw std::runtime_error("Configuration error in database");
  }

  process_object(m_object, doc);
}

void JsonInsertBuilder::process_object(std::shared_ptr<entry::Object> object,
                                       const rapidjson::Value &doc,
                                       const std::string &path) {
  std::map<std::string, TableRowData> rows;

  bool found_pk_column = false;
  bool found_row_ownership_column = false;
  for (const auto &member : doc.GetObject()) {
    std::string member_name = member.name.GetString();
    auto field = get_field(*object, member_name);

    if (!field) {
      throw http::Error(
          HttpStatusCode::BadRequest,
          "Unrecognized field '" + member_name + "' in JSON document");
    }

    if (!m_row_ownership_column.empty() &&
        field->db_name == m_row_ownership_column) {
      found_row_ownership_column = true;

      if (field->db_is_primary) {
        auto tmp = mysqlrouter::sqlstring("?");
        tmp << m_requesting_user_id;
        m_predefined_pk_values[field->db_name] = tmp;
      }
      on_table_field(*field, m_requesting_user_id, &rows, path);
    } else {
      on_table_field(*field, member.value, &rows, path);
    }
  }

  if (m_is_update) {
    if (!m_updated_pk_value.has_value() || m_updated_pk_value->str().empty()) {
      // PK value of the object to be updated MUST come with the request
      // but as a shortcut/optimization, we allow it to be skipped if the
      // PK column is also the row ownership column. This allows all tables with
      // row-level access control to be updated while taking the user id from
      // the session data
      bool pk_is_owner_id = m_row_ownership_column == m_pk_field->db_name;
      if (!pk_is_owner_id)
        throw http::Error(HttpStatusCode::BadRequest,
                          "Key value is required inside the URL.");

      mysqlrouter::sqlstring tmp("?");
      tmp << m_requesting_user_id;
      m_predefined_pk_values[m_row_ownership_column] = tmp;
    } else {
      m_predefined_pk_values[m_pk_field->db_name] = *m_updated_pk_value;
    }
  } else {
    if (path.empty() && !found_pk_column && !m_pk_field->db_auto_inc) {
      throw http::Error(
          HttpStatusCode::BadRequest,
          "Inserted document must contain a primary key, it may be auto "
          "generated by 'ownership' configuration or auto_increment.");
    }
  }

  if (!m_row_ownership_column.empty() && !found_row_ownership_column) {
    auto field = get_field(*object, m_row_ownership_column);
    if (!field) {
      log_error("Could not find metadata for row owner field '%s'",
                m_row_ownership_column.c_str());
      throw http::Error(HttpStatusCode::BadRequest,
                        "Could not find metadata for field");
    }

    mysqlrouter::sqlstring tmp("?");
    on_table_field(*field, m_requesting_user_id, &rows, "");
  }

  for (auto &rit : rows) {
    m_rows.emplace_back(std::move(rit.second));
  }

  // XXX check multi-column FKs
}

mysqlrouter::sqlstring JsonInsertBuilder::insert() const {
  mysqlrouter::sqlstring sql{"INSERT INTO !.! (?) VALUES (?)"};

  for (const auto &row : m_rows) {
    if (std::dynamic_pointer_cast<entry::BaseTable>(row.source)) {
      sql << row.source->schema << row.source->table
          << join_sqlstrings(row.columns, ", ")
          << join_sqlstrings(row.values, ", ");
      return sql;
    }
  }
  throw std::logic_error("Base table has no data");
}

mysqlrouter::sqlstring JsonInsertBuilder::update() {
  mysqlrouter::sqlstring sql{"UPDATE !.! SET "};

  for (const auto &row : m_rows) {
    if (std::dynamic_pointer_cast<entry::BaseTable>(row.source)) {
      assert(row.columns.size() == row.values.size());

      sql << row.source->schema << row.source->table;

      auto vit = row.values.begin();
      for (auto cit = row.columns.begin(); cit != row.columns.end();
           ++cit, ++vit) {
        if (cit != row.columns.begin()) {
          sql.append_preformatted(", ");
        }
        sql.append_preformatted(*cit);
        sql.append_preformatted("=");
        if (cit->str() == m_pk_field->db_name) {
          // don't allow changing PK value
        } else {
          sql.append_preformatted(*vit);
        }
      }

      mysqlrouter::sqlstring where;
      if (!m_row_ownership_column.empty()) {
        where = mysqlrouter::sqlstring(" WHERE ! = ? AND ! = ?");
        where << m_pk_field->db_name << *m_updated_pk_value;
        where << m_row_ownership_column << m_requesting_user_id;
      } else {
        where = mysqlrouter::sqlstring(" WHERE ! = ?");
        where << m_pk_field->db_name << *m_updated_pk_value;
      }

      sql.append_preformatted(where);

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
      auto columns = join_sqlstrings(row.columns, ", ");
      auto values = join_sqlstrings(row.values, ", ");

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
  if (m_pk_field->db_auto_inc) return m_pk_field->db_name;

  return "";
}

JsonInsertBuilder::PrimaryKeyColumnValues
JsonInsertBuilder::predefined_primary_key_values() const {
  return m_predefined_pk_values;
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
    std::map<std::string, TableRowData> *rows,
    [[maybe_unused]] const std::string &path) {
  if (field.nested_object) {
    throw std::runtime_error("POSTing of nested objects not supported");
#if 0
    if (auto ref = std::dynamic_pointer_cast<entry::JoinedTable>(
            field.nested_object->base_tables.back());
        ref && ref->to_many) {
      if (!value.IsArray())
        throw std::invalid_argument(field.name + ": expected to be an Array");
      for (const auto &v : value.GetArray()) {
        process_object(field.nested_object, v,
          path.empty() ? field.name : (path+"."+field.name));
      }
    } else {
      if (value.IsArray())
        throw std::invalid_argument(field.name +
                                    ": is an Array but wasn't expected to be");
      process_object(field.nested_object, value,
        path.empty() ? field.name : (path+"."+field.name));
    }
#endif
  } else {
    validate_scalar_field_value_for_insert(field, value, path);

    // XXX handle columns that are part of a FK to the base table
    auto &row = (*rows)[field.source->table_alias];

    row.source = field.source;
    {
      mysqlrouter::sqlstring tmp("!");
      tmp << field.db_name;
      row.columns.emplace_back(std::move(tmp));
    }
    {
      mysqlrouter::sqlstring tmp("?");
      tmp << value;
      row.values.emplace_back(std::move(tmp));

      if (path.empty() && field.db_is_primary) {
        m_predefined_pk_values[field.db_name] = tmp;
      }
    }
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

void JsonInsertBuilder::validate_scalar_field_value_for_insert(
    const entry::ObjectField &field, const rapidjson::Value &value,
    const std::string &path) const {
  // TODO(alfredo) check value type

  if (field.db_is_generated) {
    throw std::runtime_error(path + " is generated and cannot have a value");
  }
  if (field.db_not_null && value.IsNull()) {
    throw std::runtime_error(path + " cannot be NULL");
  }
}

}  // namespace database
}  // namespace mrs
