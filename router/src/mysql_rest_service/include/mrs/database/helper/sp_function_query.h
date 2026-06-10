/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_HELPER_SP_FUNCTION_QUERY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_HELPER_SP_FUNCTION_QUERY_H_

#include <vector>

#include "helper/http/url.h"
#include "http/base/uri.h"
#include "mrs/database/entry/row_user_ownership.h"
#include "mrs/database/helper/bind.h"
#include "mrs/database/json_mapper/select.h"
#include "mrs/interface/universal_id.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

ColumnValues create_function_argument_list(
    const entry::Object *object, rapidjson::Document &doc,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id);

ColumnValues create_function_argument_list(
    const entry::Object *object, const helper::http::Url::Parameters &query_kv,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id);

void fill_procedure_argument_list_with_binds(
    mrs::database::entry::ResultSets &rs, const rapidjson::Document &doc,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id, mrs::database::MysqlBind *out_binds,
    std::string *out_params);

void fill_procedure_argument_list_with_binds(
    mrs::database::entry::ResultSets &rs,
    const helper::http::Url::Parameters &query_kv,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id, mrs::database::MysqlBind *out_binds,
    std::string *out_params);

using DataType = mrs::database::entry::ColumnType;

inline mysqlrouter::sqlstring get_sql_format(
    DataType type, const rapidjson::Document::ValueType &value) {
  using namespace helper;
  switch (type) {
    case DataType::BINARY:
      return mysqlrouter::sqlstring("FROM_BASE64(?)");

    case DataType::GEOMETRY:
      return value.IsObject() ? mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?)")
                              : mysqlrouter::sqlstring("ST_GeomFromText(?)");

    case DataType::VECTOR:
      return mysqlrouter::sqlstring("STRING_TO_VECTOR(?)");

    case DataType::JSON:
      return mysqlrouter::sqlstring("CAST(? as JSON)");

    default: {
    }
  }

  return mysqlrouter::sqlstring("?");
}

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_HELPER_SP_FUNCTION_QUERY_H_
