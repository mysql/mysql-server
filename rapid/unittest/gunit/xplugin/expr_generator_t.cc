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
#include "ngs_common/protocol_protobuf.h"
#include "mysqlx_pb_wrapper.h"
#include <gtest/gtest.h>
#include <boost/scoped_ptr.hpp>

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


TEST(xpl_expr_generator, zeroary_operators)
{
  EXPECT_EQ("*", generate_expression(Operator("*"), EMPTY_SCHEMA, DM_TABLE));
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
  boost::scoped_ptr<Expr> var(Expr::make_variable("'variable`\""));

  //EXPECT_EQ("@`'variable``\"`", generate_expression(*var));
  EXPECT_THROW(generate_expression(*var, EMPTY_SCHEMA, DM_TABLE), Expression_generator::Error);
}


TEST(xpl_expr_generator, column_identifier)
{
  Document_path::Path doc_path;

  doc_path.push_back(
    std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "docpath \"'")
  );

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
    generate_expression(ColumnIdentifier(EMPTY, "table"), EMPTY_SCHEMA, DM_TABLE), std::invalid_argument
  );
  EXPECT_THROW(
    generate_expression(ColumnIdentifier("column", EMPTY, "schema"), EMPTY_SCHEMA, DM_TABLE),
    std::invalid_argument
  );
}


TEST(xpl_expr_generator, column_identifier_doc_id)
{
  Document_path::Path path;
  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "_id"));
  ColumnIdentifier ident(EMPTY, EMPTY, EMPTY, &path);
  EXPECT_EQ("JSON_EXTRACT(doc,'$._id')", generate_expression(ident, EMPTY_SCHEMA, DM_TABLE));
  EXPECT_EQ("`_id`", generate_expression(ident, EMPTY_SCHEMA, DM_DOCUMENT));
}


TEST(xpl_expr_generator, function_call)
{
  EXPECT_EQ("schema.func()",
            generate_expression(FunctionCall(new Identifier("func")), "schema", DM_TABLE));
  EXPECT_EQ(
    "schema.func(FALSE,5)",
    generate_expression(FunctionCall(new Identifier("func"), false, 5), "schema", DM_TABLE)
  );
  EXPECT_EQ(
    "concat(FALSE,5)",
    generate_expression(FunctionCall(new Identifier("concat"), false, 5), "schema", DM_TABLE)
  );
  EXPECT_EQ(
    "CONCAT(FALSE,5)",
    generate_expression(FunctionCall(new Identifier("CONCAT"), false, 5), "schema", DM_TABLE)
  );
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
    generate_expression(
      Expr(
        new Operator(
          "not_like",
          new ColumnIdentifier("ident`", "schema"), "string'", "x"
        )
      ), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "((1 * 2) % (3 / 4))",
    generate_expression(
      Expr(
        new Operator(
          "%",
          new Operator("*", 1, 2),
          new Operator("/", 3, 4)
        )
      ), EMPTY_SCHEMA, DM_TABLE)
  );
  EXPECT_EQ(
    "(`schema`.func(5,FALSE) IN (1,(+2),(-(7 - 0))))",
    generate_expression(
      Expr(
        new Operator(
          "in",
          new FunctionCall(
            new Identifier("func", "schema"), 5, false
          ),
          1,
          new Operator("sign_plus", 2),
          new Operator(
            "sign_minus",
            new Operator("-", 7, 0)
          )
        )
      ), EMPTY_SCHEMA, DM_TABLE)
  );
}


TEST(xpl_expr_generator, document_path_root)
{
  Document_path::Path path;

  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, EMPTY));

  EXPECT_EQ("'$.'", generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_empty_member)
{
  Document_path::Path path;

  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, EMPTY));
  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "name"));

  EXPECT_THROW(generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE), xpl::Expression_generator::Error);
}


TEST(xpl_expr_generator, document_path_empty_member_opposite)
{
  Document_path::Path path;

  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "name"));
  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, EMPTY));

  EXPECT_THROW(generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE), xpl::Expression_generator::Error);
}


