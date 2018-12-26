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


#include "insert_statement_builder.h"
#include "query_string_builder.h"
#include "mysqld_error.h"
#include "expr_generator.h"
#include "ngs_common/protocol_protobuf.h"

#include <gtest/gtest.h>

namespace xpl
{
namespace test
{

class Insert_statement_builder_impl: public Insert_statement_builder
{
public:
  Insert_statement_builder_impl(Expression_generator &gen) : Insert_statement_builder(gen) {}

  using Insert_statement_builder::Projection_list;
  using Insert_statement_builder::Row_list;
  using Insert_statement_builder::Field_list;
  using Insert_statement_builder::add_projection;
  using Insert_statement_builder::add_values;
  using Insert_statement_builder::add_row;
};


class Insert_statement_builder_test : public ::testing::Test
{
public:
  Insert_statement_builder_test()
  : args(*msg.mutable_args()),
    expr_gen(query, args, schema, true),
    builder(expr_gen)
  {}
  Insert_statement_builder::Insert msg;
  Expression_generator::Args &args;
  Query_string_builder query;
  std::string schema;
  Expression_generator expr_gen;
  Insert_statement_builder_impl builder;

  enum {DM_DOCUMENT = 0, DM_TABLE = 1};
};

namespace
{

void operator<< (::google::protobuf::Message &msg, const std::string& txt)
{
  ::google::protobuf::TextFormat::ParseFromString(txt, &msg);
}

inline std::string get_literal(const std::string& name)
{
  return "type: LITERAL literal "
      "{type: V_STRING v_string {value: '"+ name + "' }}";
}

inline std::string get_field(const std::string& name)
{
  return "field {" + get_literal(name) + "}";
}

} // namespace


TEST_F(Insert_statement_builder_test, add_row_empty_projection_empty_row)
{
  Insert_statement_builder_impl::Field_list row;
  ASSERT_THROW(builder.add_row(row, 0), ngs::Error_code);
  EXPECT_EQ("", query.get());
}


TEST_F(Insert_statement_builder_test, add_row_one_projection_empty_row)
{
  Insert_statement_builder_impl::Field_list row;
  ASSERT_THROW(builder.add_row(row, 1), ngs::Error_code);
  EXPECT_EQ("", query.get());
}


TEST_F(Insert_statement_builder_test, add_row_full_row_projection_empty)
{
  Insert_statement_builder_impl::Field_list row;
  *row.Add() << get_literal("one");
  ASSERT_NO_THROW(builder.add_row(row, 0));
  EXPECT_EQ("('one')", query.get());
}


TEST_F(Insert_statement_builder_test, add_row_half_row_full_projection)
{
  Insert_statement_builder_impl::Field_list row;
  *row.Add() << get_literal("one");
  ASSERT_THROW(builder.add_row(row, 2), ngs::Error_code);
  EXPECT_EQ("", query.get());
}


TEST_F(Insert_statement_builder_test, add_row_full_row_full_projection)
{
  Insert_statement_builder_impl::Field_list row;
  *row.Add() << get_literal("one");
  *row.Add() << get_literal("two");
  ASSERT_NO_THROW(builder.add_row(row, 2));
  EXPECT_EQ("('one','two')", query.get());
}


TEST_F(Insert_statement_builder_test, add_values_empty_list)
{
  Insert_statement_builder_impl::Row_list values;
  ASSERT_THROW(builder.add_values(values, 1), ngs::Error_code);
  EXPECT_EQ("", query.get());
}


TEST_F(Insert_statement_builder_test, add_values_one_row)
{
  Insert_statement_builder_impl::Row_list values;
  *values.Add() << get_field("one") + " " + get_field("two");
  ASSERT_NO_THROW(builder.add_values(values, 0));
  EXPECT_EQ(" VALUES ('one','two')", query.get());
}


TEST_F(Insert_statement_builder_test, add_values_one_row_with_arg)
{
  *args.Add() << "type: V_STRING v_string {value: 'two'}";

  Insert_statement_builder_impl::Row_list values;
  *values.Add() << get_field("one") + " field {type: PLACEHOLDER position: 0}";
  ASSERT_NO_THROW(builder.add_values(values, 0));
  EXPECT_EQ(" VALUES ('one','two')", query.get());
}


TEST_F(Insert_statement_builder_test, add_values_one_row_missing_arg)
{
  Insert_statement_builder_impl::Row_list values;
  *values.Add() << get_field("one") + " field {type: PLACEHOLDER position: 0}";
  EXPECT_THROW(builder.add_values(values, 0),
               Expression_generator::Error);
}


TEST_F(Insert_statement_builder_test, add_values_two_rows)
{
  Insert_statement_builder_impl::Row_list values;
  *values.Add() << get_field("one") + " " + get_field("two");
  *values.Add() << get_field("three") + " " + get_field("four");
  ASSERT_NO_THROW(builder.add_values(values, values.size()));
  EXPECT_EQ(" VALUES ('one','two'),('three','four')", query.get());
}


TEST_F(Insert_statement_builder_test, add_values_two_rows_with_args)
{
  *args.Add() << "type: V_STRING v_string {value: 'two'}";
  *args.Add() << "type: V_STRING v_string {value: 'four'}";

  Insert_statement_builder_impl::Row_list values;
  *values.Add() << get_field("one") + " field {type: PLACEHOLDER position: 0}";
  *values.Add() << get_field("three") + " field {type: PLACEHOLDER position: 1}";
  ASSERT_NO_THROW(builder.add_values(values, values.size()));
  EXPECT_EQ(" VALUES ('one','two'),('three','four')", query.get());
}


TEST_F(Insert_statement_builder_test, add_projection_tabel_empty)
{
  Insert_statement_builder_impl::Projection_list projection;
  ASSERT_NO_THROW(builder.add_projection(projection, DM_TABLE));
  EXPECT_EQ("", query.get());
}


TEST_F(Insert_statement_builder_test, add_projection_tabel_one_item)
{
  Insert_statement_builder_impl::Projection_list projection;
  *projection.Add() << "name: 'first'";
  ASSERT_NO_THROW(builder.add_projection(projection, DM_TABLE));
  EXPECT_EQ(" (`first`)", query.get());
}


TEST_F(Insert_statement_builder_test, add_projection_tabel_two_items)
{
  Insert_statement_builder_impl::Projection_list projection;
  *projection.Add() << "name: 'first'";
  *projection.Add() << "name: 'second'";
  ASSERT_NO_THROW(builder.add_projection(projection, DM_TABLE));
  EXPECT_EQ(" (`first`,`second`)", query.get());
}


TEST_F(Insert_statement_builder_test, add_projection_document_empty)
{
  Insert_statement_builder_impl::Projection_list projection;
  ASSERT_NO_THROW(builder.add_projection(projection, DM_DOCUMENT));
  EXPECT_EQ(" (doc)", query.get());
}


TEST_F(Insert_statement_builder_test, add_projection_document_one_item)
{
  Insert_statement_builder_impl::Projection_list projection;
  *projection.Add() << "name: 'first'";
  ASSERT_THROW(builder.add_projection(projection, DM_DOCUMENT), ngs::Error_code);
  EXPECT_EQ("", query.get());
}


TEST_F(Insert_statement_builder_test, build_document)
{
  msg <<
      "collection { name: 'xcoll' schema: 'xtest' } "
      "data_model: DOCUMENT "
      "row {" + get_field("first") + "}"
      "row {" + get_field("second") + "}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ("INSERT INTO `xtest`.`xcoll` (doc) VALUES ('first'),('second')", query.get());
}


TEST_F(Insert_statement_builder_test, build_table)
{
  msg <<
      "collection { name: 'xtable' schema: 'xtest' } "
      "data_model: TABLE "
      "projection { name: 'one' } "
      "projection { name: 'two' } "
      "row {" + get_field("first") + " " + get_field("second") + "}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ("INSERT INTO `xtest`.`xtable` (`one`,`two`) VALUES ('first','second')", query.get());
}

} // namespace test
} // namespace xpl


