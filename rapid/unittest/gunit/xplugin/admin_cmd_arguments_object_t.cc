/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "admin_cmd_handler.h"
#include "mysqlx_pb_wrapper.h"
#include "xpl_error.h"
#include <gtest/gtest.h>


namespace xpl
{
namespace test
{


class Admin_command_arguments_object_test : public ::testing::Test
{
public:
  enum {OPTIONAL_NO = 0, OPTIONAL_YES = 1};

  Admin_command_arguments_object_test()
  : extractor(new Admin_command_arguments_object(args))
  {}

  void set_arguments(const Any::Object::Scalar_fields &values)
  {
    args.AddAllocated(new Any(Any::Object(values)));
    extractor.reset(new Admin_command_arguments_object(args));
  }

  void set_arguments(const Any::Object::Fields &values)
  {
    args.AddAllocated(new Any(Any::Object(values)));
    extractor.reset(new Admin_command_arguments_object(args));
  }

  void set_arguments(const Scalar &value)
  {
    args.AddAllocated(new Any(value));
    extractor.reset(new Admin_command_arguments_object(args));
  }

  Admin_command_arguments_object::List args;
  ngs::unique_ptr<Admin_command_arguments_object> extractor;
};


namespace
{

::testing::AssertionResult Assert_error_code(const char* e1_expr, const char* e2_expr,
                                             int e1, const ngs::Error_code &e2)
{
  return (e1 == e2.error)
      ? ::testing::AssertionSuccess()
      : (::testing::AssertionFailure()
         << "Value of: " << e2_expr
         << "\nActual: {" << e2.error << ", " << e2.message << "}\n"
         << "Expected: " << e1_expr
         << "\nWhich is:" << e1);
}


#define ASSERT_ERROR_CODE(a,b) ASSERT_PRED_FORMAT2(Assert_error_code, a, b);
static const int ER_X_SUCCESS = 0;

template <typename T>
struct vector_of : public std::vector<T>
{
  vector_of(const T& e) { (*this)(e); }
  vector_of& operator() (const T& e) { this->push_back(e); return *this; }
};

} // namespace


TEST_F(Admin_command_arguments_object_test, is_end_empty_args)
{
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, is_end_empty_obj)
{
  set_arguments(Any::Object::Scalar_fields());
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, is_end_one_val)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(42)));
  ASSERT_FALSE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, end_empty_args)
{
  ASSERT_ERROR_CODE(ER_X_SUCCESS, extractor->end());
}


TEST_F(Admin_command_arguments_object_test, end_no_obj)
{
  set_arguments(Scalar(42));
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE, extractor->end());
}


TEST_F(Admin_command_arguments_object_test, end_empty_obj)
{
  set_arguments(Any::Object::Scalar_fields());
  ASSERT_ERROR_CODE(ER_X_SUCCESS, extractor->end());
}