TEST(xpl_expr_generator, document_path_array)
{
  Document_path::Path path;

  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "name"));
  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX, "42"));

  EXPECT_EQ("'$.name[42]'", generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_member_asterisk)
{
  Document_path::Path path;

  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "name"));
  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER_ASTERISK, EMPTY));

  EXPECT_EQ("'$.name.*'", generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, document_path_double_asterisk)
{
  Document_path::Path path;

  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "name"));
  path.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::DOUBLE_ASTERISK, EMPTY));

  EXPECT_EQ("'$.name**'", generate_expression(Document_path(path), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_not_found)
{
  boost::scoped_ptr<Expr> expr(Expr::make_placeholder(10));
  Expression_generator::Args args;
  EXPECT_THROW(generate_expression(*expr, args, EMPTY_SCHEMA, DM_TABLE), xpl::Expression_generator::Error);
}


TEST(xpl_expr_generator, placeholder_found)
{
  boost::scoped_ptr<Expr> expr(Expr::make_placeholder(0));
  Expression_generator::Args args;
  args.AddAllocated(new Scalar(2));
  EXPECT_EQ("2", generate_expression(*expr, args, EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_opearator_one_arg)
{
  Expression_generator::Args args;
  args.AddAllocated(new Scalar(2));
  EXPECT_EQ("(1 + 2)",
            generate_expression(Operator("+", 1,
                                         Expr::make_placeholder(0)),
                                args, EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_opearator_two_args)
{
  Expression_generator::Args args;
  args.AddAllocated(new Scalar(2));
  args.AddAllocated(new Scalar(1));
  EXPECT_EQ("(1 + 2)",
            generate_expression(Operator("+",
                                         Expr::make_placeholder(1),
                                         Expr::make_placeholder(0)),
                                args, EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, placeholder_function)
{
  Expression_generator::Args args;
  args.AddAllocated(new Scalar(42));
  args.AddAllocated(new Scalar("foo"));
  EXPECT_EQ("xschema.bar(42,'foo')",
            generate_expression(FunctionCall(new Identifier("bar"),
                                             Expr::make_placeholder(0),
                                             Expr::make_placeholder(1)),
                                args, "xschema", true));
}


TEST(xpl_expr_generator, placeholder_function_and_operator)
{
  Expression_generator::Args args;
  args.AddAllocated(new Scalar(42));
  args.AddAllocated(new Scalar("foo"));
  EXPECT_EQ("(xschema.bar(42,'foo') > 42)",
            generate_expression(Operator(">",
                                         new FunctionCall(new Identifier("bar"),
                                                          Expr::make_placeholder(0),
                                                          Expr::make_placeholder(1)),
                                         Expr::make_placeholder(0)),
                                args, "xschema", true));
}


TEST(xpl_expr_generator, placeholder_operator_null)
{
  Expression_generator::Args args;
  args.AddAllocated(new Scalar(Scalar::Null()));
  EXPECT_EQ("(`bar` IS NOT NULL)",
            generate_expression(Operator("is_not",
                                         new ColumnIdentifier("bar"),
                                         Expr::make_placeholder(0)),
                                args, EMPTY_SCHEMA, DM_TABLE));
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
                                         new ColumnIdentifier("bar", "foo"),
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
  Object::Values v;
  v[""] = new Expr(1);

  EXPECT_THROW(generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, object_empty_value)
{
  Object::Values v;
  v["first"] = 0;

  EXPECT_THROW(generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}


TEST(xpl_expr_generator, object_one_scalar)
{
  Object::Values v;
  v["first"] = new Expr(1);

  EXPECT_EQ("JSON_OBJECT('first',1)",
            generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_two_scalars)
{
  Object::Values v;
  v["first"] = new Expr(1);
  v["second"] = new Expr("two");

  EXPECT_EQ("JSON_OBJECT('first',1,'second','two')",
            generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_object)
{
  Object::Values v1, v2;
  v1["first"] = new Expr(1);
  v2["second"] = new Expr(new Object(v1));

  EXPECT_EQ("JSON_OBJECT('second',JSON_OBJECT('first',1))",
            generate_expression(Object(v2), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_as_expr)
{
  Object::Values v;
  v["first"] = new Expr(1);

  EXPECT_EQ("JSON_OBJECT('first',1)",
            generate_expression(Expr(new Object(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_operator)
{
  Object::Values v;
  v["sum"] = new Expr(new Operator("+", 1, 2));

  EXPECT_EQ("JSON_OBJECT('sum',(1 + 2))",
            generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_function)
{
  Object::Values v;
  v["result"] = new Expr(new FunctionCall("foo", "bar"));

  EXPECT_EQ("JSON_OBJECT('result',foo('bar'))",
            generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_array)
{
  Expr* va[] = {new Expr(1), new Expr(2)};

  Object::Values v;
  v["tab"] = new Expr(new Array(va));

  EXPECT_EQ("JSON_OBJECT('tab',JSON_ARRAY(1,2))",
            generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, object_in_function)
{
  Object::Values v;
  v["first"] = new Expr(1);

  EXPECT_EQ("foo(JSON_OBJECT('first',1))",
            generate_expression(Expr(new FunctionCall("foo",
                                                      new Object(v))), EMPTY_SCHEMA, DM_TABLE));
}



TEST(xpl_expr_generator, object_real_example)
{
  Document_path::Path path1, path2;
  path1.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "first_name"));
  path2.push_back(std::make_pair(Mysqlx::Expr::DocumentPathItem::MEMBER, "last_name"));

  Object::Values v;
  v["name"] = new Expr(new FunctionCall("concat",
                                        new ColumnIdentifier(EMPTY, EMPTY, EMPTY, &path1),
                                        " ",
                                        new ColumnIdentifier(EMPTY, EMPTY, EMPTY, &path2)
                                        ));
  v["number"] = new Expr(new Operator("+", 1, 1));

  EXPECT_EQ(
      "JSON_OBJECT('name',concat("
      "JSON_UNQUOTE(JSON_EXTRACT(doc,'$.first_name')),' ',"
      "JSON_UNQUOTE(JSON_EXTRACT(doc,'$.last_name'))),'number',(1 + 1))",
      generate_expression(Object(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_empty)
{
  EXPECT_EQ("JSON_ARRAY()",
            generate_expression(Array(), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_one_scalar)
{
  Expr* v[] = {new Expr(1)};

  EXPECT_EQ("JSON_ARRAY(1)",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_two_scalars)
{
  Expr* v[] = {new Expr(1), new Expr("two")};

  EXPECT_EQ("JSON_ARRAY(1,'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_as_expr)
{
  Expr* v[] = {new Expr(1)};

  EXPECT_EQ("JSON_ARRAY(1)",
            generate_expression(Expr(new Array(v)), EMPTY_SCHEMA, DM_TABLE));
}

TEST(xpl_expr_generator, array_array)
{
  Expr* v1[] = {new Expr(1),new Expr(2)};
  Expr* v[] = {new Expr("one"), new Expr(new Array(v1))};

  EXPECT_EQ("JSON_ARRAY('one',JSON_ARRAY(1,2))",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_object)
{
  Object::Values vo;
  vo["first"] = new Expr(1);
  Expr* v[] = {new Expr(new Object(vo)), new Expr("two")};

  EXPECT_EQ("JSON_ARRAY(JSON_OBJECT('first',1),'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_operator)
{
  Expr* v[] = {new Expr(new Operator("+", 1, 2)), new Expr("two")};

  EXPECT_EQ("JSON_ARRAY((1 + 2),'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_function)
{
  Expr* v[] = {new Expr(new FunctionCall("foo", "bar")), new Expr("two")};

  EXPECT_EQ("JSON_ARRAY(foo('bar'),'two')",
            generate_expression(Array(v), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_in_function)
{
  Expr* v[] = {new Expr("foo"), new Expr("bar")};

  EXPECT_EQ("fun(JSON_ARRAY('foo','bar'))",
            generate_expression(FunctionCall("fun", new Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_in_operator)
{
  Expr* v[] = {new Expr(1), new Expr(2)};
  EXPECT_EQ("JSON_CONTAINS(JSON_ARRAY(1,2),CAST(1 AS JSON))",
            generate_expression(Operator("in", 1, new Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_not_in_operator)
{
  Expr* v[] = {new Expr(1), new Expr(2)};
  EXPECT_EQ("NOT JSON_CONTAINS(JSON_ARRAY(1,2),CAST(1 AS JSON))",
            generate_expression(Operator("not_in", 1, new Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_in_operator_string)
{
  Expr* v[] = {new Expr("foo"), new Expr("bar")};
  EXPECT_EQ("JSON_CONTAINS(JSON_ARRAY('foo','bar'),JSON_QUOTE('foo'))",
            generate_expression(Operator("in", "foo", new Array(v)), EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, array_not_in_operator_string)
{
  Expr* v[] = {new Expr("foo"), new Expr("bar")};
  EXPECT_EQ("NOT JSON_CONTAINS(JSON_ARRAY('foo','bar'),JSON_QUOTE('foo'))",
            generate_expression(Operator("not_in", "foo", new Array(v)), EMPTY_SCHEMA, DM_TABLE));
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
  EXPECT_EQ("'ABC'", generate_expression(Scalar(new Scalar::Octets("ABC", Expression_generator::CT_PLAIN)),
                                         EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_geometry)
{
  EXPECT_EQ("ST_GEOMETRYFROMWKB('010')",
            generate_expression(Scalar(new Scalar::Octets("010", Expression_generator::CT_GEOMETRY)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_json)
{
  EXPECT_EQ("CAST('{\\\"a\\\":42}' AS JSON)",
            generate_expression(Scalar(new Scalar::Octets("{\"a\":42}", Expression_generator::CT_JSON)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_xml)
{
  EXPECT_EQ("'<a>bbb</a>'",
            generate_expression(Scalar(new Scalar::Octets("<a>bbb</a>", Expression_generator::CT_XML)),
                                EMPTY_SCHEMA, DM_TABLE));
}


TEST(xpl_expr_generator, scalar_octets_unknown)
{
  EXPECT_THROW(generate_expression(Scalar(new Scalar::Octets("foo", 666)),
                                   EMPTY_SCHEMA, DM_TABLE),
               Expression_generator::Error);
}

} // namespace test
} // namespace xpl
