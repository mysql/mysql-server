/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "helper/json/rapid_json_to_map.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/serializer_to_text.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "helper/optional.h"
#include "helper/string/contains.h"

using namespace helper;
using namespace helper::json;
using testing::ElementsAre;

using MapJsonObj = std::map<std::string, std::string>;

TEST(Json, to_string_empty_obj) { ASSERT_EQ("{}", to_string(MapJsonObj{})); }

TEST(Json, to_string_one_item) {
  ASSERT_EQ("{\"key1\":\"value1\"}", to_string(MapJsonObj{{"key1", "value1"}}));
}

TEST(Json, to_string_two_items) {
  ASSERT_EQ("{\"key1\":\"value1\",\"key2\":\"value2\"}",
            to_string(MapJsonObj{{"key1", "value1"}, {"key2", "value2"}}));
}

TEST(Json, SerializerToText_simple_values) {
  ASSERT_EQ("\"\"", SerializerToText().add_value("").get_result());
  ASSERT_EQ("100", (SerializerToText() << 100).get_result());
  ASSERT_EQ("\"100\"", SerializerToText().add_value("100").get_result());
  ASSERT_EQ("100",
            SerializerToText().add_value("100", JsonType::kJson).get_result());
  ASSERT_EQ("false", (SerializerToText() << false).get_result());
  ASSERT_EQ("true", (SerializerToText() << true).get_result());
  ASSERT_EQ(
      "null",
      SerializerToText().add_value(nullptr, JsonType::kNull).get_result());
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
    *arr1 << 10 << true << false << "txt";
  }
  ASSERT_EQ("{\"key1\":[10,true,false,\"txt\"]}", sut.get_result());
}

static std::string get_json_value(const int allowed_levels,
                                  const std::string &txt,
                                  const std::string key_name) {
  using namespace helper::json;
  helper::json::RapidReaderHandlerToMapOfSimpleValues extractor{allowed_levels};
  if (!helper::json::text_to(&extractor, txt)) {
    ADD_FAILURE() << "Testcase input data are invalid (JSON).";
    return {};
  }

  if (extractor.get_result().count(key_name)) {
    return extractor.get_result().at(key_name);
  }

  return {};
}

TEST(Json, handler_of_simple_values_level1) {
  const int k_level = 1;
  const std::string k_document{
      "{\"a\":1, \"b\":\"text_value\", \"c\":true, \"d\":false, \"e\":null, "
      "\"f\":{\"a\":10} }"};

  ASSERT_EQ("1", get_json_value(k_level, k_document, "a"));
  ASSERT_EQ("text_value", get_json_value(k_level, k_document, "b"));
  ASSERT_EQ("true", get_json_value(k_level, k_document, "c"));
  ASSERT_EQ("false", get_json_value(k_level, k_document, "d"));
  ASSERT_EQ("null", get_json_value(k_level, k_document, "e"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "f.a"));
}

TEST(Json, handler_of_simple_values_level2) {
  const int k_level = 3;
  const std::string k_document{
      "{\"a\":1, \"b\":\"text_value\", \"c\":true, \"d\":false, \"e\":null, "
      "\"f\":{\"a\":10,\"key\":{\"s\":true}} }"};

  ASSERT_EQ("1", get_json_value(k_level, k_document, "a"));
  ASSERT_EQ("text_value", get_json_value(k_level, k_document, "b"));
  ASSERT_EQ("true", get_json_value(k_level, k_document, "c"));
  ASSERT_EQ("false", get_json_value(k_level, k_document, "d"));
  ASSERT_EQ("null", get_json_value(k_level, k_document, "e"));
  ASSERT_EQ("10", get_json_value(k_level, k_document, "f.a"));
  ASSERT_EQ("true", get_json_value(k_level, k_document, "f.key.s"));
}

TEST(Json, handler_of_simple_values_array_ignored) {
  const int k_level = 100;
  const std::string k_document{"{\"a\":1, \"b\":[1], \"c\":20}"};

  ASSERT_EQ("1", get_json_value(k_level, k_document, "a"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "b"));
  ASSERT_EQ("20", get_json_value(k_level, k_document, "c"));
}

TEST(Json, handler_of_simple_values_simple_array_object_ignored) {
  const int k_level = 100;
  const std::string k_document{"{\"a\":1, \"b\":[1,{\"d\":2}], \"c\":20}"};

  ASSERT_EQ("1", get_json_value(k_level, k_document, "a"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "b"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "b.d"));
  ASSERT_EQ("20", get_json_value(k_level, k_document, "c"));
}

TEST(Json, handler_of_simple_values_array_object_ignored) {
  const int k_level = 100;
  const std::string k_document{
      "{\"a\":1, \"b\":[1,{\"d\":2, \"e\":{\"h\":3}}], \"c\":20}"};

  ASSERT_EQ("1", get_json_value(k_level, k_document, "a"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "b"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "b.d"));
  ASSERT_EQ("", get_json_value(k_level, k_document, "b.e.h"));
  ASSERT_EQ("20", get_json_value(k_level, k_document, "c"));
}

