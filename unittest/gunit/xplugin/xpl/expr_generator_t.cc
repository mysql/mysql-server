/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

const char *const EMPTY_SCHEMA = "";
const char *const EMPTY = "";
enum { DM_DOCUMENT = 0, DM_TABLE = 1 };

TEST(xpl_expr_generator, literal_uint) {
  EXPECT_EQ("0", generate_expression(Scalar(static_cast<unsigned>(0)),
                                     EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("1234567890",
            generate_expression(Scalar(static_cast<unsigned>(1234567890)),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_NE("-1234567890",
            generate_expression(Scalar(static_cast<unsigned>(-1234567890)),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, literal_sint) {
  EXPECT_EQ("0", generate_expression(Scalar(0), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("1234567890",
            generate_expression(Scalar(1234567890), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("-1234567890",
            generate_expression(Scalar(-1234567890), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, literal_null) {
  EXPECT_EQ("NULL", generate_expression(Scalar(Scalar::Null()), EMPTY_SCHEMA,
                                        DM_TABLE));
}

TEST(xpl_expr_generator, literal_octets) {
  EXPECT_EQ("'\\\"test1\\\" \t \\'test2\\''",
            generate_expression(Scalar("\"test1\" \t 'test2'"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, literal_string) {
  EXPECT_EQ("'\\\"test1\\\" \t \\'test2\\''",
            generate_expression(Scalar(Scalar::String("\"test1\" \t 'test2'")),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, literal_double) {
  EXPECT_EQ("1234567890.123456", generate_expression(Scalar(1234567890.123456),
                                                     EMPTY_SCHEMA, DM_TABLE)
                                     .substr(0, 17));
  EXPECT_EQ(
      "-1234567890.123456",
      generate_expression(Scalar(-1234567890.123456), EMPTY_SCHEMA, DM_TABLE)
          .substr(0, 18));
}

TEST(xpl_expr_generator, literal_float) {
  EXPECT_EQ("1234.12",
            generate_expression(Scalar(1234.123f), EMPTY_SCHEMA, DM_TABLE)
                .substr(0, 8));
  EXPECT_EQ("-1234.12",
            generate_expression(Scalar(-1234.123f), EMPTY_SCHEMA, DM_TABLE)
                .substr(0, 9));
}

TEST(xpl_expr_generator, literal_bool) {
  EXPECT_EQ("TRUE", generate_expression(Scalar(true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("FALSE",
            generate_expression(Scalar(false), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, invalid_operator) {
  EXPECT_THROW(generate_expression(Operator("some invalid operator"),
                                   EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}

TEST(xpl_expr_generator, nullary_operators) {
  EXPECT_EQ("*", generate_expression(Operator("*"), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("*", generate_expression(Operator("*"), EMPTY_SCHEMA, DM_DOCUMENT));
}

TEST(xpl_expr_generator, unary_operators) {
  EXPECT_EQ("(NOT TRUE)",
            generate_expression(Operator("not", true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(+TRUE)", generate_expression(Operator("sign_plus", true),
                                           EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(-TRUE)", generate_expression(Operator("sign_minus", true),
                                           EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(!TRUE)",
            generate_expression(Operator("!", true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("!"), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
  EXPECT_THROW(
      generate_expression(Operator("!", true, true), EMPTY_SCHEMA, DM_TABLE),
      std::invalid_argument);
}

TEST(xpl_expr_generator, binary_operators) {
  EXPECT_EQ("(TRUE AND TRUE)", generate_expression(Operator("&&", true, true),
                                                   EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE OR TRUE)", generate_expression(Operator("||", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE XOR TRUE)", generate_expression(Operator("xor", true, true),
                                                   EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE = TRUE)", generate_expression(Operator("==", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE != TRUE)", generate_expression(Operator("!=", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE >= TRUE)", generate_expression(Operator(">=", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE > TRUE)", generate_expression(Operator(">", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE <= TRUE)", generate_expression(Operator("<=", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE < TRUE)", generate_expression(Operator("<", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE & TRUE)", generate_expression(Operator("&", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE | TRUE)", generate_expression(Operator("|", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE ^ TRUE)", generate_expression(Operator("^", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE << TRUE)", generate_expression(Operator("<<", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE >> TRUE)", generate_expression(Operator(">>", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE + TRUE)", generate_expression(Operator("+", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE - TRUE)", generate_expression(Operator("-", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE * TRUE)", generate_expression(Operator("*", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE / TRUE)", generate_expression(Operator("/", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(4 DIV 2)",
            generate_expression(Operator("div", 4, 2), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE % TRUE)", generate_expression(Operator("%", true, true),
                                                 EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE IS TRUE)", generate_expression(Operator("is", true, true),
                                                  EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE IS NOT TRUE)",
            generate_expression(Operator("is_not", true, true), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("(TRUE REGEXP TRUE)",
            generate_expression(Operator("regexp", true, true), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("(TRUE NOT REGEXP TRUE)",
            generate_expression(Operator("not_regexp", true, true),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("+"), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("+", true), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("+", true, true, true),
                                   EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}

TEST(xpl_expr_generator, identifier) {
  EXPECT_EQ("` schema \"'`.` table \"'`",
            generate_expression(Identifier(" table \"'"), " schema \"'", true));
  EXPECT_EQ("` schema \"'`.` table \"'`",
            generate_expression(Identifier(" table \"'", " schema \"'"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, variable) {
  // EXPECT_EQ("@`'variable``\"`",
  // generate_expression(Expr(Variable("'variable`\""))));
  EXPECT_THROW(generate_expression(Expr(Variable("'variable`\"")), EMPTY_SCHEMA,
                                   DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, column_identifier) {
  Document_path doc_path{"docpath \"'"};

  EXPECT_EQ("`column ``\"'`",
            generate_expression(Column_identifier("column `\"'"), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("`table ``\"'`.`column ``\"'`",
            generate_expression(Column_identifier("column `\"'", "table `\"'"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("`schema ``\"'`.`table ``\"'`.`column ``\"'`",
            generate_expression(
                Column_identifier("column `\"'", "table `\"'", "schema `\"'"),
                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "JSON_EXTRACT(doc,'$.\\\"docpath \\\\\\\"\\'\\\"')",
      generate_expression(Column_identifier(doc_path, EMPTY, EMPTY, EMPTY),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("JSON_EXTRACT(`\"'`` column`,'$.\\\"docpath \\\\\\\"\\'\\\"')",
            generate_expression(
                Column_identifier(doc_path, "\"'` column", EMPTY, EMPTY),
                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "JSON_EXTRACT(`\"'`` table`.`\"'`` column`,'$.\\\"docpath "
      "\\\\\\\"\\'\\\"')",
      generate_expression(
          Column_identifier(doc_path, "\"'` column", "\"'` table", EMPTY),
          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "JSON_EXTRACT(`\"'`` schema`.`\"'`` table`."
      "`\"'`` column`,'$.\\\"docpath \\\\\\\"\\'\\\"')",
      generate_expression(Column_identifier(doc_path, "\"'` column",
                                            "\"'` table", "\"'` schema"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Column_identifier(EMPTY, "table"),
                                   EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
  EXPECT_THROW(generate_expression(Column_identifier("column", EMPTY, "schema"),
                                   EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}

TEST(xpl_expr_generator, column_identifier_doc_id) {
  Column_identifier ident(Document_path{"_id"});
  ASSERT_EQ("JSON_EXTRACT(doc,'$._id')",
            generate_expression(ident, EMPTY_SCHEMA, DM_TABLE));
  ASSERT_EQ("JSON_EXTRACT(doc,'$._id')",
            generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT));
}

TEST(xpl_expr_generator, column_identifier_doc_id_names) {
  Column_identifier ident(Document_path{"_id"}, "field", "table", "schema");
  ASSERT_EQ("JSON_EXTRACT(`schema`.`table`.`field`,'$._id')",
            generate_expression(ident, EMPTY_SCHEMA, DM_TABLE));
  ASSERT_EQ("JSON_EXTRACT(`schema`.`table`.`field`,'$._id')",
            generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT));
}

TEST(xpl_expr_generator, column_identifier_no_column) {
  Column_identifier ident(EMPTY, "table");
  ASSERT_THROW(generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT),
               Expression_generator::Error);

  ASSERT_THROW(generate_expression(ident, EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);

  Column_identifier ident2(Document_path{"member"}, EMPTY, "table", EMPTY);
  ASSERT_EQ("JSON_EXTRACT(`table`.doc,'$.member')",
            generate_expression(ident2, EMPTY_SCHEMA, DM_DOCUMENT));
}

TEST(xpl_expr_generator, interval_expression) {
  EXPECT_EQ(
      "DATE_ADD(FALSE, INTERVAL TRUE MICROSECOND)",
      generate_expression(Operator("date_add", false, true, "MICROSECOND"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE SECOND)",
            generate_expression(Operator("date_sub", false, true, "SECOND"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE MINUTE)",
            generate_expression(Operator("date_sub", false, true, "MINUTE"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE HOUR)",
            generate_expression(Operator("date_sub", false, true, "HOUR"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE DAY)",
            generate_expression(Operator("date_sub", false, true, "DAY"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE WEEK)",
            generate_expression(Operator("date_sub", false, true, "WEEK"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE MONTH)",
            generate_expression(Operator("date_sub", false, true, "MONTH"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE QUARTER)",
            generate_expression(Operator("date_sub", false, true, "QUARTER"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE YEAR)",
            generate_expression(Operator("date_sub", false, true, "YEAR"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE SECOND_MICROSECOND)",
            generate_expression(
                Operator("date_sub", false, true, "SECOND_MICROSECOND"),
                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE MINUTE_MICROSECOND)",
            generate_expression(
                Operator("date_sub", false, true, "MINUTE_MICROSECOND"),
                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "DATE_SUB(FALSE, INTERVAL TRUE MINUTE_SECOND)",
      generate_expression(Operator("date_sub", false, true, "MINUTE_SECOND"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "DATE_SUB(FALSE, INTERVAL TRUE HOUR_MICROSECOND)",
      generate_expression(Operator("date_sub", false, true, "HOUR_MICROSECOND"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "DATE_SUB(FALSE, INTERVAL TRUE HOUR_SECOND)",
      generate_expression(Operator("date_sub", false, true, "HOUR_SECOND"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "DATE_SUB(FALSE, INTERVAL TRUE HOUR_MINUTE)",
      generate_expression(Operator("date_sub", false, true, "HOUR_MINUTE"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ(
      "DATE_SUB(FALSE, INTERVAL TRUE DAY_MICROSECOND)",
      generate_expression(Operator("date_sub", false, true, "DAY_MICROSECOND"),
                          EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE DAY_SECOND)",
            generate_expression(Operator("date_sub", false, true, "DAY_SECOND"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE DAY_MINUTE)",
            generate_expression(Operator("date_sub", false, true, "DAY_MINUTE"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE DAY_HOUR)",
            generate_expression(Operator("date_sub", false, true, "DAY_HOUR"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("DATE_SUB(FALSE, INTERVAL TRUE YEAR_MONTH)",
            generate_expression(Operator("date_sub", false, true, "YEAR_MONTH"),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(
      generate_expression(Operator("date_sub", false, true, "invalid unit"),
                          EMPTY_SCHEMA, DM_TABLE),
      std::invalid_argument);
  EXPECT_THROW(
      generate_expression(Operator("date_sub", false, true, true, true),
                          EMPTY_SCHEMA, DM_TABLE),
      std::invalid_argument);
}

TEST(xpl_expr_generator, in_expression) {
  EXPECT_EQ(
      "(FALSE IN (TRUE))",
      generate_expression(Operator("in", false, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(FALSE NOT IN (TRUE))",
            generate_expression(Operator("not_in", false, true), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("(FALSE IN (TRUE,FALSE))",
            generate_expression(Operator("in", false, true, false),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(FALSE NOT IN (TRUE,FALSE))",
            generate_expression(Operator("not_in", false, true, false),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(
      generate_expression(Operator("in", false), EMPTY_SCHEMA, DM_TABLE),
      std::invalid_argument);
}

TEST(xpl_expr_generator, between_expression) {
  EXPECT_EQ("(2 BETWEEN 1 AND 3)",
            generate_expression(Operator("between", 2, 1, 3), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("(2 NOT BETWEEN 1 AND 3)",
            generate_expression(Operator("not_between", 2, 1, 3), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_THROW(
      generate_expression(Operator("between", 0, 0), EMPTY_SCHEMA, DM_TABLE),
      std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("between", 0, 0, 0, 0),
                                   EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}

TEST(xpl_expr_generator, like_expression) {
  EXPECT_EQ("(TRUE LIKE FALSE)",
            generate_expression(Operator("like", true, false), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("(TRUE NOT LIKE FALSE)",
            generate_expression(Operator("not_like", true, false), EMPTY_SCHEMA,
                                DM_TABLE));
  EXPECT_EQ("(TRUE LIKE FALSE ESCAPE TRUE)",
            generate_expression(Operator("like", true, false, true),
                                EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(
      generate_expression(Operator("like", true), EMPTY_SCHEMA, DM_TABLE),
      std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("like", true, true, true, true),
                                   EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}

TEST(xpl_expr_generator, complex_expressions) {
  EXPECT_EQ("(`schema`.`ident``` NOT LIKE 'string\\'' ESCAPE 'x')",
            generate_expression(
                Expr(Operator("not_like", Column_identifier("ident`", "schema"),
                              "string'", "x")),
                EMPTY_SCHEMA, DM_TABLE));

  EXPECT_EQ("((1 * 2) % (3 / 4))",
            generate_expression(
                Expr(Operator("%", Operator("*", 1, 2), Operator("/", 3, 4))),
                EMPTY_SCHEMA, DM_TABLE));

  EXPECT_EQ("(`schema`.func(5,FALSE) IN (1,(+2),(-(7 - 0))))",
            generate_expression(
                Expr(Operator(
                    "in", Function_call(Identifier("func", "schema"), 5, false),
                    1, Operator("sign_plus", 2),
                    Operator("sign_minus", Operator("-", 7, 0)))),
                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_root) {
  EXPECT_EQ("'$'", generate_expression(Document_path(Document_path()),
                                       EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_empty_member) {
  EXPECT_THROW(
      generate_expression(Document_path{EMPTY, "name"}, EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, document_path_empty_member_opposite) {
  EXPECT_THROW(
      generate_expression(Document_path{"name", EMPTY}, EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, document_path_array) {
  Document_path path;
  EXPECT_EQ("'$.name[42]'", generate_expression(Document_path{"name", 42},
                                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_root_array) {
  Document_path path;
  EXPECT_EQ("'$[42]'",
            generate_expression(Document_path{42}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_member_asterisk) {
  EXPECT_EQ(
      "'$.name.*'",
      generate_expression(
          Document_path{"name", Document_path_item::Base::MEMBER_ASTERISK},
          EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_root_asterisk) {
  EXPECT_EQ("'$.*'",
            generate_expression(
                Document_path{Document_path_item::Base::MEMBER_ASTERISK},
                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_double_asterisk) {
  EXPECT_EQ(
      "'$.name**'",
      generate_expression(
          Document_path{"name", Document_path_item::Base::DOUBLE_ASTERISK},
          EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_array_index) {
  EXPECT_EQ("'$.name[42]'", generate_expression(Document_path{"name", 42},
                                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_root_array_index) {
  EXPECT_EQ("'$[42]'",
            generate_expression(Document_path{42}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, document_path_array_index_asterisk) {
  EXPECT_STREQ(
      "'$.name[*]'",
      generate_expression(
          Document_path{"name", Document_path_item::Base::ARRAY_INDEX_ASTERISK},
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
}

TEST(xpl_expr_generator, document_path_root_array_index_asterisk) {
  EXPECT_STREQ(
      "'$[*]'",
      generate_expression(
          Document_path{Document_path_item::Base::ARRAY_INDEX_ASTERISK},
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
}

TEST(xpl_expr_generator, document_path_root_double_asterisk) {
  EXPECT_EQ("'$**'",
            generate_expression(
                Document_path{Document_path_item::Base::DOUBLE_ASTERISK},
                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, placeholder_not_found) {
  EXPECT_THROW(generate_expression(Expr(Placeholder{10}), Expression_list(),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, placeholder_found) {
  EXPECT_EQ("2", generate_expression(Expr(Placeholder{0}), Expression_list{2},
                                     EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, placeholder_opearator_one_arg) {
  EXPECT_EQ("(1 + 2)",
            generate_expression(Operator("+", 1, Placeholder{0}),
                                Expression_list{2}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, placeholder_opearator_two_args) {
  EXPECT_EQ("(1 + 2)",
            generate_expression(Operator("+", Placeholder{1}, Placeholder{0}),
                                Expression_list{2, 1}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, placeholder_function) {
  EXPECT_EQ(
      "xschema.bar(42,'foo')",
      generate_expression(Function_call("bar", Placeholder{0}, Placeholder{1}),
                          Expression_list{42, "foo"}, "xschema", true));
}

TEST(xpl_expr_generator, placeholder_function_and_operator) {
  EXPECT_EQ(
      "(xschema.bar(42,'foo') > 42)",
      generate_expression(
          Operator(">", Function_call("bar", Placeholder{0}, Placeholder{1}),
                   Placeholder{0}),
          Expression_list{42, "foo"}, "xschema", true));
}

TEST(xpl_expr_generator, placeholder_operator_null) {
  EXPECT_EQ("(`bar` IS NOT NULL)",
            generate_expression(
                Operator("is_not", Column_identifier("bar"), Placeholder{0}),
                Expression_list{Scalar::Null()}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_missing_param) {
  EXPECT_THROW(
      generate_expression(Operator("cast", 42), EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, cast_empty_type) {
  EXPECT_THROW(
      generate_expression(Operator("cast", 42), EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, cast_invalid_target_type) {
  EXPECT_THROW(
      generate_expression(Operator("cast", 42, 44), EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, cast_scalar_to_undefinied) {
  EXPECT_THROW(generate_expression(Operator("cast", 42, "UNDEFINIED"),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, cast_expr_to_json) {
  EXPECT_EQ("CAST(`foo`.`bar` AS JSON)",
            generate_expression(
                Operator("cast", Column_identifier("bar", "foo"), "JSON"),
                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_signed) {
  EXPECT_EQ("CAST(42 AS SIGNED)",
            generate_expression(Operator("cast", 42, "SIGNED"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_signed_integer) {
  EXPECT_EQ("CAST(42 AS SIGNED INTEGER)",
            generate_expression(Operator("cast", 42, "SIGNED INTEGER"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_unsigned) {
  EXPECT_EQ("CAST(42 AS UNSIGNED)",
            generate_expression(Operator("cast", 42, "UNSIGNED"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_unsigned_integer) {
  EXPECT_EQ("CAST(42 AS UNSIGNED INTEGER)",
            generate_expression(Operator("cast", 42, "UNSIGNED INTEGER"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_char) {
  EXPECT_EQ("CAST('one' AS CHAR)",
            generate_expression(Operator("cast", "one", "CHAR"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_multichar) {
  EXPECT_EQ("CAST('one' AS CHAR(42))",
            generate_expression(Operator("cast", "one", "CHAR(42)"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_binary) {
  EXPECT_EQ("CAST('one' AS BINARY)",
            generate_expression(Operator("cast", "one", "BINARY"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_binary_lowercase) {
  EXPECT_EQ("CAST('one' AS binary)",
            generate_expression(Operator("cast", "one", "binary"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_precision_binary) {
  EXPECT_EQ("CAST('one' AS BINARY(44))",
            generate_expression(Operator("cast", "one", "BINARY(44)"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_decimal) {
  EXPECT_EQ("CAST(3.141593 AS DECIMAL)",
            generate_expression(Operator("cast", 3.141593, "DECIMAL"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_precision_decimal) {
  EXPECT_EQ("CAST(3.141593 AS DECIMAL(4))",
            generate_expression(Operator("cast", 3.141593, "DECIMAL(4)"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_precision_scale_decimal) {
  EXPECT_EQ("CAST(3.141593 AS DECIMAL(4,2))",
            generate_expression(Operator("cast", 3.141593, "DECIMAL(4,2)"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_time) {
  EXPECT_EQ("CAST('3:14' AS TIME)",
            generate_expression(Operator("cast", "3:14", "TIME"), EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_date) {
  EXPECT_EQ("CAST('2015.08.10' AS DATE)",
            generate_expression(Operator("cast", "2015.08.10", "DATE"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, cast_scalar_to_datetime) {
  EXPECT_EQ("CAST('2015.08.10T3:14' AS DATETIME)",
            generate_expression(Operator("cast", "2015.08.10T3:14", "DATETIME"),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_empty) {
  EXPECT_EQ("JSON_OBJECT()",
            generate_expression(Object(), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_empty_key) {
  EXPECT_THROW(generate_expression(Object{{"", 1}}, EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, object_empty_value) {
  EXPECT_THROW(
      generate_expression(Object("first", nullptr), EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, object_one_scalar) {
  EXPECT_EQ("JSON_OBJECT('first',1)",
            generate_expression(Object{{"first", 1}}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_two_scalars) {
  EXPECT_EQ("JSON_OBJECT('first',1,'second','two')",
            generate_expression(Object{{"first", 1}, {"second", "two"}},
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_object) {
  EXPECT_EQ("JSON_OBJECT('second',JSON_OBJECT('first',1))",
            generate_expression(Object{{"second", Object{{"first", 1}}}},
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_as_expr) {
  EXPECT_EQ("JSON_OBJECT('first',1)",
            generate_expression(Object{{"first", 1}}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_operator) {
  EXPECT_EQ("JSON_OBJECT('sum',(1 + 2))",
            generate_expression(Object{{"sum", Operator("+", 1, 2)}},
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_function) {
  EXPECT_EQ("JSON_OBJECT('result',foo('bar'))",
            generate_expression(Object{{"result", Function_call("foo", "bar")}},
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_array) {
  EXPECT_EQ("JSON_OBJECT('tab',JSON_ARRAY(1,2))",
            generate_expression(Object{{"tab", Array{1, 2}}}, EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, object_in_function) {
  EXPECT_EQ("foo(JSON_OBJECT('first',1))",
            generate_expression(Function_call("foo", Object{{"first", 1}}),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, object_real_example) {
  EXPECT_EQ(
      "JSON_OBJECT('name',concat("
      "JSON_UNQUOTE(JSON_EXTRACT(doc,'$.first_name')),' ',"
      "JSON_UNQUOTE(JSON_EXTRACT(doc,'$.last_name'))),'number',(1 + 1))",
      generate_expression(
          Object{{"name",
                  Function_call(
                      "concat", Column_identifier(Document_path{"first_name"}),
                      " ", Column_identifier(Document_path{"last_name"}))},
                 {"number", Operator("+", 1, 1)}},
          EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_empty) {
  EXPECT_EQ("JSON_ARRAY()",
            generate_expression(Array(), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_one_scalar) {
  EXPECT_EQ("JSON_ARRAY(1)",
            generate_expression(Array{1}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_two_scalars) {
  EXPECT_EQ("JSON_ARRAY(1,'two')",
            generate_expression(Array{1, "two"}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_as_expr) {
  EXPECT_EQ("JSON_ARRAY(1)",
            generate_expression(Expr(Array{1}), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_array) {
  EXPECT_EQ(
      "JSON_ARRAY('one',JSON_ARRAY(1,2))",
      generate_expression(Array{"one", Array{1, 2}}, EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_object) {
  EXPECT_EQ("JSON_ARRAY(JSON_OBJECT('first',1),'two')",
            generate_expression(Array{Object{{"first", 1}}, "two"},
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_operator) {
  EXPECT_EQ("JSON_ARRAY((1 + 2),'two')",
            generate_expression(Array{Operator("+", 1, 2), "two"}, EMPTY_SCHEMA,
                                DM_TABLE));
}

TEST(xpl_expr_generator, array_function) {
  EXPECT_EQ("JSON_ARRAY(foo('bar'),'two')",
            generate_expression(Array{Function_call("foo", "bar"), "two"},
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_in_function) {
  EXPECT_EQ("fun(JSON_ARRAY('foo','bar'))",
            generate_expression(Function_call("fun", Array{"foo", "bar"}),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_in_operator) {
  EXPECT_STREQ("JSON_CONTAINS(JSON_ARRAY(1,2),CAST(1 AS JSON))",
               generate_expression(Operator("in", 1, Array{1, 2}), EMPTY_SCHEMA,
                                   DM_TABLE)
                   .c_str());
}

TEST(xpl_expr_generator, array_not_in_operator) {
  EXPECT_EQ("NOT JSON_CONTAINS(JSON_ARRAY(1,2),CAST(1 AS JSON))",
            generate_expression(Operator("not_in", 1, Array{1, 2}),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_in_operator_string) {
  EXPECT_EQ("JSON_CONTAINS(JSON_ARRAY('foo','bar'),JSON_QUOTE('foo'))",
            generate_expression(Operator("in", "foo", Array{"foo", "bar"}),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_not_in_operator_string) {
  EXPECT_EQ("NOT JSON_CONTAINS(JSON_ARRAY('foo','bar'),JSON_QUOTE('foo'))",
            generate_expression(Operator("not_in", "foo", Array{"foo", "bar"}),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, default_operator) {
  EXPECT_EQ("DEFAULT",
            generate_expression(Operator("default"), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(
      generate_expression(Operator("default", 42), EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, scalar_octets_plain) {
  EXPECT_EQ("'ABC'",
            generate_expression(
                Scalar(Scalar::Octets("ABC", Expression_generator::CT_PLAIN)),
                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, scalar_octets_geometry) {
  EXPECT_EQ("ST_GEOMETRYFROMWKB('010')",
            generate_expression(Scalar(Scalar::Octets(
                                    "010", Expression_generator::CT_GEOMETRY)),
                                EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, scalar_octets_json) {
  EXPECT_EQ(
      "CAST('{\\\"a\\\":42}' AS JSON)",
      generate_expression(
          Scalar(Scalar::Octets("{\"a\":42}", Expression_generator::CT_JSON)),
          EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, scalar_octets_xml) {
  EXPECT_EQ(
      "'<a>bbb</a>'",
      generate_expression(
          Scalar(Scalar::Octets("<a>bbb</a>", Expression_generator::CT_XML)),
          EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, scalar_octets_unknown) {
  EXPECT_THROW(generate_expression(Scalar(Scalar::Octets("foo", 666)),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, cont_in_expression_literals) {
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST(1 AS JSON),CAST(2 AS JSON))",
      generate_expression(Operator("cont_in", 2, 1), EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST(1.2 AS JSON),CAST(2.1 AS JSON))",
      generate_expression(Operator("cont_in", 2.1, 1.2), EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ("JSON_CONTAINS(CAST(FALSE AS JSON),CAST(TRUE AS JSON))",
               generate_expression(Operator("cont_in", true, false),
                                   EMPTY_SCHEMA, DM_TABLE)
                   .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST('null' AS JSON),CAST('null' AS JSON))",
      generate_expression(Operator("cont_in", Scalar::Null(), Scalar::Null()),
                          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ("JSON_CONTAINS(JSON_QUOTE('white'),JSON_QUOTE('black'))",
               generate_expression(Operator("cont_in", Scalar::String("black"),
                                            Scalar::String("white")),
                                   EMPTY_SCHEMA, DM_TABLE)
                   .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_QUOTE('white'),JSON_QUOTE('black'))",
      generate_expression(
          Operator("cont_in",
                   Scalar::Octets("black", Expression_generator::CT_PLAIN),
                   Scalar::Octets("white", Expression_generator::CT_PLAIN)),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST('{\\\"white\\\":2}' AS JSON),"
      "CAST('{\\\"black\\\":1}' AS JSON))",
      generate_expression(
          Operator(
              "cont_in",
              Scalar::Octets("{\"black\":1}", Expression_generator::CT_JSON),
              Scalar::Octets("{\"white\":2}", Expression_generator::CT_JSON)),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_QUOTE('<a>white</a>'),JSON_QUOTE('<a>black</a>'))",
      generate_expression(
          Operator(
              "cont_in",
              Scalar::Octets("<a>black</a>", Expression_generator::CT_XML),
              Scalar::Octets("<a>white</a>", Expression_generator::CT_XML)),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_QUOTE(ST_GEOMETRYFROMWKB('101')),"
      "JSON_QUOTE(ST_GEOMETRYFROMWKB('010')))",
      generate_expression(
          Operator("cont_in",
                   Scalar::Octets("010", Expression_generator::CT_GEOMETRY),
                   Scalar::Octets("101", Expression_generator::CT_GEOMETRY)),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
}

TEST(xpl_expr_generator, cont_in_expression_arrays) {
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_ARRAY(3,4),JSON_ARRAY(1,2))",
      generate_expression(Operator("cont_in", Array{1, 2}, Array{3, 4}),
                          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_ARRAY(3,FALSE,'white'),JSON_ARRAY(1,TRUE,'black'))",
      generate_expression(Operator("cont_in", Array{1, true, "black"},
                                   Array{3, false, "white"}),
                          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_ARRAY(CAST('{\\\"white\\\":2}' AS JSON)),"
      "JSON_ARRAY(CAST('{\\\"black\\\":1}' AS JSON)))",
      generate_expression(
          Operator("cont_in",
                   Array{Scalar::Octets("{\"black\":1}",
                                        Expression_generator::CT_JSON)},
                   Array{Scalar::Octets("{\"white\":2}",
                                        Expression_generator::CT_JSON)}),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
}

TEST(xpl_expr_generator, cont_in_expression_objects) {
  EXPECT_STREQ("JSON_CONTAINS(JSON_OBJECT('second',2),JSON_OBJECT('first',1))",
               generate_expression(Operator("cont_in", Object{{"first", 1}},
                                            Object{{"second", 2}}),
                                   EMPTY_SCHEMA, DM_TABLE)
                   .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_OBJECT('second',CAST('{\\\"white\\\":2}' AS JSON)),"
      "JSON_OBJECT('first',CAST('{\\\"black\\\":1}' AS JSON)))",
      generate_expression(
          Operator(
              "cont_in",
              Object{{"first", Scalar::Octets("{\"black\":1}",
                                              Expression_generator::CT_JSON)}},
              Object{
                  {"second", Scalar::Octets("{\"white\":2}",
                                            Expression_generator::CT_JSON)}}),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
}

TEST(xpl_expr_generator, cont_in_expression_operators) {
  Operator plus("+", 1, 2), minus("-", 2, 1);
  EXPECT_THROW(generate_expression(Operator("cont_in", plus, minus),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST((2 - 1) AS JSON),CAST((1 + 2) AS JSON))",
      generate_expression(Operator("cont_in", Operator("cast", plus, "JSON"),
                                   Operator("cast", minus, "JSON")),
                          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_THROW(generate_expression(
                   Operator("cont_in", plus, Operator("cast", minus, "JSON")),
                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
  EXPECT_THROW(generate_expression(
                   Operator("cont_in", Operator("cast", plus, "JSON"), minus),
                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
  EXPECT_THROW(
      generate_expression(Operator("cont_in", Operator("cast", plus, "SIGNED"),
                                   Operator("cast", minus, "JSON")),
                          EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
  EXPECT_THROW(
      generate_expression(Operator("cont_in", Operator("cast", plus, "JSON"),
                                   Operator("cast", minus, "SIGNED")),
                          EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, cont_in_expression_functions) {
  Function_call concat("concat", "foo", "bar");
  EXPECT_THROW(generate_expression(Operator("cont_in", concat, concat),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
  EXPECT_STREQ(
      "JSON_CONTAINS(json_quote(concat('foo','bar')),"
      "json_quote(concat('foo','bar')))",
      generate_expression(
          Operator("cont_in", Function_call("json_quote", concat),
                   Function_call("json_quote", concat)),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_THROW(
      generate_expression(
          Operator("cont_in", concat, Function_call("json_quote", concat)),
          EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
  EXPECT_THROW(
      generate_expression(
          Operator("cont_in", Function_call("json_quote", concat), concat),
          EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, cont_in_expression_placeholders) {
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST(2 AS JSON),CAST(1 AS JSON))",
      generate_expression(Operator("cont_in", Placeholder(0), Placeholder(1)),
                          Expression_list({1, 2}), EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_STREQ("JSON_CONTAINS(JSON_QUOTE('bar'),JSON_QUOTE('foo'))",
               generate_expression(
                   Operator("cont_in", Placeholder(0), Placeholder(1)),
                   Expression_list({"foo", "bar"}), EMPTY_SCHEMA, DM_TABLE)
                   .c_str());
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST('{\\\"white\\\":2}' AS JSON),"
      "CAST('{\\\"black\\\":1}' AS JSON))",
      generate_expression(
          Operator("cont_in", Placeholder(0), Placeholder(1)),
          Expression_list(
              {Scalar::Octets("{\"black\":1}", Expression_generator::CT_JSON),
               Scalar::Octets("{\"white\":2}", Expression_generator::CT_JSON)}),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());
  EXPECT_THROW(
      generate_expression(Operator("cont_in", Placeholder(0), Placeholder(1)),
                          EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

TEST(xpl_expr_generator, cont_in_expression_identifier) {
  EXPECT_STREQ(
      "JSON_CONTAINS(CAST(42 AS JSON),"
      "JSON_EXTRACT(`schema`.`table`.`field`,'$.member'))",
      generate_expression(
          Operator("cont_in",
                   Column_identifier(Document_path{"member"}, "field", "table",
                                     "schema"),
                   42),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());

  EXPECT_STREQ(
      "JSON_CONTAINS(JSON_EXTRACT(`schema`.`table`.`field`,'$.member'),"
      "CAST(42 AS JSON))",
      generate_expression(
          Operator("cont_in", 42,
                   Column_identifier(Document_path{"member"}, "field", "table",
                                     "schema")),
          EMPTY_SCHEMA, DM_TABLE)
          .c_str());

  EXPECT_THROW(generate_expression(
                   Operator("cont_in", 42,
                            Column_identifier("field", "table", "schema")),
                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, any_scalar) {
  EXPECT_STREQ("42",
               generate_expression(Any(42), EMPTY_SCHEMA, DM_TABLE).c_str());
}

TEST(xpl_expr_generator, any_object) {
  EXPECT_THROW(generate_expression(Any(Any::Object{{"name", Any(42)}}),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

TEST(xpl_expr_generator, any_array) {
  EXPECT_THROW(
      generate_expression(Any(Any::Array{"name", 42}), EMPTY_SCHEMA, DM_TABLE),
      Expression_generator::Error);
}

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

INSTANTIATE_TEST_CASE_P(xpl_expr_generator_function_call, Function_call_test,
                        testing::ValuesIn(function_call_param));

struct Param_placeholders {
  std::string expect;
  Expression_generator::Placeholder_id_list expect_ids;
  Expression_list args;
  Array expr;
};

class Placeholders_test : public testing::TestWithParam<Param_placeholders> {};

TEST_P(Placeholders_test, placeholders) {
  const Param_placeholders &param = GetParam();
  Query_string_builder qb;
  Expression_generator gen(&qb, param.args, EMPTY_SCHEMA, DM_TABLE);
  Expression_generator::Placeholder_id_list ids;
  gen.set_placeholder_id_list(&ids);
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

INSTANTIATE_TEST_CASE_P(xpl_expr_generator_placeholders, Placeholders_test,
                        testing::ValuesIn(placeholders_param));

}  // namespace test
}  // namespace xpl
