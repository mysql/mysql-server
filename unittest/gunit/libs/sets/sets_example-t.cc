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

#include <gtest/gtest.h>                          // TEST
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "mysql/sets/sets.h"                      // Int_set_traits

namespace {

using namespace mysql;

/// Boundary Container Basic Operations
TEST(LibsSetsExample, BoundaryContainer) {
  using My_set_traits = sets::Int_set_traits<int>;
  using My_boundary_container =
      sets::throwing::Map_boundary_container<My_set_traits>;

  // Create set1: intervals [1,2) U [3,4), boundaries 1,2,3,4
  My_boundary_container set1;
  set1.insert(1);  // Adds [1,2)
  set1.insert(3);  // Adds [3,4)

  // Create set2: intervals [2,3) U [4,5), boundaries 2,3,4,5
  My_boundary_container set2;
  set2.insert(2);  // Adds [2,3)
  set2.insert(4);  // Adds [4,5)

  // Union: [1,5), boundaries 1,5
  auto union_view = sets::make_union_view(set1, set2);

  // Iterator yields 1, then 5
  auto it = union_view.begin();
  ASSERT_EQ(*it, 1);
  ++it;
  ASSERT_EQ(*it, 5);
  ++it;
  ASSERT_EQ(it, union_view.end());
}

// Interval Container Basic Operations
TEST(LibsSetsExample, IntervalContainer) {
  using My_set_traits = sets::Int_set_traits<int>;
  using My_interval = sets::Interval<My_set_traits>;
  using My_interval_container = sets::Map_interval_container<My_set_traits>;

  My_interval_container cont;

  // Insert single elements
  auto ret = cont.insert(5);  // [5,6)
  ASSERT_OK(ret);
  ret = cont.insert(10);  // [10,11)
  ASSERT_OK(ret);

  // Check string representation
  ASSERT_EQ(strconv::throwing::encode_text(cont), "5,10");

  // Union with interval
  ret = cont.inplace_union(My_interval::throwing_make(7, 12));  // [7,11)
  ASSERT_OK(ret);
  ASSERT_EQ(strconv::throwing::encode_text(cont), "5,7-11");
}

// Nested Container Basic Usage
TEST(LibsSetsExample, NestedContainer) {
  struct My_int_traits : public sets::Int_set_traits<int64_t> {};
  struct My_string_traits
      : public sets::Ordered_set_traits_interface<My_string_traits,
                                                  std::string> {
    static bool lt_impl(const std::string &a, const std::string &b) {
      return a < b;
    }
  };

  using My_interval = sets::Interval<My_int_traits>;
  using My_interval_container = sets::Vector_interval_container<My_int_traits>;
  using My_nested_set =
      sets::Map_nested_container<My_string_traits, My_interval_container>;

  My_nested_set nested;

  // Insert intervals under different keys using inplace_union
  auto ret =
      nested.inplace_union("server1", My_interval::throwing_make(100, 200));
  ASSERT_OK(ret);
  ret = nested.inplace_union("server2", My_interval::throwing_make(150, 250));
  ASSERT_OK(ret);

  // Access nested sets
  ASSERT_TRUE(sets::contains_element(nested["server1"], 150));
  ASSERT_FALSE(sets::contains_element(nested["server2"], 100));

  // String representation
  ASSERT_EQ(strconv::throwing::encode_text(nested),
            "server1:100-199,server2:150-249");
}

// Set Predicates Usage
TEST(LibsSetsExample, Predicates) {
  using My_set_traits = sets::Int_set_traits<int>;
  using My_interval = sets::Interval<My_set_traits>;
  using My_container = sets::Map_interval_container<My_set_traits>;

  My_container set1, set2, set3;

  // Set up test sets
  auto ret = set1.inplace_union(My_interval::throwing_make(1, 10));  // [1,10)
  ASSERT_OK(ret);
  ret = set2.inplace_union(My_interval::throwing_make(5, 15));  // [5,15)
  ASSERT_OK(ret);
  ret = set3.inplace_union(My_interval::throwing_make(20, 30));  // [20,30)
  ASSERT_OK(ret);

  // Test basic predicates
  ASSERT_FALSE(sets::is_subset(set1, set2));  // [1,10) not subset of [5,15)
  // intentionally reversed arguments
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  ASSERT_FALSE(sets::is_subset(set2, set1));  // [5,15) not subset of [1,10)
  ASSERT_TRUE(sets::is_intersecting(set1, set2));   // [1,10) n [5,15) = [5,10)
  ASSERT_FALSE(sets::is_intersecting(set1, set3));  // [1,10) n [20,30) = 0
  ASSERT_TRUE(sets::is_disjoint(set1, set3));       // Disjoint sets
}

// View Operations with Empty Sets
TEST(LibsSetsExample, ViewWithEmptySets) {
  using My_set_traits = sets::Int_set_traits<int>;
  using My_interval = sets::Interval<My_set_traits>;
  using My_container = sets::Map_interval_container<My_set_traits>;

  My_container empty_set, non_empty_set;
  auto ret = non_empty_set.inplace_union(My_interval::throwing_make(1, 5));
  ASSERT_OK(ret);

  // Union with empty set
  auto union_view = sets::make_union_view(non_empty_set, empty_set);
  ASSERT_TRUE(sets::is_equal(union_view, non_empty_set));

  // Intersection with empty set
  auto intersection_view =
      sets::make_intersection_view(non_empty_set, empty_set);
  ASSERT_TRUE(intersection_view.empty());

  // Subtraction from empty set
  auto subtraction_view = sets::make_subtraction_view(empty_set, non_empty_set);
  ASSERT_TRUE(subtraction_view.empty());
}

// String Conversion Round-trip
TEST(LibsSetsExample, StringConversionRoundTrip) {
  using My_set_traits = sets::Int_set_traits<int>;
  using My_interval = sets::Interval<My_set_traits>;
  using My_container = sets::Map_interval_container<My_set_traits>;

  My_container original;
  auto ret = original.inplace_union(My_interval::throwing_make(1, 10));
  ASSERT_OK(ret);
  ret = original.insert(15);
  ASSERT_OK(ret);

  // Convert to string
  std::string text = strconv::throwing::encode_text(original);
  ASSERT_EQ(text, "1-9,15");

  // Parse back
  My_container parsed;
  auto parse_ret =
      strconv::decode(strconv::Boundary_set_text_format{}, text, parsed);
  ASSERT_TRUE(parse_ret.is_ok());

  // Verify round-trip
  ASSERT_TRUE(sets::is_equal(original, parsed));
}

// Complement View Usage
TEST(LibsSetsExample, ComplementView) {
  using My_set_traits = sets::Int_set_traits<int, 0, 10>;  // Limited range
  using My_interval = sets::Interval<My_set_traits>;
  using My_container = sets::Map_interval_container<My_set_traits>;

  My_container set;
  auto ret = set.inplace_union(My_interval::throwing_make(2, 5));
  ASSERT_OK(ret);
  ret = set.insert(7);
  ASSERT_OK(ret);

  // Create complement view
  auto complement = sets::make_complement_view(set);

  // Original set should be disjoint from its complement
  ASSERT_TRUE(sets::is_disjoint(set, complement));

  // Union of set and its complement should be the full set
  auto full_set_view = sets::make_union_view(set, complement);
  auto full_set = sets::make_full_set_view_like<My_container>();
  ASSERT_TRUE(sets::is_equal(full_set_view, full_set));

  // Complement works for interval sets, but not for Nested sets (because that
  // would not make sense).
}

}  // namespace
