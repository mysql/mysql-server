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

#include "plugin/x/src/insert_statement_builder.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

class Insert_statement_builder_stub : public Insert_statement_builder {
 public:
  explicit Insert_statement_builder_stub(Expression_generator *gen)
      : Insert_statement_builder(*gen) {}
  using Insert_statement_builder::add_projection;
  using Insert_statement_builder::add_values;
  using Insert_statement_builder::add_row;
  using Insert_statement_builder::add_upsert;
};

class Insert_statement_builder_test : public ::testing::Test {
 public:
  Insert_statement_builder_stub &builder() {
    expr_gen.reset(new Expression_generator(&query, args, schema,
                                            is_table_data_model(msg)));
    stub.reset(new Insert_statement_builder_stub(expr_gen.get()));
    return *stub;
  }

  Insert_statement_builder::Insert msg;
  Expression_generator::Args &args = *msg.mutable_args();
  Query_string_builder query;
  std::string schema;
  std::unique_ptr<Expression_generator> expr_gen;
  std::unique_ptr<Insert_statement_builder_stub> stub;

  enum {
    DM_DOCUMENT = 0,
    DM_TABLE = 1
  };
};

TEST_F(Insert_statement_builder_test, add_row_empty_projection_empty_row) {
  ASSERT_THROW(builder().add_row(Field_list(), 0), ngs::Error_code);
  EXPECT_EQ("", query.get());
}

TEST_F(Insert_statement_builder_test, add_row_one_projection_empty_row) {
  ASSERT_THROW(builder().add_row(Field_list(), 1), ngs::Error_code);
  EXPECT_EQ("", query.get());
}

TEST_F(Insert_statement_builder_test, add_row_full_row_projection_empty) {
  ASSERT_NO_THROW(builder().add_row(Field_list{"one"}, 0));
  EXPECT_EQ("('one')", query.get());
}

TEST_F(Insert_statement_builder_test, add_row_half_row_full_projection) {
  ASSERT_THROW(builder().add_row(Field_list{"one"}, 2), ngs::Error_code);
  EXPECT_EQ("", query.get());
}

TEST_F(Insert_statement_builder_test, add_row_full_row_full_projection) {
  ASSERT_NO_THROW(builder().add_row(Field_list{"one", "two"}, 2));
  EXPECT_EQ("('one','two')", query.get());
}

TEST_F(Insert_statement_builder_test, add_values_empty_list) {
  ASSERT_THROW(builder().add_values(Row_list(), 1), ngs::Error_code);
  EXPECT_EQ("", query.get());
}

TEST_F(Insert_statement_builder_test, add_values_one_row) {
  ASSERT_NO_THROW(builder().add_values(Row_list{{"one", "two"}}, 0));
  EXPECT_EQ(" VALUES ('one','two')", query.get());
}

TEST_F(Insert_statement_builder_test, add_values_one_row_with_arg) {
  *args.Add() = Scalar("two");

  ASSERT_NO_THROW(builder().add_values(Row_list{{"one", Placeholder(0)}}, 0));
  EXPECT_EQ(" VALUES ('one','two')", query.get());
}

TEST_F(Insert_statement_builder_test, add_values_one_row_missing_arg) {
  EXPECT_THROW(builder().add_values(Row_list{{"one", Placeholder(0)}}, 0),
               Expression_generator::Error);
}

TEST_F(Insert_statement_builder_test, add_values_two_rows) {
  Row_list values{{"one", "two"}, {"three", "four"}};
  ASSERT_NO_THROW(builder().add_values(values, values.size()));
  EXPECT_EQ(" VALUES ('one','two'),('three','four')", query.get());
}

TEST_F(Insert_statement_builder_test, add_values_two_rows_with_args) {
  *args.Add() = Scalar("two");
  *args.Add() = Scalar("four");

  Row_list values{{"one", Placeholder(0)}, {"three", Placeholder(1)}};
  ASSERT_NO_THROW(builder().add_values(values, values.size()));
  EXPECT_EQ(" VALUES ('one','two'),('three','four')", query.get());
}

