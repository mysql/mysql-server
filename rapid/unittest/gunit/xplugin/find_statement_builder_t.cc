/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "find_statement_builder.h"
#include "query_string_builder.h"
#include "mysqld_error.h"
#include "expr_generator.h"
#include "ngs_common/protocol_protobuf.h"

#include <gtest/gtest.h>

namespace xpl
{
namespace test
{

class Find_statement_builder_impl: public Find_statement_builder
{
public:
  Find_statement_builder_impl(Expression_generator &gen) : Find_statement_builder(gen) {}
  using Find_statement_builder::add_table_projection;
  using Find_statement_builder::add_document_projection;
  using Find_statement_builder::add_grouping;
  using Find_statement_builder::add_grouping_criteria;
  using Find_statement_builder::Grouping_list;
  using Find_statement_builder::Grouping_criteria;
};


class Find_statement_builder_test : public ::testing::Test
{
public:
  Find_statement_builder_test()
  : args(*msg.mutable_args()),
    expr_gen(query, args, schema, true),
    builder(expr_gen)
    {}
  Find_statement_builder::Find msg;
  Expression_generator::Args &args;
  Query_string_builder query;
  std::string schema;
  Expression_generator expr_gen;
  Find_statement_builder_impl builder;

  enum {DM_DOCUMENT = 0, DM_TABLE = 1};

  typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Crud::Projection > Projection_list;
};


namespace
{
void operator<< (::google::protobuf::Message &msg, const std::string& txt)
{
  ASSERT_TRUE(::google::protobuf::TextFormat::ParseFromString(txt, &msg));
}
} // namespace


TEST_F(Find_statement_builder_test, add_projection_table_empty)
{
  Projection_list projection;
  ASSERT_NO_THROW(builder.add_table_projection(projection));
  EXPECT_EQ("*", query.get());
}


TEST_F(Find_statement_builder_test, add_document_projection_empty)
{
  Projection_list projection;
  ASSERT_NO_THROW(builder.add_document_projection(projection));
  EXPECT_EQ("doc", query.get());
}


TEST_F(Find_statement_builder_test, add_projection_table_one_member_item)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ document_path { type: MEMBER value: \"alpha\" } } }";
  ASSERT_NO_THROW(builder.add_table_projection(projection));
  EXPECT_EQ("JSON_EXTRACT(doc,'$.alpha')", query.get());
}


TEST_F(Find_statement_builder_test, add_projection_table_one_item)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ name: 'alpha' } }";
  ASSERT_NO_THROW(builder.add_table_projection(projection));
  EXPECT_EQ("`alpha`", query.get());
}


TEST_F(Find_statement_builder_test, add_projection_table_two_items)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ name: 'alpha' } }";
  *projection.Add() <<  "source { type: IDENT identifier "
      "{ name: 'beta' } }";
  ASSERT_NO_THROW(builder.add_table_projection(projection));
  EXPECT_EQ("`alpha`,`beta`", query.get());
}


TEST_F(Find_statement_builder_test, add_projection_table_two_items_placeholder)
{
  *args.Add() << "type: V_DOUBLE v_double: 2.2";

  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ name: 'alpha' } }";
  *projection.Add() <<  "source { type: PLACEHOLDER position: 0 }";
  ASSERT_NO_THROW(builder.add_table_projection(projection));
  EXPECT_EQ("`alpha`,2.2", query.get());
}


TEST_F(Find_statement_builder_test, add_projection_table_one_item_with_alias)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ name: 'alpha' } } alias: 'beta'";
  ASSERT_NO_THROW(builder.add_table_projection(projection));
  EXPECT_EQ("`alpha` AS `beta`", query.get());
}


TEST_F(Find_statement_builder_test, add_projection_document_one_item_no_alias)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ name: 'alpha' } }";
  EXPECT_THROW(builder.add_document_projection(projection), ngs::Error_code);
}


TEST_F(Find_statement_builder_test, add_projection_document_one_member_item)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ document_path { type: MEMBER value: \"alpha\" } } }"
      "alias: \"beta\"";
  ASSERT_NO_THROW(builder.add_document_projection(projection));
  EXPECT_EQ("JSON_OBJECT('beta', JSON_EXTRACT(doc,'$.alpha')) AS doc",
            query.get());
}


