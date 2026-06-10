/*
  Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysqlrouter/cluster_metadata_instance_attributes.h"

#include "mysql/harness/utility/string.h"  // string_format
#include "router/tests/helpers/stdx_expected_no_error.h"

class ClusterMetadataInstanceAttributesTest : public ::testing::Test {};
using mysqlrouter::InstanceAttributes;

using ::testing::ElementsAre;
using ::testing::Pair;

TEST_F(ClusterMetadataInstanceAttributesTest, EmptyTags) {
  const auto res = InstanceAttributes::get_tags(R"({"tags" : {}})");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());
}

TEST_F(ClusterMetadataInstanceAttributesTest, TagsEmptyValue) {
  const auto res = InstanceAttributes::get_tags(R"({"tags" : {"": true}})");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);
}

TEST_F(ClusterMetadataInstanceAttributesTest, TagsOneValue) {
  const auto res = InstanceAttributes::get_tags(R"({"tags" : {"foo": "bar"}})");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);
  EXPECT_THAT(res.value(), ElementsAre(Pair("foo", "\"bar\"")));
}

TEST_F(ClusterMetadataInstanceAttributesTest, TagsMultipleValues) {
  const auto res = InstanceAttributes::get_tags(
      R"({"tags" : {"foo": "bar", "vvv": {}, "x": true, "y": [], "z": null}})");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 5);
  EXPECT_THAT(res.value(), ElementsAre(Pair("foo", "\"bar\""),
                                       Pair("vvv", "{}"), Pair("x", "true"),
                                       Pair("y", "[]"), Pair("z", "null")));
}

TEST_F(ClusterMetadataInstanceAttributesTest, NestedTags) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {"tags": "bar"}})");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);
  EXPECT_THAT(res.value(), ElementsAre(Pair("tags", "\"bar\"")));
}

TEST_F(ClusterMetadataInstanceAttributesTest, NoTags) {
  const auto res = InstanceAttributes::get_tags(R"({"foo" : {"x": "y"}})");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 0);
}

TEST_F(ClusterMetadataInstanceAttributesTest, TagsWrongType) {
  std::vector<std::string> values{"\"hidden\"", "[]", "true", "1", "null"};

  for (const auto &val : values) {
    const auto res = InstanceAttributes::get_tags(R"({"tags" : )" + val + "}");
    EXPECT_FALSE(res);
    EXPECT_STREQ("tags field is not a valid JSON object", res.error().c_str());
  }
}

TEST_F(ClusterMetadataInstanceAttributesTest, TagsCaseSensitivity) {
  const auto res =
      InstanceAttributes::get_tags(R"({"TAGS" : {"_hidden": true}})");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 0);
}

TEST_F(ClusterMetadataInstanceAttributesTest, MultipleFields) {
  const auto res = InstanceAttributes::get_tags(R"({"tags": {}, "foo": {} })");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 0);
}

TEST_F(ClusterMetadataInstanceAttributesTest, IsHiddenOnly) {
  // hidden = true
  {
    const auto res =
        InstanceAttributes::get_tags(R"({"tags" : {"_hidden": true} })");
    ASSERT_NO_ERROR(res);
    EXPECT_THAT(res.value(), ElementsAre(Pair("_hidden", "true")));
    const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
    ASSERT_NO_ERROR(hidden_res);
    EXPECT_TRUE(hidden_res.value());
  }
  // hidden = false
  {
    const auto res =
        InstanceAttributes::get_tags(R"({"tags" : {"_hidden": false} })");
    ASSERT_NO_ERROR(res);
    EXPECT_THAT(res.value(), ElementsAre(Pair("_hidden", "false")));
    const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
    ASSERT_NO_ERROR(hidden_res);
    EXPECT_FALSE(hidden_res.value());
  }
}

TEST_F(ClusterMetadataInstanceAttributesTest, IsHiddenAdditionalTags) {
  const auto res = InstanceAttributes::get_tags(
      R"({"tags" : {"foo" : "bar", "_hidden": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_THAT(res.value(),
              ElementsAre(Pair("_hidden", "true"), Pair("foo", "\"bar\"")));
  const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_TRUE(hidden_res.value());
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenDefaultValue) {
  const auto res = InstanceAttributes::get_tags(R"({"tags" : {}})");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());

  const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_FALSE(hidden_res.value());
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenWrongType) {
  const std::vector<std::string> &values{"1",        "0",        "null",
                                         "\"null\"", "\"true\"", "\"false\"",
                                         "{}",       "[]",       "\"\""};

  for (const auto &val : values) {
    const auto res =
        InstanceAttributes::get_tags(R"({"tags" : {"_hidden": )" + val + "}}");
    ASSERT_NO_ERROR(res);
    EXPECT_THAT(res.value(), ElementsAre(Pair("_hidden", val)));

    const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
    EXPECT_FALSE(hidden_res);
    EXPECT_STREQ("tags._hidden not a boolean", hidden_res.error().c_str());
  }
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenNoTags) {
  const auto res =
      InstanceAttributes::get_tags(R"({"foo" : {"_hidden": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());

  const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_FALSE(hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenNotInTags) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {"_unrecognized": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);

  const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_FALSE(hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenNoValue) {
  const auto res = InstanceAttributes::get_tags(R"({"foo" : {"_hidden": } })");
  EXPECT_FALSE(res);
  EXPECT_STREQ(res.error().c_str(), "not a valid JSON object");
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenCaseSensitivity) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {"_HIDDEN": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);

  const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_FALSE(hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenValueCaseSensitivity) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {"_hidden": TRUE} })");
  EXPECT_FALSE(res);
  EXPECT_STREQ(res.error().c_str(), "not a valid JSON object");
}

TEST_F(ClusterMetadataInstanceAttributesTest, HiddenOutsideTags) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {}, "_hidden": true} )");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());

  const auto hidden_res = InstanceAttributes::get_hidden(res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_FALSE(hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       IsDisconnectExistingSessionsWhenHiddenOnly) {
  // disconnect_existing_sessions_when_hidden = true
  {
    const auto res = InstanceAttributes::get_tags(
        R"({"tags" : {"_disconnect_existing_sessions_when_hidden": true} })");
    ASSERT_NO_ERROR(res);
    EXPECT_THAT(
        res.value(),
        ElementsAre(Pair("_disconnect_existing_sessions_when_hidden", "true")));
    const auto disconnect_existing_sessions_when_hidden_res =
        InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
            res.value(), false);
    ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
    EXPECT_TRUE(disconnect_existing_sessions_when_hidden_res.value());
  }
  // disconnect_existing_sessions_when_hidden = false
  {
    const auto res = InstanceAttributes::get_tags(
        R"({"tags" : {"_disconnect_existing_sessions_when_hidden": false} })");
    ASSERT_NO_ERROR(res);
    EXPECT_THAT(res.value(),
                ElementsAre(Pair("_disconnect_existing_sessions_when_hidden",
                                 "false")));
    const auto disconnect_existing_sessions_when_hidden_res =
        InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
            res.value(), false);
    ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
    EXPECT_FALSE(disconnect_existing_sessions_when_hidden_res.value());
  }
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       IsDisconnectExistingSessionsWhenHiddenAdditionalTags) {
  const auto res = InstanceAttributes::get_tags(
      R"({"tags" : {"foo" : "bar", "_disconnect_existing_sessions_when_hidden": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_THAT(
      res.value(),
      ElementsAre(Pair("_disconnect_existing_sessions_when_hidden", "true"),
                  Pair("foo", "\"bar\"")));
  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_TRUE(disconnect_existing_sessions_when_hidden_res.value());
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenDefaultValue) {
  const auto res = InstanceAttributes::get_tags(R"({"tags" : {}})");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());

  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_FALSE(disconnect_existing_sessions_when_hidden_res.value());
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenWrongType) {
  const std::vector<std::string> &values{"1",        "0",        "null",
                                         "\"null\"", "\"true\"", "\"false\"",
                                         "{}",       "[]",       "\"\""};

  for (const auto &val : values) {
    const auto res = InstanceAttributes::get_tags(
        R"({"tags" : {"_disconnect_existing_sessions_when_hidden": )" + val +
        "}}");
    ASSERT_NO_ERROR(res);
    EXPECT_THAT(
        res.value(),
        ElementsAre(Pair("_disconnect_existing_sessions_when_hidden", val)));

    const auto disconnect_existing_sessions_when_hidden_res =
        InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
            res.value(), false);
    EXPECT_FALSE(disconnect_existing_sessions_when_hidden_res);
    EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
                 disconnect_existing_sessions_when_hidden_res.error().c_str());
  }
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenNoTags) {
  const auto res = InstanceAttributes::get_tags(
      R"({"foo" : {"_disconnect_existing_sessions_when_hidden": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());

  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_FALSE(
      disconnect_existing_sessions_when_hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenNotInTags) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {"_unrecognized": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);

  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_FALSE(
      disconnect_existing_sessions_when_hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenNoValue) {
  const auto res = InstanceAttributes::get_tags(
      R"({"foo" : {"_disconnect_existing_sessions_when_hidden": } })");
  EXPECT_FALSE(res);
  EXPECT_STREQ(res.error().c_str(), "not a valid JSON object");
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenCaseSensitivity) {
  const auto res =
      InstanceAttributes::get_tags(R"({"tags" : {"_HIDDEN": true} })");
  ASSERT_NO_ERROR(res);
  EXPECT_EQ(res.value().size(), 1);

  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_FALSE(
      disconnect_existing_sessions_when_hidden_res.value());  // Default is used
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenValueCaseSensitivity) {
  const auto res = InstanceAttributes::get_tags(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": TRUE} })");
  EXPECT_FALSE(res);
  EXPECT_STREQ(res.error().c_str(), "not a valid JSON object");
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       DisconnectExistingSessionsWhenHiddenOutsideTags) {
  const auto res = InstanceAttributes::get_tags(
      R"({"tags" : {}, "_disconnect_existing_sessions_when_hidden": true} )");
  ASSERT_NO_ERROR(res);
  EXPECT_TRUE(res.value().empty());

  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_FALSE(
      disconnect_existing_sessions_when_hidden_res.value());  // Default is used
}

class BothHiddenAndDisconnectWhenHiddenTest
    : public ClusterMetadataInstanceAttributesTest,
      public ::testing::WithParamInterface<std::pair<bool, bool>> {};

TEST_P(BothHiddenAndDisconnectWhenHiddenTest,
       BothHiddenAndDisconnectWhenHidden) {
  auto bool_to_str = [](const bool val) { return val ? "true" : "false"; };

  const bool hidden = GetParam().first;
  const bool disconnect_when_hidden = GetParam().second;

  const auto tags_res =
      InstanceAttributes::get_tags(mysql_harness::utility::string_format(
          R"({"tags" : {"_hidden": %s, "_disconnect_existing_sessions_when_hidden": %s}} )",
          bool_to_str(hidden), bool_to_str(disconnect_when_hidden)));

  ASSERT_NO_ERROR(tags_res);
  EXPECT_EQ(tags_res.value().size(), 2);

  const auto disconnect_existing_sessions_when_hidden_res =
      InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
          tags_res.value(), false);
  ASSERT_NO_ERROR(disconnect_existing_sessions_when_hidden_res);
  EXPECT_EQ(disconnect_existing_sessions_when_hidden_res.value(),
            disconnect_when_hidden);

  const auto hidden_res =
      InstanceAttributes::get_hidden(tags_res.value(), false);
  ASSERT_NO_ERROR(hidden_res);
  EXPECT_EQ(hidden_res.value(), hidden);
}

INSTANTIATE_TEST_SUITE_P(BothHiddenAndDisconnectWhenHidden,
                         BothHiddenAndDisconnectWhenHiddenTest,
                         ::testing::Values(std::make_pair(true, true),
                                           std::make_pair(true, false),
                                           std::make_pair(false, true),
                                           std::make_pair(false, false)));

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
