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

#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/src/delete_statement_builder.h"
#include "plugin/x/src/expr_generator.h"

namespace xpl {
namespace test {

class Delete_statement_builder_test : public ::testing::Test {
 public:
  Delete_statement_builder_test()
      : args(*msg.mutable_args()),
        expr_gen(&query, args, schema, true),
        builder(expr_gen) {}
  Delete_statement_builder::Delete msg;
  Expression_generator::Args &args;
  Query_string_builder query;
  std::string schema;
  Expression_generator expr_gen;
  Delete_statement_builder builder;
};

namespace {
void operator<<(::google::protobuf::Message &msg, const std::string &txt) {
  ASSERT_TRUE(::google::protobuf::TextFormat::ParseFromString(txt, &msg));
}
}  // namespace

TEST_F(Delete_statement_builder_test, build_table) {
  msg << "collection {name: 'xtable' schema: 'xschema'}"
         "data_model: TABLE "
         "criteria {type: OPERATOR "
         "          operator {name: '>' "
         "                    param {type: IDENT identifier {name: 'delta'}}"
         "                    param {type: LITERAL literal"
         "                                             {type: V_DOUBLE"
         "                                                 v_double: 1.0}}}}"
         "order {expr {type: IDENT identifier {name: 'gamma'}}"
         "       direction: DESC} "
         "limit {row_count: 2}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ(
      "DELETE FROM `xschema`.`xtable` "
      "WHERE (`delta` > 1) "
      "ORDER BY `gamma` DESC "
      "LIMIT 2",
      query.get());
}

TEST_F(Delete_statement_builder_test, build_document) {
  msg << "collection {name: 'xcoll' schema: 'xschema'}"
         "data_model: DOCUMENT "
         "criteria {type: OPERATOR "
         "          operator {name: '>' "
         "                    param {type: IDENT identifier {document_path "
         "{type: MEMBER "
         "                                                                  "
         "value: 'delta'}}}"
         "                    param {type: LITERAL literal "
         "                                          {type: V_DOUBLE"
         "                                              v_double: 1.0}}}}"
         "order {expr {type: IDENT identifier {document_path {type: MEMBER "
         "                                                     value: "
         "'gamma'}}}"
         "       direction: DESC} "
         "limit {row_count: 2}";
  ASSERT_NO_THROW(builder.build(msg));
  EXPECT_EQ(
      "DELETE FROM `xschema`.`xcoll` "
      "WHERE (JSON_EXTRACT(doc,'$.delta') > 1) "
      "ORDER BY JSON_EXTRACT(doc,'$.gamma') DESC "
      "LIMIT 2",
      query.get());
}

}  // namespace test
}  // namespace xpl
