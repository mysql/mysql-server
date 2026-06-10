/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "mrs/database/query_entry_fields.h"

#include <map>
#include <optional>

#include "helper/mysql_row.h"

#include "mrs/database/converters/column_datatype_converter.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using entry::UniversalId;

const char *QueryEntryFields::to_cstr(OnRow r) {
  switch (r) {
    case OnRow::k_fields:
      return "object_field";
    case OnRow::k_output_name:
      return "object/kind/result";
    case OnRow::k_parameters_name:
      return "object/kind/parameter";
  }

  return "unknown";
}

bool QueryEntryFields::query_parameters(MySQLSession *session,
                                        UniversalId db_object_id) {
  result_ = {};

  processing_ = OnRow::k_parameters_name;
  output_result_ = &result_.parameters;

  query_ = {
      "SELECT o.id, o.name FROM mysql_rest_service_metadata.object as o "
      "        WHERE o.kind='PARAMETERS' and o.db_object_id=?"};

  query_ << db_object_id;
  execute(session);

  processing_ = OnRow::k_fields;
  query_ = {
      "SELECT ofx.id, ofx.represents_reference_id, ofx.name,"
      "       ofx.db_column->>'$.in', ofx.db_column->>'$.out',"
      "       ofx.db_column->>'$.name', ofx.db_column->>'$.datatype',"
      "       JSON_CONTAINS(d.options->'$.mysqlTask.userVariables',"
      "         ofx.db_column->'$.name')"
      "   FROM mysql_rest_service_metadata.object_field as ofx"
      "   JOIN mysql_rest_service_metadata.object as o on ofx.object_id=o.id"
      "   JOIN mysql_rest_service_metadata.db_object d on o.db_object_id=d.id"
      "        WHERE o.kind='PARAMETERS' and o.db_object_id=? ORDER BY "
      "ofx.position"};

  query_ << db_object_id;

  execute(session);

  processing_ = OnRow::k_output_name;
  query_ = {
      "SELECT o.id, o.name FROM mysql_rest_service_metadata.object as o "
      "        WHERE o.kind='RESULT' and o.db_object_id=?"};

  query_ << db_object_id;
  execute(session);

  processing_ = OnRow::k_fields;
  for (auto &item : result_.results) {
    output_result_ = &item;
    query_ = {
        "SELECT ofx.id, ofx.represents_reference_id, ofx.name,"
        "       ofx.db_column->>'$.in', ofx.db_column->>'$.out',"
        "       ofx.db_column->>'$.name', ofx.db_column->>'$.datatype',"
        "       JSON_CONTAINS(d.options->'$.mysqlTask.userVariables',"
        "         ofx.db_column->'$.name')"
        "   FROM mysql_rest_service_metadata.object_field as ofx"
        "   JOIN mysql_rest_service_metadata.object as o on ofx.object_id=o.id"
        "   JOIN mysql_rest_service_metadata.db_object d on o.db_object_id=d.id"
        "        WHERE o.kind='RESULT' and o.id=? ORDER BY ofx.position"};

    query_ << item.id;
    execute(session);
  }

  return true;
}

QueryEntryFields::ResultSets &QueryEntryFields::get_result() { return result_; }

void QueryEntryFields::on_row(const ResultRow &row) {
  try {
    switch (processing_) {
      case OnRow::k_fields:
        on_row_params(row);
        return;
      case OnRow::k_parameters_name:
        on_row_input_name(row);
        return;
      case OnRow::k_output_name:
        on_row_output_name(row);
        return;

      default:
        return;
    }
  } catch (const std::exception &e) {
    UniversalId id;

    if (row.size() > 0 && row[0]) {
      id = UniversalId::from_cstr(row[0], row.get_data_size(0));
    }

    log_error("%s with id:%s, will be disabled because of following error: %s",
              to_cstr(processing_), id.to_string().c_str(), e.what());
  }
}

void QueryEntryFields::on_row_input_name(const ResultRow &row) {
  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  ResultObject item;

  mysql_row.unserialize_with_converter(&item.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&item.name);

  result_.parameters = item;
}

void QueryEntryFields::on_row_output_name(const ResultRow &row) {
  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);
  ResultObject item;

  mysql_row.unserialize_with_converter(&item.id, entry::UniversalId::from_raw);
  mysql_row.unserialize(&item.name);
  result_.results.push_back(item);
}

void QueryEntryFields::on_row_params(const ResultRow &row) {
  using Field = mrs::database::entry::Field;
  using Mode = Field::Mode;

  if (row.size() < 1) return;

  helper::MySQLRow mysql_row(row, metadata_, num_of_metadata_);

  Field entry;
  entry.data_type = mrs::database::entry::ColumnType::UNKNOWN;
  bool param_in{false};
  bool param_out{false};

  // QueryEntryFields and its clients, doesn't depend on following value.
  // We fetch it to check if data_ttype value should be configured or not.
  std::optional<entry::UniversalId> represents_reference_id;

  mysql_row.unserialize_with_converter(&entry.id, entry::UniversalId::from_raw);
  mysql_row.unserialize_with_converter(&represents_reference_id,
                                       entry::UniversalId::from_raw);
  mysql_row.unserialize(&entry.name);
  mysql_row.unserialize(&param_in);
  mysql_row.unserialize(&param_out);
  mysql_row.unserialize(&entry.bind_name);
  mysql_row.unserialize(&entry.raw_data_type);
  mysql_row.unserialize(&entry.is_user_variable);
  if (!represents_reference_id.has_value()) {
    ColumnDatatypeConverter()(&entry.data_type, entry.raw_data_type);
  }

  if (param_in && param_out) {
    entry.mode = Mode::modeInOut;
  } else if (param_in) {
    entry.mode = Mode::modeIn;
  } else if (param_out) {
    entry.mode = Mode::modeOut;
  }

  output_result_->fields.push_back(entry);
}

}  // namespace database
}  // namespace mrs
