/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

bool QueryEntryFields::query_parameters(MySQLSession *session,
                                        entry::UniversalId db_object_id) {
  result_ = {};

  processing_ = Row::k_input_name;
  output_result_ = &result_.input_parameters;

  query_ = {
      "SELECT o.id, o.name FROM mysql_rest_service_metadata.object as o "
      "        WHERE o.kind='PARAMETERS' and o.db_object_id=?"};

  query_ << db_object_id;
  execute(session);

  processing_ = Row::k_fields;
  query_ = {
      "SELECT ofx.id, ofx.name,"
      "       ofx.db_column->>'$.in', ofx.db_column->>'$.out',"
      "       ofx.db_column->>'$.name', ofx.db_column->>'$.datatype'"
      "   FROM mysql_rest_service_metadata.object_field as ofx"
      "   JOIN mysql_rest_service_metadata.object as o on ofx.object_id=o.id"
      "        WHERE o.kind='PARAMETERS' and o.db_object_id=? ORDER BY "
      "ofx.position"};

  query_ << db_object_id;

  execute(session);

  processing_ = Row::k_output_name;
  query_ = {
      "SELECT o.id, o.name FROM mysql_rest_service_metadata.object as o "
      "        WHERE o.kind='RESULT' and o.db_object_id=?"};

  query_ << db_object_id;
  execute(session);

  processing_ = Row::k_fields;
  for (auto &item : result_.results) {
    output_result_ = &item;
    query_ = {
        "SELECT ofx.id, ofx.name,"
        "       ofx.db_column->>'$.in', ofx.db_column->>'$.out',"
        "       ofx.db_column->>'$.name', ofx.db_column->>'$.datatype'"
        "   FROM mysql_rest_service_metadata.object_field as ofx"
        "   JOIN mysql_rest_service_metadata.object as o on ofx.object_id=o.id"
        "        WHERE o.kind='RESULT' and o.id=? ORDER BY ofx.position"};

    query_ << item.id;
    execute(session);
  }

  return true;
}

QueryEntryFields::ResultSets &QueryEntryFields::get_result() { return result_; }

void QueryEntryFields::on_row(const ResultRow &row) {
  switch (processing_) {
    case Row::k_fields:
      on_row_params(row);
      return;
    case Row::k_input_name:
      on_row_input_name(row);
      return;
    case Row::k_output_name:
      on_row_output_name(row);
      return;

    default:
      return;
  }
}

void QueryEntryFields::on_row_input_name(const ResultRow &row) {
  auto &item = result_.input_parameters;
  helper::MySQLRow mysql_row(row, metadata_, no_od_metadata_);

  mysql_row.unserialize_with_converter(&item.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&item.name);
}

void QueryEntryFields::on_row_output_name(const ResultRow &row) {
  auto &item = result_.results.emplace_back();
  helper::MySQLRow mysql_row(row, metadata_, no_od_metadata_);

  mysql_row.unserialize_with_converter(&item.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&item.name);
}

void QueryEntryFields::on_row_params(const ResultRow &row) {
  using Field = mrs::database::entry::Field;
  using DataType = Field::DataType;
  using Mode = Field::Mode;

  if (row.size() < 1) return;

  class ParamTypeConverter {
   public:
    void operator()(DataType *out, const char *value) const {
      const static std::map<std::string, DataType> converter{
          {"STRING", DataType::typeString},
          {"TEXT", DataType::typeString},
          {"VARCHAR", DataType::typeString},
          {"CHAR", DataType::typeString},
          {"VARBINARY", DataType::typeString},
          {"BINARY", DataType::typeString},
          {"TINYBLOB", DataType::typeString},
          {"MEDIUMBLOB", DataType::typeString},
          {"BLOB", DataType::typeString},
          {"LONGBLOB", DataType::typeString},
          {"INT", DataType::typeInt},
          {"TINYINT", DataType::typeInt},
          {"SMALLINT", DataType::typeInt},
          {"MEDIUMINT", DataType::typeInt},
          {"LARGEINT", DataType::typeInt},
          {"BIGINT", DataType::typeInt},
          {"DOUBLE", DataType::typeDouble},
          {"FLOAT", DataType::typeDouble},
          {"REAL", DataType::typeDouble},
          {"DECIMAL", DataType::typeDouble},
          {"CHAR", DataType::typeString},
          {"NCHAR", DataType::typeString},
          {"VARCHAR", DataType::typeString},
          {"NVARCHAR", DataType::typeString},
          {"BINARY", DataType::typeString},
          {"VARBINARY", DataType::typeString},
          {"TINYTEXT", DataType::typeString},
          {"TEXT", DataType::typeString},
          {"MEDIUMTEXT", DataType::typeString},
          {"LONGTEXT", DataType::typeString},
          {"TINYBLOB", DataType::typeString},
          {"BLOB", DataType::typeString},
          {"MEDIUMBLOB", DataType::typeString},
          {"LONGBLOB", DataType::typeString},
          {"JSON", DataType::typeString},
          {"DATETIME", DataType::typeTimestamp},
          {"DATE", DataType::typeTimestamp},
          {"TIME", DataType::typeTimestamp},
          {"YEAR", DataType::typeTimestamp},
          {"TIMESTAMP", DataType::typeTimestamp},
          {"GEOMETRY", DataType::typeString},
          {"POINT", DataType::typeString},
          {"LINESTRING", DataType::typeString},
          {"POLYGON", DataType::typeString},
          {"GEOMETRYCOLLECTION", DataType::typeString},
          {"MULTIPOINT", DataType::typeString},
          {"MULTILINESTRING", DataType::typeString},
          {"MULTIPOLYGON", DataType::typeString},
          {"BIT", DataType::typeInt},
          {"BOOLEAN", DataType::typeInt},
          {"ENUM", DataType::typeString},
          {"SET", DataType::typeString}};

      if (!value) return;
      result_ = mysql_harness::make_upper(value);
      auto p = std::min(result_.find('('), result_.find(' '));
      if (p != std::string::npos) result_ = result_.substr(0, p);
      try {
        *out = converter.at(result_);
      } catch (const std::exception &e) {
        log_debug("'ParamTypeConverter 'do not handle value: %s",
                  result_.c_str());
        throw;
      }
    }

    std::string &result_;
  };

  helper::MySQLRow mysql_row(row, metadata_, no_od_metadata_);

  auto &entry = output_result_->fields.emplace_back();
  bool param_in{false}, param_out{false};

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.name);
  mysql_row.unserialize(&param_in);
  mysql_row.unserialize(&param_out);
  mysql_row.unserialize(&entry.bind_name);
  mysql_row.unserialize_with_converter(&entry.data_type,
                                       ParamTypeConverter{entry.raw_data_type});

  if (param_in && param_out) {
    entry.mode = Mode::modeInOut;
  } else if (param_in) {
    entry.mode = Mode::modeIn;
  } else if (param_out) {
    entry.mode = Mode::modeOut;
  }
}

}  // namespace database
}  // namespace mrs
