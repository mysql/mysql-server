/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <cstddef>

#include "plugin/x/src/expr_generator.h"
#include "unittest/gunit/xplugin/xpl/message_helpers.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

const char *const EMPTY_SCHEMA = "";
enum { DM_DOCUMENT = 0, DM_TABLE = 1 };

using Octets = Scalar::Octets;

struct Param_function_call {
  std::string expect;
  Function_call func;
  std::string schema;
};

class Function_call_test : public testing::TestWithParam<Param_function_call> {
};

TEST_P(Function_call_test, function_call) {
  const Param_function_call &param = GetParam();
  EXPECT_STREQ(param.expect.c_str(),
               generate_expression(param.func, param.schema, DM_TABLE).c_str());
}

Param_function_call function_call_param[] = {
    {"func()", Function_call("func"), EMPTY_SCHEMA},
    {"schema.func()", Function_call("func"), "schema"},
    {"schema.func(FALSE,5)", Function_call("func", false, 5), "schema"},
    {"concat(FALSE,5)", Function_call("concat", false, 5), "schema"},
    {"CONCAT(FALSE,5)", Function_call("CONCAT", false, 5), "schema"},
    {"CONCAT(FALSE,5)", Function_call("CONCAT", false, 5), EMPTY_SCHEMA},
    {"ASCII('string')", Function_call("ASCII", "string"), EMPTY_SCHEMA},
    {"ASCII(`column`)", Function_call("ASCII", Column_identifier("column")),
     EMPTY_SCHEMA},
    {"ASCII(JSON_UNQUOTE(JSON_EXTRACT(doc,'$.path')))",
     Function_call("ASCII", Column_identifier(Document_path{"path"})),
     EMPTY_SCHEMA},
    {"ABS(42)", Function_call("ABS", 42), EMPTY_SCHEMA},
    {"ABS(`column`)", Function_call("ABS", Column_identifier("column")),
     EMPTY_SCHEMA},
    {"ABS(JSON_UNQUOTE(JSON_EXTRACT(doc,'$.path')))",
     Function_call("ABS", Column_identifier(Document_path{"path"})),
     EMPTY_SCHEMA},
    {"JSON_TYPE(42)", Function_call("JSON_TYPE", 42), EMPTY_SCHEMA},
    {"JSON_TYPE(`column`)",
     Function_call("JSON_TYPE", Column_identifier("column")), EMPTY_SCHEMA},
    {"JSON_TYPE(JSON_EXTRACT(doc,'$.path'))",
     Function_call("JSON_TYPE", Column_identifier(Document_path{"path"})),
     EMPTY_SCHEMA},
    {"JSON_KEYS('{\\\"a\\\":42}')", Function_call("JSON_KEYS", "{\"a\":42}"),
     EMPTY_SCHEMA},
    {"JSON_KEYS(`column`)",
     Function_call("JSON_KEYS", Column_identifier("column")), EMPTY_SCHEMA},
    {"JSON_KEYS(JSON_EXTRACT(doc,'$.path'))",
     Function_call("JSON_KEYS", Column_identifier(Document_path{"path"})),
     EMPTY_SCHEMA}};

INSTANTIATE_TEST_SUITE_P(xpl_expr_generator_function_call, Function_call_test,
                         testing::ValuesIn(function_call_param));

struct Param_placeholders {
  std::string expect;
  Expression_generator::Prep_stmt_placeholder_list expect_ids;
  Expression_list args;
  Array expr;
};

class Placeholders_test : public testing::TestWithParam<Param_placeholders> {};

TEST_P(Placeholders_test, placeholders) {
  const Param_placeholders &param = GetParam();
  Query_string_builder qb;
  Expression_generator gen(&qb, param.args, EMPTY_SCHEMA, DM_TABLE);
  Expression_generator::Prep_stmt_placeholder_list ids;
  gen.set_prep_stmt_placeholder_list(&ids);
  gen.feed(param.expr);

  EXPECT_STREQ(param.expect.c_str(), qb.get().c_str());
  EXPECT_EQ(param.expect_ids, ids);
}

#define PH Placeholder