TEST_F(Find_statement_builder_test, add_projection_document_two_member_items)
{
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ document_path { type: MEMBER value: \"alpha\" } } }"
      "alias: \"beta\"";
  *projection.Add() << "source { type: IDENT identifier "
      "{ document_path { type: MEMBER value: \"first\" } } }"
      "alias: \"second\"";
  ASSERT_NO_THROW(builder.add_document_projection(projection));
  EXPECT_EQ("JSON_OBJECT('beta', JSON_EXTRACT(doc,'$.alpha'),"
      "'second', JSON_EXTRACT(doc,'$.first')) AS doc",
      query.get());
}


TEST_F(Find_statement_builder_test, add_projection_document_two_member_items_placeholder)
{
  *args.Add() << "type: V_DOUBLE v_double: 2.2";
  Projection_list projection;
  *projection.Add() << "source { type: IDENT identifier "
      "{ document_path { type: MEMBER value: \"alpha\" } } }"
      "alias: \"beta\"";
  *projection.Add() << "source {type: PLACEHOLDER position: 0}"
      "alias: \"second\"";
  ASSERT_NO_THROW(builder.add_document_projection(projection));
  EXPECT_EQ("JSON_OBJECT('beta', JSON_EXTRACT(doc,'$.alpha'),"
      "'second', 2.2) AS doc",
      query.get());
}


TEST_F(Find_statement_builder_test, add_gruping_empty)
{
  Find_statement_builder_impl::Grouping_list group;
  ASSERT_NO_THROW(builder.add_grouping(group));
  EXPECT_EQ("", query.get());
}


TEST_F(Find_statement_builder_test, add_gruping_one_item)
{
  Find_statement_builder_impl::Grouping_list group;
  *group.Add() << "type: IDENT identifier { name: 'alpha' }";
  ASSERT_NO_THROW(builder.add_grouping(group));
  EXPECT_EQ(" GROUP BY `alpha`", query.get());
}


TEST_F(Find_statement_builder_test, add_gruping_two_items)
{
  Find_statement_builder_impl::Grouping_list group;
  *group.Add() << "type: IDENT identifier { name: 'alpha' }";
  *group.Add() << "type: IDENT identifier { name: 'beta' }";
  ASSERT_NO_THROW(builder.add_grouping(group));
  EXPECT_EQ(" GROUP BY `alpha`,`beta`", query.get());
}


TEST_F(Find_statement_builder_test, add_gruping_two_items_placeholder)
{
  *args.Add() << "type: V_SINT v_signed_int: 2";

  Find_statement_builder_impl::Grouping_list group;
  *group.Add() << "type: IDENT identifier { name: 'alpha' }";
  *group.Add() << "type: PLACEHOLDER position: 0";
  ASSERT_NO_THROW(builder.add_grouping(group));
  EXPECT_EQ(" GROUP BY `alpha`,2", query.get());
}


TEST_F(Find_statement_builder_test, add_gruping_criteria)
{
  Find_statement_builder_impl::Grouping_criteria criteria;
  criteria << "type: OPERATOR operator {name: '>'"
      "param {type: IDENT identifier {name: 'alpha'}}"
      "param {type: LITERAL literal {type: V_DOUBLE v_double: 1.0}}}";
  ASSERT_NO_THROW(builder.add_grouping_criteria(criteria));
  EXPECT_EQ(" HAVING (`alpha` > 1)", query.get());
}


TEST_F(Find_statement_builder_test, add_gruping_criteria_placeholder)
{
  *args.Add() << "type: V_DOUBLE v_double: 2.3";

  Find_statement_builder_impl::Grouping_criteria criteria;
  criteria << "type: OPERATOR operator {name: '>'"
      "param {type: IDENT identifier {name: 'alpha'}}"
      "param {type: PLACEHOLDER position: 0}}";
  ASSERT_NO_THROW(builder.add_grouping_criteria(criteria));
  EXPECT_EQ(" HAVING (`alpha` > 2.3)", query.get());
}


TEST_F(Find_statement_builder_test, build_table)
{
  msg <<
      "collection {name: 'xtable' schema: 'xschema'}"
      "data_model: TABLE "
      "projection {source {type: IDENT identifier {name: 'alpha'}}"
      "            alias: 'zeta'} "
      "criteria {type: OPERATOR "
      "          operator {name: '>' "
      "                    param {type: IDENT identifier {name: 'delta'}}"
      "                    param {type: LITERAL literal "
      "                                                 {type: V_DOUBLE"
      "                                                  v_double: 1.0}}}}"
      "order {expr {type: IDENT identifier {name: 'gamma'}}"
      "       direction: DESC} "
      "grouping {type: IDENT identifier {name: 'beta'}}"
      "grouping_criteria {type: OPERATOR "
      "          operator {name: '<' "
      "                    param {type: IDENT identifier {name: 'lambda'}}"
      "                    param {type: LITERAL literal"
      "                                                {type: V_DOUBLE"
      "                                             v_double: 2.0}}}}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ(
      "SELECT `alpha` AS `zeta` "
      "FROM `xschema`.`xtable` "
      "WHERE (`delta` > 1) "
      "GROUP BY `beta` "
      "HAVING (`lambda` < 2) "
      "ORDER BY `gamma` DESC", query.get());
}


