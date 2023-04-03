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
#include <vector>
#include "mrs/database/entry/object.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class JsonInsertBuilder {
 public:
  using PrimaryKeyColumnValues = std::map<std::string, mysqlrouter::sqlstring>;

  explicit JsonInsertBuilder(std::shared_ptr<entry::Object> object)
      : m_object(object) {}

  void process(const rapidjson::Value &doc);

  mysqlrouter::sqlstring insert() const;
  std::vector<mysqlrouter::sqlstring> additional_inserts(
      const PrimaryKeyColumnValues &base_primary_key) const;

  std::string column_for_last_insert_id() const;
  PrimaryKeyColumnValues fixed_primary_key_values() const;

 private:
  std::shared_ptr<entry::Object> m_object;

  struct TableRowData {
    std::shared_ptr<entry::FieldSource> source;
    mysqlrouter::sqlstring columns;
    mysqlrouter::sqlstring values;
  };

  std::vector<TableRowData> m_rows;

  void process_object(std::shared_ptr<entry::Object> object,
                      const rapidjson::Value &doc);

  std::shared_ptr<entry::ObjectField> get_field(const entry::Object &object,
                                                const std::string &name);

  void on_table_field(const entry::ObjectField &field,
                      const rapidjson::Value &value,
                      std::map<std::string, TableRowData> *rows);

  void check_scalar_field_value_for_insert(const entry::ObjectField &field,
                                           const rapidjson::Value &value,
                                           const std::string &path) const;

  std::shared_ptr<entry::BaseTable> get_base_table() const;
  std::vector<std::shared_ptr<entry::ObjectField>> get_base_table_fields()
      const;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_OBJECT_INSERT_H_
