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

#include <gtest/gtest.h>
#include "statement_builder.h"
#include "mysqlx_pb_wrapper.h"


namespace xpl
{
namespace test
{

class Crud_statement_builder_impl: public Crud_statement_builder
{
public:
  Crud_statement_builder_impl(Expression_generator &gen) : Crud_statement_builder(gen) {}

  using Crud_statement_builder::add_collection;
  using Crud_statement_builder::add_filter;
  using Crud_statement_builder::add_order;
  using Crud_statement_builder::add_limit;
};


class Crud_statement_builder_test : public ::testing::Test
{
public:
  Crud_statement_builder_test()
  : expr_gen(query, args, schema, true),
    builder(expr_gen)
  {}
  Expression_generator::Args args;
  Query_string_builder query;
  std::string schema;
  Expression_generator expr_gen;
  Crud_statement_builder_impl builder;

  enum {DM_DOCUMENT = 0, DM_TABLE = 1};
};


TEST_F(Crud_statement_builder_test, add_table_only_name)
{
  ASSERT_NO_THROW(builder.add_collection(Collection("xtable")));
  EXPECT_EQ("`xtable`", query.get());
}


TEST_F(Crud_statement_builder_test, add_collection_only_schema)
{
  ASSERT_THROW(builder.add_collection(Collection("", "xschema")), ngs::Error_code);
}


TEST_F(Crud_statement_builder_test, add_collection_name_and_schema)
{
  ASSERT_NO_THROW(builder.add_collection(Collection("xtable", "xschema")));
  EXPECT_EQ("`xschema`.`xtable`", query.get());
}


TEST_F(Crud_statement_builder_test, add_filter_uninitialized)
{
  Filter filter;
  filter.Clear();
  ASSERT_NO_THROW(builder.add_filter(filter));
  EXPECT_EQ("", query.get());
}


TEST_F(Crud_statement_builder_test, add_filter_initialized_column)
{
  ASSERT_NO_THROW(builder.add_filter(Filter(Operator(">", ColumnIdentifier("A"), Scalar(1.0)))));
  EXPECT_EQ(" WHERE (`A` > 1)", query.get());
}


TEST_F(Crud_statement_builder_test, add_filter_initialized_column_and_memeber)
{
  ASSERT_NO_THROW(builder.add_filter(Filter(Operator(">", ColumnIdentifier(Document_path::Path("first"), "A"), Scalar(1.0)))));
  EXPECT_EQ(" WHERE (JSON_EXTRACT(`A`,'$.first') > 1)", query.get());
}


TEST_F(Crud_statement_builder_test, add_filter_bad_expression)
{
  ASSERT_THROW(builder.add_filter(Filter(Operator("><", ColumnIdentifier("A"), ColumnIdentifier("B")))),
               Expression_generator::Error);
}


TEST_F(Crud_statement_builder_test, add_filter_with_arg)
{
  *args.Add() = Scalar(1.0);

  ASSERT_NO_THROW(builder.add_filter(Filter(Operator(">", ColumnIdentifier("A"), Placeholder(0)))));
  EXPECT_EQ(" WHERE (`A` > 1)", query.get());
}


TEST_F(Crud_statement_builder_test, add_filter_missing_arg)
{
  ASSERT_THROW(builder.add_filter(Filter(Operator(">", ColumnIdentifier("A"), Placeholder(0)))),
               Expression_generator::Error);
}


TEST_F(Crud_statement_builder_test, add_order_empty_list)
{
  ASSERT_NO_THROW(builder.add_order(Order_list()));
  EXPECT_EQ("", query.get());
}


TEST_F(Crud_statement_builder_test, add_order_one_item)
{
  ASSERT_NO_THROW(builder.add_order(Order_list(Order(ColumnIdentifier("A")))));
  EXPECT_EQ(" ORDER BY `A`", query.get());
}


TEST_F(Crud_statement_builder_test, add_order_two_items)
{
  ASSERT_NO_THROW(builder.add_order(Order_list(Order(ColumnIdentifier("A"), Mysqlx::Crud::Order_Direction_DESC))
                                              (Order(ColumnIdentifier("B")))));
  EXPECT_EQ(" ORDER BY `A` DESC,`B`", query.get());
}


TEST_F(Crud_statement_builder_test, add_order_two_items_placeholder)
{
  *args.Add() = Scalar(2);

  ASSERT_NO_THROW(builder.add_order(Order_list(Order(ColumnIdentifier("A"), Mysqlx::Crud::Order_Direction_DESC))
                                              (Order(Placeholder(0)))));
  EXPECT_EQ(" ORDER BY `A` DESC,2", query.get());
}


TEST_F(Crud_statement_builder_test, add_limit_uninitialized)
{
  ASSERT_NO_THROW(builder.add_limit(Limit(), false));
  EXPECT_EQ("", query.get());
}


TEST_F(Crud_statement_builder_test, add_limit_only)
{
  ASSERT_NO_THROW(builder.add_limit(Limit(2), false));
  EXPECT_EQ(" LIMIT 2", query.get());
}


TEST_F(Crud_statement_builder_test, add_limit_and_offset)
{
  ASSERT_NO_THROW(builder.add_limit(Limit(2, 5), false));
  EXPECT_EQ(" LIMIT 5, 2", query.get());
}


TEST_F(Crud_statement_builder_test, add_limit_forbbiden_offset)
{
  EXPECT_THROW(builder.add_limit(Limit(2, 5), true), ngs::Error_code);
}


} // namespace test
} // namespace xpl


