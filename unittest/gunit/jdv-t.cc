/* Copyright (c) 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>
#include <sstream>

#include "sql-common/json_dom.h"
#include "sql/json_duality_view/content_tree.h"
#include "sql/json_duality_view/ostream_utils.h"

// Unit testing of jdv dml code. So far it only contains test cases to improve
// code coverage of functions and operators which are only used for debugging
// and which would not otherwise be run during gcov testing.

namespace jdv_unit {
std::string test_ostream_operators();
}

TEST(Jdv, ostream_Bindings) {
  std::string expected =
      "empty:Single{{INVALID,, dw:0}:EMPTY}\n"
      "unresolved:Single{{INVALID,, dw:0}, path_str(bin.bound_object):$ "
      ":UNRESOLVED}\n"
      "resolved:Single{{INVALID,, dw:0}, path_str(bin.bound_object):$ "
      "pkrc:{,(P), nullptr:Json_dom{nullptr}, dr: no} }\n"
      "empty:Two{{INVALID,, dw:0}:EMPTY}\n"
      "unresolved:Two{{INVALID,, dw:0} bo:$ eo:nullptr :UNRESOLVED}\n"
      "resolved:Two{{INVALID,, dw:0} bo:$ eo:nullptr pkrc:{,(P), "
      "nullptr:Json_dom{nullptr}, dr: no} }}";
  EXPECT_EQ(expected, jdv_unit::test_ostream_operators());
}

TEST(Jdv, Content_tree_node_type_str) {
  EXPECT_STREQ("INVALID", jdv::str(jdv::Content_tree_node::Type::INVALID));
  EXPECT_STREQ("ROOT", jdv::str(jdv::Content_tree_node::Type::ROOT));
  EXPECT_STREQ("SINGLETON_CHILD",
               jdv::str(jdv::Content_tree_node::Type::SINGLETON_CHILD));
  EXPECT_STREQ("NESTED_CHILD",
               jdv::str(jdv::Content_tree_node::Type::NESTED_CHILD));
}

TEST(Jdv, ostream_Content_tree_node) {
  jdv::Content_tree_node node;
  node.set_name("Unit test node");
  node.set_qualified_table_name("`some_schema`.`some_table`");
  node.set_dependency_weight(42);
  std::stringstream s;
  s << node;
  EXPECT_EQ("Unit test node{INVALID,`some_schema`.`some_table`, dw:42}",
            s.str());
  jdv::Key_column_info kci;
  kci.set_key("k1");
  kci.set_column_name("c1");

  s.str("");
  s << kci;
  EXPECT_EQ("k1,c1(P)", s.view());
}

TEST(Jdv, path_str) {
  auto forty_two = Json_uint{42};
  EXPECT_EQ("$", jdv::path_str(&forty_two));
}
