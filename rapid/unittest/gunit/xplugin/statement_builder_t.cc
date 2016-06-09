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

#include "statement_builder.h"
#include "query_string_builder.h"
#include "mysqld_error.h"
#include "expr_generator.h"
#include "ngs_common/protocol_protobuf.h"

#include <gtest/gtest.h>

namespace xpl
{
namespace test
{

class Statement_builder_impl: public Statement_builder
{
public:
  Statement_builder_impl(Query_string_builder &qb,
                         const Expression_generator::Args &args,
                         const std::string &schema)
  : Statement_builder(qb, args, schema, true),
    m_flag(NO_THROW)
  {}

  virtual void add_statement() const
  {
    switch (m_flag)
    {
    case NO_THROW:
      m_builder.put("ok");
      return;

    case ER_THROW:
      m_builder.put("error");
      throw ngs::Error_code(ER_THROW, "");

    case EX_THROW:
      m_builder.put("expr error");
      throw Expression_generator::Error(EX_THROW, "");
    }
  }

  using Statement_builder::add_table;
  using Statement_builder::add_filter;
  using Statement_builder::add_order;
  using Statement_builder::add_limit;
  using Statement_builder::Filter;
  using Statement_builder::Limit;
  using Statement_builder::Collection;
  using Statement_builder::Order_list;

  enum {NO_THROW = 0, ER_THROW, EX_THROW} m_flag;
};


class Statement_builder_test : public ::testing::Test
{
public:
  Statement_builder_test()
  : builder(query, args, schema)
  {}

