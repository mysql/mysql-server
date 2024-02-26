/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "base64.h"
#include "my_aes.h"
#include "my_byteorder.h"
#include "my_inttypes.h"
#include "my_rnd.h"
#include "my_systime.h"
#include "sql-common/json_binary.h"
#include "sql-common/json_dom.h"
#include "sql-common/json_path.h"
#include "sql/json_diff.h"
#include "sql/my_decimal.h"
#include "sql/persisted_variable.h"
#include "sql/sql_class.h"
#include "sql/sql_time.h"
#include "sql_string.h"
#include "template_utils.h"  // down_cast
#include "unittest/gunit/test_utils.h"

/**
 Test Json_dom class hierarchy API, cf. json_dom.h
 */
namespace persisted_variables_unittest {

using my_testing::Server_initializer;
using std::string;

class Persisted_variablesTest : public ::testing::Test {
 protected:
  void SetUp() override { initializer.SetUp(); }
  void TearDown() override { initializer.TearDown(); }
  THD *thd() const { return initializer.thd(); }

  Server_initializer initializer;
};

TEST_F(Persisted_variablesTest, HexEncryption) {
  string secret("thisisa32bitlongsecretpassword1");
  const size_t file_key_length = 32;
  auto file_key = std::make_unique<unsigned char[]>(file_key_length);
  const size_t iv_length = 16;
  auto iv = std::make_unique<unsigned char[]>(iv_length);

  Persisted_variables_cache cache;

  ASSERT_TRUE(file_key.get() != nullptr);
  ASSERT_TRUE(iv.get() != nullptr);

  ASSERT_FALSE(my_rand_buffer(file_key.get(), file_key_length) ||
               my_rand_buffer(iv.get(), iv_length));

  size_t encrypted_key_length =
      (file_key_length / MY_AES_BLOCK_SIZE) * MY_AES_BLOCK_SIZE;

  auto encrypted_key = std::make_unique<unsigned char[]>(encrypted_key_length);

  auto error =
      my_aes_encrypt(file_key.get(), file_key_length, encrypted_key.get(),
                     reinterpret_cast<const unsigned char *>(secret.c_str()),
                     secret.length(), my_aes_256_cbc, iv.get(), false);

  ASSERT_TRUE(error != -1);
  ASSERT_TRUE(static_cast<size_t>(error) == encrypted_key_length);
  ASSERT_TRUE(file_key_length == encrypted_key_length);

  string pre_ivstring(reinterpret_cast<char *>(iv.get()), iv_length);
  string pre_filekeystring(reinterpret_cast<char *>(file_key.get()),
                           file_key_length);

  string filekeyhex = cache.to_hex(std::string{
      reinterpret_cast<char *>(encrypted_key.get()), encrypted_key_length});
  string ivhex =
      cache.to_hex(std::string{reinterpret_cast<char *>(iv.get()), iv_length});

  string pre_encrypted_key{reinterpret_cast<char *>(encrypted_key.get()),
                           encrypted_key_length};

  string filekeystring = cache.from_hex(filekeyhex);
  string post_ivstring = cache.from_hex(ivhex);

  ASSERT_TRUE(filekeystring == pre_encrypted_key);

  auto decrypted_key =
      std::make_unique<unsigned char[]>(filekeystring.length());

  error = my_aes_decrypt(
      reinterpret_cast<const unsigned char *>(filekeystring.c_str()),
      filekeystring.length(), decrypted_key.get(),
      reinterpret_cast<const unsigned char *>(secret.c_str()), secret.length(),
      my_aes_256_cbc,
      reinterpret_cast<const unsigned char *>(post_ivstring.c_str()), false);

  ASSERT_TRUE(error != -1);
  ASSERT_TRUE(error == file_key_length);

  string post_filekeystring{reinterpret_cast<char *>(decrypted_key.get()),
                            file_key_length};

  ASSERT_TRUE(pre_filekeystring == post_filekeystring);
}

TEST_F(Persisted_variablesTest, ReadWrite) {
  ulonglong timestamp = my_micro_time();

  /* RW variables */
  st_persist_var rw_variable_1("rw_variable_1", "rw_variable_1_value",
                               timestamp, "user", "host", false);
  st_persist_var rw_variable_2("rw_variable_2", "rw_variable_2_value",
                               timestamp, "user", "host", false);
  st_persist_var rw_variable_3("rw_variable_3", "rw_variable_3_value",
                               timestamp, "user", "host", false);
  st_persist_var rw_variable_4("rw_variable_4", "", timestamp, "user", "host",
                               true);

  std::vector<st_persist_var> rw_variables_vector;
  rw_variables_vector.push_back(rw_variable_1);
  rw_variables_vector.push_back(rw_variable_2);
  rw_variables_vector.push_back(rw_variable_3);
  rw_variables_vector.push_back(rw_variable_4);

  string rw_variables_vector_key("mysql_rw_variables");

  /* Static variables */
  st_persist_var static_variable_1("static_variable_1",
                                   "static_variable_1_value", timestamp, "user",
                                   "host", false);
  st_persist_var static_variable_2("static_variable_2",
                                   "static_variable_2_value", timestamp, "user",
                                   "host", false);
  st_persist_var static_variable_3("static_variable_3",
                                   "static_variable_3_value", timestamp, "user",
                                   "host", false);
  st_persist_var static_variable_4("static_variable_4", "", timestamp, "user",
                                   "host", true);

  std::map<string, st_persist_var> static_variables_map;
  static_variables_map[static_variable_1.key] = static_variable_1;
  static_variables_map[static_variable_2.key] = static_variable_2;
  static_variables_map[static_variable_3.key] = static_variable_3;
  static_variables_map[static_variable_4.key] = static_variable_4;

  string static_variables_map_key("mysql_static_variables");

  /* Format JSON */

  auto format_json = [](auto &entry, Json_object &section_object) -> bool {
    const string key_value("Value");
    const string key_metadata("Metadata");
    const string key_timestamp("Timestamp");
    const string key_user("User");
    const string key_host("Host");
    /*
      Create metadata array
      "Metadata" : {
      "Timestamp" : timestamp_value,
      "User" : "user_name",
      "Host" : "host_name"
      }
    */
    Json_object object_metadata;
    Json_uint value_timestamp(entry.timestamp);
    if (object_metadata.add_clone(key_timestamp, &value_timestamp)) return true;

    Json_string value_user(entry.user.c_str());
    if (object_metadata.add_clone(key_user, &value_user)) return true;

    Json_string value_host(entry.host.c_str());
    if (object_metadata.add_clone(key_host, &value_host)) return true;

    /*
      Create variable object

      "variable_name" : {
        "Value" : "variable_value",
        "Metadata" : {
        "Timestamp" : timestamp_value,
        "User" : "user_name",
        "Host" : "host_name"
        }
      },
    */
    Json_object variable;

    Json_string value_value(entry.value.c_str());
    if (variable.add_clone(key_value, &value_value)) return true;

    if (variable.add_clone(key_metadata, &object_metadata)) return true;

    /* Push object to array */
    if (section_object.add_clone(entry.key, &variable)) return true;
    return false;
  };

  auto add_json_object = [](Json_object &section_object,
                            Json_object &json_object,
                            string key_section) -> bool {
    if (section_object.cardinality() > 0) {
      if (json_object.add_clone(key_section.c_str(), &section_object))
        return true;
    }
    return false;
  };

  auto format_vector = [&format_json, &add_json_object](
                           std::vector<st_persist_var> &section,
                           string key_section,
                           Json_object &json_object) -> bool {
    Json_object section_object;
    for (auto &it : section) {
      if (format_json(it, section_object)) return true;
    }
    if (add_json_object(section_object, json_object, key_section)) return true;
    return false;
  };

  auto format_map = [&format_json, &add_json_object](
                        std::map<string, st_persist_var> &section,
                        string key_section, Json_object &json_object) -> bool {
    Json_object section_object;
    for (auto &it : section) {
      if (format_json(it.second, section_object)) return true;
    }
    if (add_json_object(section_object, json_object, key_section)) return true;
    return false;
  };

  Json_object main_json_object;

  ulonglong version = 2;
  string key_version("Version");
  Json_uint value_version(2);
  main_json_object.add_clone(key_version.c_str(), &value_version);

  ASSERT_TRUE(format_vector(rw_variables_vector, rw_variables_vector_key,
                            main_json_object) == false);
  ASSERT_TRUE(format_map(static_variables_map, static_variables_map_key,
                         main_json_object) == false);

  /* Extract and Validate */

  auto validate_version = [](const Json_object &json_object,
                             const std::string &version_key,
                             const ulonglong &expected_version) -> bool {
    if (json_object.json_type() != enum_json_type::J_OBJECT) return true;
    Json_dom *version_dom = json_object.get(version_key);
    if (version_dom == nullptr ||
        version_dom->json_type() != enum_json_type::J_UINT)
      return true;
    Json_uint *fetched_version = down_cast<Json_uint *>(version_dom);
    if (fetched_version->value() != expected_version) return true;

    return false;
  };

  ASSERT_TRUE(validate_version(main_json_object, key_version, version) ==
              false);

  auto extract_entry = [](const Json_object *json_object,
                          st_persist_var &output) -> bool {
    const string key_value("Value");
    const string key_metadata("Metadata");
    const string key_timestamp("Timestamp");
    const string key_user("User");
    const string key_host("Host");
    if (json_object->json_type() != enum_json_type::J_OBJECT) return true;

    Json_dom *dom = nullptr;
    Json_string *value_string = nullptr;
    Json_uint *value_uint = nullptr;

    dom = json_object->get(key_value);
    if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
      return true;
    value_string = down_cast<Json_string *>(dom);
    output.value.assign(value_string->value());
    output.is_null = (output.value.length() == 0);

    dom = json_object->get(key_metadata);
    if (dom == nullptr || dom->json_type() != enum_json_type::J_OBJECT)
      return true;
    Json_object *object_metadata = down_cast<Json_object *>(dom);

    dom = object_metadata->get(key_timestamp);
    if (dom == nullptr || dom->json_type() != enum_json_type::J_UINT)
      return true;
    value_uint = down_cast<Json_uint *>(dom);
    output.timestamp = value_uint->value();

    dom = object_metadata->get(key_user);
    if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
      return true;
    value_string = down_cast<Json_string *>(dom);
    output.user.assign(value_string->value());

    dom = object_metadata->get(key_host);
    if (dom == nullptr || dom->json_type() != enum_json_type::J_STRING)
      return true;
    value_string = down_cast<Json_string *>(dom);
    output.host.assign(value_string->value());

    return false;
  };

  auto extract_vector = [&extract_entry](
                            const Json_object &json_object,
                            const std::string vector_key,
                            std::vector<st_persist_var> &output) -> bool {
    if (json_object.json_type() != enum_json_type::J_OBJECT) return true;
    Json_dom *vector_dom = json_object.get(vector_key);
    /* Not having any entry is fine */
    if (vector_dom == nullptr) return false;
    if (vector_dom->json_type() != enum_json_type::J_OBJECT) return true;
    Json_object *section_object = down_cast<Json_object *>(vector_dom);
    for (auto &it : *section_object) {
      /* Ignore problematic entry */
      if (it.second == nullptr ||
          it.second->json_type() != enum_json_type::J_OBJECT)
        continue;
      const Json_object *element =
          down_cast<const Json_object *>(it.second.get());
      st_persist_var entry;
      entry.key = it.first;
      /* Ignore problematic entry */
      if (extract_entry(element, entry)) continue;
      output.push_back(entry);
    }
    return false;
  };

  auto extract_map =
      [&extract_entry](const Json_object &json_object,
                       const std::string vector_key,
                       std::map<std::string, st_persist_var> &output) -> bool {
    if (json_object.json_type() != enum_json_type::J_OBJECT) return true;
    Json_dom *map_dom = json_object.get(vector_key);
    /* Not having any entry is fine */
    if (map_dom == nullptr) return false;
    if (map_dom->json_type() != enum_json_type::J_OBJECT) return true;
    Json_object *section_object = down_cast<Json_object *>(map_dom);
    for (auto &it : *section_object) {
      /* Ignore problematic entry */
      if (it.second == nullptr ||
          it.second->json_type() != enum_json_type::J_OBJECT)
        continue;
      const Json_object *element =
          down_cast<const Json_object *>(it.second.get());
      st_persist_var entry;
      entry.key = it.first;
      /* Ignore problematic entry */
      if (extract_entry(element, entry)) continue;
      output[entry.key] = entry;
    }
    return false;
  };

  auto compare = [](st_persist_var &lhs, st_persist_var &rhs) -> bool {
    return (lhs.key == rhs.key && lhs.value == rhs.value &&
            lhs.timestamp == rhs.timestamp && lhs.user == rhs.user &&
            lhs.host == rhs.host);
  };

  std::vector<st_persist_var> output_rw_variables_vector;
  ASSERT_TRUE(extract_vector(main_json_object, rw_variables_vector_key,
                             output_rw_variables_vector) == false);

  ASSERT_TRUE(rw_variables_vector.size() == output_rw_variables_vector.size());
  for (size_t index = 0; index < output_rw_variables_vector.size(); ++index) {
    auto &input = rw_variables_vector[index];
    auto &output = output_rw_variables_vector[index];
    ASSERT_TRUE(compare(input, output));
  }

  std::map<std::string, st_persist_var> output_static_variables_map;
  ASSERT_TRUE(extract_map(main_json_object, static_variables_map_key,
                          output_static_variables_map) == false);

  ASSERT_TRUE(static_variables_map.size() ==
              output_static_variables_map.size());

  for (auto it_input = static_variables_map.cbegin(),
            it_output = output_static_variables_map.cbegin();
       it_input != static_variables_map.cend() &&
       it_output != output_static_variables_map.cend();
       ++it_input, ++it_output) {
    auto input = it_input->second;
    auto output = it_output->second;
    ASSERT_TRUE(compare(input, output));
  }
}

}  // namespace persisted_variables_unittest
