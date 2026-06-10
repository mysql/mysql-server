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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_COLUMN_DATATYPE_CONVERTER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_COLUMN_DATATYPE_CONVERTER_H_

#include <map>
#include <string>

#include "mrs/database/entry/object.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

class ColumnDatatypeConverter {
 public:
  ColumnDatatypeConverter() {}

  const std::map<std::string, entry::ColumnType> &get_map() const {
    const static std::map<std::string, entry::ColumnType> k_datatype_map{
        {"TINYINT", entry::ColumnType::INTEGER},
        {"SMALLINT", entry::ColumnType::INTEGER},
        {"MEDIUMINT", entry::ColumnType::INTEGER},
        {"INT", entry::ColumnType::INTEGER},
        {"BIGINT", entry::ColumnType::INTEGER},
        {"FLOAT", entry::ColumnType::DOUBLE},
        {"REAL", entry::ColumnType::DOUBLE},
        {"DOUBLE", entry::ColumnType::DOUBLE},
        {"DECIMAL", entry::ColumnType::DOUBLE},
        {"CHAR", entry::ColumnType::STRING},
        {"NCHAR", entry::ColumnType::STRING},
        {"VARCHAR", entry::ColumnType::STRING},
        {"NVARCHAR", entry::ColumnType::STRING},
        {"BINARY", entry::ColumnType::BINARY},
        {"VARBINARY", entry::ColumnType::BINARY},
        {"TINYTEXT", entry::ColumnType::STRING},
        {"TEXT", entry::ColumnType::STRING},
        {"MEDIUMTEXT", entry::ColumnType::STRING},
        {"LONGTEXT", entry::ColumnType::STRING},
        {"TINYBLOB", entry::ColumnType::BINARY},
        {"BLOB", entry::ColumnType::BINARY},
        {"MEDIUMBLOB", entry::ColumnType::BINARY},
        {"LONGBLOB", entry::ColumnType::BINARY},
        {"JSON", entry::ColumnType::JSON},
        {"DATETIME", entry::ColumnType::STRING},
        {"DATE", entry::ColumnType::STRING},
        {"TIME", entry::ColumnType::STRING},
        {"YEAR", entry::ColumnType::INTEGER},
        {"TIMESTAMP", entry::ColumnType::STRING},
        {"GEOMETRY", entry::ColumnType::GEOMETRY},
        {"POINT", entry::ColumnType::GEOMETRY},
        {"LINESTRING", entry::ColumnType::GEOMETRY},
        {"POLYGON", entry::ColumnType::GEOMETRY},
        {"GEOMCOLLECTION", entry::ColumnType::GEOMETRY},
        {"GEOMETRYCOLLECTION", entry::ColumnType::GEOMETRY},
        {"MULTIPOINT", entry::ColumnType::GEOMETRY},
        {"MULTILINESTRING", entry::ColumnType::GEOMETRY},
        {"MULTIPOLYGON", entry::ColumnType::GEOMETRY},
        {"BIT", entry::ColumnType::BINARY},
        {"BOOLEAN", entry::ColumnType::BOOLEAN},
        {"ENUM", entry::ColumnType::STRING},
        {"SET", entry::ColumnType::STRING},
        {"VECTOR", entry::ColumnType::VECTOR}};

    return k_datatype_map;
  }

  void operator()(entry::ColumnType *out, const std::string &datatype) const {
    const auto &k_datatype_map = get_map();
    auto spc = datatype.find(' ');
    auto p = datatype.find('(');

    p = std::min(spc, p);
    if (p != std::string::npos) {
      if (auto it = k_datatype_map.find(
              mysql_harness::make_upper(datatype.substr(0, p)));
          it != k_datatype_map.end()) {
        if (it->second == entry::ColumnType::BINARY) {
          if (mysql_harness::make_upper(datatype) == "BIT(1)") {
            *out = entry::ColumnType::BOOLEAN;
            return;
          }
        }
        *out = it->second;
        return;
      }
    } else {
      if (auto it = k_datatype_map.find(mysql_harness::make_upper(datatype));
          it != k_datatype_map.end()) {
        *out = it->second;
        return;
      }
    }
    throw std::runtime_error("Unknown datatype " + datatype);
  }
};

}  // namespace database
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_CONVERTERS_COLUMN_DATATYPE_CONVERTER_H_ \
        */
