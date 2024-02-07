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

#include <cstdio> /* std::remove */

#include <components/keyrings/common/json_data/json_reader.h>
#include <components/keyrings/common/json_data/json_writer.h>
#include "components/keyrings/common/data_file/reader.h"
#include "components/keyrings/common/data_file/writer.h"

#include <gtest/gtest.h>

namespace json_file_operations_unittest {

class KeyringCommonJsonFileOperations_test : public ::testing::Test {};

using keyring_common::data::Data;
using keyring_common::data_file::File_reader;
using keyring_common::data_file::File_writer;
using keyring_common::json_data::Json_data_extension;
using keyring_common::json_data::Json_reader;
using keyring_common::json_data::Json_writer;
using keyring_common::json_data::output_vector;
using keyring_common::meta::Metadata;

TEST_F(KeyringCommonJsonFileOperations_test, JsonFileTests) {
  /* Write JSON data */
  Json_writer json_writer;
  Json_data_extension json_data_extension;
  {
    Metadata metadata("key1", "foo@bar");
    Data data("abcdefghijklmnop", "AES");
    ASSERT_FALSE(json_writer.add_element(metadata, data, json_data_extension));
  }
  {
    Metadata metadata("key1", "bar@foo");
    Data data("qrstuvwx", "RSA");
    ASSERT_FALSE(json_writer.add_element(metadata, data, json_data_extension));
  }
  {
    Metadata metadata("master_key", "");
    Data data("yzabcdefghijklmn", "AES");
    ASSERT_FALSE(json_writer.add_element(metadata, data, json_data_extension));
  }

  /* Write JSON data to file */
  std::string file_name("json_file_operations_test");
  std::string json_data = json_writer.to_string();
  ASSERT_TRUE(json_data.length() > 0);

  File_writer file_writer(file_name, json_data);
  ASSERT_TRUE(file_writer.valid());

  /* Read JSON data from file */
  std::string read_data;
  File_reader file_reader(file_name, true, read_data);
  ASSERT_TRUE(file_reader.valid());

  ASSERT_TRUE(read_data == json_data);

  /* Move data to JSON document */
  Json_reader json_reader(read_data);
  ASSERT_TRUE(json_reader.valid());

  ASSERT_TRUE(json_reader.num_elements() == 3);

  output_vector output;
  ASSERT_FALSE(json_reader.get_elements(output));
  ASSERT_TRUE(output.size() == 3);

  /* Validate retrieved data */
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

  ASSERT_TRUE(std::remove(file_name.c_str()) == 0);
}

}  // namespace json_file_operations_unittest
