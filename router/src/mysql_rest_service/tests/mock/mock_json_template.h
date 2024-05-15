/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_MOCK_MOCK_JSON_TEMPLATE_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_MOCK_MOCK_JSON_TEMPLATE_H_

#include <memory>

#include "mrs/database/json_template.h"

namespace mrs {
namespace database {

class MockJsonTemplate : public JsonTemplate {
 public:
  MOCK_METHOD(void, begin_resultset,
              (const std::string &url, const std::string &items_name,
               const std::vector<helper::Column> &columns),
              (override));
  MOCK_METHOD(void, begin_resultset,
              (uint64_t offset, uint64_t limit, bool is_default_limit,
               const std::string &url,
               const std::vector<helper::Column> &columns),
              (override));
  MOCK_METHOD(bool, push_json_document, (const char *document), (override));
  MOCK_METHOD(bool, push_json_document,
              (const ResultRow &values, const char *ignore_column), (override));
  MOCK_METHOD(void, end_resultset, (), (override));
  MOCK_METHOD(void, begin, (), (override));
  MOCK_METHOD(void, finish, (), (override));
  MOCK_METHOD(void, flush, (), (override));
  MOCK_METHOD(std::string, get_result, (), (override));
};

}  // namespace database
}  // namespace mrs

namespace mrs {
namespace database {

class MockJsonTemplateFactory : public JsonTemplateFactory {
 public:
  MOCK_METHOD(std::shared_ptr<JsonTemplate>, create_template,
              (const JsonTemplateType type,
               const bool encode_bigints_as_strings, const bool include_links),
              (const, override));
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_TESTS_MOCK_MOCK_JSON_TEMPLATE_H_