class JsonIntArray
    : public helper::json::RapidReaderHandlerToStruct<std::vector<int>> {
 public:
  JsonIntArray(const std::string &path) : path_{path} {}

  bool Int64(int64_t value) override {
    handle_int(value);
    return true;
  }

  bool Uint64(uint64_t value) override {
    handle_int(value);
    return true;
  }

  bool Int(int value) override {
    handle_int(value);
    return true;
  }

  bool Uint(unsigned value) override {
    handle_int(value);
    return true;
  }
  bool RawNumber(const Ch *c, rapidjson::SizeType, bool) override {
    return Int64(as_int64(c));
  }

  int64_t as_int64(const char *s) {
    int64_t i;
    if (1 == sscanf(s, "%" SCNd64, &i)) {
      return i;
    }

    return 0;
  }

  template <typename V>
  void handle_int(V value) {
    if (is_object_path()) return;
    if (path_ != get_current_key()) {
      if (!helper::starts_with(get_current_key(), path_ + separator_)) return;
    }

    result_.push_back(static_cast<int>(value));
  }

  std::string path_;
};

class JsonInt : public helper::json::RapidReaderHandlerToStruct<int> {
 public:
  JsonInt(const std::string &path) : path_{path} { result_ = 0; }

  bool on_new_value() override {
    if (Handler::on_new_value()) {
      std::cout << "level: " << get_level()
                << ", on new value: " << get_current_key() << std::endl;
    }
    return false;
  }

  bool Int64(int64_t value) override {
    Handler::Int64(value);
    handle_int(value);
    return true;
  }

  bool Uint64(uint64_t value) override {
    Handler::Uint64(value);
    handle_int(value);
    return true;
  }

  bool Int(int value) override {
    Handler::Int(value);
    handle_int(value);
    return true;
  }

  bool Uint(unsigned value) override {
    Handler::Uint(value);
    handle_int(value);
    return true;
  }

  bool RawNumber(const Ch *c, rapidjson::SizeType s, bool b) override {
    Handler::RawNumber(c, s, b);
    handle_int(as_int64(c));

    return true;
  }

  int64_t as_int64(const char *s) {
    int64_t i;
    if (1 == sscanf(s, "%" SCNd64, &i)) {
      return i;
    }

    return 0;
  }

  template <typename V>
  void handle_int(V value) {
    std::cout << "key -> " << get_current_key() << "->" << value << std::endl;
    if (path_ != get_current_key()) {
      return;
    }

    result_ = value;
  }

  std::string path_;
};

template <typename Extractor>
static auto get_json_array_int_value(const std::string &txt,
                                     const std::string key_name) {
  using namespace helper::json;
  Extractor extractor{key_name};
  if (!helper::json::text_to(&extractor, txt)) {
    ADD_FAILURE() << "Testcase input data are invalid (JSON).";
    return typename Extractor::Result();
  }

  return extractor.get_result();
}

TEST(JsonStruct, handler_of_values_array) {
  auto extract_array = &get_json_array_int_value<JsonIntArray>;
  const std::string k_document1{"[1,2,3,10]"};
  const std::string k_document2{"{\"a\":[1,2,3,10], \"b\":{\"c\":[8,20]}}"};

  ASSERT_THAT(extract_array(k_document1, ""), ElementsAre(1, 2, 3, 10));
  ASSERT_THAT(extract_array(k_document2, "a"), ElementsAre(1, 2, 3, 10));
  ASSERT_THAT(extract_array(k_document2, "b.c"), ElementsAre(8, 20));
}

TEST(JsonStruct, handler_of_int) {
  auto extract_array = &get_json_array_int_value<JsonInt>;
  const std::string k_document1{"[1,2,3,10]"};
  const std::string k_document2{
      "{\"a\":[2,3,4,11], \"b\":{\"c\":[8,20, {\"d\":30},[44,55,66]]}}"};

  ASSERT_THAT(extract_array(k_document1, "1"), 1);
  ASSERT_THAT(extract_array(k_document1, "4"), 10);
  ASSERT_THAT(extract_array(k_document2, "a.1"), 2);
  ASSERT_THAT(extract_array(k_document2, "a.4"), 11);
  ASSERT_THAT(extract_array(k_document2, "b.c.2"), 20);
  ASSERT_THAT(extract_array(k_document2, "b.c.3.d"), 30);
  ASSERT_THAT(extract_array(k_document2, "b.c.3.d"), 30);
  ASSERT_THAT(extract_array(k_document2, "b.c.4.1"), 44);
  ASSERT_THAT(extract_array(k_document2, "b.c.4.3"), 66);
}