  Expression_generator::Args args;
  std::string schema;
  Query_string_builder query;
  Statement_builder_impl builder;
};


TEST_F(Statement_builder_test, build_no_throw)
{
  EXPECT_EQ(ngs::Error_code(), builder.build());
  EXPECT_EQ(std::string("ok"), query.get());
}


TEST_F(Statement_builder_test, build_throw_general_error)
{
  builder.m_flag = Statement_builder_impl::ER_THROW;
  EXPECT_EQ(ngs::Error_code(Statement_builder_impl::ER_THROW, ""),
            builder.build());
  EXPECT_EQ("error", query.get());
}


TEST_F(Statement_builder_test, build_throw_expresion_error)
{
  builder.m_flag = Statement_builder_impl::EX_THROW;
  EXPECT_EQ(ngs::Error_code(Statement_builder_impl::EX_THROW, ""),
            builder.build());
  EXPECT_EQ("expr error", query.get());
}


TEST_F(Statement_builder_test, add_table_empty_name)
{
  Statement_builder_impl::Collection coll;
  EXPECT_THROW(builder.add_table(coll), ngs::Error_code);
}

namespace
{

void operator<< (::google::protobuf::Message &msg, const std::string& txt)
{
  ::google::protobuf::TextFormat::ParseFromString(txt, &msg);
}

} // namespace


TEST_F(Statement_builder_test, add_table_only_name)
{
  Statement_builder_impl::Collection coll;
  coll << "name: 'xtable'";
  ASSERT_THROW(builder.add_table(coll), ngs::Error_code);
}


TEST_F(Statement_builder_test, add_table_only_schema)
{
  Statement_builder_impl::Collection coll;
  coll << "schema: 'xschema' name: ''";
  ASSERT_THROW(builder.add_table(coll), ngs::Error_code);
}


TEST_F(Statement_builder_test, add_table_name_and_schema)
{
  Statement_builder_impl::Collection coll;
  coll << "name: 'xtable' schema: 'xschema'";
  ASSERT_NO_THROW(builder.add_table(coll));
  EXPECT_EQ("`xschema`.`xtable`", query.get());
}


TEST_F(Statement_builder_test, add_fillter_uninitialized)
{
  Statement_builder_impl::Filter filter;
  filter.Clear();
  ASSERT_NO_THROW(builder.add_filter(filter));
  EXPECT_EQ("", query.get());
}


TEST_F(Statement_builder_test, add_fillter_initialized_column)
{
  Statement_builder_impl::Filter filter;
  filter << "type: OPERATOR operator {name: '>'"
      "param {type: IDENT identifier {name: 'A'}}"
      "param {type: LITERAL literal {type: V_DOUBLE v_double: 1.0}}}";
  ASSERT_NO_THROW(builder.add_filter(filter));
  EXPECT_EQ(" WHERE (`A` > 1)", query.get());
}


TEST_F(Statement_builder_test, add_fillter_initialized_column_and_memeber)
{
  Statement_builder_impl::Filter filter;
  filter << "type: OPERATOR operator {name: '>'"
      "param {type: IDENT identifier {name: 'A' document_path {type: MEMBER value: 'first'}}}"
      "param {type: LITERAL literal {type: V_DOUBLE v_double: 1.0}}}";
  ASSERT_NO_THROW(builder.add_filter(filter));
  EXPECT_EQ(" WHERE (JSON_EXTRACT(`A`,'$.first') > 1)", query.get());
}


TEST_F(Statement_builder_test, add_fillter_bad_expression)
{
  Statement_builder_impl::Filter filter;
  filter << "type: OPERATOR operator {name: '><'"
      "param {type: IDENT identifier {name: 'A'}}"
      "param {type: IDENT identifier {name: 'B'}}}";
  EXPECT_THROW(builder.add_filter(filter),
               Expression_generator::Error);
}


TEST_F(Statement_builder_test, add_fillter_with_arg)
{
  *args.Add() << "type: V_DOUBLE v_double: 1.0";

  Statement_builder_impl::Filter filter;
  filter << "type: OPERATOR operator {name: '>'"
      "param {type: IDENT identifier {name: 'A'}}"
      "param {type: PLACEHOLDER position: 0}}";
  ASSERT_NO_THROW(builder.add_filter(filter));
  EXPECT_EQ(" WHERE (`A` > 1)", query.get());
}


TEST_F(Statement_builder_test, add_fillter_missing_arg)
{
  Statement_builder_impl::Filter filter;
  filter << "type: OPERATOR operator {name: '>'"
      "param {type: IDENT identifier {name: 'A'}}"
      "param {type: PLACEHOLDER position: 0}}";
  EXPECT_THROW(builder.add_filter(filter),
               Expression_generator::Error);
}


TEST_F(Statement_builder_test, add_order_empty_list)
{
  Statement_builder_impl::Order_list order;
  ASSERT_NO_THROW(builder.add_order(order));
  EXPECT_EQ("", query.get());
}


TEST_F(Statement_builder_test, add_order_one_item)
{
  Statement_builder_impl::Order_list order;
  *order.Add() << "expr {type: IDENT identifier {name: 'A'}}";
  ASSERT_NO_THROW(builder.add_order(order));
  EXPECT_EQ(" ORDER BY `A`", query.get());
}


TEST_F(Statement_builder_test, add_order_two_items)
{
  Statement_builder_impl::Order_list order;
  *order.Add() << "expr {type: IDENT identifier {name: 'A'}} direction: DESC";
  *order.Add() << "expr {type: IDENT identifier {name: 'B'}}";
  ASSERT_NO_THROW(builder.add_order(order));
  EXPECT_EQ(" ORDER BY `A` DESC,`B`", query.get());
}


TEST_F(Statement_builder_test, add_order_two_items_placeholder)
{
  *args.Add() << "type: V_SINT v_signed_int: 2";

  Statement_builder_impl::Order_list order;
  *order.Add() << "expr {type: IDENT identifier {name: 'A'}} direction: DESC";
  *order.Add() << "expr {type: PLACEHOLDER position: 0}";
  ASSERT_NO_THROW(builder.add_order(order));
  EXPECT_EQ(" ORDER BY `A` DESC,2", query.get());
}


TEST_F(Statement_builder_test, add_limit_uninitialized)
{
  Statement_builder_impl::Limit limit;
  limit.Clear();
  ASSERT_NO_THROW(builder.add_limit(limit, false));
  EXPECT_EQ("", query.get());
}


TEST_F(Statement_builder_test, add_limit_only)
{
  Statement_builder_impl::Limit limit;
  limit << "row_count: 2";
  ASSERT_NO_THROW(builder.add_limit(limit, false));
  EXPECT_EQ(" LIMIT 2", query.get());
}


TEST_F(Statement_builder_test, add_limit_and_offset)
{
  Statement_builder_impl::Limit limit;
  limit << "row_count: 2 offset: 5";
  ASSERT_NO_THROW(builder.add_limit(limit, false));
  EXPECT_EQ(" LIMIT 5, 2", query.get());
}


TEST_F(Statement_builder_test, add_limit_forbbiden_offset)
{
  Statement_builder_impl::Limit limit;
  limit << "row_count: 2 offset: 5";
  EXPECT_THROW(builder.add_limit(limit, true), ngs::Error_code);
}

} // namespace test
} // namespace xpl


