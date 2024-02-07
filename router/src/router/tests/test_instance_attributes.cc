/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

#include "mysqlrouter/cluster_metadata_instance_attributes.h"

class ClusterMetadataInstanceAttributesTest : public ::testing::Test {};
using mysqlrouter::InstanceAttributes;

TEST_F(ClusterMetadataInstanceAttributesTest, IsHidden) {
  stdx::expected<bool, std::string> res;

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": true} })", false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_hidden(
      R"({"tags" : {"foo" : "bar", "_hidden": true} })", false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": false} })", true);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_hidden("", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_hidden("not json", true);
  EXPECT_FALSE(res);
  EXPECT_STREQ("not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden("{}", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_hidden(R"({"tags": {} })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_unrecognized": true} })",
                                       true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"": true} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_hidden(R"({"tags": {}, "foo": {} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res =
      InstanceAttributes::get_hidden(R"({"foo" : {"_hidden": false} })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_hidden(R"({"tags" : "_hidden" })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : [] })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : null })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : true })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : "foo" })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : 0 })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": 0} })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": 1 } })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": "true" } })",
                                       false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": "false" } })",
                                       false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": "foo" } })",
                                       false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": "null" } })",
                                       false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": {} } })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": [] } })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": "" } })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._hidden not a boolean", res.error().c_str());

  res = InstanceAttributes::get_hidden(R"({"tags" : {"foo": 0 } })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  // we are case sensitive
  res =
      InstanceAttributes::get_hidden(R"({"TAGS" : {"_hidden": true} })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res =
      InstanceAttributes::get_hidden(R"({"TAGS" : {"_hidden": false} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_HIDDEN": true} })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_HIDDEN": false} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": TRUE} })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("not a valid JSON object", res.error().c_str());

  res =
      InstanceAttributes::get_hidden(R"({"tags" : {"_hidden": FALSE} })", true);
  EXPECT_FALSE(res);
  EXPECT_STREQ("not a valid JSON object", res.error().c_str());

  // outside of the tags object does not have an effect
  res = InstanceAttributes::get_hidden(R"({"tags" : {}, "_hidden": true })",
                                       false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_hidden(R"({"tags" : {}, "_hidden": false })",
                                       true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       IsDisconnect_existing_sessions_when_hidden) {
  stdx::expected<bool, std::string> res;

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"foo" : "bar", "_disconnect_existing_sessions_when_hidden": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": false} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden("",
                                                                         false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      "not json", true);
  EXPECT_FALSE(res);
  EXPECT_STREQ("not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden("{}",
                                                                         false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags": {} })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_unrecognized": true} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"": true} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags": {}, "foo": {} })", true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"foo" : {"_disconnect_existing_sessions_when_hidden": false} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : "_disconnect_existing_sessions_when_hidden" })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : [] })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : null })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : true })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : "foo" })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : 0 })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags - not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": 0} })", false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": 1 } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "true" } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "false" } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "foo" } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "null" } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": {} } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": [] } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "" } })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"foo": 0 } })", false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  // we are case sensitive
  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"TAGS" : {"_disconnect_existing_sessions_when_hidden": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"TAGS" : {"_disconnect_existing_sessions_when_hidden": false} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_DISCONNECT_EXISTING_SESSIONS_WHEN_HIDDEN": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_DISCONNECT_EXISTING_SESSIONS_WHEN_HIDDEN": false} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": TRUE} })",
      false);
  EXPECT_FALSE(res);
  EXPECT_STREQ("not a valid JSON object", res.error().c_str());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": FALSE} })",
      true);
  EXPECT_FALSE(res);
  EXPECT_STREQ("not a valid JSON object", res.error().c_str());

  // outside of the tags object does not have an effect
  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {}, "_disconnect_existing_sessions_when_hidden": true })",
      false);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {}, "_disconnect_existing_sessions_when_hidden": false })",
      true);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());
}

TEST_F(ClusterMetadataInstanceAttributesTest,
       BothHiddenAndDisconnectWhenHidden) {
  stdx::expected<bool, std::string> res;

  // true, true
  res = InstanceAttributes::get_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  // true, false
  res = InstanceAttributes::get_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  // false, true
  res = InstanceAttributes::get_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": true} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": true} })",
      false);
  EXPECT_TRUE(res);
  EXPECT_TRUE(res.value());

  // false, false
  res = InstanceAttributes::get_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": false} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());

  res = InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": false} })",
      true);
  EXPECT_TRUE(res);
  EXPECT_FALSE(res.value());
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
