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

#include "plugin/x/src/find_statement_builder.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {

class Find_statement_builder_stub : public Find_statement_builder {
 public:
  explicit Find_statement_builder_stub(Expression_generator *gen)
      : Find_statement_builder(*gen) {}
  using Find_statement_builder::add_table_projection;
  using Find_statement_builder::add_document_projection;
  using Find_statement_builder::add_grouping;
  using Find_statement_builder::add_grouping_criteria;
  using Find_statement_builder::add_row_locking;
};

class Find_statement_builder_test : public ::testing::Test {
 public:
  Find_statement_builder_test()
      : args(*msg.mutable_args()), expr_gen(0), stub(0) {}

  ~Find_statement_builder_test() {
    delete stub;
    delete expr_gen;
  }

  Find_statement_builder_stub *builder() {
    expr_gen =
        new Expression_generator(&query, args, schema, is_table_data_model(msg));
    return stub = new Find_statement_builder_stub(expr_gen);
  }

  Find_statement_builder::Find msg;
  Expression_generator::Args &args;
  Query_string_builder query;
  std::string schema;
  Expression_generator *expr_gen;
  Find_statement_builder_stub *stub;

  enum {
    DM_DOCUMENT = 0,
    DM_TABLE = 1
  };
};

TEST_F(Find_statement_builder_test, add_projection_table_empty) {
  ASSERT_NO_THROW(builder()->add_table_projection(Projection_list()));
  EXPECT_EQ("*", query.get());
}

TEST_F(Find_statement_builder_test, add_document_projection_empty) {
  ASSERT_NO_THROW(builder()->add_document_projection(Projection_list()));
  EXPECT_EQ("doc", query.get());
}

TEST_F(Find_statement_builder_test, add_document_projection_wildcards) {
  ASSERT_THROW(builder()->add_document_projection(Projection_list{
                   {Operator("*", ColumnIdentifier("", "xtable"))}}),
               ngs::Error_code);
}

TEST_F(Find_statement_builder_test, add_document_projection_wildcards_mix) {
  ASSERT_THROW(builder()->add_document_projection(Projection_list{
                   {ColumnIdentifier("xfield", "xtable")},
                   {Operator("*", ColumnIdentifier("", "xtable"))}}),
               ngs::Error_code);
}

TEST_F(Find_statement_builder_test, add_projection_table_one_member_item) {
  ASSERT_NO_THROW(builder()->add_table_projection(
      Projection_list{{ColumnIdentifier(Document_path{"alpha"})}}));
  EXPECT_EQ("JSON_EXTRACT(doc,'$.alpha')", query.get());
}

TEST_F(Find_statement_builder_test, add_projection_table_one_item) {
  ASSERT_NO_THROW(builder()->add_table_projection(
      Projection_list{{ColumnIdentifier("alpha")}}));
  EXPECT_EQ("`alpha`", query.get());
}

TEST_F(Find_statement_builder_test, add_projection_table_two_items) {
  ASSERT_NO_THROW(builder()->add_table_projection(Projection_list{
      {ColumnIdentifier("alpha")}, {ColumnIdentifier("beta")}}));
  EXPECT_EQ("`alpha`,`beta`", query.get());
}

TEST_F(Find_statement_builder_test,
       add_projection_table_two_items_placeholder) {
  args = Expression_args{2.2};
  ASSERT_NO_THROW(builder()->add_table_projection(
      Projection_list{{ColumnIdentifier("alpha")}, {Placeholder(0)}}));
  EXPECT_EQ("`alpha`,2.2", query.get());
}

TEST_F(Find_statement_builder_test, add_projection_table_one_item_with_alias) {
  ASSERT_NO_THROW(builder()->add_table_projection(
      Projection_list{{ColumnIdentifier("alpha"), "beta"}}));
  EXPECT_EQ("`alpha` AS `beta`", query.get());
}

TEST_F(Find_statement_builder_test, add_projection_document_one_item_no_alias) {
  ASSERT_THROW(builder()->add_document_projection(
                   Projection_list{{ColumnIdentifier("alpha")}}),
               ngs::Error_code);
}

TEST_F(Find_statement_builder_test, add_projection_document_one_item) {
  ASSERT_NO_THROW(builder()->add_document_projection(Projection_list{
      {ColumnIdentifier("alpha", "xtable"), "beta"}}));
  EXPECT_EQ("JSON_OBJECT('beta', `xtable`.`alpha`) AS doc", query.get());
}

TEST_F(Find_statement_builder_test, add_projection_document_one_member_item) {
  ASSERT_NO_THROW(builder()->add_document_projection(Projection_list{
      {ColumnIdentifier(Document_path{"alpha"}), "beta"}}));
  EXPECT_EQ("JSON_OBJECT('beta', JSON_EXTRACT(doc,'$.alpha')) AS doc",
            query.get());
}

