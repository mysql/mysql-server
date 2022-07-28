/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

/**
 * Tests the metadata cache plugin implementation.
 */
#include <gtest/gtest.h>

#include "cluster_metadata.h"

class MetadataCacheInstanceAttributesTest : public ::testing::Test {};

TEST_F(MetadataCacheInstanceAttributesTest, IsHidden) {
  std::string warning;

  EXPECT_TRUE(get_hidden(R"({"tags" : {"_hidden": true} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(
      get_hidden(R"({"tags" : {"foo" : "bar", "_hidden": true} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": false} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden("", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden("not json", warning));
  EXPECT_STREQ("not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden("{}", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags": {} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_unrecognized": true} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"": true} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags": {}, "foo": {} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"foo" : {"_hidden": false} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : "_hidden" })", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : [] })", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : null })", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : true})", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : "foo"})", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : 0})", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  // we do not do any conversion, _hidden has to be boolean
  // if it's updated via shell API
  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": 0} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": 1} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": "true"} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": "false"} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": "foo"} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": "null"} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": {} } })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": [] } })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": ""} })", warning));
  EXPECT_STREQ("tags._hidden not a boolean", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"foo": 0} })", warning));
  EXPECT_STREQ("", warning.c_str());

  // we are case sensitive
  EXPECT_FALSE(get_hidden(R"({"TAGS" : {"_hidden": true} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"TAGS" : {"_hidden": false} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_HIDDEN": true} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_HIDDEN": false} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": TRUE} })", warning));
  EXPECT_STREQ("not a valid JSON object", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {"_hidden": FALSE} })", warning));
  EXPECT_STREQ("not a valid JSON object", warning.c_str());

  // outside of the tags object does not have an effect
  EXPECT_FALSE(get_hidden(R"({"tags" : {}, "_hidden": true })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_hidden(R"({"tags" : {}, "_hidden": false })", warning));
  EXPECT_STREQ("", warning.c_str());
}

TEST_F(MetadataCacheInstanceAttributesTest,
       IsDisconnect_existing_sessions_when_hidden) {
  std::string warning;

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"foo" : "bar", "_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden("", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(
      get_disconnect_existing_sessions_when_hidden("not json", warning));
  EXPECT_STREQ("not a valid JSON object", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden("{}", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"foo" : {"_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : "_disconnect_existing_sessions_when_hidden" })", warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": ""} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  // we do not do any conversion, _disconnect_existing_sessions_when_hidden has
  // to be boolean if it's updated via shell API it should always be
  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": 0} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": 1} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "true"} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "false"} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": "foo"} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": null} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": {} } })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": [] } })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": ""} })",
      warning));
  EXPECT_STREQ("tags._disconnect_existing_sessions_when_hidden not a boolean",
               warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"foo": 0} })", warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(R"({"tags" : 1 })",
                                                           warning));
  EXPECT_STREQ("tags - not a valid JSON object", warning.c_str());

  // we are case sensitive
  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"TAGS" : {"_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"TAGS" : {"_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_DISCONNECT_EXISTING_SESSIONS_WHEN_HIDDEN": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_DISCONNECT_EXISTING_SESSIONS_WHEN_HIDDEN": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": FALSE} })",
      warning));
  EXPECT_STREQ("not a valid JSON object", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_disconnect_existing_sessions_when_hidden": TRUE} })",
      warning));
  EXPECT_STREQ("not a valid JSON object", warning.c_str());

  // outside of the tags object does not have an effect
  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {}, "_disconnect_existing_sessions_when_hidden": false })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {}, "_disconnect_existing_sessions_when_hidden": true })",
      warning));
  EXPECT_STREQ("", warning.c_str());
}

TEST_F(MetadataCacheInstanceAttributesTest, BothHiddenAndDisconnectWhenHidden) {
  std::string warning;

  // true, true
  EXPECT_TRUE(get_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  // true, false
  EXPECT_TRUE(get_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": true, "_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  // false, true
  EXPECT_FALSE(get_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_TRUE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": true} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  // false, false
  EXPECT_FALSE(get_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());

  EXPECT_FALSE(get_disconnect_existing_sessions_when_hidden(
      R"({"tags" : {"_hidden": false, "_disconnect_existing_sessions_when_hidden": false} })",
      warning));
  EXPECT_STREQ("", warning.c_str());
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
