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

#include "mysql/harness/dynamic_config.h"

////////////////////////////////////////
// Standard include files
#include <sstream>
#include <stdexcept>
#include <string>

////////////////////////////////////////
// Third-party include files
#include <gmock/gmock.h>
#include <gtest/gtest.h>

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

using mysql_harness::DynamicConfig;

class DynamicConfigTest : public ::testing::Test {
 protected:
  DynamicConfig &dynamic_conf_ = DynamicConfig::instance();

  void SetUp() override { DynamicConfig::instance().clear(); }
};

TEST_F(DynamicConfigTest, Empty) {
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            "{}");
}

TEST_F(DynamicConfigTest, NoKeySection) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            R"({"SECTION":{"OPTION1":"VALUE1"}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, SectionWithKey) {
  DynamicConfig::SectionId section_id{"SECTION", "KEY"};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            R"({"SECTION":{"KEY":{"OPTION1":"VALUE1"}}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, OverwriteValue) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE2");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            R"({"SECTION":{"OPTION1":"VALUE2"}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, ClearValue) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  dynamic_conf_.set_option_configured(section_id, "OPTION1", std::monostate{});
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            R"({"SECTION":{}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, MultipleOptions) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  dynamic_conf_.set_option_configured(section_id, "OPTION2", "VALUE2");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            R"({"SECTION":{"OPTION1":"VALUE1","OPTION2":"VALUE2"}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, MultipleSections) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  DynamicConfig::SectionId section2_id{"SECTION2", "KEY2"};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  dynamic_conf_.set_option_configured(section2_id, "OPTION2", "VALUE2");
  EXPECT_EQ(
      dynamic_conf_.get_json_as_string(
          DynamicConfig::ValueType::ConfiguredValue),
      R"({"SECTION":{"OPTION1":"VALUE1"},"SECTION2":{"KEY2":{"OPTION2":"VALUE2"}}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, DifferentOptionTypes) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VALUE1");
  dynamic_conf_.set_option_configured(section_id, "OPTION2", -1);
  dynamic_conf_.set_option_configured(section_id, "OPTION3", false);
  dynamic_conf_.set_option_configured(section_id, "OPTION4", 2.22);
  dynamic_conf_.set_option_configured(section_id, "OPTION5", std::monostate{});
  EXPECT_EQ(
      dynamic_conf_.get_json_as_string(
          DynamicConfig::ValueType::ConfiguredValue),
      R"({"SECTION":{"OPTION1":"VALUE1","OPTION2":-1,"OPTION3":false,"OPTION4":2.22}})");

  // no defaults are set
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            "{}");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            "{}");
}

TEST_F(DynamicConfigTest, SameDefaultForClusterAndClusterSet) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_default(section_id, "OPTION1", "DEF1");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            R"({"SECTION":{"OPTION1":"DEF1"}})");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            R"({"SECTION":{"OPTION1":"DEF1"}})");

  // no values are set, only defaults
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            "{}");
}

TEST_F(DynamicConfigTest, DifferneDefaultForClusterAndClusterSet) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_default(section_id, "OPTION1", "DEF1", "DEF2");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            R"({"SECTION":{"OPTION1":"DEF1"}})");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            R"({"SECTION":{"OPTION1":"DEF2"}})");

  // no values are set, only defaults
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            "{}");
}

TEST_F(DynamicConfigTest, ConfiguredOptionsAndDefaults) {
  DynamicConfig::SectionId section_id{"SECTION", ""};
  dynamic_conf_.set_option_configured(section_id, "OPTION1", "VAL1");
  dynamic_conf_.set_option_default(section_id, "OPTION1", "DEF1", "DEF2");

  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::ConfiguredValue),
            R"({"SECTION":{"OPTION1":"VAL1"}})");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForCluster),
            R"({"SECTION":{"OPTION1":"DEF1"}})");
  EXPECT_EQ(dynamic_conf_.get_json_as_string(
                DynamicConfig::ValueType::DefaultForClusterSet),
            R"({"SECTION":{"OPTION1":"DEF2"}})");
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
