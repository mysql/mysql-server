/*  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <map>

#include "helper/json/serializer_to_text.h"
#include "helper/json/to_string.h"
#include "helper/optional.h"

using namespace helper;
using namespace helper::json;

using MapJsonObj = std::map<std::string, std::string>;

TEST(Json, to_string_empty_obj) { ASSERT_EQ("{}", to_string(MapJsonObj{})); }

TEST(Json, to_string_one_item) {
  ASSERT_EQ("{\"key1\": \"value1\"}",
            to_string(MapJsonObj{{"key1", "value1"}}));
}

TEST(Json, to_string_two_items) {
  ASSERT_EQ("{\"key1\": \"value1\", \"key2\": \"value2\"}",
            to_string(MapJsonObj{{"key1", "value1"}, {"key2", "value2"}}));
}

TEST(Json, SerializerToText_simple_values) {
  ASSERT_EQ("\"\"", SerializerToText().add_value("").get_result());
  ASSERT_EQ("100", SerializerToText().add_value(100).get_result());
  ASSERT_EQ("\"100\"", SerializerToText().add_value("100").get_result());
  ASSERT_EQ(
      "100",
      SerializerToText().add_value("100", ColumnJsonTypes::kJson).get_result());
  ASSERT_EQ("false", SerializerToText().add_value(false).get_result());
  ASSERT_EQ("true", SerializerToText().add_value(true).get_result());
  ASSERT_EQ("null", SerializerToText()
                        .add_value(nullptr, ColumnJsonTypes::kNull)
                        .get_result());
}

TEST(Json, SerializerToText_object_empty) {
  SerializerToText sut;
  { auto obj1 = sut.add_object(); }
  ASSERT_EQ("{}", sut.get_result());
}

TEST(Json, SerializerToText_object_one_value) {
  SerializerToText sut;
  {
    auto obj1 = sut.add_object();
    obj1->member_add_value("key1", "Value1");
  }
  ASSERT_EQ("{\"key1\":\"Value1\"}", sut.get_result());
}

TEST(Json, SerializerToText_object_optional_values) {
  SerializerToText sut;
  {
    Optional<const char *> v1;
    Optional<const char *> v2{"test"};
    Optional<uint32_t> v3{1};
    auto obj1 = sut.add_object();
    obj1->member_add_value("key1", v1);
    obj1->member_add_value("key2", v2);
    obj1->member_add_value("key3", v3);
  }
  ASSERT_EQ("{\"key2\":\"test\",\"key3\":1}", sut.get_result());
}

TEST(Json, SerializerToText_object_with_empty_array) {
  SerializerToText sut;
  {
    auto obj1 = sut.add_object();
    auto arr1 = obj1->member_add_array("key1");
  }
  ASSERT_EQ("{\"key1\":[]}", sut.get_result());
}

TEST(Json, SerializerToText_object_with_array) {
  SerializerToText sut;
  {
    auto obj1 = sut.add_object();
    auto arr1 = obj1->member_add_array("key1");
    arr1->add_value(10).add_value(true).add_value(false).add_value("txt");
  }
  ASSERT_EQ("{\"key1\":[10,true,false,\"txt\"]}", sut.get_result());
}
