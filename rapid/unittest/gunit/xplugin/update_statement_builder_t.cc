/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "update_statement_builder.h"
#include "mysqld_error.h"
#include "ngs_common/protocol_protobuf.h"

#include <gtest/gtest.h>

namespace xpl
{
namespace test
{

class Update_statement_builder_impl: public Update_statement_builder
{
public:
  Update_statement_builder_impl(Expression_generator &gen) : Update_statement_builder(gen) {}
  using Update_statement_builder::add_operation;
  using Update_statement_builder::add_table_operation;
  using Update_statement_builder::add_document_operation;
  using Update_statement_builder::add_document_operation_item;
  using Update_statement_builder::Operation_list;
  using Update_statement_builder::Operation_item;
  using Update_statement_builder::Generator;
};


class Update_statement_builder_test : public ::testing::Test
{
public:
  Update_statement_builder_test()
  : args(*msg.mutable_args()),
    expr_gen(query, args, schema, true),
    builder(expr_gen),
    oper(-1)
  {}
  Update_statement_builder::Update msg;
  Expression_generator::Args &args;
  Query_string_builder query;
  std::string schema;
  Expression_generator expr_gen;
  Update_statement_builder_impl builder;
  int oper;

  enum {DM_DOCUMENT = 0, DM_TABLE = 1};

