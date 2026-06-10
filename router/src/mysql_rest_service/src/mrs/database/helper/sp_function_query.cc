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

#include "mrs/database/helper/sp_function_query.h"

#include <stdexcept>

#include "helper/http/url.h"
#include "helper/json/rapid_json_iterator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/mysql_numeric_value.h"
#include "mrs/database/entry/object.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/utility/container/generic.h"
#include "mysqlrouter/utils_sqlstring.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace helper::json::sql;
using Url = helper::http::Url;
using ColumnType = mrs::database::entry::ColumnType;
using Column = mrs::database::entry::Column;
using ObjectField = database::entry::ObjectField;
using ObjectFieldPtr = std::shared_ptr<ObjectField>;

namespace {

// CLANG doesn't allow capture, already captured variable.
// Instead using lambda let use class (llvm-issue #48582).
class CompareFieldNameP {
 public:
  CompareFieldNameP(const std::string &k) : key_{k} {}

  bool operator()(const mrs::database::entry::Field &f) const {
    return f.name == key_;
  }

 private:
  const std::string &key_;
};

// CLANG doesn't allow capture, already captured variable.
// Instead using lambda let use class (llvm-issue #48582).
class CompareFieldNameF {
 public:
  CompareFieldNameF(const std::string &k) : key_{k} {}

  bool operator()(const ObjectFieldPtr &of) const { return of->name == key_; }

 private:
  const std::string &key_;
};

}  // namespace

ColumnValues create_function_argument_list(
    const entry::Object *object, rapidjson::Document &doc,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id) {
  using namespace std::string_literals;

  if (!doc.IsObject())
    throw std::invalid_argument(
        "Parameters must be encoded as fields in Json object.");

  auto table = object->table;
  if (entry::KindType::PARAMETERS != object->kind) {
    throw std::logic_error(
        "Bad object kind for function or procedure db-object.");
  }
  auto &object_fields = object->fields;

  // Check if all parameters in documents are present in parameter list.
  for (auto json_field : helper::json::member_iterator(doc)) {
    auto key = json_field.first;
    std::shared_ptr<database::entry::ObjectField> object_field;
    if (!mysql_harness::utility::container::get_if(
            object_fields, [key](auto &v) { return v->name == key; },
            &object_field)) {
      throw std::invalid_argument("Not allowed object_field:"s + key);
    }
  }

  ColumnValues result;

  for (auto &ofield : object_fields) {
    auto pfield = dynamic_cast<entry::ParameterField *>(ofield.get());
    if (!pfield) continue;

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == pfield->column_name)) {
      if (user_id.is_empty())
        throw std::invalid_argument("Authentication is required.");
      result.push_back(user_id);
    } else if (pfield->mode == entry::ModeType::kIN) {
      auto it = doc.FindMember(pfield->name.c_str());
      if (it != doc.MemberEnd()) {
        mysqlrouter::sqlstring sql("?");
        sql << std::make_pair(&it->value, pfield->type);
        result.push_back(sql);
      } else {
        result.emplace_back("NULL");
      }
    }
  }

  return result;
}

static mysqlrouter::sqlstring to_sqlstring(const std::string &value,
                                           Column *column) {
  using namespace helper;
  auto v = get_type_inside_text(value);
  auto type = column->type;

  log_debug("to_sqlstring (value:%s, column:%i)", value.c_str(), (int)type);
  switch (type) {
    case ColumnType::BOOLEAN:
      if (kDataInteger == v) return mysqlrouter::sqlstring{value.c_str()};
      return mysqlrouter::sqlstring("?") << value;

    case ColumnType::DOUBLE:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case ColumnType::INTEGER:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case ColumnType::GEOMETRY: {
      auto position = value.find_first_not_of(" \t");
      if (std::string::npos != position && '{' == value[position]) {
        return mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?,1,?)")
               << value << column->srid;
      }
      return mysqlrouter::sqlstring("ST_GeomFromText(?, ?)")
             << value << column->srid;
    }
    case ColumnType::VECTOR: {
      return mysqlrouter::sqlstring("STRING_TO_VECTOR(?)") << value;
    }

    case ColumnType::BINARY:
    case ColumnType::UNKNOWN:
    case ColumnType::JSON:
    case ColumnType::STRING:
      return mysqlrouter::sqlstring("?") << value;
  }

  assert(nullptr && "Shouldn't happen");
  return {};
}

ColumnValues create_function_argument_list(
    const entry::Object *object, const helper::http::Url::Parameters &query_kv,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id) {
  using namespace std::string_literals;

  auto table = object->table;
  if (entry::KindType::PARAMETERS != object->kind) {
    throw std::logic_error(
        "Bad object kind for function or procedure db-object.");
  }
  auto &object_fields = object->fields;

  // Check if all parameters in documents are present in parameter list.
  Url::Keys keys;
  Url::Values values;

  for (const auto &[key, _] : query_kv) {
    const ObjectFieldPtr *param;
    CompareFieldNameF search_for(key);
    if (!mysql_harness::utility::container::get_ptr_if(object_fields,
                                                       search_for, &param)) {
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not allowed parameter:"s + key);
    }
  }

  ColumnValues result;

  for (auto &ofield : object_fields) {
    auto pfield = dynamic_cast<entry::ParameterField *>(ofield.get());
    if (!pfield) continue;

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == pfield->column_name)) {
      if (user_id.is_empty())
        throw std::invalid_argument("Authentication is required.");
      result.push_back(user_id);
    } else if (pfield->mode == entry::ModeType::kIN) {
      auto it = query_kv.find(pfield->name);
      if (it != query_kv.end()) {
        mysqlrouter::sqlstring sql("?");
        sql << to_sqlstring(it->second, pfield);
        result.push_back(sql);
      } else {
        result.emplace_back("NULL");
      }
    }
  }

  return result;
}