TEST_F(Admin_command_arguments_object_test, string_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_arg_no_obj)
{
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_NUM_ARGUMENTS,
                    extractor->string_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_arg_empty_arg)
{
  set_arguments(Any::Object::Scalar_fields());
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_NUM_ARGUMENTS,
                    extractor->string_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_arg_no_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_NUM_ARGUMENTS,
                    extractor->string_arg("second", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_arg_twice)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny"))
                                ("second", Scalar::String("carrot")));
  std::string value1("none"), value2("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("second", value1, OPTIONAL_NO)
                    .string_arg("first", value2, OPTIONAL_NO).end());
  ASSERT_EQ("carrot", value1);
  ASSERT_EQ("bunny", value2);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_arg_twice_no_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny")));
  std::string value1("none"), value2("none");
  ASSERT_ERROR_CODE(ER_X_CMD_NUM_ARGUMENTS,
                    extractor->string_arg("first", value1, OPTIONAL_NO)
                    .string_arg("second", value2, OPTIONAL_NO).end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ("none", value2);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_arg_diff_type)
{
  set_arguments(Any::Object::Scalar_fields("first", Scalar(42)));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE,
                    extractor->string_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, sint_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(42)));
  int64_t value = -666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->sint_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ(42, value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, sint_arg_bad_val)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("42!")));
  int64_t value = -666;
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE,
                    extractor->sint_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ(-666, value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, sint_arg_negative)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(-42)));
  int64_t value = -666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->sint_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ(-42, value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, uint_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(42u)));
  uint64_t value = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->uint_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ(42, value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, uint_arg_negative)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(-42)));
  uint64_t value = 666;
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE,
                    extractor->uint_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ(666, value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, bool_arg_true)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(true)));
  bool value = false;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->bool_arg("first", value, OPTIONAL_NO).end());
  ASSERT_TRUE(value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, bool_arg_false)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar(false)));
  bool value = true;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->bool_arg("first", value, OPTIONAL_NO).end());
  ASSERT_FALSE(value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, docpath_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("$.path.to.member")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->docpath_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("$.path.to.member", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, docpath_arg_no_dollar)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String(".path.to.member")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_VALUE,
                    extractor->docpath_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, docpath_arg_bad_arg)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("is.not.path")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_VALUE,
                    extractor->docpath_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, docpath_arg_bad_arg_space)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("$.is.not.pa th")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_VALUE,
                    extractor->docpath_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, docpath_arg_bad_arg_tab)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("$.is.not.pa\th")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_VALUE,
                    extractor->docpath_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, optional)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny")));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("first", value, OPTIONAL_YES).end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, optional_empty_args)
{
  set_arguments(Any::Object::Scalar_fields());
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("first", value, OPTIONAL_YES).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, optional_no_obj)
{
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("first", value, OPTIONAL_YES).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, optional_second)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny")));
  std::string value1("none");
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("first", value1, OPTIONAL_NO)
                    .uint_arg("second", value2, OPTIONAL_YES).end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ(666, value2);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, optional_inside)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny"))
                                ("third", Scalar(42u)));
  std::string value1("none"), value2("none");
  uint64_t value3 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_arg("first", value1, OPTIONAL_NO)
                    .string_arg("second", value2, OPTIONAL_YES)
                    .uint_arg("third", value3, OPTIONAL_NO).end());
  ASSERT_EQ("bunny", value1);
  ASSERT_EQ("none", value2);
  ASSERT_EQ(42, value3);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, end_to_many_args)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny"))
                                ("third", Scalar(42u)));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_NUM_ARGUMENTS,
                    extractor->string_arg("first", value, OPTIONAL_NO).end());
  ASSERT_EQ("bunny", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, end_to_many_args_optional)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny"))
                                ("third", Scalar(42u)));
  std::string value("none");
  ASSERT_ERROR_CODE(ER_X_CMD_NUM_ARGUMENTS,
                    extractor->string_arg("second", value, OPTIONAL_YES).end());
  ASSERT_EQ("none", value);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_list_one_value)
{
  set_arguments(
      Any::Object::Scalar_fields("first", Scalar::String("bunny")));
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_list("first", values, OPTIONAL_NO).end());
  ASSERT_EQ(vector_of<std::string>("bunny"), values);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_list_array_one)
{
  set_arguments(
      Any::Object::Fields("first", Any::Array(Scalar::String("bunny"))));
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_list("first", values, OPTIONAL_NO).end());
  ASSERT_EQ(vector_of<std::string>("bunny"), values);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_list_array)
{
  set_arguments(
      Any::Object::Fields("first", Any::Array(Scalar::String("bunny"))
                                             (Scalar::String("carrot"))));
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_list("first", values, OPTIONAL_NO).end());
  ASSERT_EQ(vector_of<std::string>("bunny")("carrot"), values);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_list_array_mix)
{
  set_arguments(
      Any::Object::Fields("first", Any::Array(Scalar::String("bunny"))
                                             (Scalar::String("carrot")))
                         ("second", Scalar(42u)));
  std::vector<std::string> values1;
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_list("first", values1, OPTIONAL_NO)
                        .uint_arg("second", value2, OPTIONAL_NO).end());
  ASSERT_EQ(vector_of<std::string>("bunny")("carrot"), values1);
  ASSERT_EQ(42u, value2);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_list_empty)
{
  set_arguments(
      Any::Object::Fields("first", Any::Array()));
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->string_list("first", values, OPTIONAL_NO).end());
  ASSERT_EQ(std::vector<std::string>(), values);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, string_list_bad_arg)
{
  set_arguments(
      Any::Object::Fields("first",
                          Any::Array(Scalar::String("bunny"))
                                    (Scalar(42u))));
  std::vector<std::string> values;
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE,
                    extractor->string_list("first", values, OPTIONAL_NO).end());
  ASSERT_EQ(std::vector<std::string>(), values);
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, object_list_one_value)
{
  set_arguments(
    Any::Object::Fields("first",
                        Any::Object(Any::Object::Fields("second", Scalar(42u)))));

  std::vector<Admin_command_arguments_object::Command_arguments*> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->object_list("first", values, OPTIONAL_NO, 0).end());
  ASSERT_EQ(1u, values.size());
  ASSERT_TRUE(extractor->is_end());
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values[0]->uint_arg("second", value2, OPTIONAL_NO).end());
  ASSERT_EQ(42u, value2);
  ASSERT_TRUE(values[0]->is_end());
}


