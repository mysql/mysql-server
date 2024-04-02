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

#include "mrs/json/response_json_template.h"

using namespace helper;
using namespace helper::json;
using namespace mrs::json;
using testing::ElementsAre;

using MapJsonObj = std::map<std::string, std::string>;

TEST(ResponseJson, single_items_list) {
  ResponseJsonTemplate sut;
  sut.begin_resultset("url", "items", {});
  sut.end_resultset();
  ASSERT_EQ(
      "{\"items\":[],"
      "\"count\":0,\"links\":[{\"rel\":\"self\",\"href\":\"url/\"}]}",
      sut.get_result());
}

TEST(ResponseJson, multiple_items_list) {
  ResponseJsonTemplate sut;
  sut.begin_resultset("url", "items", {});
  sut.begin_resultset("url", "items2", {});
  sut.begin_resultset("url", "items3", {});
  sut.end_resultset();
  ASSERT_EQ(
      "{\"items\":[],\"items2\":[],\"items3\":[],"
      "\"count\":0,\"links\":[{\"rel\":\"self\",\"href\":\"url/\"}]}",
      sut.get_result());
}

TEST(ResponseJson, single_items_list_not_empty) {
  ResponseJsonTemplate sut;
  sut.begin_resultset("url", "items", {});
  sut.push_json_document("{\"a1\":1}");
  sut.push_json_document("{\"a2\":2}");
  sut.end_resultset();
  ASSERT_EQ(
      "{\"items\":[{\"a1\":1},{\"a2\":2}],"
      "\"count\":2,\"links\":[{\"rel\":\"self\",\"href\":\"url/\"}]}",
      sut.get_result());
}

TEST(ResponseJson, multiple_items_list_not_empty) {
  ResponseJsonTemplate sut;
  sut.begin_resultset("url", "items", {});
  sut.push_json_document("{\"a1\":1}");
  sut.begin_resultset("url", "items2", {});
  sut.push_json_document("{\"a2\":2}");
  sut.begin_resultset("url", "items3", {});
  sut.push_json_document("{\"a3\":3}");
  sut.end_resultset();
  ASSERT_EQ(
      "{\"items\":[{\"a1\":1}],\"items2\":[{\"a2\":2}],\"items3\":[{\"a3\":3}],"
      "\"count\":3,\"links\":[{\"rel\":\"self\",\"href\":\"url/\"}]}",
      sut.get_result());
}
