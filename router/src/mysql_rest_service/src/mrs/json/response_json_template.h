/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_RESPONSE_ITEMS_FORMATTER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_RESPONSE_ITEMS_FORMATTER_H_

#include <string>
#include <vector>

#include "helper/json/serializer_to_text.h"
#include "helper/mysql_column.h"
#include "mysqlrouter/mysql_session.h"

namespace mrs {
namespace json {

class ResponseJsonTemplate {
 public:
  using JsonSerializer = helper::json::SerializerToText;
  using ResultRow = mysqlrouter::MySQLSession::ResultRow;

 public:
  void begin(const std::string &url, const std::string &items_name);
  void begin(uint32_t offset, uint32_t limit, bool is_default_limit,
             const std::string &url);
  bool push_json_document(const char *document);
  bool push_json_document(const ResultRow &values,
                          const std::vector<helper::Column> &columns,
                          const char *ignore_column = nullptr);
  void end();

  void flush();
  std::string get_result();

 private:
  bool count_check_if_push_is_allowed();

  // External data needed by the template
  uint32_t offset_;
  uint32_t limit_;
  bool is_default_limit_;
  bool limit_not_set_{false};
  std::string url_;

  // Needed for serialization of json document
  JsonSerializer serializer_;
  JsonSerializer::Object json_root_;
  JsonSerializer::Array json_root_items_;

  // Internal state, for use case verification and template filling
  bool has_more_{false};
  uint32_t pushed_documents_{0};
  bool began_{false};
};

}  // namespace json
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_RESPONSE_ITEMS_FORMATTER_H_
