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
#include <iterator>                 // forward_iterator
#include "mysql/sets/sets.h"        // Int_set_traits
#include "mysql/strconv/strconv.h"  // throwing::encode_text

namespace {

using namespace mysql;

// ==== Basic tests for std::sets::volume ====
//
// This test verifies that volume(set) gives the correct result even when
// the result exceeds 2^64, and even for deeply nested sets (here, 3 levels of
// nested sets).

struct My_int_traits : public sets::Int_set_traits<int64_t> {};

using My_interval = sets::Interval<My_int_traits>;
using My_interval_container = sets::Vector_interval_container<My_int_traits>;
using My_nested_set1 =
    sets::Map_nested_container<My_int_traits, My_interval_container>;
using My_nested_set2 =
    sets::Map_nested_container<My_int_traits, My_nested_set1>;
using My_nested_set3 =
    sets::Map_nested_container<My_int_traits, My_nested_set2>;

using Return_status_t = utils::Return_status;
constexpr auto return_ok = Return_status_t::ok;

TEST(LibsSetsVolume, Basic) {
  My_nested_set3 nested_set3[2];
  auto add = [&](int n, int x, int y, int z, int w) {
    auto ret = nested_set3[n].inplace_union(
        x, y, z, My_interval::throwing_make(w, My_int_traits::max_exclusive()));
    ASSERT_EQ(ret, return_ok);
  };
  // Create two sets that are different but have the same volumes, and have
  // volumes that can't be represented in 64 bit integers.
  add(0, 1, 1, 1, 1);  // 1:1:1:1-N
  add(0, 1, 1, 2, 2);  // 1:1:1:1-N,2:2-N
  add(0, 1, 2, 3, 8);  // 1:1:1:1-N,2:2-N,2:3:8-N
  add(0, 2, 3, 4, 9);  // 1:1:1:1-N,2:2-N,2:3:8-N,2:3:4:9-N

  add(1, 1, 1, 1, 3);  // 1:1:1:3-N
  add(1, 1, 1, 2, 4);  // 1:1:1:3-N,2:4-N
  add(1, 1, 2, 3, 6);  // 1:1:1:3-N,2:4-N,2:3:6-N
  add(1, 2, 3, 4, 7);  // 1:1:1:3-N,2:4-N,2:3:6-N,2:3:4:7-N

  ASSERT_EQ(sets::volume(nested_set3[0]), sets::volume(nested_set3[1]));
  ASSERT_EQ(sets::volume_difference(nested_set3[0], nested_set3[1]), 0);
  ASSERT_EQ(sets::volume_difference(nested_set3[1], nested_set3[0]), 0);

  // Now make the sets be different. Due to floating point rounding errors, the
  // (rounded) volume of the first set equals that of the second. But the
  // volume_difference is exact.
  add(1, 2, 3, 4, 6);  // 1:1:1:3-N,2:4-N,2:3:6-N,2:3:4:7-N
  // It is not a requirement that the computed approximations for the volumes
  // are equal. We just compare them to ensure that the test actually verifies
  // that volume_difference is better.
  ASSERT_EQ(sets::volume(nested_set3[0]), sets::volume(nested_set3[1]));
  ASSERT_EQ(sets::volume_difference(nested_set3[0], nested_set3[1]), -1);
  ASSERT_EQ(sets::volume_difference(nested_set3[1], nested_set3[0]), 1);
}

}  // namespace
