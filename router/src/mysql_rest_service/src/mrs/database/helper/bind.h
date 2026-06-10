/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_BIND_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_BIND_H_

#include <memory>
#include <string>
#include <vector>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/fwd.h>

#include "mrs/database/entry/field.h"
#include "mysql.h"

namespace mrs {
namespace database {

class MysqlBind {
 public:
  using Mode = mrs::database::entry::Field::Mode;
  using DataType = mrs::database::entry::ColumnType;

 public:
  void fill_mysql_bind_for_out(DataType data_type);

  template <typename Value>
  void fill_mysql_bind_for_inout(const Value &value_with_user_type,
                                 DataType data_type) {
    if (is_null(value_with_user_type)) {
      fill_null_as_inout(data_type);
      return;
    }
    if (data_type == DataType::VECTOR) {
      fill_mysql_bind_inout_vector(value_with_user_type);
      return;
    }
    fill_mysql_bind_impl(to_string(value_with_user_type), data_type);
  }

  void fill_null_as_inout(DataType data_type);

  std::vector<MYSQL_BIND> parameters;

 private:
  void fill_mysql_bind_inout_vector(const rapidjson::Value &value);
  void fill_mysql_bind_inout_vector(const std::string &value);
  void fill_mysql_bind_impl(const std::string &value_with_user_type,
                            DataType data_type);
  static enum_field_types to_mysql_type(DataType pdt);
  static const std::string &to_string(const std::string &value);
  static std::string to_string(const rapidjson::Value &value);
  static bool is_null(const std::string &value);
  static bool is_null(const rapidjson::Value &value);

  MYSQL_BIND *allocate_for_blob(const std::string &value);
  MYSQL_BIND *allocate_for_string(const std::string &value);
  MYSQL_BIND *allocate_for(const std::string &value);
  MYSQL_BIND *allocate_bind_buffer(DataType data_type);

  std::vector<std::unique_ptr<char[]>> buffers_;
  std::vector<std::unique_ptr<unsigned long>> lengths_;
  std::vector<std::unique_ptr<bool>> nulls_;
};

}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_BIND_H_ */
