/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include <memory>
#include <string>

#include "mrs/json/response_sp_json_template_nest.h"

using testing::Test;
using namespace mrs::json;

class JsonTemplateNestedTests : public Test {
 public:
  ResponseSpJsonTemplateNest sut_;
};

TEST_F(JsonTemplateNestedTests, no_iteration_doesnt_generates) {
  ASSERT_EQ("", sut_.get_result());
}

TEST_F(JsonTemplateNestedTests, begin_end_generated_empty_resultsets_list) {
  sut_.begin();
  sut_.finish();
  ASSERT_EQ("{\"items\":[]}", sut_.get_result());
}

TEST_F(JsonTemplateNestedTests,
       begin_bresultset_eresultset_end_generates_single_resultset_withoutdata) {
  sut_.begin();
  sut_.begin_resultset("local", "myitems", {});
  sut_.end_resultset();
  sut_.finish();
  ASSERT_EQ(
      "{\"items\":[{\"type\":\"myitems\",\"items\":[],\"metadata\":{"
      "\"columns\":[]}}]}",
      sut_.get_result());
}

TEST_F(
    JsonTemplateNestedTests,
    begin_bresultset_eresultset_end_generates_single_resultset_with_only_metadata) {
  sut_.begin();
  sut_.begin_resultset("local", "myitems", {{"c1", "INTEGER"}, {"c2", "TEXT"}});
  sut_.end_resultset();
  sut_.finish();
  ASSERT_EQ(
      "{\"items\":["
      "{"
      "\"type\":\"myitems\","
      "\"items\":[],"
      "\"metadata\":{"
      "\"columns\":["
      "{\"name\":\"c1\",\"type\":\"INTEGER\"},"
      "{\"name\":\"c2\",\"type\":\"TEXT\"}"
      "]}}]}",
      sut_.get_result());
}
