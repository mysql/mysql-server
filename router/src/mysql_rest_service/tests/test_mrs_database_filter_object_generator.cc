/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
#include <string>

#include "helper/json/text_to.h"
#include "mrs/database/filter_object_generator.h"
#include "test_mrs_object_utils.h"

using namespace mrs::database;

using testing::Test;

class FilterObjectsTest : public Test {
 public:
  FilterObjectsTest() : sut_({}, false, 0) {}

  FilterObjectGenerator sut_;
};

TEST_F(FilterObjectsTest, empty_json_has_nothing_configured) {
  sut_.parse("");
  ASSERT_FALSE(sut_.has_asof());
  ASSERT_FALSE(sut_.has_order());
  ASSERT_FALSE(sut_.has_where());
}

TEST_F(FilterObjectsTest, int_json_throws) {
  ASSERT_THROW(sut_.parse("10"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, string_json_throws) {
  ASSERT_THROW(sut_.parse("\"value\""), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, bool_json_throws) {
  ASSERT_THROW(sut_.parse("true"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, empty_array_json_throws) {
  ASSERT_THROW(sut_.parse("[]"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, int_array_json_throws) {
  ASSERT_THROW(sut_.parse("[1,2,3]"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, non_json_value_throws) {
  ASSERT_THROW(sut_.parse("some-string"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, operator_null_with_other_arg_throws) {
  ASSERT_THROW(sut_.parse(R"({"f1":{"$null":1}})"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, operator_notnull_with_other_arg_throws) {
  ASSERT_THROW(sut_.parse(R"({"f1":{"$notnull":"some string"}})"),
               mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, unknown_operator_throws) {
  ASSERT_THROW(sut_.parse(R"({"col1": {"eq": "pENELOPE"}})"),
               mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, empty_object_accepted) {
  sut_.parse("{}");
  ASSERT_EQ("", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, orderby_field_must_be_an_object) {
  ASSERT_THROW(sut_.parse("{\"$orderby\":1}"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, orderby_field_must_be_an_object_with_fields) {
  ASSERT_THROW(sut_.parse("{\"$orderby\":{}}"), mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, orderby_one_field_asc) {
  sut_.parse("{\"$orderby\":{\"test_field\":1}}");
  ASSERT_EQ(" ORDER BY `test_field` ASC", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, orderby_two_fields_asc) {
  sut_.parse("{\"$orderby\":{\"test_field\":1, \"field2\":-1}}");
  ASSERT_EQ(" ORDER BY `test_field` ASC, `field2` DESC",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_by_int_value) {
  sut_.parse(R"({"f1":1})");
  ASSERT_EQ("(`f1`=1)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_by_string_value) {
  sut_.parse(R"({"f1": "abc123!"})");
  ASSERT_EQ("(`f1`='abc123!')", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_by_binary_value) {
  auto root = JsonMappingBuilder("mrstestdb", "test")
                  .field("f1", "f1", "BINARY(16)", FieldFlag::PRIMARY)
                  .resolve();

  FilterObjectGenerator sut(root);

  sut.parse(R"({"f1":"MzMAAAAAAAAAAAAAAAAAAA=="})");
  ASSERT_EQ("(`f1`=FROM_BASE64('MzMAAAAAAAAAAAAAAAAAAA=='))",
            sut.get_result().str());
}

TEST_F(FilterObjectsTest, match_fields) {
  sut_.parse(R"({"f1":"abc123!", "f2":10})");
  ASSERT_EQ("(`f1`='abc123!') AND (`f2`=10)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_equal) {
  sut_.parse(R"({"f1":{"$eq":1}})");
  ASSERT_EQ("(`f1` = 1)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_null) {
  sut_.parse(R"({"f1":{"$null":null}})");
  ASSERT_EQ("(`f1` IS NULL)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_notnull) {
  sut_.parse(R"({"f1":{"$notnull":null}})");
  ASSERT_EQ("(`f1` IS NOT NULL)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_greater) {
  sut_.parse(R"({"f1":{"$gt":1}})");
  ASSERT_EQ("(`f1` > 1)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_between) {
  sut_.parse(R"({"f1":{"$between":[1,100]}})");
  ASSERT_EQ("(`f1` BETWEEN 1 AND 100)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_between_null1) {
  sut_.parse(R"({"f1":{"$between":[null,100]}})");
  ASSERT_EQ("(`f1` BETWEEN NULL AND 100)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_between_null2) {
  sut_.parse(R"({"f1":{"$between":["abc", null]}})");
  ASSERT_EQ("(`f1` BETWEEN 'abc' AND NULL)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_simple_operator_between_null3) {
  sut_.parse(R"({"f1":{"$between":[null, null]}})");
  ASSERT_EQ("(`f1` BETWEEN NULL AND NULL)", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_field_complex_less_and_greater) {
  sut_.parse(R"({"f1":[{"$gt":1}, {"$lt":100}]})");
  ASSERT_EQ("((`f1` > 1) AND (`f1` < 100))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_and_one_element) {
  sut_.parse(R"({"$and":[{"v1":1}]})");
  ASSERT_EQ("((`v1`=1))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_and_two_elements) {
  sut_.parse(R"({"$and":[{"v1":1},{"v2":"a"}]})");
  ASSERT_EQ("((`v1`=1) AND (`v2`='a'))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_or_one_element) {
  sut_.parse(R"({"$or":[{"v1":1}]})");
  ASSERT_EQ("((`v1`=1))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_or_two_elements) {
  sut_.parse(R"({"$or":[{"v1":1},{"v2":"a"}]})");
  ASSERT_EQ("((`v1`=1) OR (`v2`='a'))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, invalid_match_objects) {
  ASSERT_THROW(sut_.parse(R"({"$match":[]})"), mrs::interface::RestError);
  ASSERT_THROW(sut_.parse(R"({"$match":{}})"), mrs::interface::RestError);
  ASSERT_THROW(sut_.parse(R"({"$match":{"$params":["c1"], "$against":{}}})"),
               mrs::interface::RestError);
  ASSERT_THROW(
      sut_.parse(
          R"({"$match":{"$params":["c1"], "$against":{"$expr":false}}})"),
      mrs::interface::RestError);
  ASSERT_THROW(
      sut_.parse(R"({"$match":{"$params":{}, "$against":{"$expr":"c1"}}})"),
      mrs::interface::RestError);
  ASSERT_THROW(
      sut_.parse(R"({"$match":{"$params":false, "$against":{"$expr":"c1"}}})"),
      mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, match_expression_without_modifiers) {
  sut_.parse(R"({"$match":{"$params":["c1"], "$against":{"$expr":"q1"}}})");
  EXPECT_EQ("(MATCH (`c1`) AGAINST('q1' ) )", sut_.get_result().str());

  sut_.parse(
      R"({"$match":{"$params":["c1", "c2"], "$against":{"$expr":"q1!"}}})");
  EXPECT_EQ("(MATCH (`c1`,`c2`) AGAINST('q1!' ) )", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, match_expression_invalid_modifier) {
  ASSERT_THROW(
      sut_.parse(R"({"$match":{"$params":["c1"], "$against":{"$expr":"q1",
                      "$modifier":""}}})"),
      mrs::interface::RestError);

  ASSERT_THROW(sut_.parse(
                   R"({"$match":{"$params":["c1"],
                  "$against":{"$expr":"q1", "$modifier":"SOME TEXT"}}})"),
               mrs::interface::RestError);

  ASSERT_THROW(sut_.parse(
                   R"({"$match":{"$params":["c1"], "$against":{"$expr":"q1",
                   "$modifier":false}}})"),
               mrs::interface::RestError);

  ASSERT_THROW(
      sut_.parse(R"({"$match":{"$params":["c1"], "$against":{"$expr":"q1",
                      "$modifier":10}}})"),
      mrs::interface::RestError);
}

TEST_F(FilterObjectsTest, match_expression_with_modifier) {
  sut_.parse(R"({"$match":{"$params":["c1"],  "$against":{"$expr":"q1",
                  "$modifier":"WITH QUERY EXPANSION"}}})");
  EXPECT_EQ("(MATCH (`c1`) AGAINST('q1' WITH QUERY EXPANSION) )",
            sut_.get_result().str());

  sut_.parse(R"({"$match":{"$params":["c1", "c2"],
                    "$against":{"$expr":"q1", "$modifier":"IN BOOLEAN MODE"}}})");
  EXPECT_EQ("(MATCH (`c1`,`c2`) AGAINST('q1' IN BOOLEAN MODE) )",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_and_two_columns) {
  sut_.parse(
      R"({"$and": [{"SALARY":{"$gt": 1000}}, {"ENAME":{"$like":"!S?%"}}]})");
  ASSERT_EQ("((`SALARY` > 1000) AND (`ENAME` like '!S?%'))",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_and_column_name_outside) {
  sut_.parse(R"({"SALARY": {"$and": [{"$gt": 1000}, {"$lt":4000}]}})");
  ASSERT_EQ("((`SALARY` > 1000) AND (`SALARY` < 4000))",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_or_column_name_outside) {
  sut_.parse(R"({"SALARY": {"$or": [{"$gt": 1000}, {"$lt":4000}]}})");
  ASSERT_EQ("((`SALARY` > 1000) OR (`SALARY` < 4000))",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_implicit_and_one_elem) {
  sut_.parse(R"({"SALARY": [{"$gt": 1000}]})");
  ASSERT_EQ("((`SALARY` > 1000))", sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_implicit_and_two_elem) {
  sut_.parse(R"({"SALARY": [{"$gt": 1000}, {"$lt":4000}]})");
  ASSERT_EQ("((`SALARY` > 1000) AND (`SALARY` < 4000))",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_implicit_and_elem_different_column_integer) {
  sut_.parse(R"({"SALARY": [{"$gt": 1000}, {"$lt":4000}, {"AGE": 20}]})");
  ASSERT_EQ("((`SALARY` > 1000) AND (`SALARY` < 4000) AND (`AGE`=20))",
            sut_.get_result().str());
}

TEST_F(FilterObjectsTest,
       complex_implicit_and_elem_different_column_simple_operator) {
  sut_.parse(
      R"({"SALARY": [{"$gt": 1000}, {"$lt":4000}, {"AGE": {"$gt": 20}}]})");
  ASSERT_EQ("((`SALARY` > 1000) AND (`SALARY` < 4000) AND (`AGE` > 20))",
            sut_.get_result().str());
}

TEST_F(
    FilterObjectsTest,
    complex_implicit_and_elem_different_column_complex_operator_implicit_and) {
  sut_.parse(
      R"(
        {"SALARY": [
                    {"$gt": 1000},
                    {"$lt": 4000},
                    {
                      "AGE": {"$and": [{"$gt": 20}, {"$lt": 40}]}
                    }
                   ]
          })");

  ASSERT_EQ(
      "((`SALARY` > 1000) AND (`SALARY` < 4000) AND ((`AGE` > 20) AND (`AGE` < "
      "40)))",
      sut_.get_result().str());
}

TEST_F(
    FilterObjectsTest,
    complex_implicit_and_elem_different_column_complex_operator_explicit_or) {
  sut_.parse(
      R"(
        {"SALARY": [
                    {"$gt": 1000},
                    {"$lt": 4000},
                    {
                      "AGE": {"$or": [{"$gt": 20}, {"$lt": 40}]}
                    }
                   ]
          })");
  ASSERT_EQ(
      "((`SALARY` > 1000) AND (`SALARY` < 4000) AND ((`AGE` > 20) OR (`AGE` < "
      "40)))",
      sut_.get_result().str());
}

TEST_F(FilterObjectsTest, complex_several_levels) {
  sut_.parse(
      R"(
        {"SALARY": [
                    {"$gt": 1000},
                    {"$lt": 4000},
                    {
                      "AGE": {
                        "$and": [
                            {"$gt": 20},
                            {"$lt": 40},
                            {"$eq": 500},
                            {"$or": [{"$lt": 200}, {"$gt": 100}]}
                         ]
                      }
                    }
                   ]
          })");
  ASSERT_EQ(
      "((`SALARY` > 1000) AND (`SALARY` < 4000) AND ((`AGE` > 20) AND (`AGE` < "
      "40) AND (`AGE` = 500) AND ((`AGE` < 200) OR (`AGE` > 100))))",
      sut_.get_result().str());
}

// This looks weird but it's allowed by the grammar
TEST_F(FilterObjectsTest, complex_or_simple_operator) {
  sut_.parse(R"({"SALARY": {"$or": {"$lt": 1000}}})");
  ASSERT_EQ("(`SALARY` < 1000)", sut_.get_result().str());
}
