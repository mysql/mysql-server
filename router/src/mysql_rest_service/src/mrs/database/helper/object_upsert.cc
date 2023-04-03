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

#include "mrs/database/helper/object_upsert.h"
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

namespace {

// Generates INSERT with ODKU statements for PUT operations
//
// All operations are handled with INSERT ON DUPLICATE KEY UPDATE
// - if the object already exists, the whole object must be replaced
//      - which means if there are unspecified values, they must be reset to
//      default
//
// for each field:
// - if it's a plain value, add to the INSERT
// - if it's a nested object, then:
//    - if it's an outgoing reference
//        - ?
//    - if it's an incoming 1:n reference
//        - ?
//    - if it's an incoming n:m reference
//        - ?
//
class JsonUpsertBuilder {
 public:
  JsonUpsertBuilder() {}

  void process_object(std::shared_ptr<entry::Object> object,
                      const rapidjson::Value &doc) {
    if (!doc.IsObject())
      throw std::invalid_argument("JSON data must be of type Object");

    for (const auto &member : doc.GetObject()) {
      std::string member_name = member.name.GetString();
      auto field = get_field(*object, member_name);
      if (!field)
        throw std::invalid_argument("Unrecognized field '" + member_name +
                                    "' in JSON document");
      on_table_field(*field, member.value);
    }
  }

  std::vector<mysqlrouter::sqlstring> upserts() const {
    std::vector<mysqlrouter::sqlstring> l;

    for (const auto &item : m_rows) {
      mysqlrouter::sqlstring sql{
          "INSERT INTO !.! (?) VALUES (?) ON DUPLICATE KEY UPDATE ?"};

      sql << item.second.source->schema << item.second.source->table
          << item.second.columns << item.second.values << item.second.update;
      l.emplace_back(std::move(sql));
    }

    return l;
  }

 private:
  struct TableRowData {
    std::shared_ptr<entry::FieldSource> source;
    mysqlrouter::sqlstring columns;
    mysqlrouter::sqlstring values;
    mysqlrouter::sqlstring update;
  };

  std::map<std::string, TableRowData> m_rows;

  std::shared_ptr<entry::ObjectField> get_field(const entry::Object &object,
                                                const std::string &name) {
    for (const auto &f : object.fields) {
      if (f->name == name) return f;
    }
    return {};
  }

  void on_table_field(const entry::ObjectField &field,
                      const rapidjson::Value &value) {
    if (field.nested_object) {
      process_object(field.nested_object, value);
    } else {
      check_scalar_field_value_for_insert(field, value, "TODO");

      auto &row = m_rows[field.source->table_key()];

      row.source = field.source;
      {
        mysqlrouter::sqlstring tmp("!");
        tmp << field.db_name;
        row.columns.append_preformatted_sep(", ", tmp);
      }
      using namespace helper::json::sql;  // NOLINT(build/namespaces)
      {
        mysqlrouter::sqlstring tmp("?");
        tmp << value;
        row.values.append_preformatted_sep(", ", tmp);
      }
      {
        mysqlrouter::sqlstring tmp("!=?");
        tmp << field.db_name;
        tmp << value;
        row.update.append_preformatted_sep(", ", tmp);
      }
    }
  }

  void check_scalar_field_value_for_insert(const entry::ObjectField &field,
                                           const rapidjson::Value &value,
                                           const std::string &path) const {
    if (field.db_is_generated) {
      throw std::runtime_error(path + " is generated and cannot have a value");
    }
    if (field.db_not_null && value.IsNull()) {
      throw std::runtime_error(path + " cannot be NULL");
    }
  }
};

}  // namespace

std::vector<mysqlrouter::sqlstring> build_upsert_json_object(
    std::shared_ptr<database::entry::Object> object,
    const rapidjson::Document &json_doc) {
  JsonUpsertBuilder ib;

  ib.process_object(object, json_doc);

  return ib.upserts();
}
}  // namespace database
}  // namespace mrs
