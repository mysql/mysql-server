/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "plugin/x/src/insert_statement_builder.h"
#include "unittest/gunit/xplugin/xpl/mock/session.h"
#include "unittest/gunit/xplugin/xpl/mysqlx_pb_wrapper.h"

namespace xpl {
namespace test {
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

class Insert_statement_builder_stub : public Insert_statement_builder {
 public:
  explicit Insert_statement_builder_stub(Expression_generator *gen)
      : Insert_statement_builder(*gen, &m_id_agg) {}
  using Insert_statement_builder::add_document;
  using Insert_statement_builder::add_documents;
  using Insert_statement_builder::add_projection;
  using Insert_statement_builder::add_row;
  using Insert_statement_builder::add_upsert;
  using Insert_statement_builder::add_values;
  StrictMock<ngs::test::Mock_id_generator> mock_id_generator;
  Insert_statement_builder::Document_id_list m_id_list;
  Insert_statement_builder::Document_id_aggregator m_id_agg{&mock_id_generator,
                                                            &m_id_list};
};

class Insert_statement_builder_test : public ::testing::Test {
 public:
  void SetUp() {
    expr_gen.reset(new Expression_generator(&query, args, schema,
                                            is_table_data_model(msg)));
    stub.reset(new Insert_statement_builder_stub(expr_gen.get()));

    EXPECT_CALL(stub->mock_id_generator, generate(_))
        .WillRepeatedly(Return("0ff0"));
  }

  Insert_statement_builder_stub &builder() { return *stub; }

  Insert_statement_builder::Insert msg;
  Expression_generator::Args &args = *msg.mutable_args();
  Query_string_builder query;
  std::string schema;
  std::unique_ptr<Expression_generator> expr_gen;
  std::unique_ptr<Insert_statement_builder_stub> stub;