TEST_F(Find_statement_builder_test, build_document_no_grouping)
{
  msg <<
      "collection {name: 'xtable' schema: 'xschema'}"
      "data_model: DOCUMENT "
      "projection {source {type: IDENT identifier {document_path {type: MEMBER "
      "                                                             value: 'alpha'}}}"
      "            alias: 'zeta'} "
      "criteria {type: OPERATOR "
      "          operator {name: '>' "
      "                    param {type: IDENT identifier {document_path {type: MEMBER "
      "                                                                  value: 'delta'}}}"
      "                    param {type: LITERAL literal"
      "                                                {type: V_DOUBLE"
      "                                             v_double: 1.0}}}}"
      "order {expr {type: IDENT identifier {document_path {type: MEMBER "
      "                                                     value: 'gamma'}}}"
      "       direction: DESC}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ(
      "SELECT JSON_OBJECT('zeta', JSON_EXTRACT(doc,'$.alpha')) AS doc "
      "FROM `xschema`.`xtable` "
      "WHERE (JSON_EXTRACT(doc,'$.delta') > 1) "
      "ORDER BY JSON_EXTRACT(doc,'$.gamma') DESC",
      query.get());
}


TEST_F(Find_statement_builder_test, build_document_with_grouping_and_criteria)
{
  msg <<
      "collection {name: 'xtable' schema: 'xschema'}"
      "data_model: DOCUMENT "
      "projection {source {type: IDENT identifier {document_path {type: MEMBER "
      "                                                             value: 'alpha'}}}"
      "            alias: 'zeta'} "
      "criteria {type: OPERATOR "
      "          operator {name: '>' "
      "                    param {type: IDENT identifier {document_path {type: MEMBER "
      "                                                                  value: 'delta'}}}"
      "                    param {type: LITERAL literal"
      "                                                {type: V_DOUBLE"
      "                                             v_double: 1.0}}}}"
      "order {expr {type: IDENT identifier {document_path {type: MEMBER "
      "                                                     value: 'beta'}}}"
      "       direction: DESC} "
      "grouping {type: IDENT identifier {document_path {type: MEMBER "
      "                                                  value: 'alpha'}}}"
      "grouping_criteria {type: OPERATOR "
      "          operator {name: '<' "
      "                    param {type: IDENT identifier {document_path {type: MEMBER "
      "                                                                  value: 'lambda'}}}"
      "                    param {type: LITERAL literal"
      "                                                {type: V_DOUBLE"
      "                                                v_double: 2.0}}}}";
  ASSERT_NO_THROW(builder.build(msg));
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


TEST_F(Find_statement_builder_test, build_document_with_grouping)
{
  msg <<
      "collection {name: 'xtable' schema: 'xschema'}"
      "data_model: DOCUMENT "
      "projection {source {type: IDENT identifier {document_path {type: MEMBER "
      "                                                             value: 'alpha'}}}"
      "            alias: 'zeta'} "
      "projection {source {type: IDENT identifier {document_path {type: MEMBER "
      "                                                             value: 'gama'}}}"
      "            alias: 'ksi'} "
      "grouping {type: IDENT identifier {document_path {type: MEMBER "
      "                                                  value: 'alpha'}}}"
      "grouping {type: IDENT identifier {document_path {type: MEMBER "
      "                                                  value: 'gama'}}}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ(
      "SELECT JSON_OBJECT('zeta', `_DERIVED_TABLE_`.`zeta`,'ksi', `_DERIVED_TABLE_`.`ksi`) AS doc FROM ("
      "SELECT JSON_EXTRACT(doc,'$.alpha') AS `zeta`,JSON_EXTRACT(doc,'$.gama') AS `ksi` "
      "FROM `xschema`.`xtable` "
      "GROUP BY JSON_EXTRACT(doc,'$.alpha'),JSON_EXTRACT(doc,'$.gama')"
      ") AS `_DERIVED_TABLE_`",
      query.get());
}


TEST_F(Find_statement_builder_test, build_document_with_grouping_no_projection)
{
  msg <<
      "collection {name: 'xtable' schema: 'xschema'}"
      "data_model: DOCUMENT "
      "grouping {type: IDENT identifier {document_path {type: MEMBER "
      "                                                  value: 'beta'}}}";
  EXPECT_THROW(builder.build(msg), ngs::Error_code);
}

} // namespace test
} // namespace xpl
