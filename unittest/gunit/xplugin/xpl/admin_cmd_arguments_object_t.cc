/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <gtest/gtest.h>

#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/xpl_error.h"
#include "unittest/gunit/xplugin/xpl/assert_error_code.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

class Admin_command_arguments_object_test : public ::testing::Test {
 public:
  using Argument_appearance =
      Admin_command_handler::Command_arguments::Appearance_type;

  Admin_command_arguments_object_test()
      : extractor(new Admin_command_arguments_object(args)) {}

  void set_arguments(const Any &value) {
    args.Add()->CopyFrom(value);
    extractor.reset(new Admin_command_arguments_object(args));
  }

  Admin_command_arguments_object::List args;
  std::unique_ptr<Admin_command_arguments_object> extractor;
};

TEST_F(Admin_command_arguments_object_test, is_end_empty_args) {
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, is_end_empty_obj) {
  set_arguments(Any::Object{});
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, is_end_one_val) {
  set_arguments(Any::Object{{"first", Scalar(42)}});
  ASSERT_FALSE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, end_empty_args) {
  ASSERT_ERROR_CODE(ER_X_SUCCESS, extractor->end());
}

TEST_F(Admin_command_arguments_object_test, end_no_obj) {
  set_arguments(Scalar(42));
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE, extractor->end());
}

TEST_F(Admin_command_arguments_object_test, end_empty_obj) {
  set_arguments(Any::Object{});
  ASSERT_ERROR_CODE(ER_X_SUCCESS, extractor->end());
}

