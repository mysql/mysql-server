/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <gmock/gmock.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "configuration_update_schema.h"

namespace {
using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonSchemaDocument =
    rapidjson::GenericSchemaDocument<JsonValue, rapidjson::CrtAllocator>;
using JsonSchemaValidator =
    rapidjson::GenericSchemaValidator<JsonSchemaDocument>;
}  // namespace

class TestConfigurationUpdateSchema : public ::testing::Test {
 protected:
  // if valid returns std::nullopt
  // if NOT valid returs <invalid_keyword, location> pair
  std::optional<std::pair<std::string, std::string>>
  is_json_valid_against_schema(const std::string &json,
                               const std::string &schema) {
    // 1. create schema object from string
    JsonDocument schema_json;
    if (schema_json
            .Parse<rapidjson::kParseCommentsFlag>(schema.data(),
                                                  schema.length())
            .HasParseError()) {
      throw std::invalid_argument(
          "Parsing JSON schema failed at offset " +
          std::to_string(schema_json.GetErrorOffset()) + ": " +
          rapidjson::GetParseError_En(schema_json.GetParseError()));
    }
    JsonSchemaDocument schema_doc(schema_json);

    // 2. create json document from string to verify
    JsonDocument verified_json_doc;
    if (verified_json_doc
            .Parse<rapidjson::kParseCommentsFlag>(json.data(), json.length())
            .HasParseError()) {
      throw std::invalid_argument(
          "Parsing JSON failed at offset " +
          std::to_string(schema_json.GetErrorOffset()) + ": " +
          rapidjson::GetParseError_En(verified_json_doc.GetParseError()));
    }

    // 3. validate JSON set in the metadata against the schema
    return is_json_valid_against_schema(schema_doc, verified_json_doc);
  }

  const std::string configuration_update_schema =
      std::string(ConfigurationUpdateJsonSchema::data(),
                  ConfigurationUpdateJsonSchema::size());

 private:
  // if valid returns std::nullopt
  // if NOT valid returs <invalid_keyword, location> pair
  std::optional<std::pair<std::string, std::string>>
  is_json_valid_against_schema(const JsonSchemaDocument &schema,
                               const JsonDocument &json) {
    JsonSchemaValidator validator(schema);
    if (!json.Accept(validator)) {
      // validation failed - throw an error with info of where the problem is
      rapidjson::StringBuffer sb_schema;
      validator.GetInvalidSchemaPointer().StringifyUriFragment(sb_schema);
      rapidjson::StringBuffer sb_json;
      validator.GetInvalidDocumentPointer().StringifyUriFragment(sb_json);
      return std::make_pair(validator.GetInvalidSchemaKeyword(),
                            sb_json.GetString());
    }

    return std::nullopt;
  }
};

struct ConfigurationUpdateSchemaParam {
  std::string json;
  std::string test_name;
};

class TestConfigurationUpdateSchemaValid
    : public TestConfigurationUpdateSchema,
      public ::testing::WithParamInterface<ConfigurationUpdateSchemaParam> {};

TEST_P(TestConfigurationUpdateSchemaValid, Spec) {
  RecordProperty("Worklog", "15649");
  RecordProperty("RequirementId", "FR3,FR3.1");
  RecordProperty(
      "Description",
      "Testing if exposed schema validates the example inputs correctly");

  auto result = is_json_valid_against_schema(GetParam().json,
                                             configuration_update_schema);

  if (result) {
    FAIL() << "Unexpected schema validation error at: " << result->first << ":"
           << result->second;
  }
}

auto get_test_description(
    const ::testing::TestParamInfo<ConfigurationUpdateSchemaParam> &info) {
  return info.param.test_name;
}

INSTANTIATE_TEST_SUITE_P(
    Spec, TestConfigurationUpdateSchemaValid,
    ::testing::Values(
        ConfigurationUpdateSchemaParam{"{}", "empty"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"target_cluster": "some_cluster"}})",
            "target_cluster_val"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"target_cluster": "" } })",
            "target_cluster_empty"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"use_replica_primary_as_rw": true}})",
            "use_replica_primary_as_rw_true"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"use_replica_primary_as_rw": false}})",
            "use_replica_primary_as_rw_false"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"stats_updates_frequency": -1}})",
            "stats_updates_frequency_minus_1"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"stats_updates_frequency": 10}})",
            "stats_updates_frequency_10"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"read_only_targets": "all"}})",
            "read_only_targets_all"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"read_only_targets": "read_replicas"}})",
            "read_only_targets_read_replicas"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"read_only_targets": "secondaries"}})",
            "read_only_targets_secondaries"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"unreachable_quorum_allowed_traffic": "none"}})",
            "unreachable_quorum_allowed_traffic_none"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"unreachable_quorum_allowed_traffic": "read"}})",
            "unreachable_quorum_allowed_traffic_read"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"unreachable_quorum_allowed_traffic": "all"}})",
            "unreachable_quorum_allowed_traffic_all"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"invalidated_cluster_policy": "accept_ro"}})",
            "invalidated_cluster_policy_accept_ro"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"invalidated_cluster_policy": "drop_all"}})",
            "invalidated_cluster_policy_drop_all"},
        ConfigurationUpdateSchemaParam{R"({"routing_rules" : {
          "target_cluster": "some_cluster",
          "use_replica_primary_as_rw": true,
          "stats_updates_frequency": 10,
          "read_only_targets": "all",
          "unreachable_quorum_allowed_traffic": "read",
          "invalidated_cluster_policy": "accept_ro"
          }})",
                                       "all_supported_options"}),
    get_test_description);

class TestConfigurationUpdateSchemaInvalid
    : public TestConfigurationUpdateSchema,
      public ::testing::WithParamInterface<ConfigurationUpdateSchemaParam> {};

TEST_P(TestConfigurationUpdateSchemaInvalid, Spec) {
  RecordProperty("Worklog", "15649");
  RecordProperty("RequirementId", "FR3,FR3.1");
  RecordProperty("Description",
                 "Testing if exposed schema validates the example INVALID "
                 "inputs correctly");
  auto result = is_json_valid_against_schema(GetParam().json,
                                             configuration_update_schema);

  if (!result) {
    FAIL() << "Expected the schema validation to fail: " << GetParam().json;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Spec, TestConfigurationUpdateSchemaInvalid,
    ::testing::Values(
        ConfigurationUpdateSchemaParam{
            R"({"unsupported_section" : {"target_cluster": ""}})",
            "unsupported_section"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"unsupported_option": ""}})",
            "unsupported_option"},
        // wrong types
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"target_cluster": false } })",
            "target_cluster_bool"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"use_replica_primary_as_rw": "abc"}})",
            "use_replica_primary_as_rw_string"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"stats_updates_frequency": true}})",
            "stats_updates_frequency_bool"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"read_only_targets": 1}})",
            "read_only_targets_int"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"unreachable_quorum_allowed_traffic": false}})",
            "unreachable_quorum_allowed_traffic_bool"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"invalidated_cluster_policy": 1}})",
            "invalidated_cluster_policy_int"},
        // invalid values
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"read_only_targets": "unsupported"}})",
            "read_only_targets_unsupported"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"unreachable_quorum_allowed_traffic": "unsupported"}})",
            "unreachable_quorum_allowed_traffic_unsupported"},
        ConfigurationUpdateSchemaParam{
            R"({"routing_rules" : {"invalidated_cluster_policy": "unsupported"}})",
            "invalidated_cluster_policy_unsupported"}),
    get_test_description);

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
