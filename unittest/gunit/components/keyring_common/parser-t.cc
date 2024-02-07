/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include "template_utils.h"

namespace component_load_parser_unittest {

class ComponentLoadParser_test : public ::testing::Test {};

std::string group_separator = ";";
std::string component_separator = ",";

void get_next_group(std::string &groups, std::string &one_group) {
  if (groups.find(group_separator) != std::string::npos) {
    one_group = groups.substr(0, groups.find(group_separator));
    groups.erase(0, groups.find(group_separator) + 1);
  } else {
    one_group = groups;
    groups.clear();
  }
}

void get_next_component(std::string &components, std::string &one_component) {
  if (components.find(component_separator) != std::string::npos) {
    one_component = components.substr(0, components.find(component_separator));
    components.erase(0, components.find(component_separator) + 1);
  } else {
    one_component = components;
    components.clear();
  }
}

// Use myu::IsSpace rather than ::isspace to avoid linker warnings on MacOS.
void remove_spaces(std::string &groups) {
  groups.erase(std::remove_if(groups.begin(), groups.end(), myu::IsSpace),
               groups.end());
}

TEST_F(ComponentLoadParser_test, Parser) {
  std::string with_spaces =
      "   file://component1, file://component2, file://component3; "
      "file://component4, file://component5, file://component6 ;   ";
  std::string without_spaces =
      "file://component1,file://component2,file://component3;file://"
      "component4,file://component5,file://component6;";
  ;
  remove_spaces(with_spaces);
  EXPECT_TRUE(with_spaces == without_spaces);
  EXPECT_TRUE(with_spaces == without_spaces);
}

}  // namespace component_load_parser_unittest
