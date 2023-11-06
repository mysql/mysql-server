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

#include "mrs/database/query_rest_function.h"

#include "helper/json/serializer_to_text.h"
#include "helper/mysql_column_types.h"

namespace mrs {
namespace database {

using namespace helper::json;
using namespace mrs::database::entry;

const char *QueryRestFunction::get_sql_state() {
  if (!sqlstate_.has_value()) return nullptr;
  return sqlstate_.value().c_str();
}

void QueryRestFunction::query_raw(MySQLSession *session,
                                  const std::string &schema,
                                  const std::string &object,
                                  const mysqlrouter::sqlstring &values) {
  store_raw_ = true;
  query_entries_impl(session, schema, object, values);
}

void QueryRestFunction::query_entries(MySQLSession *session,
                                      const std::string &schema,
                                      const std::string &object,
                                      const mysqlrouter::sqlstring &values) {
  store_raw_ = false;
  query_entries_impl(session, schema, object, values);
}

void QueryRestFunction::query_entries_impl(
    MySQLSession *session, const std::string &schema, const std::string &object,
    const mysqlrouter::sqlstring &values) {
  items = 0;
  json_type_ = helper::JsonType::kNull;
  query_ = {"SELECT !.!(!)"};
  query_ << schema << object << values;

  query(session, query_.str());
}
void QueryRestFunction::on_row(const ResultRow &r) {
  assert(0 == items && "The function should generate a single row.");
  assert(1 == r.size() && "Function should return a single value.");

  if (!store_raw_) {
    SerializerToText stt;
    {
      auto obj = stt.add_object();

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