TEST_F(Insert_statement_builder_test, add_projection_tabel_empty) {
  ASSERT_NO_THROW(builder().add_projection(Column_projection_list(), DM_TABLE));
  EXPECT_EQ("", query.get());
}

TEST_F(Insert_statement_builder_test, add_projection_tabel_one_item) {
  ASSERT_NO_THROW(builder().add_projection(
      Column_projection_list{Column("first")}, DM_TABLE));
  EXPECT_EQ(" (`first`)", query.get());
}

TEST_F(Insert_statement_builder_test, add_projection_tabel_two_items) {
  ASSERT_NO_THROW(builder().add_projection(
      Column_projection_list{Column("first"), Column("second")}, DM_TABLE));
  EXPECT_EQ(" (`first`,`second`)", query.get());
}

TEST_F(Insert_statement_builder_test, add_projection_document_empty) {
  ASSERT_NO_THROW(
      builder().add_projection(Column_projection_list(), DM_DOCUMENT));
  EXPECT_EQ(" (doc)", query.get());
}

TEST_F(Insert_statement_builder_test, add_projection_document_one_item) {
  ASSERT_THROW(builder().add_projection(Column_projection_list{Column("first")},
                                        DM_DOCUMENT),
               ngs::Error_code);
}

TEST_F(Insert_statement_builder_test, add_upsert) {
  ASSERT_NO_THROW(builder().add_upsert(DM_DOCUMENT));
  EXPECT_STREQ(
      " ON DUPLICATE KEY UPDATE"
      " doc = IF(JSON_EXTRACT(doc, '$._id')"
      " = JSON_EXTRACT(VALUES(doc), '$._id'),"
      " VALUES(doc), MYSQLX_ERROR(5018))",
      query.get().c_str());
  ASSERT_THROW(builder().add_upsert(DM_TABLE), ngs::Error_code);
}

TEST_F(Insert_statement_builder_test, build_document) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  *msg.mutable_collection() = Collection("xcoll", "xtest");
  *msg.mutable_row() = Row_list{{"first"}, {"second"}};
  ASSERT_NO_THROW(builder().build(msg));
  EXPECT_EQ(
      "INSERT INTO `xtest`.`xcoll` (doc) "
      "VALUES ('first'),('second')",
      query.get());
}

TEST_F(Insert_statement_builder_test, build_table) {
  msg.set_data_model(Mysqlx::Crud::TABLE);
  *msg.mutable_collection() = Collection("xtable", "xtest");
  *msg.mutable_projection() =
      Column_projection_list{Column("one"), Column("two")};
  *msg.mutable_row() = Row_list{{"first", "second"}};
  ASSERT_NO_THROW(builder().build(msg));
  EXPECT_EQ(
      "INSERT INTO `xtest`.`xtable` (`one`,`two`) "
      "VALUES ('first','second')",
      query.get());
}

TEST_F(Insert_statement_builder_test, build_document_upsert) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  msg.set_upsert(true);
  *msg.mutable_collection() = Collection("xcoll", "xtest");
  *msg.mutable_row() = Row_list{{"first"}, {"second"}};
  ASSERT_NO_THROW(builder().build(msg));
  EXPECT_STREQ(
      "INSERT INTO `xtest`.`xcoll` (doc) VALUES ('first'),('second')"
      " ON DUPLICATE KEY UPDATE"
      " doc = IF(JSON_EXTRACT(doc, '$._id') = JSON_EXTRACT(VALUES(doc),"
      " '$._id'), VALUES(doc), MYSQLX_ERROR(5018))",
      query.get().c_str());
}

TEST_F(Insert_statement_builder_test, build_table_upsert) {
  msg.set_data_model(Mysqlx::Crud::TABLE);
  msg.set_upsert(true);
  *msg.mutable_collection() = Collection("xcoll", "xtest");
  *msg.mutable_row() = Row_list{{"first"}, {"second"}};
  ASSERT_THROW(builder().build(msg), ngs::Error_code);
}

}  // namespace test
}  // namespace xpl