TEST_F(Admin_command_arguments_object_test, string_arg) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_no_obj) {
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_NUM_ARGUMENTS,
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_empty_arg) {
  set_arguments(Any::Object{});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_NUM_ARGUMENTS,
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_no_arg) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_NUM_ARGUMENTS,
      extractor
          ->string_arg({"second"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_twice) {
  set_arguments(Any::Object{{"first", "bunny"}, {"second", "carrot"}});
  std::string value1("none"), value2("none");
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_arg({"second"}, &value1, Argument_appearance::k_obligatory)
          .string_arg({"first"}, &value2, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("carrot", value1);
  ASSERT_EQ("bunny", value2);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_twice_no_arg) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value1("none"), value2("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_NUM_ARGUMENTS,
      extractor
          ->string_arg({"first"}, &value1, Argument_appearance::k_obligatory)
          .string_arg({"second"}, &value2, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ("none", value2);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_diff_type) {
  set_arguments(Any::Object{{"first", 42}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_ARGUMENT_VALUE,
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_arg_second_name) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->string_arg({"second", "first"}, &value,
                                     Argument_appearance::k_obligatory)
                        .end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, sint_arg) {
  set_arguments(Any::Object{{"first", 42}});
  int64_t value = -666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->sint_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(42, value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, sint_arg_bad_val) {
  set_arguments(Any::Object{{"first", "42!"}});
  int64_t value = -666;
  ASSERT_ERROR_CODE(
      ER_X_CMD_ARGUMENT_VALUE,
      extractor->sint_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(-666, value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, sint_arg_negative) {
  set_arguments(Any::Object{{"first", -42}});
  int64_t value = -666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->sint_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(-42, value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, uint_arg) {
  set_arguments(Any::Object{{"first", 42u}});
  uint64_t value = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->uint_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(42, value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, uint_arg_negative) {
  set_arguments(Any::Object{{"first", -42}});
  uint64_t value = 666;
  ASSERT_ERROR_CODE(
      ER_X_CMD_ARGUMENT_VALUE,
      extractor->uint_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(666, value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, bool_arg_true) {
  set_arguments(Any::Object{{"first", true}});
  bool value = false;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->bool_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_TRUE(value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, bool_arg_false) {
  set_arguments(Any::Object{{"first", false}});
  bool value = true;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->bool_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_FALSE(value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->string_arg({"first"}, &value, Argument_appearance::k_optional)
          .end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional_second_name) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->string_arg({"second", "first"}, &value,
                                     Argument_appearance::k_optional)
                        .end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional_empty_args) {
  set_arguments(Any::Object{});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->string_arg({"first"}, &value, Argument_appearance::k_optional)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional_no_obj) {
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor->string_arg({"first"}, &value, Argument_appearance::k_optional)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional_second) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value1("none");
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_arg({"first"}, &value1, Argument_appearance::k_obligatory)
          .uint_arg({"second"}, &value2, Argument_appearance::k_optional)
          .end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ(666, value2);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional_inside) {
  set_arguments(Any::Object{{"first", "bunny"}, {"third", 42u}});
  std::string value1("none"), value2("none");
  uint64_t value3 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_arg({"first"}, &value1, Argument_appearance::k_obligatory)
          .string_arg({"second"}, &value2, Argument_appearance::k_optional)
          .uint_arg({"third"}, &value3, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ("none", value2);
  ASSERT_EQ(42, value3);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, optional_inside_second_name) {
  set_arguments(Any::Object{{"first", "bunny"}, {"third", 42u}});
  std::string value1("none"), value2("none");
  uint64_t value3 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_arg({"first"}, &value1, Argument_appearance::k_obligatory)
          .string_arg({"fourth", "second"}, &value2,
                      Argument_appearance::k_optional)
          .uint_arg({"third"}, &value3, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ("none", value2);
  ASSERT_EQ(42, value3);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, end_to_many_args) {
  set_arguments(Any::Object{{"first", "bunny"}, {"third", 42u}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_NUM_ARGUMENTS,
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, end_to_many_args_optional) {
  set_arguments(Any::Object{{"first", "bunny"}, {"third", 42u}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      ER_X_CMD_NUM_ARGUMENTS,
      extractor->string_arg({"second"}, &value, Argument_appearance::k_optional)
          .end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_list_one_value) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(std::vector<std::string>{"bunny"}, values);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_list_array_one) {
  set_arguments(Any::Object{{"first", Any::Array{"bunny"}}});
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(std::vector<std::string>{"bunny"}, values);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_list_array) {
  set_arguments(Any::Object{{"first", Any::Array{"bunny", "carrot"}}});
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  std::vector<std::string> expect{"bunny", "carrot"};
  ASSERT_EQ(expect, values);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_list_array_mix) {
  set_arguments(
      Any::Object{{"first", Any::Array{"bunny", "carrot"}}, {"second", 42u}});
  std::vector<std::string> values1;
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      extractor
          ->string_list({"first"}, &values1, Argument_appearance::k_obligatory)
          .uint_arg({"second"}, &value2, Argument_appearance::k_obligatory)
          .end());
  std::vector<std::string> expect{"bunny", "carrot"};
  ASSERT_EQ(expect, values1);
  ASSERT_EQ(42u, value2);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_list_empty) {
  set_arguments(Any::Object{{"first", Any::Array{}}});

  std::vector<std::string> values;
  ASSERT_ERROR_CODE(
      ER_X_CMD_ARGUMENT_VALUE,
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(std::vector<std::string>(), values);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, string_list_bad_arg) {
  set_arguments(Any::Object{{"first", Any::Array{"bunny", 42u}}});

  std::vector<std::string> values;
  ASSERT_ERROR_CODE(
      ER_X_CMD_ARGUMENT_VALUE,
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(std::vector<std::string>(), values);
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, object_list_one_value) {
  set_arguments(Any::Object{{"first", Any::Object{{"second", 42u}}}});

  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_EQ(1u, values.size());
  ASSERT_TRUE(extractor->is_end());
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      values[0]
          ->uint_arg({"second"}, &value2, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(42u, value2);
  ASSERT_TRUE(values[0]->is_end());
}

TEST_F(Admin_command_arguments_object_test, object_list_array_one) {
  set_arguments(
      Any::Object{{"first", Any::Array{Any::Object{{"second", 42u}}}}});

  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_EQ(1u, values.size());
  ASSERT_TRUE(extractor->is_end());
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      values[0]
          ->uint_arg({"second"}, &value2, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(42u, value2);
  ASSERT_TRUE(values[0]->is_end());
}

TEST_F(Admin_command_arguments_object_test, object_list_array) {
  set_arguments(
      Any::Object{{"first", Any::Array{Any::Object{{"second", 42u}},
                                       Any::Object{{"third", -44}}}}});

  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_EQ(2u, values.size());
  ASSERT_TRUE(extractor->is_end());
  uint64_t value1 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      values[0]
          ->uint_arg({"second"}, &value1, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(42u, value1);
  ASSERT_TRUE(values[0]->is_end());
  int64_t value2 = 666;
  ASSERT_ERROR_CODE(
      ER_X_SUCCESS,
      values[1]
          ->sint_arg({"third"}, &value2, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(-44, value2);
  ASSERT_TRUE(values[1]->is_end());
}

TEST_F(Admin_command_arguments_object_test, object_list_empty) {
  set_arguments(Any::Object{{"first", Any::Array{}}});

  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_VALUE,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_EQ(0u, values.size());
  ASSERT_TRUE(extractor->is_end());
}

TEST_F(Admin_command_arguments_object_test, object_list_array_bad_arg) {
  set_arguments(Any::Object{
      {"first", Any::Array{Any::Object{{"second", 42u}}, "bunny"}}});

  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_VALUE,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_EQ(0u, values.size());
  ASSERT_TRUE(extractor->is_end());
}

struct Param_docpath_arg {
  int expect_error;
  std::string path;
};

class Admin_command_arguments_docpath_test
    : public Admin_command_arguments_object_test,
      public testing::WithParamInterface<Param_docpath_arg> {};

TEST_P(Admin_command_arguments_docpath_test, docpath_arg) {
  const Param_docpath_arg &param = GetParam();
  set_arguments(Any::Object{{"first", param.path.c_str()}});
  std::string value("none");
  ASSERT_ERROR_CODE(
      param.expect_error,
      extractor
          ->docpath_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ(param.expect_error == ER_X_SUCCESS ? param.path : "none", value);
  ASSERT_TRUE(extractor->is_end());
}

Param_docpath_arg docpath_arg_param[] = {
    {ER_X_SUCCESS, "$"},
    {ER_X_SUCCESS, "$.path"},
    {ER_X_SUCCESS, "$.path.to.member"},
    {ER_X_CMD_ARGUMENT_VALUE, "$."},
    {ER_X_CMD_ARGUMENT_VALUE, ".path"},
    {ER_X_CMD_ARGUMENT_VALUE, "path"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.1"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.1path"},
    {ER_X_SUCCESS, "$.p1ath"},
    {ER_X_SUCCESS, "$.path1"},
    {ER_X_SUCCESS, "$.$"},
    {ER_X_SUCCESS, "$.$$"},
    {ER_X_SUCCESS, "$.$$$"},
    {ER_X_SUCCESS, "$.$.path"},
    {ER_X_SUCCESS, "$.path.$"},
    {ER_X_SUCCESS, "$.$path"},
    {ER_X_SUCCESS, "$.pa$th"},
    {ER_X_SUCCESS, "$.path$"},
    {ER_X_SUCCESS, "$.$pa$th$"},
    {ER_X_SUCCESS, "$._"},
    {ER_X_SUCCESS, "$.__"},
    {ER_X_SUCCESS, "$.___"},
    {ER_X_SUCCESS, "$._.path"},
    {ER_X_SUCCESS, "$.path._"},
    {ER_X_SUCCESS, "$._path"},
    {ER_X_SUCCESS, "$.pa_th"},
    {ER_X_SUCCESS, "$.path_"},
    {ER_X_SUCCESS, "$._pa_th_"},
    {ER_X_SUCCESS, "$.*"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.**"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.***"},
    {ER_X_SUCCESS, "$.*.path"},
    {ER_X_SUCCESS, "$.path.*"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.*path"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.pa*th"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path*"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.*pa*th*"},
    {ER_X_SUCCESS, "$.path[1]"},
    {ER_X_SUCCESS, "$.path[123]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[-1]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[a]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path["},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.[path]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.[1]"},
    {ER_X_SUCCESS, "$.path[1].path[2]"},
    {ER_X_SUCCESS, "$.path[1].path"},
    {ER_X_SUCCESS, "$.path[1].*"},
    {ER_X_SUCCESS, "$.*.path[1]"},
    {ER_X_SUCCESS, "$.path[*]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[**]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[*1]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[1*]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path[1*1]"},
    {ER_X_SUCCESS, "$[1]"},
    {ER_X_SUCCESS, "$[1][2]"},
    {ER_X_SUCCESS, "$[1].path[2]"},
    {ER_X_SUCCESS, "$[1][2].path"},
    {ER_X_SUCCESS, "$.path[1][2]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.pa th"},
    {ER_X_SUCCESS, "$.\"pa th\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.pa\th"},
    {ER_X_SUCCESS, "$.\"pa\tth\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.\""},
    {ER_X_SUCCESS, "$.\"\"\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.\"path"},
    {ER_X_SUCCESS, "$.\"\"path\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path\""},
    {ER_X_SUCCESS, "$.\"path\"\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.#"},
    {ER_X_SUCCESS, "$.\"#\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path#"},
    {ER_X_SUCCESS, "$.\"path#\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$.#path"},
    {ER_X_SUCCESS, "$.\"#path\""},
    {ER_X_SUCCESS, "$.\"#\"[1]"},
    {ER_X_SUCCESS, "$.\"\""},
    {ER_X_SUCCESS, "$.część"},
    {ER_X_SUCCESS, "$.łódź"},
    {ER_X_SUCCESS, "$**.path"},
    {ER_X_SUCCESS, "$**[1]"},
    {ER_X_SUCCESS, "$.path**.path"},
    {ER_X_SUCCESS, "$.path**[1]"},
    {ER_X_SUCCESS, "$[1]**.path"},
    {ER_X_SUCCESS, "$[1]**[1]"},
    {ER_X_CMD_ARGUMENT_VALUE, "$**"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path**"},
    {ER_X_CMD_ARGUMENT_VALUE, "$[1]**"},
    {ER_X_CMD_ARGUMENT_VALUE, "$***"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.path***"},
    {ER_X_CMD_ARGUMENT_VALUE, "$[1]***"},
    {ER_X_CMD_ARGUMENT_VALUE, "$.**.path"},
    {ER_X_SUCCESS, "$.***.path"},
    {ER_X_SUCCESS, "$.\"**\""},
    {ER_X_SUCCESS, "$.\"***\""},
    {ER_X_SUCCESS, "$.\"pa.th\""},
    {ER_X_CMD_ARGUMENT_VALUE, "$*"},
};

INSTANTIATE_TEST_CASE_P(docpath_arg, Admin_command_arguments_docpath_test,
                        testing::ValuesIn(docpath_arg_param));

#define INVALID_VALUE_ERROR(arg_name)      \
  ngs::Error_code(ER_X_CMD_ARGUMENT_VALUE, \
                  "Invalid value for argument '" arg_name "'")
#define INVALID_NUMBER_ERROR(arg_name)            \
  ngs::Error_code(ER_X_CMD_NUM_ARGUMENTS,         \
                  "Invalid number of arguments, " \
                  "expected value for '" arg_name "'")

TEST_F(Admin_command_arguments_object_test, string_arg_error_msg_invalid_type) {
  set_arguments(Any::Object{{"first", 42}});
  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first"),
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       string_arg_error_msg_invalid_number) {
  set_arguments(Any::Object{{"second", "bunny"}});
  std::string value("none");
  ASSERT_ERROR(
      INVALID_NUMBER_ERROR("first"),
      extractor
          ->string_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       docpath_arg_error_msg_invalid_value) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first"),
      extractor
          ->docpath_arg({"first"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       string_list_error_msg_invalid_type_object) {
  set_arguments(Any::Object{{"first", Any::Object{{"bunny", "carrot"}}}});
  std::vector<std::string> values;
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first"),
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_TRUE(values.empty());
}

TEST_F(Admin_command_arguments_object_test,
       string_list_error_msg_invalid_type_number) {
  set_arguments(Any::Object{{"first", Any::Array{42u}}});
  std::vector<std::string> values;
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first[0]"),
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_TRUE(values.empty());
}

TEST_F(Admin_command_arguments_object_test,
       string_list_error_msg_invalid_type_number_2nd) {
  set_arguments(Any::Object{{"first", Any::Array{"bunny", 42u}}});
  std::vector<std::string> values;
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first[1]"),
      extractor
          ->string_list({"first"}, &values, Argument_appearance::k_obligatory)
          .end());
  ASSERT_TRUE(values.empty());
}

TEST_F(Admin_command_arguments_object_test,
       object_arg_error_msg_invalid_value) {
  set_arguments(Any::Object{{"first", "bunny"}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR(INVALID_VALUE_ERROR("first"),
               extractor
                   ->object_list({"first"}, &values,
                                 Argument_appearance::k_obligatory, 0)
                   .end());
  ASSERT_TRUE(values.empty());
}

TEST_F(Admin_command_arguments_object_test,
       object_arg_string_arg_error_msg_invalid_value) {
  set_arguments(Any::Object{{"first", Any::Object{{"second", 42u}}}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values.empty());

  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first.second"),
      values[0]
          ->string_arg({"second"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       object_arg_object_arg_string_arg_error_msg_invalid_value) {
  set_arguments(Any::Object{
      {"first", Any::Object{{"second", Any::Object{{"third", 42u}}}}}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values1;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values1,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values1.empty());

  std::vector<Admin_command_arguments_object::Command_arguments *> values2;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values1[0]
                        ->object_list({"second"}, &values2,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values2.empty());

  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first.second.third"),
      values2[0]
          ->string_arg({"third"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       object_arg_string_arg_error_msg_invalid_number) {
  set_arguments(Any::Object{{"first", Any::Object{{"second", "bunny"}}}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values.empty());

  std::string value("none");
  ASSERT_ERROR(
      INVALID_NUMBER_ERROR("first.third"),
      values[0]
          ->string_arg({"third"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       object_list_string_arg_error_msg_invalid_value) {
  set_arguments(
      Any::Object{{"first", Any::Array{Any::Object{{"second", 42u}}}}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values.empty());

  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first[0].second"),
      values[0]
          ->string_arg({"second"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       object_list_object_arg_string_arg_error_msg_invalid_value) {
  set_arguments(Any::Object{
      {"first",
       Any::Array{Any::Object{{"second", Any::Object{{"third", 42u}}}}}}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values1;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values1,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values1.empty());

  std::vector<Admin_command_arguments_object::Command_arguments *> values2;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values1[0]
                        ->object_list({"second"}, &values2,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values2.empty());

  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first[0].second.third"),
      values2[0]
          ->string_arg({"third"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

TEST_F(Admin_command_arguments_object_test,
       object_list_object_list_string_arg_error_msg_invalid_value) {
  set_arguments(Any::Object{
      {"first", Any::Array{Any::Object{
                    {"second", Any::Array{Any::Object{{"third", 42u}}}}}}}});
  std::vector<Admin_command_arguments_object::Command_arguments *> values1;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor
                        ->object_list({"first"}, &values1,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values1.empty());

  std::vector<Admin_command_arguments_object::Command_arguments *> values2;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values1[0]
                        ->object_list({"second"}, &values2,
                                      Argument_appearance::k_obligatory, 0)
                        .end());
  ASSERT_FALSE(values2.empty());

  std::string value("none");
  ASSERT_ERROR(
      INVALID_VALUE_ERROR("first[0].second[0].third"),
      values2[0]
          ->string_arg({"third"}, &value, Argument_appearance::k_obligatory)
          .end());
  ASSERT_EQ("none", value);
}

}  // namespace test
}  // namespace xpl
