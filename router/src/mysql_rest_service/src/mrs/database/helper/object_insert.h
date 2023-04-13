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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_INSERT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_INSERT_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "mrs/database/entry/object.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class JsonInsertBuilder {
 public:
  using PrimaryKeyColumnValues = std::map<std::string, mysqlrouter::sqlstring>;

  explicit JsonInsertBuilder(std::shared_ptr<entry::Object> object,
                             const std::string &row_ownership_column = {},
                             rapidjson::Value requesting_user_id = {})
      : m_object(object),
        m_row_ownership_column(row_ownership_column),
        m_requesting_user_id(std::move(requesting_user_id)),
        m_is_update(false) {}

  JsonInsertBuilder(std::shared_ptr<entry::Object> object,
                    const mysqlrouter::sqlstring &updated_pk_value,
                    const std::string &row_ownership_column = {},
                    rapidjson::Value requesting_user_id = {})
      : m_object(object),
        m_row_ownership_column(row_ownership_column),
        m_requesting_user_id(std::move(requesting_user_id)),
        m_is_update(true),
        m_updated_pk_value(updated_pk_value) {}

  void process(const rapidjson::Document &doc);

  mysqlrouter::sqlstring insert() const;
  std::vector<mysqlrouter::sqlstring> additional_inserts(
      const PrimaryKeyColumnValues &base_primary_key) const;

  std::string column_for_last_insert_id() const;
  PrimaryKeyColumnValues predefined_primary_key_values() const;

  mysqlrouter::sqlstring update();

 private:
  std::shared_ptr<entry::Object> m_object;
  std::string m_row_ownership_column;
  rapidjson::Value m_requesting_user_id;

  std::shared_ptr<entry::ObjectField> m_pk_field;

  bool m_is_update = false;
  std::optional<mysqlrouter::sqlstring> m_updated_pk_value;

  struct TableRowData {
    std::shared_ptr<entry::FieldSource> source;
    std::vector<mysqlrouter::sqlstring> columns;
    std::vector<mysqlrouter::sqlstring> values;
  };
  PrimaryKeyColumnValues m_predefined_pk_values;

  std::vector<TableRowData> m_rows;

  void process_object(std::shared_ptr<entry::Object> object,
                      const rapidjson::Value &doc,
                      const std::string &path = "");

  std::shared_ptr<entry::ObjectField> get_field(const entry::Object &object,
                                                const std::string &name);

  void on_table_field(const entry::ObjectField &field,
                      const rapidjson::Value &value,
                      std::map<std::string, TableRowData> *rows,
                      const std::string &path);

  std::shared_ptr<entry::BaseTable> get_base_table() const;
  std::vector<std::shared_ptr<entry::ObjectField>> get_base_table_fields()
      const;

  void validate_scalar_field_value_for_insert(const entry::ObjectField &field,
                                              const rapidjson::Value &value,
                                              const std::string &path) const;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_INSERT_H_
