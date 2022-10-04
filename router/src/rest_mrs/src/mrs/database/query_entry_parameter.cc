/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mrs/database/query_entry_parameter.h"

#include <map>

#include "helper/mysql_row.h"

namespace mrs {
namespace database {

bool QueryEntryParameter::query_parameters(MySQLSession *session,
                                           uint64_t db_object_id) {
  parameters_.clear();
  query_ = {
      "SELECT id, name, crud_operation + 0, "
      "bind_column_name, param_datatype FROM "
      "mysql_rest_service_metadata.parameter WHERE "
      "db_object_id=?"};
  query_ << db_object_id;
  query(session);

  return true;
}

QueryEntryParameter::Parameters &QueryEntryParameter::get_result() {
  return parameters_;
}

void QueryEntryParameter::on_row(const Row &row) {
  using ParameterDataType = mrs::database::entry::Parameter::ParameterDataType;
  if (row.size() < 1) return;

  class ParamTypeConverter {
   public:
    void operator()(ParameterDataType *out, const char *value) const {
      const static std::map<std::string, ParameterDataType> converter{
          {"STRING", ParameterDataType::parameterString},
          {"INT", ParameterDataType::parameterInt},
          {"DOUBLE", ParameterDataType::parameterDouble},
          {"BOOLEAN", ParameterDataType::parameterBoolean},
          {"LONG", ParameterDataType::parameterLong},
          {"TIMESTAMP", ParameterDataType::parameterTimestamp}};
      *out = converter.at(value);
    }
  };

  helper::MySQLRow mysql_row(row);

  auto &entry = parameters_.emplace_back();

  mysql_row.unserialize(&entry.id);
  mysql_row.unserialize(&entry.name);
  mysql_row.unserialize(&entry.operation);
  mysql_row.unserialize(&entry.bind_column_name);
  mysql_row.unserialize_with_converter(&entry.parameter_data_type,
                                       ParamTypeConverter{});
}

}  // namespace database
}  // namespace mrs
