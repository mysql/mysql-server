// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>            // TEST
#include <string>                   // string
#include "mysql/sets/sets.h"        // Int_set_traits
#include "mysql/strconv/strconv.h"  // throwing::encode_text

namespace {

using namespace mysql;

struct My_int_traits : public sets::Int_set_traits<int64_t> {};

struct My_string_traits
    : public sets::Ordered_set_traits_interface<My_string_traits, std::string> {
  static bool lt_impl(const std::string &a, const std::string &b) {
    return a < b;
  }
};
using My_interval_container = sets::Vector_interval_container<My_int_traits>;
using My_nested_set =
    sets::Map_nested_container<My_string_traits, My_interval_container>;

using Return_status_t = utils::Return_status;
constexpr auto return_ok = Return_status_t::ok;

TEST(LibsSetsNestedSetBasic, Basic) {
  My_nested_set nested_set;
  auto ret = nested_set.insert("a", 3);
  ASSERT_EQ(ret, return_ok);
  ret = nested_set.insert("a", 4);
  ASSERT_EQ(ret, return_ok);
  ret = nested_set.insert("b", 3);
  ASSERT_EQ(ret, return_ok);
  ret = nested_set.insert("b", 5);
  ASSERT_EQ(ret, return_ok);
  ASSERT_EQ(nested_set.size(), 2);
  ASSERT_EQ(strconv::throwing::encode_text(nested_set), "a:3-4,b:3,5");
}

}  // namespace