TEST_F(Find_statement_builder_test, add_projection_document_two_member_items) {
  ASSERT_NO_THROW(builder()->add_document_projection(
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "beta"},
                      {ColumnIdentifier(Document_path{"first"}), "second"}}));
  EXPECT_EQ(
      "JSON_OBJECT('beta', JSON_EXTRACT(doc,'$.alpha'),"
      "'second', JSON_EXTRACT(doc,'$.first')) AS doc",
      query.get());
}

TEST_F(Find_statement_builder_test,
       add_projection_document_two_member_items_placeholder) {
  args = Expression_args{2.2};
  ASSERT_NO_THROW(builder()->add_document_projection(
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "beta"},
                      {Placeholder(0), "second"}}));
  EXPECT_EQ(
      "JSON_OBJECT('beta', JSON_EXTRACT(doc,'$.alpha'),"
      "'second', 2.2) AS doc",
      query.get());
}

TEST_F(Find_statement_builder_test, add_gruping_empty) {
  ASSERT_NO_THROW(builder()->add_grouping(Grouping_list{}));
  EXPECT_EQ("", query.get());
}

TEST_F(Find_statement_builder_test, add_gruping_one_item) {
  ASSERT_NO_THROW(
      builder()->add_grouping(Grouping_list{ColumnIdentifier("alpha")}));
  EXPECT_EQ(" GROUP BY `alpha`", query.get());
}

TEST_F(Find_statement_builder_test, add_gruping_two_items) {
  ASSERT_NO_THROW(builder()->add_grouping(
      Grouping_list{{ColumnIdentifier("alpha")}, {ColumnIdentifier("beta")}}));
  EXPECT_EQ(" GROUP BY `alpha`,`beta`", query.get());
}

TEST_F(Find_statement_builder_test, add_gruping_two_items_placeholder) {
  args = Expression_args{2};
  ASSERT_NO_THROW(builder()->add_grouping(
      Grouping_list{{ColumnIdentifier("alpha")}, {Placeholder(0)}}));
  EXPECT_EQ(" GROUP BY `alpha`,2", query.get());
}

TEST_F(Find_statement_builder_test, add_gruping_criteria) {
  ASSERT_NO_THROW(builder()->add_grouping_criteria(Grouping_criteria(
      Operator(">", ColumnIdentifier("alpha"), Scalar(1.0)))));
  EXPECT_STREQ(" HAVING (`alpha` > 1)", query.get().c_str());
}

TEST_F(Find_statement_builder_test, add_gruping_criteria_placeholder) {
  args = Expression_args{2.3};
  ASSERT_NO_THROW(builder()->add_grouping_criteria(Grouping_criteria(
      Operator(">", ColumnIdentifier("alpha"), Placeholder(0)))));
  EXPECT_EQ(" HAVING (`alpha` > 2.3)", query.get());
}

TEST_F(Find_statement_builder_test, add_row_lock_shared) {
  msg.set_locking(Mysqlx::Crud::Find::SHARED_LOCK);

  ASSERT_NO_THROW(builder()->add_row_locking(msg));
  EXPECT_EQ(" FOR SHARE", query.get());
}

TEST_F(Find_statement_builder_test, add_row_lock_exclusive) {
  msg.set_locking(Mysqlx::Crud::Find::EXCLUSIVE_LOCK);

  ASSERT_NO_THROW(builder()->add_row_locking(msg));
  EXPECT_EQ(" FOR UPDATE", query.get());
}

TEST_F(Find_statement_builder_test, build_table) {
  msg.set_data_model(Mysqlx::Crud::TABLE);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_projection() =
      Projection_list{{ColumnIdentifier("alpha"), "zeta"}};
  *msg.mutable_criteria() =
      Filter(Operator(">", ColumnIdentifier("delta"), Scalar(1.0)));
  *msg.mutable_order() =
      Order_list{{ColumnIdentifier("gamma"), Order::Base::DESC}};
  *msg.mutable_grouping() = Grouping_list{{ColumnIdentifier("beta")}};
  *msg.mutable_grouping_criteria() =
      Grouping_criteria(Operator("<", ColumnIdentifier("lambda"), Scalar(2.0)));
  ASSERT_NO_THROW(builder()->build(msg));
  EXPECT_EQ(
      "SELECT `alpha` AS `zeta` "
      "FROM `xschema`.`xtable` "
      "WHERE (`delta` > 1) "
      "GROUP BY `beta` "
      "HAVING (`lambda` < 2) "
      "ORDER BY `gamma` DESC",
      query.get());
}

TEST_F(Find_statement_builder_test, build_document_no_grouping) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_projection() =
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "zeta"}};
  *msg.mutable_criteria() = Filter(
      Operator(">", ColumnIdentifier(Document_path{"delta"}), Scalar(1.0)));
  *msg.mutable_order() =
      Order_list{{ColumnIdentifier(Document_path{"gamma"}), Order::Base::DESC}};
  ASSERT_NO_THROW(builder()->build(msg));
  EXPECT_STREQ(
      "SELECT JSON_OBJECT('zeta', JSON_EXTRACT(doc,'$.alpha')) AS doc "
      "FROM `xschema`.`xtable` "
      "WHERE (JSON_EXTRACT(doc,'$.delta') > 1) "
      "ORDER BY JSON_EXTRACT(doc,'$.gamma') DESC",
      query.get().c_str());
}

