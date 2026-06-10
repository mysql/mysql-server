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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include <gtest/gtest.h>                          // TEST
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "mysql/sets/sets.h"                      // Map_interval_container

namespace {

using namespace mysql;

// Describes the type of values stored in an interval set: the data type is @c
// int, and it uses defaults for min/max/comparison/etc.
using My_set_traits = sets::Int_set_traits<int>;

/// Interval where endpoints are as described by My_set_traits.
using My_interval = sets::Interval<My_set_traits>;

/// Interval container where endpoints are as described by My_set_traits,
/// and the backing container is std::map.
using My_interval_container = sets::Map_interval_container<My_set_traits>;

using namespace strconv;

// Illustrate operations on containers of intervals.
TEST(LibsSetsIntervalsBasic, Containers) {
  My_interval_container cont;
  auto ret = cont.insert(1);
  ASSERT_OK(ret);
  ret = cont.insert(2);
  ASSERT_OK(ret);
  ret = cont.insert(3);
  ASSERT_OK(ret);
  ASSERT_EQ(strconv::throwing::encode_text(cont), "1-3");

  ret = cont.remove(2);
  ASSERT_OK(ret);
  ASSERT_EQ(strconv::throwing::encode_text(cont), "1,3");

  // Endpoints are always exclusive, except in the text format, where they are
  // inclusive. Exclusive endpoints in APIs is the most reasonable and most
  // common program semantics, and in particular the paradigm for C++ end()
  // iterators. Inclusive endpoints are in analogy with most text written for
  // humans, like "I'm on vacation from first to the twenty-fourth July", which
  // usually means you will be back on the twenty-fifth.
  ret = cont.inplace_union(My_interval::throwing_make(2, 10));
  ASSERT_OK(ret);
  ASSERT_EQ(strconv::throwing::encode_text(cont), "1-9");

  cont.inplace_intersect(My_interval::throwing_make(3, 1000));  // can't fail
  ASSERT_EQ(strconv::throwing::encode_text(cont), "3-9");

  ret = cont.inplace_subtract(My_interval::throwing_make(6, 8));
  ASSERT_OK(ret);
  ASSERT_EQ(strconv::throwing::encode_text(cont), "3-5,8-9");
}

// Illustrate how to parse interval sets from strings.
TEST(LibsSetsIntervalsBasic, Parsing) {
  // Interval container using map as backing storage.
  My_interval_container cont;

  // Parse the empty string, in text format, into the interval set.
  auto ret = strconv::decode(strconv::Boundary_set_text_format{}, "", cont);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);

  // Parse a singleton interval, in text format, into the interval set.
  ret = strconv::decode(strconv::Boundary_set_text_format{}, "1", cont);

  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);

  // Parse an interval, in text format, into the interval set.
  ret = strconv::decode(strconv::Boundary_set_text_format{}, "1-10", cont);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);

  // Parse two intervals, in text format, into the interval set.
  ret = strconv::decode(strconv::Boundary_set_text_format{},
                        " 1 - 10 , 99-100  ", cont);
  ASSERT_TRUE(ret.is_ok()) << strconv::throwing::encode_text(ret);

  // Parse a wrong string and handle the error.
  ret = strconv::decode(strconv::Boundary_set_text_format{}, "1-10,9-blubb",
                        cont);
  ASSERT_FALSE(ret.is_ok());
  ASSERT_EQ(strconv::throwing::encode_text(ret),
            "Expected number after 7 characters, marked by [HERE] in: "
            "\"1-10,9-[HERE]blubb\"");
}

// Illustrate the use of Boolean set predicates.
TEST(LibsSetsIntervalsBasic, Predicates) {
  My_interval_container cont1;
  My_interval_container cont2;
  My_interval_container cont3;
  My_interval_container cont4;
  My_interval_container cont5;

  auto ret = cont1.inplace_union(My_interval::throwing_make(0, 100));
  ASSERT_OK(ret);
  ret = cont2.inplace_union(My_interval::throwing_make(0, 100));
  ASSERT_OK(ret);
  ret = cont3.inplace_union(My_interval::throwing_make(50, 100));
  ASSERT_OK(ret);
  ret = cont4.inplace_union(My_interval::throwing_make(50, 200));
  ASSERT_OK(ret);
  ret = cont5.inplace_union(My_interval::throwing_make(100, 200));
  ASSERT_OK(ret);

  ASSERT_TRUE(sets::is_equal(cont1, cont2));
  ASSERT_TRUE(!sets::is_equal(cont2, cont3));

  // cont3 is a subet of cont2, which is the same as saying that
  // cont2 is a superset of cont3.
  ASSERT_TRUE(sets::is_subset(cont3, cont2));
  ASSERT_TRUE(sets::is_superset(cont2, cont3));

  // every set is a subset (and superset) of itself
  ASSERT_TRUE(sets::is_subset(cont3, cont3));
  ASSERT_TRUE(sets::is_superset(cont3, cont3));

  // superset is not the same as not subset; set containment is not a total
  // order. cont2 and cont4 are neither superset nor subset of each other.
  ASSERT_TRUE(!sets::is_superset(cont2, cont4));
  ASSERT_TRUE(!sets::is_subset(cont2, cont4));
  ASSERT_TRUE(!sets::is_superset(cont4, cont2));
  ASSERT_TRUE(!sets::is_subset(cont4, cont2));

  // intersecting is the same as not disjoint
  ASSERT_TRUE(sets::is_intersecting(cont2, cont4));
  ASSERT_TRUE(sets::is_disjoint(cont2, cont5));
}

// Illustrate the use of views to compute set operations on-the-fly, without
// instantiating the result and without allocating memory.
TEST(LibsSetsIntervalsBasic, Views) {
  My_interval_container cont1;
  My_interval_container cont2;
  My_interval_container union_1_2;
  My_interval_container intersection_1_2;
  My_interval_container subtraction_1_2;
  My_interval_container symmetric_difference_1_2;

  auto ret = cont1.inplace_union(My_interval::throwing_make(1, 10));
  ASSERT_OK(ret);
  ret = cont2.inplace_union(My_interval::throwing_make(5, 15));
  ASSERT_OK(ret);
  ret = union_1_2.inplace_union(My_interval::throwing_make(1, 15));
  ASSERT_OK(ret);
  ret = intersection_1_2.inplace_union(My_interval::throwing_make(5, 10));
  ASSERT_OK(ret);
  ret = subtraction_1_2.inplace_union(My_interval::throwing_make(1, 5));
  ASSERT_OK(ret);
  ret =
      symmetric_difference_1_2.inplace_union(My_interval::throwing_make(1, 5));
  ASSERT_OK(ret);
  ret = symmetric_difference_1_2.inplace_union(
      My_interval::throwing_make(10, 15));
  ASSERT_OK(ret);

  auto union_view = sets::make_union_view(cont1, cont2);
  auto intersection_view = sets::make_intersection_view(cont1, cont2);
  auto subtraction_view = sets::make_subtraction_view(cont1, cont2);
  ASSERT_TRUE(sets::is_equal(union_1_2, union_view));
  ASSERT_TRUE(sets::is_equal(intersection_1_2, intersection_view));
  ASSERT_TRUE(sets::is_equal(subtraction_1_2, subtraction_view));

  auto subtraction_view_reverse = sets::make_subtraction_view(cont2, cont1);
  auto symmetric_difference_view =
      sets::make_union_view(subtraction_view, subtraction_view_reverse);
  ASSERT_TRUE(
      sets::is_equal(symmetric_difference_1_2, symmetric_difference_view));
}

}  // namespace
