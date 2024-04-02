/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#include <string>

#include "mrs/database/filter_object_generator.h"

#include "helper/json/text_to.h"

using namespace mrs::database;

using testing::Test;

class FilterObjectsTest : public Test {
 public:
  static rapidjson::Document json(const std::string &j) {
    rapidjson::Document result;
    helper::json::text_to(&result, j);
    return result;
  }

  FilterObjectsTest() : sut_({}, false, 0) {}

  FilterObjectGenerator sut_;
};

TEST_F(FilterObjectsTest, empty_json_has_nothing_configured) {
  sut_.parse(json(""));
  ASSERT_FALSE(sut_.has_asof());
  ASSERT_FALSE(sut_.has_order());
  ASSERT_FALSE(sut_.has_where());
}

TEST_F(FilterObjectsTest, int_json_throws) {
  ASSERT_THROW(sut_.parse(json("10")), std::exception);
}

TEST_F(FilterObjectsTest, string_json_throws) {
  ASSERT_THROW(sut_.parse(json("\"value\"")), std::exception);
}

TEST_F(FilterObjectsTest, bool_json_throws) {
  ASSERT_THROW(sut_.parse(json("true")), std::exception);
}

TEST_F(FilterObjectsTest, empty_array_json_throws) {
  ASSERT_THROW(sut_.parse(json("[]")), std::exception);
}

TEST_F(FilterObjectsTest, int_array_json_throws) {
  ASSERT_THROW(sut_.parse(json("[1,2,3]")), std::exception);
}