TEST_F(Find_statement_builder_test, build_document_with_grouping_and_criteria) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_projection() =
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "zeta"}};
  *msg.mutable_criteria() = Filter(
      Operator(">", ColumnIdentifier(Document_path{"delta"}), Scalar(1.0)));
  *msg.mutable_order() =
      Order_list{{ColumnIdentifier(Document_path{"beta"}), Order::Base::DESC}};
  *msg.mutable_grouping() =
      Grouping_list{{ColumnIdentifier(Document_path{"alpha"})}};
  *msg.mutable_grouping_criteria() = Grouping_criteria(
      Operator("<", ColumnIdentifier(Document_path{"lambda"}), Scalar(2.0)));
  ASSERT_NO_THROW(builder()->build(msg));
  EXPECT_STREQ(
      "SELECT JSON_OBJECT('zeta', `_DERIVED_TABLE_`.`zeta`) AS doc FROM ("
      "SELECT JSON_EXTRACT(doc,'$.alpha') AS `zeta` "
      "FROM `xschema`.`xtable` "
      "WHERE (JSON_EXTRACT(doc,'$.delta') > 1) "
      "GROUP BY JSON_EXTRACT(doc,'$.alpha') "
      "HAVING (JSON_EXTRACT(doc,'$.lambda') < 2) "
      "ORDER BY JSON_EXTRACT(doc,'$.beta') DESC"
      ") AS `_DERIVED_TABLE_`",
      query.get().c_str());
}

TEST_F(Find_statement_builder_test, build_document_with_grouping) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_projection() =
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "zeta"},
                      {ColumnIdentifier(Document_path{"gama"}), "ksi"}};
  *msg.mutable_grouping() =
      Grouping_list{{ColumnIdentifier(Document_path{"alpha"})},
                    {ColumnIdentifier(Document_path{"gama"})}};
  ASSERT_NO_THROW(builder()->build(msg));
  EXPECT_STREQ(
      "SELECT JSON_OBJECT('zeta', `_DERIVED_TABLE_`.`zeta`,'ksi', "
      "`_DERIVED_TABLE_`.`ksi`) AS doc FROM ("
      "SELECT JSON_EXTRACT(doc,'$.alpha') AS `zeta`,JSON_EXTRACT(doc,'$.gama') "
      "AS `ksi` "
      "FROM `xschema`.`xtable` "
      "GROUP BY JSON_EXTRACT(doc,'$.alpha'),JSON_EXTRACT(doc,'$.gama')"
      ") AS `_DERIVED_TABLE_`",
      query.get().c_str());
}

TEST_F(Find_statement_builder_test,
       build_document_with_grouping_no_projection) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_grouping()->Add() =
      Group(ColumnIdentifier(Document_path{"beta"}));
  EXPECT_THROW(builder()->build(msg), ngs::Error_code);
}

TEST_F(Find_statement_builder_test, build_document_shared_lock) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  msg.set_locking(Mysqlx::Crud::Find::SHARED_LOCK);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_projection() =
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "zeta"}};
  *msg.mutable_criteria() = Filter(
      Operator(">", ColumnIdentifier(Document_path{"delta"}), Scalar(1.0)));
  *msg.mutable_order() =
      Order_list{{ColumnIdentifier(Document_path{"gamma"}), Order::Base::DESC}};
  ASSERT_NO_THROW(builder()->build(msg));
  EXPECT_STREQ(
      "SELECT JSON_OBJECT('zeta', JSON_EXTRACT(doc,'$.alpha')) AS doc "
      "FROM `xschema`.`xtable` "
      "WHERE (JSON_EXTRACT(doc,'$.delta') > 1) "
      "ORDER BY JSON_EXTRACT(doc,'$.gamma') DESC "
      "FOR SHARE",
      query.get().c_str());
}

TEST_F(Find_statement_builder_test, build_document_exclusive_lock) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  msg.set_locking(Mysqlx::Crud::Find::EXCLUSIVE_LOCK);
  *msg.mutable_collection() = Collection("xtable", "xschema");
  *msg.mutable_projection() =
      Projection_list{{ColumnIdentifier(Document_path{"alpha"}), "zeta"}};
  *msg.mutable_criteria() = Filter(
      Operator(">", ColumnIdentifier(Document_path{"delta"}), Scalar(1.0)));
  *msg.mutable_order() =
      Order_list{{ColumnIdentifier(Document_path{"gamma"}), Order::Base::DESC}};
  ASSERT_NO_THROW(builder()->build(msg));
  EXPECT_STREQ(
      "SELECT JSON_OBJECT('zeta', JSON_EXTRACT(doc,'$.alpha')) AS doc "
      "FROM `xschema`.`xtable` "
      "WHERE (JSON_EXTRACT(doc,'$.delta') > 1) "
      "ORDER BY JSON_EXTRACT(doc,'$.gamma') DESC "
      "FOR UPDATE",
      query.get().c_str());
}

}  // namespace test
}  // namespace xpl
