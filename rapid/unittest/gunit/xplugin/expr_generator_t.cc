/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "expr_generator.h"
#include "mysqlx_pb_wrapper.h"
#include <gtest/gtest.h>

namespace xpl
{

namespace test
{

const std::string EMPTY_SCHEMA;
const std::string EMPTY;
enum {
  DM_DOCUMENT = 0,
  DM_TABLE = 1
};


TEST(xpl_expr_generator, literal_uint)
{
  EXPECT_EQ("0", generate_expression(Scalar(static_cast<unsigned>(0)), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("1234567890",
            generate_expression(Scalar(static_cast<unsigned>(1234567890)), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_NE("-1234567890",
            generate_expression(Scalar(static_cast<unsigned>(-1234567890)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, literal_sint)
{
  EXPECT_EQ("0", generate_expression(Scalar(0), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("1234567890", generate_expression(Scalar(1234567890), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("-1234567890", generate_expression(Scalar(-1234567890), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, literal_null)
{
  EXPECT_EQ("NULL", generate_expression(Scalar(Scalar::Null()), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, literal_octets)
{
  EXPECT_EQ("'\\\"test1\\\" \t \\'test2\\''",
            generate_expression(Scalar("\"test1\" \t 'test2'"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, literal_string)
{
  EXPECT_EQ("'\\\"test1\\\" \t \\'test2\\''",
            generate_expression(Scalar(new Scalar::String("\"test1\" \t 'test2'")), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, literal_double)
{
  EXPECT_EQ("1234567890.123456",
            generate_expression(Scalar(1234567890.123456), EMPTY_SCHEMA, DM_TABLE).substr(0, 17));
  EXPECT_EQ("-1234567890.123456",
            generate_expression(Scalar(-1234567890.123456), EMPTY_SCHEMA, DM_TABLE).substr(0, 18));
}


TEST(xpl_expr_generator, literal_float)
{
  EXPECT_EQ("1234.12", generate_expression(Scalar(1234.123f), EMPTY_SCHEMA, DM_TABLE).substr(0, 8));
  EXPECT_EQ("-1234.12", generate_expression(Scalar(-1234.123f), EMPTY_SCHEMA, DM_TABLE).substr(0, 9));
}


TEST(xpl_expr_generator, literal_bool)
{
  EXPECT_EQ("TRUE", generate_expression(Scalar(true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("FALSE", generate_expression(Scalar(false), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, invalid_operator)
{
  EXPECT_THROW(generate_expression(Operator("some invalid operator"), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}


TEST(xpl_expr_generator, nullary_operators)
{
  EXPECT_EQ("*", generate_expression(Operator("*"), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("*", generate_expression(Operator("*"), EMPTY_SCHEMA, DM_DOCUMENT));
}


TEST(xpl_expr_generator, unary_operators)
{
  EXPECT_EQ("(NOT TRUE)", generate_expression(Operator("not", true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(+TRUE)", generate_expression(Operator("sign_plus", true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(-TRUE)", generate_expression(Operator("sign_minus", true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(!TRUE)", generate_expression(Operator("!", true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("!"), EMPTY_SCHEMA, DM_TABLE), std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("!", true, true), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}


TEST(xpl_expr_generator, binary_operators)
{
  EXPECT_EQ("(TRUE AND TRUE)", generate_expression(Operator("&&", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE OR TRUE)", generate_expression(Operator("||", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE XOR TRUE)",
            generate_expression(Operator("xor", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE = TRUE)", generate_expression(Operator("==", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE != TRUE)", generate_expression(Operator("!=", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE >= TRUE)", generate_expression(Operator(">=", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE > TRUE)", generate_expression(Operator(">", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE <= TRUE)", generate_expression(Operator("<=", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE < TRUE)", generate_expression(Operator("<", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE & TRUE)", generate_expression(Operator("&", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE | TRUE)", generate_expression(Operator("|", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE ^ TRUE)", generate_expression(Operator("^", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE << TRUE)", generate_expression(Operator("<<", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE >> TRUE)", generate_expression(Operator(">>", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE + TRUE)", generate_expression(Operator("+", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE - TRUE)", generate_expression(Operator("-", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE * TRUE)", generate_expression(Operator("*", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE / TRUE)", generate_expression(Operator("/", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(4 DIV 2)", generate_expression(Operator("div", 4, 2), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE % TRUE)", generate_expression(Operator("%", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE IS TRUE)", generate_expression(Operator("is", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE IS NOT TRUE)",
            generate_expression(Operator("is_not", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE REGEXP TRUE)",
            generate_expression(Operator("regexp", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE NOT REGEXP TRUE)",
            generate_expression(Operator("not_regexp", true, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("+"), EMPTY_SCHEMA, DM_TABLE), std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("+", true), EMPTY_SCHEMA, DM_TABLE), std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("+", true, true, true), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}


TEST(xpl_expr_generator, identifier)
{
  EXPECT_EQ("` schema \"'`.` table \"'`", generate_expression(Identifier(" table \"'"), " schema \"'", true));
  EXPECT_EQ("` schema \"'`.` table \"'`",
            generate_expression(Identifier(" table \"'", " schema \"'"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, variable)
{
  //EXPECT_EQ("@`'variable``\"`", generate_expression(Expr(Variable("'variable`\""))));
  EXPECT_THROW(generate_expression(Expr(Variable("'variable`\"")),
                                   EMPTY_SCHEMA, DM_TABLE), Expression_generator::Error);
}


TEST(xpl_expr_generator, column_identifier)
{
  Document_path::Path doc_path("docpath \"'");

  EXPECT_EQ(
    "`column ``\"'`",
    generate_expression(ColumnIdentifier("column `\"'"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "`table ``\"'`.`column ``\"'`",
    generate_expression(ColumnIdentifier("column `\"'", "table `\"'"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "`schema ``\"'`.`table ``\"'`.`column ``\"'`",
    generate_expression(
      ColumnIdentifier("column `\"'", "table `\"'", "schema `\"'"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "JSON_EXTRACT(doc,'$.\\\"docpath \\\\\\\"\\'\\\"')",
    generate_expression(
      ColumnIdentifier(EMPTY, EMPTY, EMPTY, &doc_path), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "JSON_EXTRACT(`\"'`` column`,'$.\\\"docpath \\\\\\\"\\'\\\"')",
    generate_expression(
      ColumnIdentifier("\"'` column", EMPTY, EMPTY, &doc_path), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "JSON_EXTRACT(`\"'`` table`.`\"'`` column`,'$.\\\"docpath \\\\\\\"\\'\\\"')",
    generate_expression(
      ColumnIdentifier("\"'` column", "\"'` table", EMPTY, &doc_path), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "JSON_EXTRACT(`\"'`` schema`.`\"'`` table`."
                "`\"'`` column`,'$.\\\"docpath \\\\\\\"\\'\\\"')",
    generate_expression(
      ColumnIdentifier(
        "\"'` column", "\"'` table", "\"'` schema", &doc_path
      ), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_THROW(
    generate_expression(ColumnIdentifier(EMPTY, "table"), EMPTY_SCHEMA, DM_TABLE),
    std::invalid_argument
  );
  EXPECT_THROW(
    generate_expression(ColumnIdentifier("column", EMPTY, "schema"), EMPTY_SCHEMA, DM_TABLE),
    std::invalid_argument
  );
}


TEST(xpl_expr_generator, column_identifier_doc_id)
{
  Document_path::Path path("_id");
  ColumnIdentifier ident(path);
  ASSERT_EQ("JSON_EXTRACT(doc,'$._id')", generate_expression(ident, EMPTY_SCHEMA, DM_TABLE));
  ASSERT_EQ("JSON_EXTRACT(doc,'$._id')", generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT));
}


TEST(xpl_expr_generator, column_identifier_doc_id_names)
{
  Document_path::Path path("_id");
  ColumnIdentifier ident(path, "field", "table", "schema");
  ASSERT_EQ("JSON_EXTRACT(`schema`.`table`.`field`,'$._id')", generate_expression(ident, EMPTY_SCHEMA, DM_TABLE));
  ASSERT_EQ("JSON_EXTRACT(`schema`.`table`.`field`,'$._id')", generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT));
}


TEST(xpl_expr_generator, column_identifier_no_column)
{
  ColumnIdentifier ident(EMPTY, "table");
  ASSERT_THROW(generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT),
               Expression_generator::Error);

  ASSERT_THROW(generate_expression(ident, EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);

  Document_path::Path path("member");
  ColumnIdentifier ident2(EMPTY, "table", EMPTY, &path);
  ASSERT_EQ("JSON_EXTRACT(`table`.doc,'$.member')",
            generate_expression(ident2, EMPTY_SCHEMA, DM_DOCUMENT));
}


TEST(xpl_expr_generator, function_call)
{
  EXPECT_EQ("schema.func()",
            generate_expression(FunctionCall("func"), "schema", DM_TABLE));
  EXPECT_EQ("schema.func(FALSE,5)",
            generate_expression(FunctionCall("func", false, 5), "schema", DM_TABLE));
  EXPECT_EQ("concat(FALSE,5)",
            generate_expression(FunctionCall("concat", false, 5), "schema", DM_TABLE));
  EXPECT_EQ("CONCAT(FALSE,5)",
            generate_expression(FunctionCall("CONCAT", false, 5), "schema", DM_TABLE));
}


TEST(xpl_expr_generator, interval_expression)
{
  EXPECT_EQ(
    "DATE_ADD(FALSE, INTERVAL TRUE MICROSECOND)",
    generate_expression(Operator("date_add", false, true, "MICROSECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE SECOND)",
    generate_expression(Operator("date_sub", false, true, "SECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE MINUTE)",
    generate_expression(Operator("date_sub", false, true, "MINUTE"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE HOUR)",
    generate_expression(Operator("date_sub", false, true, "HOUR"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE DAY)",
    generate_expression(Operator("date_sub", false, true, "DAY"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE WEEK)",
    generate_expression(Operator("date_sub", false, true, "WEEK"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE MONTH)",
    generate_expression(Operator("date_sub", false, true, "MONTH"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE QUARTER)",
    generate_expression(Operator("date_sub", false, true, "QUARTER"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE YEAR)",
    generate_expression(Operator("date_sub", false, true, "YEAR"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE SECOND_MICROSECOND)",
    generate_expression(Operator("date_sub", false, true, "SECOND_MICROSECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE MINUTE_MICROSECOND)",
    generate_expression(Operator("date_sub", false, true, "MINUTE_MICROSECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE MINUTE_SECOND)",
    generate_expression(Operator("date_sub", false, true, "MINUTE_SECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE HOUR_MICROSECOND)",
    generate_expression(Operator("date_sub", false, true, "HOUR_MICROSECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE HOUR_SECOND)",
    generate_expression(Operator("date_sub", false, true, "HOUR_SECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE HOUR_MINUTE)",
    generate_expression(Operator("date_sub", false, true, "HOUR_MINUTE"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE DAY_MICROSECOND)",
    generate_expression(Operator("date_sub", false, true, "DAY_MICROSECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE DAY_SECOND)",
    generate_expression(Operator("date_sub", false, true, "DAY_SECOND"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE DAY_MINUTE)",
    generate_expression(Operator("date_sub", false, true, "DAY_MINUTE"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE DAY_HOUR)",
    generate_expression(Operator("date_sub", false, true, "DAY_HOUR"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "DATE_SUB(FALSE, INTERVAL TRUE YEAR_MONTH)",
    generate_expression(Operator("date_sub", false, true, "YEAR_MONTH"), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_THROW(
    generate_expression(Operator("date_sub", false, true, "invalid unit"), EMPTY_SCHEMA, DM_TABLE),
    std::invalid_argument
  );
  EXPECT_THROW(
    generate_expression(Operator("date_sub", false, true, true, true), EMPTY_SCHEMA, DM_TABLE),
    std::invalid_argument
  );
}


TEST(xpl_expr_generator, in_expression)
{
  EXPECT_EQ("(FALSE IN (TRUE))",
            generate_expression(Operator("in", false, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(FALSE NOT IN (TRUE))",
            generate_expression(Operator("not_in", false, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(FALSE IN (TRUE,FALSE))",
            generate_expression(Operator("in", false, true, false), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(FALSE NOT IN (TRUE,FALSE))",
            generate_expression(Operator("not_in", false, true, false), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("in", false), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}


TEST(xpl_expr_generator, between_expression)
{
  EXPECT_EQ("(2 BETWEEN 1 AND 3)",
            generate_expression(Operator("between", 2, 1, 3), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(2 NOT BETWEEN 1 AND 3)",
            generate_expression(Operator("not_between", 2, 1, 3), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("between", 0, 0), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("between", 0, 0, 0, 0), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}


TEST(xpl_expr_generator, like_expression)
{
  EXPECT_EQ("(TRUE LIKE FALSE)",
            generate_expression(Operator("like", true, false), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE NOT LIKE FALSE)",
            generate_expression(Operator("not_like", true, false), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("(TRUE LIKE FALSE ESCAPE TRUE)",
            generate_expression(Operator("like", true, false, true), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("like", true), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
  EXPECT_THROW(generate_expression(Operator("like", true, true, true, true), EMPTY_SCHEMA, DM_TABLE),
               std::invalid_argument);
}


TEST(xpl_expr_generator, complex_expressions)
{
  EXPECT_EQ(
    "(`schema`.`ident``` NOT LIKE 'string\\'' ESCAPE 'x')",
    generate_expression(Expr(Operator("not_like",
                                      ColumnIdentifier("ident`", "schema"), "string'", "x")),
                        EMPTY_SCHEMA, DM_TABLE));

  EXPECT_EQ(
    "((1 * 2) % (3 / 4))",
    generate_expression(Expr(Operator("%",
                                      Operator("*", 1, 2),
                                      Operator("/", 3, 4))),
                        EMPTY_SCHEMA, DM_TABLE));

  EXPECT_EQ(
    "(`schema`.func(5,FALSE) IN (1,(+2),(-(7 - 0))))",
    generate_expression(Expr(Operator("in",
                                      FunctionCall(Identifier("func", "schema"), 5, false),
                                      1,
                                      Operator("sign_plus", 2),
                                      Operator("sign_minus", Operator("-", 7, 0)))),
                        EMPTY_SCHEMA, DM_TABLE)
  );
}


TEST(xpl_expr_generator, document_path_root)
{
  EXPECT_EQ("'$'",
            generate_expression(Document_path(Document_path::Path(EMPTY)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_empty_member)
{
  Document_path::Path path;
  EXPECT_THROW(generate_expression(Document_path(path.add_member(EMPTY).add_member("name")),
                                   EMPTY_SCHEMA, DM_TABLE),
               xpl::Expression_generator::Error);
}


TEST(xpl_expr_generator, document_path_empty_member_opposite)
{
  Document_path::Path path;
  EXPECT_THROW(generate_expression(Document_path(path.add_member("name").add_member(EMPTY)),
                                   EMPTY_SCHEMA, DM_TABLE),
               xpl::Expression_generator::Error);
}


TEST(xpl_expr_generator, document_path_array)
{
  Document_path::Path path;
  EXPECT_EQ("'$.name[42]'",
            generate_expression(Document_path(path.add_member("name").add_index(42)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_root_array)
{
  Document_path::Path path;
  EXPECT_EQ("'$[42]'", generate_expression(Document_path(path.add_index(42)),
                                           EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_member_asterisk)
{
  Document_path::Path path;
  EXPECT_EQ("'$.name.*'",
            generate_expression(Document_path(path.add_member("name").add_asterisk()),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_root_asterisk)
{
  Document_path::Path path;
  EXPECT_EQ("'$.*'",
            generate_expression(Document_path(path.add_asterisk()),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_double_asterisk)
{
  Document_path::Path path;
  path.add_member("name").add_double_asterisk();
  EXPECT_EQ("'$.name**'",
            generate_expression(Document_path(path),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_root_double_asterisk)
{
  Document_path::Path path;
  path.add_double_asterisk();

  EXPECT_EQ("'$**'",
            generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_not_found)
{
  EXPECT_THROW(generate_expression(Expr(Placeholder(10)),
                                   Expression_args(), EMPTY_SCHEMA, DM_TABLE),
               xpl::Expression_generator::Error);
}


TEST(xpl_expr_generator, placeholder_found)
{
  EXPECT_EQ("2", generate_expression(Expr(Placeholder(0)),
                                     Expression_args(2), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_opearator_one_arg)
{
  EXPECT_EQ("(1 + 2)",
            generate_expression(Operator("+", 1, Placeholder(0)),
                                Expression_args(2), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_opearator_two_args)
{
  EXPECT_EQ("(1 + 2)",
            generate_expression(Operator("+",
                                         Placeholder(1),
                                         Placeholder(0)),
                                Expression_args(2)(1), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_function)
{
  EXPECT_EQ("xschema.bar(42,'foo')",
            generate_expression(FunctionCall("bar",
                                             Placeholder(0),
                                             Placeholder(1)),
                                Expression_args(42)("foo"), "xschema", true));
}


TEST(xpl_expr_generator, placeholder_function_and_operator)
{
  EXPECT_EQ("(xschema.bar(42,'foo') > 42)",
            generate_expression(Operator(">",
                                         FunctionCall("bar",
                                                      Placeholder(0),
                                                      Placeholder(1)),
                                         Placeholder(0)),
                                Expression_args(42)("foo"), "xschema", true));
}


TEST(xpl_expr_generator, placeholder_operator_null)
{
  EXPECT_EQ("(`bar` IS NOT NULL)",
            generate_expression(Operator("is_not",
                                         ColumnIdentifier("bar"),
                                         Placeholder(0)),
                                Expression_args(Scalar::Null()), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_missing_param)
{
  EXPECT_THROW(generate_expression(Operator("cast", 42), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, cast_empty_type)
{
  EXPECT_THROW(generate_expression(Operator("cast", 42), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, cast_invalid_target_type)
{
  EXPECT_THROW(generate_expression(Operator("cast", 42, 44), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, cast_scalar_to_undefinied)
{
  EXPECT_THROW(generate_expression(Operator("cast", 42, "UNDEFINIED"), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, cast_expr_to_json)
{
  EXPECT_EQ("CAST(`foo`.`bar` AS JSON)",
            generate_expression(Operator("cast",
                                         ColumnIdentifier("bar", "foo"),
                                         "JSON"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_signed)
{
  EXPECT_EQ("CAST(42 AS SIGNED)",
            generate_expression(Operator("cast", 42, "SIGNED"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_signed_integer)
{
  EXPECT_EQ("CAST(42 AS SIGNED INTEGER)",
            generate_expression(Operator("cast", 42, "SIGNED INTEGER"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_unsigned)
{
  EXPECT_EQ("CAST(42 AS UNSIGNED)",
            generate_expression(Operator("cast", 42, "UNSIGNED"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_unsigned_integer)
{
  EXPECT_EQ("CAST(42 AS UNSIGNED INTEGER)",
            generate_expression(Operator("cast", 42, "UNSIGNED INTEGER"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_char)
{
  EXPECT_EQ("CAST('one' AS CHAR)",
            generate_expression(Operator("cast", "one", "CHAR"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_multichar)
{
  EXPECT_EQ("CAST('one' AS CHAR(42))",
            generate_expression(Operator("cast", "one", "CHAR(42)"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_binary)
{
  EXPECT_EQ("CAST('one' AS BINARY)",
            generate_expression(Operator("cast", "one", "BINARY"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_precision_binary)
{
  EXPECT_EQ("CAST('one' AS BINARY(44))",
            generate_expression(Operator("cast", "one", "BINARY(44)"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_decimal)
{
  EXPECT_EQ("CAST(3.141593 AS DECIMAL)",
            generate_expression(Operator("cast", 3.141593, "DECIMAL"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_precision_decimal)
{
  EXPECT_EQ("CAST(3.141593 AS DECIMAL(4))",
            generate_expression(Operator("cast", 3.141593, "DECIMAL(4)"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_precision_scale_decimal)
{
  EXPECT_EQ("CAST(3.141593 AS DECIMAL(4,2))",
            generate_expression(Operator("cast", 3.141593, "DECIMAL(4,2)"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_time)
{
  EXPECT_EQ("CAST('3:14' AS TIME)",
            generate_expression(Operator("cast", "3:14", "TIME"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_date)
{
  EXPECT_EQ("CAST('2015.08.10' AS DATE)",
            generate_expression(Operator("cast", "2015.08.10", "DATE"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, cast_scalar_to_datetime)
{
  EXPECT_EQ("CAST('2015.08.10T3:14' AS DATETIME)",
            generate_expression(Operator("cast", "2015.08.10T3:14", "DATETIME"), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_empty)
{
  EXPECT_EQ("JSON_OBJECT()",
            generate_expression(Object(), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_empty_key)
{
  EXPECT_THROW(generate_expression(Object(Object::Values("", 1)),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, object_empty_value)
{
  EXPECT_THROW(generate_expression(Object("first", NULL),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, object_one_scalar)
{
  EXPECT_EQ("JSON_OBJECT('first',1)",
            generate_expression(Object(Object::Values("first", 1)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_two_scalars)
{
  EXPECT_EQ("JSON_OBJECT('first',1,'second','two')",
            generate_expression(Object(Object::Values("first",1)("second","two")),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_object)
{
  EXPECT_EQ("JSON_OBJECT('second',JSON_OBJECT('first',1))",
            generate_expression(Object(Object::Values("second", Object::Values("first", 1))),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_as_expr)
{
  EXPECT_EQ("JSON_OBJECT('first',1)",
            generate_expression(Expr(Object(Object::Values("first",1))), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_operator)
{
  EXPECT_EQ("JSON_OBJECT('sum',(1 + 2))",
            generate_expression(Object(Object::Values("sum", Operator("+", 1, 2))),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_function)
{
  EXPECT_EQ("JSON_OBJECT('result',foo('bar'))",
            generate_expression(Object(Object::Values("result", FunctionCall("foo", "bar"))),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_array)
{
  Expr va[] = {1, 2};
  EXPECT_EQ("JSON_OBJECT('tab',JSON_ARRAY(1,2))",
            generate_expression(Object(Object::Values("tab", Array(va))),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_in_function)
{
  EXPECT_EQ("foo(JSON_OBJECT('first',1))",
            generate_expression(Expr(FunctionCall("foo",
                                                  Object(Object::Values("first", 1)))),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_real_example)
{
  Document_path::Path path1("first_name"), path2("last_name");
  EXPECT_EQ(
      "JSON_OBJECT('name',concat("
      "JSON_UNQUOTE(JSON_EXTRACT(doc,'$.first_name')),' ',"
      "JSON_UNQUOTE(JSON_EXTRACT(doc,'$.last_name'))),'number',(1 + 1))",
      generate_expression(Object(Object::Values("name", FunctionCall("concat",
                                                                     ColumnIdentifier(path1),
                                                                     " ",
                                                                     ColumnIdentifier(path2)))
                                 ("number", Operator("+", 1, 1))),
                          EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_empty)
{
  EXPECT_EQ("JSON_ARRAY()",
            generate_expression(Array(), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_one_scalar)
{
  Expr v[] = {1};

  EXPECT_EQ("JSON_ARRAY(1)",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_two_scalars)
{
  Expr v[] = {1, "two"};

  EXPECT_EQ("JSON_ARRAY(1,'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_as_expr)
{
  Expr v[] = {1};

  EXPECT_EQ("JSON_ARRAY(1)",
            generate_expression(Expr(Array(v)), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_array)
{
  Expr v1[] = {1, 2};
  Expr v[] = {"one", Array(v1)};

  EXPECT_EQ("JSON_ARRAY('one',JSON_ARRAY(1,2))",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_object)
{
  Expr v[] = {Object(Object::Values("first", 1)), "two"};

  EXPECT_EQ("JSON_ARRAY(JSON_OBJECT('first',1),'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_operator)
{
  Expr v[] = {Operator("+", 1, 2), "two"};

  EXPECT_EQ("JSON_ARRAY((1 + 2),'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_function)
{
  Expr v[] = {FunctionCall("foo", "bar"), "two"};

  EXPECT_EQ("JSON_ARRAY(foo('bar'),'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_in_function)
{
  Expr v[] = {"foo", "bar"};

  EXPECT_EQ("fun(JSON_ARRAY('foo','bar'))",
            generate_expression(FunctionCall("fun", Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_in_operator)
{
  Expr v[] = {1, 2};
  EXPECT_EQ("JSON_CONTAINS(JSON_ARRAY(1,2),CAST(1 AS JSON))",
            generate_expression(Operator("in", 1, Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_not_in_operator)
{
  Expr v[] = {1, 2};
  EXPECT_EQ("NOT JSON_CONTAINS(JSON_ARRAY(1,2),CAST(1 AS JSON))",
            generate_expression(Operator("not_in", 1, Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_in_operator_string)
{
  Expr v[] = {"foo", "bar"};
  EXPECT_EQ("JSON_CONTAINS(JSON_ARRAY('foo','bar'),JSON_QUOTE('foo'))",
            generate_expression(Operator("in", "foo", Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_not_in_operator_string)
{
  Expr v[] = {"foo", "bar"};
  EXPECT_EQ("NOT JSON_CONTAINS(JSON_ARRAY('foo','bar'),JSON_QUOTE('foo'))",
            generate_expression(Operator("not_in", "foo", Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, default_operator)
{
  EXPECT_EQ("DEFAULT",
            generate_expression(Operator("default"), EMPTY_SCHEMA, DM_TABLE));
  EXPECT_THROW(generate_expression(Operator("default", 42), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, scalar_octets_plain)
{
  EXPECT_EQ("'ABC'", generate_expression(Scalar(Scalar::Octets("ABC", Expression_generator::CT_PLAIN)),
                                         EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_geometry)
{
  EXPECT_EQ("ST_GEOMETRYFROMWKB('010')",
            generate_expression(Scalar(Scalar::Octets("010", Expression_generator::CT_GEOMETRY)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_json)
{
  EXPECT_EQ("CAST('{\\\"a\\\":42}' AS JSON)",
            generate_expression(Scalar(Scalar::Octets("{\"a\":42}", Expression_generator::CT_JSON)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_xml)
{
  EXPECT_EQ("'<a>bbb</a>'",
            generate_expression(Scalar(Scalar::Octets("<a>bbb</a>", Expression_generator::CT_XML)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_unknown)
{
  EXPECT_THROW(generate_expression(Scalar(Scalar::Octets("foo", 666)),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

} // namespace test
} // namespace xpl