Param_placeholders placeholders_param[] = {
    {"JSON_ARRAY(?)", {0}, {}, {PH{0}}},
    {"JSON_ARRAY('a')", {}, {"a"}, {PH{0}}},
    {"JSON_ARRAY(?)", {0}, {"a"}, {PH{1}}},
    {"JSON_ARRAY(?,?)", {0, 0}, {}, {PH{0}, PH{0}}},
    {"JSON_ARRAY(?,?)", {1, 0}, {}, {PH{1}, PH{0}}},
    {"JSON_ARRAY('a',?)", {0}, {"a"}, {PH{0}, PH{1}}},
    {"JSON_ARRAY(?,'a')", {0}, {"a"}, {PH{1}, PH{0}}},
    {"JSON_ARRAY('a','b')", {}, {"a", "b"}, {PH{0}, PH{1}}},
    {"JSON_ARRAY('a','b','a')", {}, {"a", "b"}, {PH{0}, PH{1}, PH{0}}},
    {"JSON_ARRAY('a','b',?)", {0}, {"a", "b"}, {PH{0}, PH{1}, PH{2}}},
    {"JSON_ARRAY('a',?,'b')", {0}, {"a", "b"}, {PH{0}, PH{2}, PH{1}}},
    {"JSON_ARRAY(?,'a','b')", {0}, {"a", "b"}, {PH{2}, PH{0}, PH{1}}},
    {"JSON_ARRAY(?,'a',?,'b',?)",
     {0, 0, 0},
     {"a", "b"},
     {PH{2}, PH{0}, PH{2}, PH{1}, PH{2}}},
    {"JSON_ARRAY(?,'a',?,'b',?)",
     {0, 1, 0},
     {"a", "b"},
     {PH{2}, PH{0}, PH{3}, PH{1}, PH{2}}},
};

INSTANTIATE_TEST_SUITE_P(xpl_expr_generator_placeholders, Placeholders_test,
                         testing::ValuesIn(placeholders_param));

struct Param_operator_pass {
  std::string expect;
  Operator operator_;
  Expression_list args;
};

class Operator_pass_test
    : public testing::TestWithParam<std::shared_ptr<Param_operator_pass>> {};

TEST_P(Operator_pass_test, operator_pass) {
  const auto &param = *GetParam();
  EXPECT_STREQ(
      param.expect.c_str(),
      generate_expression(param.operator_, param.args, EMPTY_SCHEMA, DM_TABLE)
          .c_str());
}

