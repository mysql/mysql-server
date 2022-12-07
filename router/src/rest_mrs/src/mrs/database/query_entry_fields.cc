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

#include "mrs/database/query_entry_fields.h"

#include <map>

#include "helper/mysql_row.h"

namespace mrs {
namespace database {

bool QueryEntryFields::query_parameters(MySQLSession *session,
                                        entry::UniversalId db_object_id) {
  parameters_.clear();
  query_ = {
      "SELECT id, name, mode, "
      "bind_field_name, datatype, position FROM "
      "mysql_rest_service_metadata.field WHERE "
      "db_object_id=? ORDER BY position"};
  query_ << db_object_id;
  execute(session);

  return true;
}

QueryEntryFields::Fields &QueryEntryFields::get_result() { return parameters_; }

void QueryEntryFields::on_row(const Row &row) {
  using Field = mrs::database::entry::Field;
  using DataType = Field::DataType;
  using Mode = Field::Mode;

  if (row.size() < 1) return;

  class ParamTypeConverter {
   public:
    void operator()(DataType *out, const char *value) const {
      const static std::map<std::string, DataType> converter{
          {"STRING", DataType::typeString},
          {"INT", DataType::typeInt},
          {"DOUBLE", DataType::typeDouble},
          {"BOOLEAN", DataType::typeBoolean},
          {"LONG", DataType::typeLong},
          {"TIMESTAMP", DataType::typeTimestamp},
          {"JSON", DataType::typeString}};
      *out = converter.at(value);
    }
  };
  class ParamModeConverter {
   public:
    void operator()(Mode *out, const char *value) const {
      const static std::map<std::string, Mode> converter{
          {"IN", Mode::modeIn},
          {"OUT", Mode::modeOut},
          {"INOUT", Mode::modeInOut}};
      if (nullptr == value) {
        *out = Mode::modeIn;
        return;
      }
      *out = converter.at(value);
    }
  };

  helper::MySQLRow mysql_row(row);

  auto &entry = parameters_.emplace_back();

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.name);
  mysql_row.unserialize_with_converter(&entry.mode, ParamModeConverter{});
  mysql_row.unserialize(&entry.bind_name);
  mysql_row.unserialize_with_converter(&entry.data_type, ParamTypeConverter{});
}

}  // namespace database
}  // namespace mrs