mysqlrouter::sqlstring to_sqlstring(const std::string &value, DataType type) {
  using namespace helper;
  auto v = get_type_inside_text(value);
  switch (type) {
    case DataType::BOOLEAN:
      if (kDataInteger == v) return mysqlrouter::sqlstring{value.c_str()};
      return mysqlrouter::sqlstring("?") << value;

    case DataType::DOUBLE:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case DataType::INTEGER:
      if (kDataString == v) return mysqlrouter::sqlstring("?") << value;
      return mysqlrouter::sqlstring{value.c_str()};

    case DataType::BINARY:
      return mysqlrouter::sqlstring("FROM_BASE64(?)") << value;

    case DataType::GEOMETRY: {
      auto position = value.find_first_not_of(" \t");
      if (std::string::npos != position && '{' == value[position]) {
        return mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?)") << value;
      }
      return mysqlrouter::sqlstring("ST_GeomFromText(?)") << value;
    }

    case DataType::VECTOR:
      return mysqlrouter::sqlstring("STRING_TO_VECTOR(?)") << value;

    case DataType::JSON:
      return mysqlrouter::sqlstring("CAST(? as JSON)") << value;

    case DataType::STRING:
      return mysqlrouter::sqlstring("?") << value;

    case DataType::UNKNOWN:
      // Lets handle it by function return.
      break;
  }

  assert(nullptr && "Shouldn't happen");
  return {};
}

void fill_procedure_argument_list_with_binds(
    mrs::database::entry::ResultSets &rs,
    const helper::http::Url::Parameters &query_kv,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id, mrs::database::MysqlBind *out_binds,
    std::string *out_params) {
  using namespace std::string_literals;

  Url::Keys keys;
  Url::Values values;

  auto &pf = rs.parameters.fields;
  for (const auto &[key, _] : query_kv) {
    const database::entry::Field *param;
    CompareFieldNameP search_for(key);
    if (!mysql_harness::utility::container::get_ptr_if(pf, search_for,
                                                       &param)) {
      throw http::Error(HttpStatusCode::BadRequest,
                        "Not allowed parameter:"s + key);
    }
  }

  for (auto &el : pf) {
    if (!out_params->empty()) *out_params += ",";

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == el.bind_name)) {
      if (user_id.is_empty())
        throw std::invalid_argument("Authentication is required.");
      *out_params += user_id.str();
    } else if (el.mode == mrs::database::entry::Field::Mode::modeIn) {
      auto it = query_kv.find(el.name);
      if (query_kv.end() != it) {
        *out_params += to_sqlstring(it->second, el.data_type).str();
      } else {
        *out_params += "NULL";
      }
    } else if (el.mode == mrs::database::entry::Field::Mode::modeOut) {
      out_binds->fill_mysql_bind_for_out(el.data_type);
      *out_params += "?";
    } else {
      auto it = query_kv.find(el.name);
      *out_params += "?";
      if (query_kv.end() != it) {
        log_debug("Bind param el.data_type:%i %s", (int)el.data_type,
                  el.raw_data_type.c_str());
        out_binds->fill_mysql_bind_for_inout(it->second, el.data_type);
      } else {
        out_binds->fill_null_as_inout(el.data_type);
      }
    }
  }
}

void fill_procedure_argument_list_with_binds(
    mrs::database::entry::ResultSets &rs, const rapidjson::Document &doc,
    const entry::RowUserOwnership &ownership,
    const mysqlrouter::sqlstring &user_id, mrs::database::MysqlBind *out_binds,
    std::string *out_params) {
  using namespace std::string_literals;

  Url::Keys keys;
  Url::Values values;

  auto &pf = rs.parameters.fields;

  for (auto &el : pf) {
    if (!out_params->empty()) *out_params += ",";
    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == el.bind_name)) {
      if (user_id.is_empty())
        throw std::invalid_argument("Authentication is required.");
      *out_params += "UNHEX(";
      *out_params += user_id;
      *out_params += ")";
    } else if (el.mode == mrs::database::entry::Field::Mode::modeIn) {
      auto it = doc.FindMember(el.name.c_str());
      if (it != doc.MemberEnd()) {
        mysqlrouter::sqlstring sql = get_sql_format(el.data_type, it->value);
        sql << it->value;
        *out_params += sql.str();
      } else {
        *out_params += "NULL";
      }
    } else if (el.mode == mrs::database::entry::Field::Mode::modeOut) {
      out_binds->fill_mysql_bind_for_out(el.data_type);
      *out_params += "?";
    } else {
      auto it = doc.FindMember(el.name.c_str());
      *out_params += "?";
      if (it != doc.MemberEnd()) {
        out_binds->fill_mysql_bind_for_inout(it->value, el.data_type);
      } else {
        out_binds->fill_null_as_inout(el.data_type);
      }
    }
  }
}

}  // namespace database
}  // namespace mrs