std::shared_ptr<Param_operator_pass> make_op_pass(std::string &&expect,
                                                  Operator &&operator_,
                                                  Expression_list &&args) {
  return std::shared_ptr<Param_operator_pass>{new Param_operator_pass{
      std::move(expect), std::move(operator_), std::move(args)}};
}
std::shared_ptr<Param_operator_pass> cont_in_pass_param[] = {
    // literals
    make_op_pass("JSON_CONTAINS(CAST(1 AS JSON),CAST(2 AS JSON))",
                 Operator("cont_in", 2, 1), {}),
    make_op_pass("JSON_CONTAINS(CAST(1.2 AS JSON),CAST(2.1 AS JSON))",
                 Operator("cont_in", 2.1, 1.2), {}),
    make_op_pass("JSON_CONTAINS(CAST(FALSE AS JSON),CAST(TRUE AS JSON))",
                 Operator("cont_in", true, false), {}),
    make_op_pass("JSON_CONTAINS(CAST('null' AS JSON),CAST('null' AS JSON))",
                 Operator("cont_in", Scalar::Null(), Scalar::Null()), {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_QUOTE('white'),JSON_QUOTE('black'))",
        Operator("cont_in", Scalar::String("black"), Scalar::String("white")),
        {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_QUOTE('white'),JSON_QUOTE('black'))",
        Operator("cont_in", Octets("black", Octets::Content_type::k_plain),
                 Octets("white", Octets::Content_type::k_plain)),
        {}),
    make_op_pass(
        "JSON_CONTAINS(CAST('{\\\"white\\\":2}' AS JSON),"
        "CAST('{\\\"black\\\":1}' AS JSON))",
        Operator("cont_in",
                 Octets("{\"black\":1}", Octets::Content_type::k_json),
                 Octets("{\"white\":2}", Octets::Content_type::k_json)),
        {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_QUOTE('<a>white</a>'),JSON_QUOTE('<a>black</a>'))",
        Operator("cont_in", Octets("<a>black</a>", Octets::Content_type::k_xml),
                 Octets("<a>white</a>", Octets::Content_type::k_xml)),
        {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_QUOTE(ST_GEOMETRYFROMWKB('101')),"
        "JSON_QUOTE(ST_GEOMETRYFROMWKB('010')))",
        Operator("cont_in", Octets("010", Octets::Content_type::k_geometry),
                 Octets("101", Octets::Content_type::k_geometry)),
        {}),
    //  arrays
    make_op_pass("JSON_CONTAINS(JSON_ARRAY(3,4),JSON_ARRAY(1,2))",
                 Operator("cont_in", Array{1, 2}, Array{3, 4}), {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_ARRAY(3,FALSE,'white'),JSON_ARRAY(1,TRUE,'black')"
        ")",
        Operator("cont_in", Array{1, true, "black"}, Array{3, false, "white"}),
        {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_ARRAY(CAST('{\\\"white\\\":2}' AS JSON)),"
        "JSON_ARRAY(CAST('{\\\"black\\\":1}' AS JSON)))",
        Operator("cont_in",
                 Array{Octets("{\"black\":1}", Octets::Content_type::k_json)},
                 Array{Octets("{\"white\":2}", Octets::Content_type::k_json)}),
        {}),
    //  objects
    make_op_pass(
        "JSON_CONTAINS(JSON_OBJECT('second',2),JSON_OBJECT('first',1))",
        Operator("cont_in", Object{{"first", 1}}, Object{{"second", 2}}), {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_OBJECT('second',CAST('{\\\"white\\\":2}' AS "
        "JSON)),"
        "JSON_OBJECT('first',CAST('{\\\"black\\\":1}' AS JSON)))",
        Operator("cont_in",
                 Object{{"first", Octets("{\"black\":1}",
                                         Octets::Content_type::k_json)}},
                 Object{{"second", Octets("{\"white\":2}",
                                          Octets::Content_type::k_json)}}),
        {}),
    make_op_pass(
        "JSON_CONTAINS(CAST((2 - 1) AS JSON),CAST((1 + 2) AS JSON))",
        Operator("cont_in",
                 Operator("cast", Operator("+", 1, 2), Octets("JSON")),
                 Operator("cast", Operator("-", 2, 1), Octets("JSON"))),
        {}),
    // functions
    make_op_pass(
        "JSON_CONTAINS(json_quote(concat('foo','bar')),"
        "json_quote(concat('foo','bar')))",
        Operator(
            "cont_in",
            Function_call("json_quote", Function_call("concat", "foo", "bar")),
            Function_call("json_quote", Function_call("concat", "foo", "bar"))),
        {}),
    // placeholders
    make_op_pass("JSON_CONTAINS(CAST(2 AS JSON),CAST(1 AS JSON))",
                 Operator("cont_in", Placeholder(0), Placeholder(1)), {1, 2}),
    make_op_pass("JSON_CONTAINS(JSON_QUOTE('bar'),JSON_QUOTE('foo'))",
                 Operator("cont_in", Placeholder(0), Placeholder(1)),
                 {"foo", "bar"}),
    make_op_pass("JSON_CONTAINS(CAST('{\\\"white\\\":2}' AS JSON),"
                 "CAST('{\\\"black\\\":1}' AS JSON))",
                 Operator("cont_in", Placeholder(0), Placeholder(1)),
                 {Octets("{\"black\":1}", Octets::Content_type::k_json),
                  Octets("{\"white\":2}", Octets::Content_type::k_json)}),
    //  identifier
    make_op_pass("JSON_CONTAINS(CAST(42 AS JSON),"
                 "JSON_EXTRACT(`schema`.`table`.`field`,'$.member'))",
                 Operator("cont_in",
                          Column_identifier(Document_path{"member"}, "field",
                                            "table", "schema"),
                          42),
                 {}),
    make_op_pass(
        "JSON_CONTAINS(JSON_EXTRACT(`schema`.`table`.`field`,'$.member'),"
        "CAST(42 AS JSON))",
        Operator("cont_in", 42,
                 Column_identifier(Document_path{"member"}, "field", "table",
                                   "schema")),
        {}),
    make_op_pass(
        "JSON_CONTAINS(`schema`.`table`.`field`,CAST(42 AS JSON))",
        Operator("cont_in", 42, Column_identifier("field", "table", "schema")),
        {}),
};

INSTANTIATE_TEST_SUITE_P(xpl_expr_generator_cont_in_pass, Operator_pass_test,
                         testing::ValuesIn(cont_in_pass_param));

struct Param_operator_fail {
  Operator operator_;
  Expression_list args;
};

class Operator_fail_test
    : public testing::TestWithParam<std::shared_ptr<Param_operator_fail>> {};

TEST_P(Operator_fail_test, operator_fail) {
  const auto &param = *GetParam();
  EXPECT_THROW(
      generate_expression(param.operator_, param.args, EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error)
      << "Should throw for: " << msg_to_string(param.operator_.base());
}

template <typename... Arguments>
std::shared_ptr<Param_operator_fail> make_op_fail(Operator &&op,
                                                  Expression_list &&list) {
  return std::shared_ptr<Param_operator_fail>{
      new Param_operator_fail{std::move(op), std::move(list)}};
}

std::shared_ptr<Param_operator_fail> cont_in_fail_param[] = {
    //  literals
    //  arrays
    //  objects
    //  operators
    make_op_fail(Operator("cont_in", Operator("+", 1, 2), Operator("-", 2, 1)),
                 {}),
    make_op_fail(
        Operator("cont_in", Operator("+", 1, 2),
                 Operator("cast", Operator("-", 2, 1), Octets("JSON"))),
        {}),
    make_op_fail(Operator("cont_in",
                          Operator("cast", Operator("+", 1, 2), Octets("JSON")),
                          Operator("-", 2, 1)),
                 {}),
    make_op_fail(
        Operator("cont_in",
                 Operator("cast", Operator("+", 1, 2), Octets("SIGNED")),
                 Operator("cast", Operator("-", 2, 1), Octets("JSON"))),
        {}),
    make_op_fail(
        Operator("cont_in",
                 Operator("cast", Operator("+", 1, 2), Octets("JSON")),
                 Operator("cast", Operator("-", 2, 1), Octets("SIGNED"))),
        {}),
    //  functions
    make_op_fail(Operator("cont_in", Function_call("concat", "foo", "bar"),
                          Function_call("concat", "foo", "bar")),
                 {}),
    make_op_fail(Operator("cont_in", Function_call("concat", "foo", "bar"),
                          Function_call("json_quote",
                                        Function_call("concat", "foo", "bar"))),
                 {}),
    make_op_fail(Operator("cont_in",
                          Function_call("json_quote",
                                        Function_call("concat", "foo", "bar")),
                          Function_call("concat", "foo", "bar")),
                 {}),
    //  placeholders
    make_op_fail(Operator("cont_in", Placeholder(0), Placeholder(1)), {}),
    //  identifier
};

INSTANTIATE_TEST_SUITE_P(xpl_expr_generator_cont_in_fail, Operator_fail_test,
                         testing::ValuesIn(cont_in_fail_param));

std::shared_ptr<Param_operator_pass> overlaps_pass_param[] = {
    // literals
    make_op_pass("JSON_OVERLAPS(CAST(2 AS JSON),CAST(1 AS JSON))",
                 Operator("overlaps", 2, 1), {}),
    make_op_pass("JSON_OVERLAPS(CAST(2.1 AS JSON),CAST(1.2 AS JSON))",
                 Operator("overlaps", 2.1, 1.2), {}),
    make_op_pass("JSON_OVERLAPS(CAST(TRUE AS JSON),CAST(FALSE AS JSON))",
                 Operator("overlaps", true, false), {}),
    make_op_pass("JSON_OVERLAPS(CAST('null' AS JSON),CAST('null' AS JSON))",
                 Operator("overlaps", Scalar::Null(), Scalar::Null()), {}),
    make_op_pass(
        "JSON_OVERLAPS(JSON_QUOTE('black'),JSON_QUOTE('white'))",
        Operator("overlaps", Scalar::String("black"), Scalar::String("white")),
        {}),
    make_op_pass(
        "JSON_OVERLAPS(JSON_QUOTE('black'),JSON_QUOTE('white'))",
        Operator("overlaps", Octets("black", Octets::Content_type::k_plain),
                 Octets("white", Octets::Content_type::k_plain)),
        {}),
    make_op_pass(
        "JSON_OVERLAPS("
        "CAST('{\\\"black\\\":1}' AS JSON),CAST('{\\\"white\\\":2}' AS JSON))",
        Operator("overlaps",
                 Octets("{\"black\":1}", Octets::Content_type::k_json),
                 Octets("{\"white\":2}", Octets::Content_type::k_json)),
        {}),
    make_op_pass(
        "JSON_OVERLAPS(JSON_QUOTE('<a>black</a>'),JSON_QUOTE('<a>white</a>'))",
        Operator("overlaps",
                 Octets("<a>black</a>", Octets::Content_type::k_xml),
                 Octets("<a>white</a>", Octets::Content_type::k_xml)),
        {}),
    make_op_pass(
        "JSON_OVERLAPS("
        "JSON_QUOTE(ST_GEOMETRYFROMWKB('010')),"
        "JSON_QUOTE(ST_GEOMETRYFROMWKB('101')))",
        Operator("overlaps", Octets("010", Octets::Content_type::k_geometry),
                 Octets("101", Octets::Content_type::k_geometry)),
        {}),
    //  arrays
    make_op_pass("JSON_OVERLAPS(JSON_ARRAY(1,2),JSON_ARRAY(3,4))",
                 Operator("overlaps", Array{1, 2}, Array{3, 4}), {}),
    make_op_pass(
        "JSON_OVERLAPS(JSON_ARRAY(1,TRUE,'black'),JSON_ARRAY(3,FALSE,"
        "'white'))",
        Operator("overlaps", Array{1, true, "black"}, Array{3, false, "white"}),
        {}),
    make_op_pass(
        "JSON_OVERLAPS("
        "JSON_ARRAY(CAST('{\\\"black\\\":1}' AS JSON)),"
        "JSON_ARRAY(CAST('{\\\"white\\\":2}' AS JSON)))",
        Operator("overlaps",
                 Array{Octets("{\"black\":1}", Octets::Content_type::k_json)},
                 Array{Octets("{\"white\":2}", Octets::Content_type::k_json)}),
        {}),
    //  objects
    make_op_pass(
        "JSON_OVERLAPS(JSON_OBJECT('first',1),JSON_OBJECT('second',2))",
        Operator("overlaps", Object{{"first", 1}}, Object{{"second", 2}}), {}),
    make_op_pass(
        "JSON_OVERLAPS("
        "JSON_OBJECT('first',CAST('{\\\"black\\\":1}' AS JSON)),"
        "JSON_OBJECT('second',CAST('{\\\"white\\\":2}' AS JSON)))",
        Operator("overlaps",
                 Object{{"first", Octets("{\"black\":1}",
                                         Octets::Content_type::k_json)}},
                 Object{{"second", Octets("{\"white\":2}",
                                          Octets::Content_type::k_json)}}),
        {}),
    make_op_pass(
        "JSON_OVERLAPS(CAST((1 + 2) AS JSON),CAST((2 - 1) AS JSON))",
        Operator("overlaps",
                 Operator("cast", Operator("+", 1, 2), Octets("JSON")),
                 Operator("cast", Operator("-", 2, 1), Octets("JSON"))),
        {}),
    // functions
    make_op_pass(
        "JSON_OVERLAPS("
        "json_quote(concat('foo','bar')),"
        "json_quote(concat('foo','bar')))",
        Operator(
            "overlaps",
            Function_call("json_quote", Function_call("concat", "foo", "bar")),
            Function_call("json_quote", Function_call("concat", "foo", "bar"))),
        {}),
    // placeholders
    make_op_pass("JSON_OVERLAPS(CAST(1 AS JSON),CAST(2 AS JSON))",
                 Operator("overlaps", Placeholder(0), Placeholder(1)), {1, 2}),
    make_op_pass("JSON_OVERLAPS(JSON_QUOTE('foo'),JSON_QUOTE('bar'))",
                 Operator("overlaps", Placeholder(0), Placeholder(1)),
                 {"foo", "bar"}),
    make_op_pass("JSON_OVERLAPS("
                 "CAST('{\\\"black\\\":1}' AS JSON),"
                 "CAST('{\\\"white\\\":2}' AS JSON))",
                 Operator("overlaps", Placeholder(0), Placeholder(1)),
                 {Octets("{\"black\":1}", Octets::Content_type::k_json),
                  Octets("{\"white\":2}", Octets::Content_type::k_json)}),
    //  identifier
    make_op_pass("JSON_OVERLAPS("
                 "JSON_EXTRACT(`schema`.`table`.`field`,'$.member'),"
                 "CAST(42 AS JSON))",
                 Operator("overlaps",
                          Column_identifier(Document_path{"member"}, "field",
                                            "table", "schema"),
                          42),
                 {}),
    make_op_pass("JSON_OVERLAPS("
                 "CAST(42 AS JSON),"
                 "JSON_EXTRACT(`schema`.`table`.`field`,'$.member'))",
                 Operator("overlaps", 42,
                          Column_identifier(Document_path{"member"}, "field",
                                            "table", "schema")),
                 {}),
    make_op_pass(
        "JSON_OVERLAPS("
        "CAST(42 AS JSON),"
        "`schema`.`table`.`field`)",
        Operator("overlaps", 42, Column_identifier("field", "table", "schema")),
        {}),
};

INSTANTIATE_TEST_SUITE_P(xpl_expr_generator_overlaps_pass, Operator_pass_test,
                         testing::ValuesIn(overlaps_pass_param));

std::shared_ptr<Param_operator_fail> overlaps_fail_param[] = {
    //  literals
    //  arrays
    //  objects
    //  operators
    make_op_fail(Operator("overlaps", Operator("+", 1, 2), Operator("-", 2, 1)),
                 {}),
    make_op_fail(
        Operator("overlaps", Operator("+", 1, 2),
                 Operator("cast", Operator("-", 2, 1), Octets("JSON"))),
        {}),
    make_op_fail(Operator("overlaps",
                          Operator("cast", Operator("+", 1, 2), Octets("JSON")),
                          Operator("-", 2, 1)),
                 {}),
    make_op_fail(
        Operator("overlaps",
                 Operator("cast", Operator("+", 1, 2), Octets("SIGNED")),
                 Operator("cast", Operator("-", 2, 1), Octets("JSON"))),
        {}),
    make_op_fail(
        Operator("overlaps",
                 Operator("cast", Operator("+", 1, 2), Octets("JSON")),
                 Operator("cast", Operator("-", 2, 1), Octets("SIGNED"))),
        {}),
    //  functions
    make_op_fail(Operator("overlaps", Function_call("concat", "foo", "bar"),
                          Function_call("concat", "foo", "bar")),
                 {}),
    make_op_fail(Operator("overlaps", Function_call("concat", "foo", "bar"),
                          Function_call("json_quote",
                                        Function_call("concat", "foo", "bar"))),
                 {}),
    make_op_fail(Operator("overlaps",
                          Function_call("json_quote",
                                        Function_call("concat", "foo", "bar")),
                          Function_call("concat", "foo", "bar")),
                 {}),
    //  placeholders
    make_op_fail(Operator("overlaps", Placeholder(0), Placeholder(1)), {}),
    //  identifier
};

INSTANTIATE_TEST_SUITE_P(xpl_expr_generator_overlaps_fail, Operator_fail_test,
                         testing::ValuesIn(overlaps_fail_param));

}  // namespace test
}  // namespace xpl
