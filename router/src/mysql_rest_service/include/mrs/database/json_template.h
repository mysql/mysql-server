/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_ITEMS_FORMATTER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_ITEMS_FORMATTER_H_

#include <string>
#include <vector>

#include "helper/mysql_column.h"
#include "mysqlrouter/mysql_session.h"

namespace mrs {
namespace database {

class JsonTemplate {
 public:
  using ResultRow = mysqlrouter::MySQLSession::ResultRow;
  virtual ~JsonTemplate() = default;

 public:
  virtual void begin_resultset(const std::string &url,
                               const std::string &items_name,
                               const std::vector<helper::Column> &columns) = 0;
  virtual void begin_resultset(uint64_t offset, uint64_t limit,
                               bool is_default_limit, const std::string &url,
                               const std::vector<helper::Column> &columns) = 0;
  virtual bool push_json_document(const char *document) = 0;
  virtual bool push_json_document(const ResultRow &values,
                                  const char *ignore_column = nullptr) = 0;
  virtual void end_resultset() = 0;

  virtual void begin() = 0;
  virtual void finish() = 0;

  virtual void flush() = 0;
  virtual std::string get_result() = 0;

 protected:
  static bool should_encode_numeric_as_string(enum_field_types field_type) {
    switch (field_type) {
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE:
      case MYSQL_TYPE_DECIMAL:
        return true;
      default:
        return false;
    }

    return false;
  }
};

enum class JsonTemplateType { kStandard, kObjectNested, kObjectUnnested };
class JsonTemplateFactory {
 public:
  virtual ~JsonTemplateFactory() = default;
  virtual std::shared_ptr<JsonTemplate> create_template(
      const JsonTemplateType type = JsonTemplateType::kStandard,
      const bool encode_bigints_as_strings = false,
      const bool include_links = true) const = 0;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_ITEMS_FORMATTER_H_