  typedef ::Mysqlx::Crud::UpdateOperation UpdateOperation;
};



namespace
{

void operator<< (::google::protobuf::Message &msg, const std::string& txt)
{
  ASSERT_TRUE(::google::protobuf::TextFormat::ParseFromString(txt, &msg));
}

const std::string value_ = "value: {type: LITERAL literal {type: ";
const std::string value_1 = value_ + "V_DOUBLE v_double: 1.0}}";
const std::string value_2 = value_ + "V_STRING v_string: {value: 'two'}}}";
const std::string value_3 = value_ + "V_SINT v_signed_int: -3}}";
const std::string placeholder_0 = "value: {type: PLACEHOLDER position: 0}";
} // namespace


TEST_F(Update_statement_builder_test, add_operation_empty_list)
{
  Update_statement_builder_impl::Operation_list operation;
  EXPECT_THROW(builder.add_operation(operation, DM_TABLE), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_table_operation_one_item)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield'}"
      "operation: SET " + value_1;
  EXPECT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ("`xfield`=1", query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_two_items)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield'}"
      "operation: SET " + value_1;;
  *operation.Add() << "source {name: 'yfield'}"
      "operation: SET "  + value_2;
  EXPECT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ("`xfield`=1,`yfield`='two'", query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_two_items_same_source)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield'}"
      "operation: SET " + value_1;;
  *operation.Add() << "source {name: 'xfield'}"
      "operation: SET "  + value_2;
  EXPECT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ("`xfield`=1,`xfield`='two'", query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_two_items_placeholder)
{
  *args.Add() << "type: V_DOUBLE v_double: 2.2";

  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield'}"
      "operation: SET " + value_1;;
  *operation.Add() << "source {name: 'yfield'}"
      "operation: SET " + placeholder_0;
  EXPECT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ("`xfield`=1,`yfield`=2.2", query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_empty_name)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {} operation: SET " + value_1;;
  EXPECT_THROW(builder.add_table_operation(operation), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_table_operation_item_name_with_table)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield' table_name: 'xtable'}"
      "operation: SET " + value_1;
  EXPECT_THROW(builder.add_table_operation(operation), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_table_operation_item_name_with_table_and_schema)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield' table_name: 'xtable' schema_name: 'xschema'}"
      "operation: SET " + value_1;
  EXPECT_THROW(builder.add_table_operation(operation), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_operation_one_item_for_table)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {name: 'xfield'}"
        "operation: SET " + value_1;
  EXPECT_NO_THROW(builder.add_operation(operation, DM_TABLE));
  EXPECT_EQ(" SET `xfield`=1", query.get());
}

namespace
{
const std::string table_full_message(
    "collection {name: 'xtable' schema: 'xschema'}"
    "data_model: TABLE "
    "operation {source {name: 'yfield'}"
    "           operation: SET"
    "           value {type: LITERAL literal {type: V_OCTETS"
    "                                     v_octets {value: 'booom'}}}}"
    "criteria {type: OPERATOR "
    "          operator {name: '>' "
    "                    param {type: IDENT identifier {name: 'xfield'}}"
    "                    param {type: LITERAL literal {type: V_DOUBLE"
    "                                                     v_double: 1.0}}}}"
    "order {expr {type: IDENT identifier {name: 'xfield'}}"
    "       direction: DESC}");
} // namespace


TEST_F(Update_statement_builder_test, build_update_for_table)
{
  msg << table_full_message + "limit {row_count: 2}";
  EXPECT_NO_THROW(builder.build(msg));
  EXPECT_EQ("UPDATE `xschema`.`xtable`"
      " SET `yfield`='booom'"
      " WHERE (`xfield` > 1)"
      " ORDER BY `xfield` DESC"
      " LIMIT 2", query.get());
}

TEST_F(Update_statement_builder_test, build_update_for_table_forrbiden_offset_in_limit)
{
  msg << table_full_message + "limit {row_count: 2 offset: 5}";
  EXPECT_THROW(builder.build(msg), ngs::Error_code);
}


namespace
{
const std::string source_ = "source {document_path {type: MEMBER value: ";
const std::string source_index_ = "source {document_path {type: ARRAY_INDEX index: ";
const std::string source_first = source_ + "'first'}}";
const std::string source_second = source_ + "'second'}}";
const std::string source_third = source_ + "'third'}}";
const std::string source_index_first_0 = source_ + "'first'} document_path {type:ARRAY_INDEX index: 0}}";
const std::string source_index_0 = source_index_ + " 0}}";
const std::string source_index_1 = source_index_ + " 1}}";

const std::string document_full_message(
    "collection {name: 'xtable' schema: 'xschema'}"
    "data_model: DOCUMENT "
    "operation {source {document_path {type: MEMBER value: 'first'}}"
    "           operation: ITEM_SET"
    "           value: {type: LITERAL literal {type: V_DOUBLE v_double: 1.0}}}"
    "criteria {type: OPERATOR "
    "          operator {name: '>' "
    "                    param {type: IDENT identifier {document_path {type: MEMBER value: 'second'}}}"
    "                    param {type: LITERAL literal {type: V_DOUBLE"
    "                                              v_double: 1.0}}}}"
    "order {expr {type: IDENT identifier {document_path {type: MEMBER value: 'third'}}}"
    "       direction: DESC}");
} // namespace


TEST_F(Update_statement_builder_test, add_document_operation_not_allowed_set)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: SET " + value_1;
  EXPECT_THROW(builder.add_document_operation(operation), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_document_operation_remove)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_REMOVE ";
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_REMOVE(doc,'$.first')", query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_set)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_SET " + value_1;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(doc,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_replace)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_REPLACE " + value_1;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_REPLACE(doc,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_merge)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first +
      "operation: ITEM_MERGE "
      "value {type: LITERAL literal {type: V_OCTETS v_octets {value: '{\\\"two\\\": 2.0}'}}}";
  ASSERT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ(
      "doc=JSON_MERGE(doc,IF(JSON_TYPE('{\\\"two\\\": 2.0}')='OBJECT',"
      "JSON_REMOVE('{\\\"two\\\": 2.0}','$._id'),'_ERROR_'))",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_array_insert)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_index_first_0 + "operation: ARRAY_INSERT " + value_1;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_ARRAY_INSERT(doc,'$.first[0]',1)",
        query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_array_append)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ARRAY_APPEND " + value_1;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_ARRAY_APPEND(doc,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_array_append_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ARRAY_APPEND " + value_1;
  *operation.Add() << source_first + "operation: ARRAY_APPEND " + value_2;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_ARRAY_APPEND(doc,'$.first',1,'$.first','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_remove_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_REMOVE ";
  *operation.Add() << source_second + "operation: ITEM_REMOVE ";
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_REMOVE(doc,'$.first','$.second')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_set_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_SET " + value_1;
  *operation.Add() << source_second + "operation: ITEM_SET " + value_2;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(doc,'$.first',1,'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_set_twice_placeholder)
{
  *args.Add() << "type: V_DOUBLE v_double: 2.2";
  *args.Add() << "type: V_OCTETS v_octets {value: '$.second'}";
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_SET " + value_1;
  *operation.Add() << source_second + "operation: ITEM_SET " + placeholder_0;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(doc,'$.first',1,'$.second',2.2)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_merge_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << "source {} operation: ITEM_MERGE "
      "value {type: LITERAL literal {type: V_OCTETS v_octets {value: '{\\\"two\\\": 2.0}'}}}";
  *operation.Add() << "source {} operation: ITEM_MERGE "
      "value {type: LITERAL literal {type: V_OCTETS v_octets {value: '{\\\"three\\\": 3.0}'}}}";
  ASSERT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ(
      "doc=JSON_MERGE(doc,IF(JSON_TYPE('{\\\"two\\\": 2.0}')='OBJECT',"
      "JSON_REMOVE('{\\\"two\\\": 2.0}','$._id'),'_ERROR_'),"
      "IF(JSON_TYPE('{\\\"three\\\": 3.0}')='OBJECT',"
      "JSON_REMOVE('{\\\"three\\\": 3.0}','$._id'),'_ERROR_'))",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_remove_set)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_REMOVE ";
  *operation.Add() << source_second + "operation: ITEM_SET " + value_2;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(JSON_REMOVE(doc,'$.first'),'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_remove_twice_set)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_REMOVE ";
  *operation.Add() << source_second + "operation: ITEM_REMOVE ";
  *operation.Add() << source_third + "operation: ITEM_SET " + value_3;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(JSON_REMOVE(doc,'$.first','$.second'),'$.third',-3)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_set_remove_set)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_SET " + value_1;
  *operation.Add() << source_second + "operation: ITEM_REMOVE ";
  *operation.Add() << source_third + "operation: ITEM_SET "  + value_3;
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(JSON_REMOVE("
            "JSON_SET(doc,'$.first',1),'$.second'),'$.third',-3)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_set_merge)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_SET " + value_1;
  *operation.Add() << "source {} operation: ITEM_MERGE "
      "value {type: LITERAL literal {type: V_OCTETS v_octets {value: '{\\\"three\\\": 3.0}'}}}";
  ASSERT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ(
      "doc=JSON_MERGE(JSON_SET(doc,'$.first',1),"
      "IF(JSON_TYPE('{\\\"three\\\": 3.0}')='OBJECT',"
      "JSON_REMOVE('{\\\"three\\\": 3.0}','$._id'),'_ERROR_'))",
            query.get());
}


TEST_F(Update_statement_builder_test, add_document_operation_item_forbiden_column)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {name: 'xcolumn'} operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_forbiden_schema)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {schema_name: 'xschema'} operation: ITEM_SET "
      + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_forbiden_table)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {table_name: 'xtable'} operation: ITEM_SET "
      + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_forbiden_id_change)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {document_path {type: MEMBER value: '_id'}} operation: ITEM_SET "
      + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_empty_document_path)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {} operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_root_path)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {document_path {type: MEMBER value: ''}} operation: ITEM_SET " + value_3;
  ASSERT_NO_THROW(builder.add_document_operation_item(operation, oper));
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_empty_member)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {document_path {type: MEMBER value: 'first'} "
      "document_path {type: MEMBER value: ''}} "
      "operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               xpl::Expression_generator::Error);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_empty_member_reverse)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {document_path {type: MEMBER value: ''} "
      "document_path {type: MEMBER value: 'first'}} "
      "operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               xpl::Expression_generator::Error);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_root_as_array)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << source_index_0 + "operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_root_as_array_asterisk)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {document_path {type: ARRAY_INDEX_ASTERISK}} "
      "operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_document_operation_item_root_double_asterisk)
{
  Update_statement_builder_impl::Operation_item operation;
  operation << "source {document_path {type: DOUBLE_ASTERISK}} "
      "operation: ITEM_SET " + value_3;
  ASSERT_THROW(builder.add_document_operation_item(operation, oper),
               ngs::Error_code);
  ASSERT_EQ(UpdateOperation::ITEM_SET, oper);
}


