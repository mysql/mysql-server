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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_COLUMN_MAPPING_CONVERTER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_COLUMN_MAPPING_CONVERTER_H_

#include <stdexcept>
#include <string>

#include "helper/json/text_to.h"
#include "mrs/database/entry/object.h"

namespace mrs {
namespace database {

class ColumnMappingConverter {
 public:
  ColumnMappingConverter() {}

  void operator()(entry::ForeignKeyReference::ColumnMapping *out,
                  const char *value) const {
    if (nullptr == value) {
      *out = {};
      return;
    }

    rapidjson::Document doc = helper::json::text_to_document(value);
    if (!doc.IsArray()) {
      throw std::runtime_error(
          "'object_reference's column 'metadata', must be an array");
    }

    out->clear();
    for (const auto &col : doc.GetArray()) {
      if (!col.IsObject())
        throw std::runtime_error(
            "'object_reference's column 'metadata', element must be an "
            "object.");
      if (!col.HasMember("base") || !col["base"].IsString())
        throw std::runtime_error(
            "'object_reference's column 'metadata', element must contain "
            "'base' field with "
            "string value.");
      if (!col.HasMember("ref") || !col["ref"].IsString())
        throw std::runtime_error(
            "'object_reference's column 'metadata', element must contain 'ref' "
            "field with string "
            "value.");

      out->emplace_back(col["base"].GetString(), col["ref"].GetString());
    }
  }
};

}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_COLUMN_MAPPING_CONVERTER_H_ \
        */