  enum { k_dm_document = 0, k_dm_table = 1 };
};

const char *const k_doc_example1 = R"({"_id":"abc1", "one":1})";
const char *const k_doc_example2 = R"({"_id":"abc2", "two":2})";
const char *const k_doc_example_no_id = R"({"three":3})";
#define EXPECT_DOC_EXAMPLE1 "{\\\"_id\\\":\\\"abc1\\\", \\\"one\\\":1}"
#define EXPECT_DOC_EXAMPLE2 "{\\\"_id\\\":\\\"abc2\\\", \\\"two\\\":2}"
#define EXPECT_DOC_EXAMPLE_NO_ID "{\\\"three\\\":3}"

TEST_F(Insert_statement_builder_test, add_row_empty_projection_empty_row) {
  ASSERT_THROW(builder().add_row(Field_list(), 0), ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_row_one_projection_empty_row) {
  ASSERT_THROW(builder().add_row(Field_list(), 1), ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_row_full_row_projection_empty) {
  ASSERT_NO_THROW(builder().add_row(Field_list{"one"}, 0));
  EXPECT_STREQ("('one')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_row_half_row_full_projection) {
  ASSERT_THROW(builder().add_row(Field_list{"one"}, 2), ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_row_full_row_full_projection) {
  ASSERT_NO_THROW(builder().add_row(Field_list{"one", "two"}, 2));
  EXPECT_STREQ("('one','two')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_values_empty_list) {
  ASSERT_THROW(builder().add_values(Row_list(), 1), ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_values_one_row) {
  ASSERT_NO_THROW(builder().add_values(Row_list{{"one", "two"}}, 0));
  EXPECT_STREQ(" VALUES ('one','two')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_values_one_row_with_arg) {
  *args.Add() = Scalar("two");

  ASSERT_NO_THROW(builder().add_values(Row_list{{"one", Placeholder(0)}}, 0));
  EXPECT_STREQ(" VALUES ('one','two')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_values_one_row_missing_arg) {
  EXPECT_THROW(builder().add_values(Row_list{{"one", Placeholder(0)}}, 0),
               Expression_generator::Error);
}

TEST_F(Insert_statement_builder_test, add_values_two_rows) {
  Row_list values{{"one", "two"}, {"three", "four"}};
  ASSERT_NO_THROW(builder().add_values(values, values.size()));
  EXPECT_STREQ(" VALUES ('one','two'),('three','four')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_values_two_rows_with_args) {
  *args.Add() = Scalar("two");
  *args.Add() = Scalar("four");

  Row_list values{{"one", Placeholder(0)}, {"three", Placeholder(1)}};
  ASSERT_NO_THROW(builder().add_values(values, values.size()));
  EXPECT_STREQ(" VALUES ('one','two'),('three','four')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_projection_tabel_empty) {
  ASSERT_NO_THROW(
      builder().add_projection(Column_projection_list(), k_dm_table));
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_projection_tabel_one_item) {
  ASSERT_NO_THROW(builder().add_projection(
      Column_projection_list{Column("first")}, k_dm_table));
  EXPECT_STREQ(" (`first`)", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_projection_tabel_two_items) {
  ASSERT_NO_THROW(builder().add_projection(
      Column_projection_list{Column("first"), Column("second")}, k_dm_table));
  EXPECT_STREQ(" (`first`,`second`)", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_projection_document_empty) {
  ASSERT_NO_THROW(
      builder().add_projection(Column_projection_list(), k_dm_document));
  EXPECT_STREQ(" (doc)", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_projection_document_one_item) {
  ASSERT_THROW(builder().add_projection(Column_projection_list{Column("first")},
                                        k_dm_document),
               ngs::Error_code);
}

TEST_F(Insert_statement_builder_test, add_upsert) {
  ASSERT_NO_THROW(builder().add_upsert(k_dm_document));
  EXPECT_STREQ(
      " ON DUPLICATE KEY UPDATE"
      " doc = IF(JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))"
      " = JSON_UNQUOTE(JSON_EXTRACT(VALUES(doc), '$._id')),"
      " VALUES(doc), MYSQLX_ERROR(5018))",
      query.get().c_str());
  ASSERT_THROW(builder().add_upsert(k_dm_table), ngs::Error_code);
}

TEST_F(Insert_statement_builder_test, build_document) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  *msg.mutable_collection() = Collection("xcoll", "xtest");
  *msg.mutable_row() = Row_list{{k_doc_example1}, {k_doc_example2}};
  ASSERT_NO_THROW(builder().build(msg));
  EXPECT_STREQ(
      "INSERT INTO `xtest`.`xcoll` (doc) "
      "VALUES ('" EXPECT_DOC_EXAMPLE1 "'),('" EXPECT_DOC_EXAMPLE2 "')",
      query.get().c_str());
}

TEST_F(Insert_statement_builder_test, build_table) {
  msg.set_data_model(Mysqlx::Crud::TABLE);
  *msg.mutable_collection() = Collection("xtable", "xtest");
  *msg.mutable_projection() =
      Column_projection_list{Column("one"), Column("two")};
  *msg.mutable_row() = Row_list{{"first", "second"}};
  ASSERT_NO_THROW(builder().build(msg));
  EXPECT_STREQ(
      "INSERT INTO `xtest`.`xtable` (`one`,`two`) "
      "VALUES ('first','second')",
      query.get().c_str());
}

TEST_F(Insert_statement_builder_test, build_document_upsert) {
  msg.set_data_model(Mysqlx::Crud::DOCUMENT);
  msg.set_upsert(true);
  *msg.mutable_collection() = Collection("xcoll", "xtest");
  *msg.mutable_row() = Row_list{{k_doc_example1}, {k_doc_example2}};
  ASSERT_NO_THROW(builder().build(msg));
  EXPECT_STREQ(
      "INSERT INTO `xtest`.`xcoll` (doc) VALUES "
      "('" EXPECT_DOC_EXAMPLE1 "'),('" EXPECT_DOC_EXAMPLE2
      "') ON DUPLICATE KEY UPDATE"
      " doc = IF(JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))"
      " = JSON_UNQUOTE(JSON_EXTRACT(VALUES(doc), '$._id')),"
      " VALUES(doc), MYSQLX_ERROR(5018))",
      query.get().c_str());
}

TEST_F(Insert_statement_builder_test, build_table_upsert) {
  msg.set_data_model(Mysqlx::Crud::TABLE);
  msg.set_upsert(true);
  *msg.mutable_collection() = Collection("xcoll", "xtest");
  *msg.mutable_row() = Row_list{{"first"}, {"second"}};
  ASSERT_THROW(builder().build(msg), ngs::Error_code);
}

TEST_F(Insert_statement_builder_test, add_documents_empty_list) {
  ASSERT_THROW(builder().add_documents(Row_list()), ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_documents_one_row) {
  ASSERT_NO_THROW(builder().add_documents(Row_list{{k_doc_example1}}));
  EXPECT_STREQ(" VALUES ('" EXPECT_DOC_EXAMPLE1 "')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_documents_one_row_with_arg) {
  *args.Add() = Scalar(k_doc_example2);

  ASSERT_NO_THROW(builder().add_documents(Row_list{{Placeholder(0)}}));
  EXPECT_STREQ(" VALUES ('" EXPECT_DOC_EXAMPLE2 "')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_documents_one_row_missing_arg) {
  EXPECT_THROW(builder().add_documents(Row_list{{Placeholder(0)}}),
               Expression_generator::Error);
}

TEST_F(Insert_statement_builder_test, add_documents_two_rows) {
  ASSERT_NO_THROW(
      builder().add_documents(Row_list{{k_doc_example1}, {k_doc_example2}}));
  EXPECT_STREQ(" VALUES ('" EXPECT_DOC_EXAMPLE1 "'),('" EXPECT_DOC_EXAMPLE2
               "')",
               query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_document_empty_row) {
  ASSERT_THROW(builder().add_document(Field_list()), ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_document_two_docs) {
  ASSERT_THROW(
      builder().add_document(Field_list{k_doc_example1, k_doc_example2}),
      ngs::Error_code);
  EXPECT_STREQ("", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_document_placeholder) {
  *args.Add() = Scalar(k_doc_example1);
  ASSERT_NO_THROW(builder().add_document(Field_list{Placeholder(0)}));
  EXPECT_STREQ("('" EXPECT_DOC_EXAMPLE1 "')", query.get().c_str());
}

TEST_F(Insert_statement_builder_test, add_document_placeholder_wrong_type) {
  *args.Add() = Scalar(3.14);
  ASSERT_NO_THROW(builder().add_document(Field_list{Placeholder(0)}));
  EXPECT_STREQ("(3.14)", query.get().c_str());
}

struct Param_add_document {
  std::string expect;
  Expr fields;
};

class Add_document_param_test
    : public Insert_statement_builder_test,
      public ::testing::WithParamInterface<Param_add_document> {};

TEST_P(Add_document_param_test, add_document) {
  const Param_add_document &param = GetParam();
  ASSERT_NO_THROW(builder().add_document(Field_list{param.fields}));
  EXPECT_STREQ(param.expect.c_str(), query.get().c_str());
}

Param_add_document add_document_param[] = {
    {"('" EXPECT_DOC_EXAMPLE1 "')", k_doc_example1},
    {"(3.14)", 3.14},
    {"(JSON_OBJECT('_id','abc1','one',1))",
     Object{{"_id", "abc1"}, {"one", 1}}},
    {"('" EXPECT_DOC_EXAMPLE1 "')",
     Scalar{k_doc_example1, Expression_generator::CT_PLAIN}},
    {"('" EXPECT_DOC_EXAMPLE1 "')",
     Scalar{k_doc_example1, Expression_generator::CT_JSON}},
    {"('abc')", Scalar{"abc", Expression_generator::CT_XML}},
    {"(JSON_SET('" EXPECT_DOC_EXAMPLE_NO_ID "', '$._id', '0ff0'))",
     k_doc_example_no_id},
    {"(JSON_SET('{}', '$._id', '0ff0'))", "{}"},
    {"(JSON_SET(JSON_OBJECT('tree',3), '$._id', '0ff0'))", Object{{"tree", 3}}},
    {"(JSON_SET(JSON_OBJECT(), '$._id', '0ff0'))", Object{}},
    {"(JSON_SET(JSON_OBJECT('extra',"
     "JSON_OBJECT('_id','abc1','one',1)), '$._id', '0ff0'))",
     Object{{"extra", Object{{"_id", "abc1"}, {"one", 1}}}}},
    {"(JSON_SET(JSON_OBJECT('extra','" EXPECT_DOC_EXAMPLE1
     "'), '$._id', '0ff0'))",
     Object{{"extra", k_doc_example1}}},
    {"(JSON_SET('{\\\"extra\\\":" EXPECT_DOC_EXAMPLE2 "}', '$._id', '0ff0'))",
     Scalar::String(std::string(R"({"extra":)") + k_doc_example2 + "}")},
    {"('{\\\"_id\\\":\\\"abc3\\\","
     " \\\"extra\\\":" EXPECT_DOC_EXAMPLE2 "}')",
     Scalar::String(std::string(R"({"_id":"abc3", "extra":)") + k_doc_example2 +
                    "}")},
    {"('{\\\"extra\\\":" EXPECT_DOC_EXAMPLE2 ", \\\"_id\\\":\\\"abc3\\\"}')",
     Scalar::String(std::string(R"({"extra":)") + k_doc_example2 +
                    R"(, "_id":"abc3"})")}};

INSTANTIATE_TEST_CASE_P(Insert_statement_builder_add_document,
                        Add_document_param_test,
                        testing::ValuesIn(add_document_param));
}  // namespace test
}  // namespace xpl