TEST_F(Update_statement_builder_test, add_operation_one_item_for_document)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << source_first + "operation: ITEM_SET " + value_1;
  EXPECT_NO_THROW(builder.add_operation(operation, DM_DOCUMENT));
  EXPECT_EQ(" SET doc=JSON_SET(doc,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, build_update_for_document)
{
  msg << document_full_message + "limit {row_count: 2}";
  EXPECT_NO_THROW(builder.build(msg));
  EXPECT_EQ("UPDATE `xschema`.`xtable` "
      "SET doc=JSON_SET(doc,'$.first',1) "
      "WHERE (JSON_EXTRACT(doc,'$.second') > 1) "
      "ORDER BY JSON_EXTRACT(doc,'$.third') "
      "DESC LIMIT 2",
      query.get());
}


namespace
{
std::string get_operation(const std::string &name, const std::string &member,
                          const std::string &oper, const std::string &value)
{
  std::string str("source {");
  if (!name.empty())
    str += "name: '" + name + "' ";
  if (!member.empty())
  {
    str += "document_path {type: ";
    if (isdigit(member[0]))
    {
      str += "ARRAY_INDEX index: " + member;
    }
    else
    {
      str += "MEMBER ";
      if (member != "$")
        str += "value: '" + member + "' ";
    }
    str += "}";
  }
  str += "} operation: " + oper;
  if (!value.empty())
    str += " " + value;
  return str;
}

} // namespace


TEST_F(Update_statement_builder_test, add_document_operation_set_whole_doc)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("", "$", "ITEM_SET", value_2);
  EXPECT_NO_THROW(builder.add_document_operation(operation));
  EXPECT_EQ("doc=JSON_SET(doc,'$','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_set_needless_doc_path)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "SET", value_1);
  EXPECT_THROW(builder.add_table_operation(operation), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_missing_doc_path)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "", "ITEM_SET", value_1);
  EXPECT_THROW(builder.add_table_operation(operation), ngs::Error_code);
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_SET", value_1);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ("`xfield`=JSON_SET(`xfield`,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_SET", value_1);
  *operation.Add() << get_operation("xfield", "second", "ITEM_SET", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_SET(`xfield`,'$.first',1,'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_twice_but_different)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_SET", value_1);
  *operation.Add() << get_operation("yfield", "second", "ITEM_SET", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_SET(`xfield`,'$.first',1),"
      "`yfield`=JSON_SET(`yfield`,'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_triple)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_SET", value_1);
  *operation.Add() << get_operation("xfield", "second", "ITEM_SET", value_2);
  *operation.Add() << get_operation("xfield", "third", "ITEM_SET", value_3);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_SET(`xfield`,'$.first',1,'$.second','two','$.third',-3)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_mix_first)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "", "SET", value_1);
  *operation.Add() << get_operation("xfield", "second", "ITEM_SET", value_2);
  *operation.Add() << get_operation("xfield", "third", "ITEM_SET", value_3);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=1,"
      "`xfield`=JSON_SET(`xfield`,'$.second','two','$.third',-3)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_mix_last)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "second", "ITEM_SET", value_2);
  *operation.Add() << get_operation("xfield", "third", "ITEM_SET", value_3);
  *operation.Add() << get_operation("xfield", "", "SET", value_1);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_SET(`xfield`,'$.second','two','$.third',-3),"
      "`xfield`=1",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_mix_middle)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "second", "ITEM_SET", value_2);
  *operation.Add() << get_operation("xfield", "", "SET", value_1);
  *operation.Add() << get_operation("xfield", "third", "ITEM_SET", value_3);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_SET(`xfield`,'$.second','two'),"
      "`xfield`=1,"
      "`xfield`=JSON_SET(`xfield`,'$.third',-3)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_set_fourth)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_SET", value_1);
  *operation.Add() << get_operation("xfield", "second", "ITEM_SET", value_2);
  *operation.Add() << get_operation("yfield", "first", "ITEM_SET", value_1);
  *operation.Add() << get_operation("yfield", "second", "ITEM_SET", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_SET(`xfield`,'$.first',1,'$.second','two'),"
      "`yfield`=JSON_SET(`yfield`,'$.first',1,'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_remove_one)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_REMOVE", "");
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_REMOVE(`xfield`,'$.first')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_remove_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_REMOVE", "");
  *operation.Add() << get_operation("xfield", "second", "ITEM_REMOVE", "");
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_REMOVE(`xfield`,'$.first','$.second')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_replace_one)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_REPLACE", value_1);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_REPLACE(`xfield`,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_replace_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_REPLACE", value_1);
  *operation.Add() << get_operation("xfield", "second", "ITEM_REPLACE", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_REPLACE(`xfield`,'$.first',1,'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_merge_one)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_MERGE", value_1);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_MERGE(`xfield`,1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_item_merge_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ITEM_MERGE", value_1);
  *operation.Add() << get_operation("xfield", "second", "ITEM_MERGE", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_MERGE(`xfield`,1,'two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_array_insert_one)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "0", "ARRAY_INSERT", value_1);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_ARRAY_INSERT(`xfield`,'$[0]',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_array_insert_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "0", "ARRAY_INSERT", value_1);
  *operation.Add() << get_operation("xfield", "1", "ARRAY_INSERT", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_ARRAY_INSERT(`xfield`,'$[0]',1,'$[1]','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_array_append_one)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ARRAY_APPEND", value_1);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_ARRAY_APPEND(`xfield`,'$.first',1)",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_array_append_twice)
{
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ARRAY_APPEND", value_1);
  *operation.Add() << get_operation("xfield", "second", "ARRAY_APPEND", value_2);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_ARRAY_APPEND(`xfield`,'$.first',1,'$.second','two')",
            query.get());
}


TEST_F(Update_statement_builder_test, add_table_operation_array_append_twice_placeholder)
{
  *args.Add() << "type: V_DOUBLE v_double: 2.2";
  Update_statement_builder_impl::Operation_list operation;
  *operation.Add() << get_operation("xfield", "first", "ARRAY_APPEND", value_1);
  *operation.Add() << get_operation("xfield", "second", "ARRAY_APPEND", placeholder_0);
  ASSERT_NO_THROW(builder.add_table_operation(operation));
  EXPECT_EQ(
      "`xfield`=JSON_ARRAY_APPEND(`xfield`,'$.first',1,'$.second',2.2)",
            query.get());
}

} // namespace test
} // namespace xpl


