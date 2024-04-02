/*
 Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/database/query_rest_function.h"

#include "mysql/harness/logging/logging.h"

#include "helper/json/serializer_to_text.h"
#include "helper/mysql_column_types.h"
#include "mrs/database/helper/object_query.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace helper::json;
using namespace mrs::database::entry;
using JsonQueryBuilder = mrs::database::JsonQueryBuilder;

static bool needs_bigint_workaround(enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_LONGLONG:
      [[fallthrough]];
    case MYSQL_TYPE_DECIMAL:
      [[fallthrough]];
    case MYSQL_TYPE_NEWDECIMAL:
      [[fallthrough]];
    case MYSQL_TYPE_FLOAT:
      [[fallthrough]];
    case MYSQL_TYPE_DOUBLE:
      return true;
    default:
      return false;
  }
}

const char *QueryRestFunction::get_sql_state() {
  if (!sqlstate_.has_value()) return nullptr;
  return sqlstate_.value().c_str();
}

void QueryRestFunction::query_raw(MySQLSession *session,
                                  std::shared_ptr<entry::Object> object,
                                  const ParametersValues &values) {
  store_raw_ = true;
  query_entries_impl(session, object, values);
}

void QueryRestFunction::query_entries(MySQLSession *session,
                                      std::shared_ptr<entry::Object> object,
                                      const ParametersValues &values) {
  store_raw_ = false;
  query_entries_impl(session, object, values);
}

void QueryRestFunction::query_entries_impl(
    MySQLSession *session, std::shared_ptr<entry::Object> object,
    const ParametersValues &values) {
  items = 0;
  json_type_ = JsonType::kNull;

  auto parameters = format_parameters(object, values);
  auto from = format_from_clause(object->base_tables, {}, false);

  query(session, mysqlrouter::sqlstring{"SELECT !(!)"} << from << parameters);
}

void QueryRestFunction::on_row(const ResultRow &r) {
  assert(0 == items && "The function should generate a single row.");
  assert(1 == r.size() && "Function should return a single value.");

  if (!store_raw_) {
    SerializerToText stt;
    {
      auto obj = stt.add_object();

      if (encode_bigints_as_strings_ && needs_bigint_workaround(mysql_type_)) {
        json_type_ = helper::JsonType::kString;
      }

      if (MYSQL_TYPE_BIT == mysql_type_ &&
          json_type_ == helper::JsonType::kBool)
        obj->member_add_value(
            "result",
            *reinterpret_cast<const uint8_t *>(r[0]) ? "true" : "false",
            json_type_);
      else
        obj->member_add_value("result", r[0], r.get_data_size(0), json_type_);
    }
    response = stt.get_result();
    return;
  }
  log_debug("on_row -> size:%i", (int)r.get_data_size(0));

  if (MYSQL_TYPE_BIT == mysql_type_) {
    response.assign(r[0] ? "TRUE" : "FALSE");
    return;
  }

  response.assign(r[0], r.get_data_size(0));
}

void QueryRestFunction::on_metadata(unsigned int number, MYSQL_FIELD *fields) {
  if (number) {
    mysql_type_ = fields[0].type;
    json_type_ = helper::from_mysql_column_type(&fields[0]);
  }
}

}  // namespace database
}  // namespace mrs
