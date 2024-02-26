/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <cstdio> /* std::remove */
#include <fstream>

#include "components/keyrings/common/config/config_reader.h"

namespace config_reader_unitest {

class KeyringCommonConfigReader_test : public ::testing::Test {};

using keyring_common::config::Config_reader;

TEST_F(KeyringCommonConfigReader_test, Config_reader_test) {
  std::string config_file_name("config_file");
  std::string config_data =
      "{"
      "\"config_1\": \"This is a string config.\","
      "\"config_2\": false,"
      "\"config_3\": 42"
      "}";
  std::ofstream out_file(config_file_name.c_str());
  ASSERT_TRUE(out_file.is_open());
  out_file.write(config_data.c_str(), config_data.length());
  out_file.close();

  Config_reader config_reader(config_file_name);

  std::string string_config;
  ASSERT_FALSE(
      config_reader.get_element<std::string>("config_1", string_config));
  ASSERT_EQ(string_config, "This is a string config.");

  bool bool_config = true;
  ASSERT_FALSE(config_reader.get_element<bool>("config_2", bool_config));
  ASSERT_FALSE(bool_config);

  int int_config = 0;
  ASSERT_FALSE(config_reader.get_element<int>("config_3", int_config));
  ASSERT_EQ(int_config, 42);

  int non_existing = 0;
  ASSERT_TRUE(
      config_reader.get_element<int>("config_nonexisting", non_existing));

  std::remove(config_file_name.c_str());
}

}  // namespace config_reader_unitest