TEST_F(Admin_command_arguments_object_test, object_list_array_one)
{
  set_arguments(
    Any::Object::Fields("first",
                        Any::Array(Any::Object(Any::Object::Fields("second",
                                                                   Scalar(42u))))));

  std::vector<Admin_command_arguments_object::Command_arguments*> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->object_list("first", values, OPTIONAL_NO, 0).end());
  ASSERT_EQ(1u, values.size());
  ASSERT_TRUE(extractor->is_end());
  uint64_t value2 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values[0]->uint_arg("second", value2, OPTIONAL_NO).end());
  ASSERT_EQ(42u, value2);
  ASSERT_TRUE(values[0]->is_end());
}


TEST_F(Admin_command_arguments_object_test, object_list_array)
{
  set_arguments(
    Any::Object::Fields("first",
                        Any::Array(Any::Object(Any::Object::Fields("second", Scalar(42u))))
                                  (Any::Object(Any::Object::Fields("third", Scalar(-44))))));

  std::vector<Admin_command_arguments_object::Command_arguments*> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->object_list("first", values, OPTIONAL_NO, 0).end());
  ASSERT_EQ(2u, values.size());
  ASSERT_TRUE(extractor->is_end());
  uint64_t value1 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values[0]->uint_arg("second", value1, OPTIONAL_NO).end());
  ASSERT_EQ(42u, value1);
  ASSERT_TRUE(values[0]->is_end());
  int64_t value2 = 666;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    values[1]->sint_arg("third", value2, OPTIONAL_NO).end());
  ASSERT_EQ(-44, value2);
  ASSERT_TRUE(values[1]->is_end());
}


TEST_F(Admin_command_arguments_object_test, object_list_empty)
{
  set_arguments(
    Any::Object::Fields("first", Any::Array()));

  std::vector<Admin_command_arguments_object::Command_arguments*> values;
  ASSERT_ERROR_CODE(ER_X_SUCCESS,
                    extractor->object_list("first", values, OPTIONAL_NO, 0).end());
  ASSERT_EQ(0u, values.size());
  ASSERT_TRUE(extractor->is_end());
}


TEST_F(Admin_command_arguments_object_test, object_list_array_bad_arg)
{
  set_arguments(
    Any::Object::Fields("first",
                        Any::Array(Any::Object(Any::Object::Fields("second", Scalar(42u))))
                                  (Scalar::String("bunny"))));

  std::vector<Admin_command_arguments_object::Command_arguments*> values;
  ASSERT_ERROR_CODE(ER_X_CMD_ARGUMENT_TYPE,
                    extractor->object_list("first", values, OPTIONAL_NO, 0).end());
  ASSERT_EQ(0u, values.size());
  ASSERT_TRUE(extractor->is_end());
}

} // namespace test
} // namespace xpl