TEST_F(FilterObjectsTest, empty_object_accepted) {
  sut_.parse(json("{}"));
  ASSERT_EQ("", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, orderby_field_must_be_an_object) {
  ASSERT_THROW(sut_.parse(json("{\"$orderby\":1}")), std::exception);
}

TEST_F(FilterObjectsTest, orderby_field_must_be_an_object_with_fields) {
  ASSERT_THROW(sut_.parse(json("{\"$orderby\":{}}")), std::exception);
}

TEST_F(FilterObjectsTest, orderby_one_field_asc) {
  sut_.parse(json("{\"$orderby\":{\"test_field\":1}}"));
  ASSERT_EQ(" ORDER BY `test_field` ASC", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, orderby_two_fields_asc) {
  sut_.parse(json("{\"$orderby\":{\"test_field\":1, \"field2\":-1}}"));
  ASSERT_EQ(" ORDER BY `test_field` ASC, `field2` DESC",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_by_int_value) {
  sut_.parse(json("{\"f1\":1}"));
  ASSERT_EQ(" `f1`=1", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_by_string_value) {
  sut_.parse(json("{\"f1\":\"abc123\"}"));
  ASSERT_EQ(" `f1`='abc123'", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_fields) {
  sut_.parse(json("{\"f1\":\"abc123\", \"f2\":10}"));
  ASSERT_EQ(" `f1`='abc123' AND `f2`=10", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_complex) {
  sut_.parse(json("{\"f1\":{\"$eq\":1}}"));
  ASSERT_EQ(" `f1` = 1", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_complex_greater) {
  sut_.parse(json("{\"f1\":{\"$gt\":1}}"));
  ASSERT_EQ(" `f1` > 1", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_complex_between) {
  sut_.parse(json("{\"f1\":{\"$between\":[1,100]}}"));
  ASSERT_EQ(" `f1` BETWEEN 1 AND 100", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, not_supported_match_field_complex_less_and_greater) {
  ASSERT_THROW(sut_.parse(json("{\"f1\":{\"$gt\":1, \"$lt\":100}}")),
               std::exception);
}

TEST_F(FilterObjectsTest, complex_and_one_element) {
  sut_.parse(json("{\"$and\":[{\"v1\":1}]}"));
  ASSERT_EQ("(( `v1`=1))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_and_two_elements) {
  sut_.parse(json("{\"$and\":[{\"v1\":1},{\"v2\":\"a\"}]}"));
  ASSERT_EQ("(( `v1`=1) AND( `v2`='a'))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_or_one_element) {
  sut_.parse(json("{\"$or\":[{\"v1\":1}]}"));
  ASSERT_EQ("(( `v1`=1))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_or_two_elements) {
  sut_.parse(json("{\"$or\":[{\"v1\":1},{\"v2\":\"a\"}]}"));
  ASSERT_EQ("(( `v1`=1) OR( `v2`='a'))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, invalid_match_objects) {
  ASSERT_THROW(sut_.parse(json("{\"$match\":[]}")), std::exception);
  ASSERT_THROW(sut_.parse(json("{\"$match\":{}}")), std::exception);
  ASSERT_THROW(
      sut_.parse(json("{\"$match\":{\"$params\":[\"c1\"], \"$against\":{}}}")),
      std::exception);
  ASSERT_THROW(sut_.parse(json("{\"$match\":{\"$params\":[\"c1\"], "
                               "\"$against\":{\"$expr\":false}}}")),
               std::exception);
  ASSERT_THROW(sut_.parse(json("{\"$match\":{\"$params\":{}, "
                               "\"$against\":{\"$expr\":\"c1\"}}}")),
               std::exception);
  ASSERT_THROW(sut_.parse(json("{\"$match\":{\"$params\":false, "
                               "\"$against\":{\"$expr\":\"c1\"}}}")),
               std::exception);
}

TEST_F(FilterObjectsTest, match_expression_without_modifiers) {
  sut_.parse(
      json("{\"$match\":{\"$params\":[\"c1\"], "
           "\"$against\":{\"$expr\":\"q1\"}}}"));
  EXPECT_EQ("(MATCH (`c1`) AGAINST('q1' ) )", sut_.get_result().str());

  sut_.parse(
      json("{\"$match\":{\"$params\":[\"c1\", \"c2\"], "
           "\"$against\":{\"$expr\":\"q1\"}}}"));
  EXPECT_EQ("(MATCH (`c1`,`c2`) AGAINST('q1' ) )", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_expression_invalid_modifier) {
  ASSERT_THROW(
      sut_.parse(json("{\"$match\":{\"$params\":[\"c1\"], "
                      "\"$against\":{\"$expr\":\"q1\", \"$modifier\":\"\"}}}")),
      std::exception);
  ASSERT_THROW(
      sut_.parse(json(
          "{\"$match\":{\"$params\":[\"c1\"], "
          "\"$against\":{\"$expr\":\"q1\", \"$modifier\":\"SOME TEXT\"}}}")),
      std::exception);
  ASSERT_THROW(sut_.parse(json(
                   "{\"$match\":{\"$params\":[\"c1\"], "
                   "\"$against\":{\"$expr\":\"q1\", \"$modifier\":false}}}")),
               std::exception);
  ASSERT_THROW(
      sut_.parse(json("{\"$match\":{\"$params\":[\"c1\"], "
                      "\"$against\":{\"$expr\":\"q1\", \"$modifier\":10}}}")),
      std::exception);
}

TEST_F(FilterObjectsTest, match_expression_with_modifier) {
  sut_.parse(
      json("{\"$match\":{\"$params\":[\"c1\"], "
           "\"$against\":{\"$expr\":\"q1\", \"$modifier\":\"WITH QUERY "
           "EXPANSION\"}}}"));
  EXPECT_EQ("(MATCH (`c1`) AGAINST('q1' WITH QUERY EXPANSION) )",
            sut_.get_result().str());

  sut_.parse(json(
      "{\"$match\":{\"$params\":[\"c1\", \"c2\"], "
      "\"$against\":{\"$expr\":\"q1\", \"$modifier\":\"IN BOOLEAN MODE\"}}}"));
  EXPECT_EQ("(MATCH (`c1`,`c2`) AGAINST('q1' IN BOOLEAN MODE) )",
            sut_.get_result().str());
}
