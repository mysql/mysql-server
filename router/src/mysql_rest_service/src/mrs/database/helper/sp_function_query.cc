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

#include "mrs/database/helper/sp_function_query.h"

#include <stdexcept>

#include "helper/container/generic.h"
#include "helper/http/url.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/json/to_string.h"
#include "helper/mysql_numeric_value.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace helper::json::sql;
using Url = helper::http::Url;
using ColumnType = mrs::database::entry::ColumnType;
using Column = mrs::database::entry::Column;

ColumnValues create_function_argument_list(
    const entry::Object *object, const std::vector<uint8_t> &json_document,
    const entry::RowUserOwnership &ownership, mrs::UniversalId *user_id) {
  using namespace std::string_literals;
  rapidjson::Document doc;

  if (!helper::json::text_to(&doc, json_document))
    throw std::invalid_argument(
        "Can't parse requests payload. I must be an Json object.");

  if (!doc.IsObject())
    throw std::invalid_argument(
        "Parameters must be encoded as fields in Json object.");

  auto table = object->base_tables.front();
  if (entry::KindType::PARAMETERS != object->kind) {
    throw std::logic_error(
        "Bad object kind for function or procedure db-object.");
  }
  auto &object_fields = object->fields;

  // Check if all parameters in documents are present in parameter list.
  for (auto json_field : helper::json::member_iterator(doc)) {
    auto key = json_field.first;
    std::shared_ptr<database::entry::ObjectField> object_field;
    if (!helper::container::get_if(
            object_fields, [key](auto &v) { return v->name == key; },
            &object_field)) {
      throw std::invalid_argument("Not allowed object_field:"s + key);
    }
  }

  ColumnValues result;

  for (auto &ofield : object_fields) {
    auto pfiled = dynamic_cast<entry::ParameterField *>(ofield.get());
    if (!pfiled) continue;

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == pfiled->source->name)) {
      if (!user_id) throw std::invalid_argument("Authentication is required.");
      result.push_back(to_sqlstring(*user_id));
    } else if (pfiled->mode == entry::ModeType::IN) {
      auto it = doc.FindMember(pfiled->name.c_str());
      if (it == doc.MemberEnd())
        throw std::invalid_argument("Missing required field '"s + pfiled->name +
                                    "'.");
      mysqlrouter::sqlstring sql("?");
      sql << std::make_pair(&it->value, pfiled->source->type);
      result.push_back(sql);
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
    const entry::Object *object,
    const http::base::Uri::QueryElements &url_query,
    const entry::RowUserOwnership &ownership, mrs::UniversalId *user_id) {
  using namespace std::string_literals;
  Url::Keys keys;

  auto table = object->base_tables.front();
  if (entry::KindType::PARAMETERS != object->kind) {
    throw std::logic_error(
        "Bad object kind for function or procedure db-object.");
  }

  auto &object_fields = object->fields;

  for (auto &[key, _] : url_query) {
    std::shared_ptr<database::entry::ObjectField> object_field;
    if (!helper::container::get_if(
            object_fields, [key](auto &v) { return v->name == key; },
            &object_field)) {
      throw std::invalid_argument("Not allowed object_field:"s + key);
    }
  }

  ColumnValues result;

  for (auto &ofield : object_fields) {
    auto pfiled = dynamic_cast<entry::ParameterField *>(ofield.get());
    if (!pfiled) continue;

    if (ownership.user_ownership_enforced &&
        (ownership.user_ownership_column == pfiled->source->name)) {
      if (!user_id) throw std::invalid_argument("Authentication is required.");
      result.push_back(to_sqlstring(*user_id));
    } else if (pfiled->mode == entry::ModeType::IN) {
      auto it = url_query.find(ofield->name);

      if (url_query.end() == it)
        throw std::invalid_argument("Missing required field '"s + pfiled->name +
                                    "'.");
      result.push_back(to_sqlstring(it->second, pfiled->source.get()));
    }
  }

  return result;
}

}  // namespace database
}  // namespace mrs
