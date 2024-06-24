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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_OBJECT_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_OBJECT_H_

#include <string>

#include "mrs/interface/object.h"

class MockRoute : public mrs::interface::Object {
 public:
  MOCK_METHOD(void, turn, (const mrs::State state), (override));
  MOCK_METHOD(bool, update, (const void *pe, RouteSchemaPtr schema),
              (override));
  MOCK_METHOD(const std::string &, get_rest_canonical_url, (), (override));
  MOCK_METHOD(const std::string &, get_rest_url, (), (override));
  MOCK_METHOD(const std::string &, get_json_description, (), (override));
  MOCK_METHOD(const std::vector<std::string>, get_rest_path, (), (override));
  MOCK_METHOD(const std::string &, get_rest_path_raw, (), (override));
  MOCK_METHOD(const std::string &, get_rest_canonical_path, (), (override));
  MOCK_METHOD(const std::string &, get_object_path, (), (override));
  MOCK_METHOD(const std::string &, get_schema_name, (), (override));
  MOCK_METHOD(const std::string &, get_object_name, (), (override));
  MOCK_METHOD(const std::string &, get_version, (), (override));
  MOCK_METHOD(const std::string &, get_options, (), (override));
  MOCK_METHOD(EntryObjectPtr, get_object, (), (override));
  MOCK_METHOD(const Fields &, get_parameters, (), (override));
  MOCK_METHOD(uint32_t, get_on_page, (), (override));
  MOCK_METHOD(bool, requires_authentication, (), (override, const));
  MOCK_METHOD(mrs::UniversalId, get_service_id, (), (override, const));
  MOCK_METHOD(mrs::UniversalId, get_id, (), (override, const));
  MOCK_METHOD(bool, has_access, (const Access access), (override, const));
  MOCK_METHOD(Format, get_format, (), (override, const));
  MOCK_METHOD(Media, get_media_type, (), (override, const));
  MOCK_METHOD(uint32_t, get_access, (), (override, const));
  MOCK_METHOD(const RowUserOwnership &, get_user_row_ownership, (),
              (override, const));
  MOCK_METHOD(const VectorOfRowGroupOwnership &, get_group_row_ownership, (),
              (override, const));
  MOCK_METHOD(mrs::interface::ObjectSchema *, get_schema, (), (override));
  MOCK_METHOD(collector::MysqlCacheManager *, get_cache, (), (override));
  MOCK_METHOD(const std::string *, get_default_content, (), (override));
  MOCK_METHOD(const std::string *, get_redirection, (), (override));

  MOCK_METHOD(void, destroy, (), ());
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_OBJECT_H_
