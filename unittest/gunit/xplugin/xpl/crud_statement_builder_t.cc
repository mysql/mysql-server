/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/statement_builder.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

class Crud_statement_builder_stub : public Crud_statement_builder {
 public:
  explicit Crud_statement_builder_stub(Expression_generator *gen)
      : Crud_statement_builder(*gen) {}

  using Crud_statement_builder::add_collection;
  using Crud_statement_builder::add_filter;
  using Crud_statement_builder::add_limit;
  using Crud_statement_builder::add_order;
};

class Crud_statement_builder_test : public ::testing::Test {
 public:
  Crud_statement_builder_stub &builder(
      const bool is_table_data_model = DM_TABLE) {
    expr_gen.reset(
        new Expression_generator(&query, args, schema, is_table_data_model));
    stub.reset(new Crud_statement_builder_stub(expr_gen.get()));
    return *stub;
  }

  Expression_generator::Args args;
  Query_string_builder query;
  std::string schema;
  std::unique_ptr<Expression_generator> expr_gen;
  std::unique_ptr<Crud_statement_builder_stub> stub;

  enum { DM_DOCUMENT = 0, DM_TABLE = 1 };
};

TEST_F(Crud_statement_builder_test, add_table_only_name) {
  ASSERT_NO_THROW(builder().add_collection(Collection("xtable")));
  EXPECT_EQ("`xtable`", query.get());
}

TEST_F(Crud_statement_builder_test, add_collection_only_schema) {
  ASSERT_THROW(builder().add_collection(Collection("", "xschema")),
               ngs::Error_code);
}

TEST_F(Crud_statement_builder_test, add_collection_name_and_schema) {
  ASSERT_NO_THROW(builder().add_collection(Collection("xtable", "xschema")));
  EXPECT_EQ("`xschema`.`xtable`", query.get());
}

TEST_F(Crud_statement_builder_test, add_filter_uninitialized) {
  Filter filter;
  ASSERT_NO_THROW(builder().add_filter(filter));
  EXPECT_EQ("", query.get());
}

TEST_F(Crud_statement_builder_test, add_filter_initialized_column) {
  ASSERT_NO_THROW(builder().add_filter(
      Filter(Operator(">", ColumnIdentifier("A"), Scalar(1.0)))));
  EXPECT_EQ(" WHERE (`A` > 1)", query.get());
}

TEST_F(Crud_statement_builder_test, add_filter_initialized_column_and_memeber) {
  ASSERT_NO_THROW(builder().add_filter(Filter(Operator(
      ">", ColumnIdentifier(Document_path{"first"}, "A"), Scalar(1.0)))));
  EXPECT_EQ(" WHERE (JSON_EXTRACT(`A`,'$.first') > 1)", query.get());
}

TEST_F(Crud_statement_builder_test, add_filter_bad_expression) {
  ASSERT_THROW(builder().add_filter(Filter(Operator("><", ColumnIdentifier("A"),
                                                    ColumnIdentifier("B")))),
               Expression_generator::Error);
}

TEST_F(Crud_statement_builder_test, add_filter_with_arg) {
  args = Expression_args{1.0};

  ASSERT_NO_THROW(builder().add_filter(
      Filter(Operator(">", ColumnIdentifier("A"), Placeholder(0)))));
  EXPECT_EQ(" WHERE (`A` > 1)", query.get());
}

TEST_F(Crud_statement_builder_test, add_filter_missing_arg) {
  ASSERT_THROW(builder().add_filter(Filter(
                   Operator(">", ColumnIdentifier("A"), Placeholder(0)))),
               Expression_generator::Error);
}

TEST_F(Crud_statement_builder_test, add_order_empty_list) {
  ASSERT_NO_THROW(builder().add_order(Order_list()));
  EXPECT_EQ("", query.get());
}

TEST_F(Crud_statement_builder_test, add_order_one_item) {
  ASSERT_NO_THROW(
      builder().add_order(Order_list{Order(ColumnIdentifier("A"))}));
  EXPECT_EQ(" ORDER BY `A`", query.get());
}

TEST_F(Crud_statement_builder_test, add_order_two_items) {
  ASSERT_NO_THROW(builder().add_order(
      Order_list{{ColumnIdentifier("A"), Mysqlx::Crud::Order::DESC},
                 {ColumnIdentifier("B")}}));
  EXPECT_EQ(" ORDER BY `A` DESC,`B`", query.get());
}

TEST_F(Crud_statement_builder_test, add_order_two_items_placeholder) {
  args = Expression_args{2};

  ASSERT_NO_THROW(builder().add_order(Order_list{
      {ColumnIdentifier("A"), Mysqlx::Crud::Order::DESC}, {Placeholder(0)}}));
  EXPECT_EQ(" ORDER BY `A` DESC,2", query.get());
}

TEST_F(Crud_statement_builder_test, add_limit_uninitialized) {
  ASSERT_NO_THROW(builder().add_limit(Limit(), false));
  EXPECT_EQ("", query.get());
}

TEST_F(Crud_statement_builder_test, add_limit_only) {
  ASSERT_NO_THROW(builder().add_limit(Limit(2), false));
  EXPECT_EQ(" LIMIT 2", query.get());
}

TEST_F(Crud_statement_builder_test, add_limit_and_offset) {
  ASSERT_NO_THROW(builder().add_limit(Limit(2, 5), false));
  EXPECT_EQ(" LIMIT 5, 2", query.get());
}

TEST_F(Crud_statement_builder_test, add_limit_forbbiden_offset) {
  EXPECT_THROW(builder().add_limit(Limit(2, 5), true), ngs::Error_code);
}

}  // namespace test
}  // namespace xpl
