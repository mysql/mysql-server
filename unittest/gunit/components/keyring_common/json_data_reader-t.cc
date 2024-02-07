/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string>

#include "components/keyrings/common/json_data/json_reader.h"

#include <gtest/gtest.h>

namespace json_reader_unitest {

class KeyringCommonJsonReader_test : public ::testing::Test {};

using keyring_common::json_data::Json_data_extension;
using keyring_common::json_data::Json_reader;
using keyring_common::json_data::output_vector;
using std::string;

TEST_F(KeyringCommonJsonReader_test, JsonReaderTest) {
  std::string data;
  data.assign(
      "{"
      "  \"version\": \"1.0\","
      "  \"elements\": ["
      "    {"
      "      \"user\": \"foo@bar\","
      "      \"data_id\": \"key1\","
      "      \"data_type\": \"AES\","
      "      \"data\": \"6162636465666768696a6b6c6d6e6f70\","
      "      \"extension\": []"
      "    },"
      "    {"
      "      \"user\": \"bar@foo\","
      "      \"data_id\": \"key1\","
      "      \"data_type\": \"RSA\","
      "      \"data\": \"7172737475767778\","
      "      \"extension\": []"
      "    },"
      "    {"
      "      \"user\": \"\","
      "      \"data_id\": \"master_key\","
      "      \"data_type\": \"AES\","
      "      \"data\": \"797a6162636465666768696a6b6c6d6e\","
      "      \"extension\": []"
      "    }"
      "  ]"
      "}");
  Json_reader json_reader(data);
  ASSERT_TRUE(json_reader.valid());
  string expected_version("1.0");
  ASSERT_TRUE(json_reader.version() == expected_version);
  ASSERT_TRUE(json_reader.num_elements() == 3);

  output_vector output;
  ASSERT_FALSE(json_reader.get_elements(output));
  ASSERT_TRUE(output.size() == 3);
  auto element = output[0].first;

  ASSERT_TRUE(element.first.owner_id() == "foo@bar");
  ASSERT_TRUE(element.first.key_id() == "key1");
  ASSERT_TRUE(element.second.data() == "abcdefghijklmnop");
  ASSERT_TRUE(element.second.type() == "AES");

  auto ext = output[0].second.get();
  ASSERT_TRUE(ext->version() == "1.0");

  element = output[1].first;
  ASSERT_TRUE(element.first.owner_id() == "bar@foo");
  ASSERT_TRUE(element.first.key_id() == "key1");
  ASSERT_TRUE(element.second.data() == "qrstuvwx");
  ASSERT_TRUE(element.second.type() == "RSA");

  ext = output[1].second.get();
  ASSERT_TRUE(ext->version() == "1.0");

  element = output[2].first;
  ASSERT_TRUE(element.first.owner_id() == "");
  ASSERT_TRUE(element.first.key_id() == "master_key");
  ASSERT_TRUE(element.second.data() == "yzabcdefghijklmn");
  ASSERT_TRUE(element.second.type() == "AES");

  ext = output[2].second.get();
  ASSERT_TRUE(ext->version() == "1.0");
}

}  // namespace json_reader_unitest
