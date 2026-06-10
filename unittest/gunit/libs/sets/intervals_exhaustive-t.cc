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

#include <gtest/gtest.h>                      // TEST
#include <cassert>                            // assert
#include <concepts>                           // same_as
#include <iterator>                           // forward_iterator
#include <source_location>                    // source_location
#include <thread>                             // thread
#include "bitset_boundary_container.h"        // Bitset_boundary_container
#include "bitset_interval_container.h"        // Bitset_interval_container
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/sets/sets.h"                  // Interval_container
#include "mysql/strconv/strconv.h"            // encode_debug
#include "set_assertions.h"                   // assert_equal_sets
#include "test_decode_prefix.h"               // test_decode_prefix
#include "test_inplace_operation.h"           // test_inplace_operation
#include "test_one_container.h"               // test_one_container
#include "test_one_set.h"                     // test_one_set
#include "test_two_containers.h"              // test_two_containers
#include "test_two_set_types.h"               // test_two_set_types

namespace {

using namespace unittest::libs::sets;
using namespace mysql;

// Normally 0. While debugging, set higher values for more progress information,
// for debugging. This test checks combinations of two sets: progress_level=1
// gives information for each left-hand-side set, progress_level=2 gives
// information for each combination of the two sets.
constexpr int progress_level = 0;

// Set this 'true' to run a thread per bitset1 value; 'false' to execute
// sequentially.
constexpr bool use_multithreading = true;

// Normally 0. Set to N to start testing the N'th left-hand-side set, for
// debugging.
constexpr int left_start = 0;

// Normally 0. Set to N to start testing the N'th right-hand-side set, for
// debugging.
constexpr int right_start = 0;

template <class... Args_t>
void progress(int level, Args_t &&...args) {
  if (level <= progress_level) {
    std::cout << strconv::throwing::concat_debug(std::forward<Args_t>(args)...)
              << std::flush;
  }
}

/// Test strategy:
///
/// We implement a boundary container for sets of integers bounded by
/// `bit_count`, using a single integer bitmap in which bit N is set if value N
/// is in the set. The set operations union, intersection, and complement are
/// then easily implemented using bit operations |, &, and ~. Then:
/// - For any operation we test, we run the same operation on both bit sets
///   and the data structure under test
///   (Vector_boundary_container/Map_boundary_container), and assert that the
///   results are equal.
/// - We test operations on one set (such as size and membership queries, or
///   insertion of a single value, etc) by iterating over all integer values up
///   to 1<<bit_count, thus testing all possible sets having elements of value
///   less than `bit_count`.
/// - We test operations on two sets (such as copy, union, etc) by a nested
///   loop where we generate all integer values for the left-hand operand in
///   the outer loop and for the right-hand operand in the inner loop, thus
///   testing all combinations of all possible sets.

/// The number of bits to use in each bitset.
constexpr Bitset_value bit_count = 7;

/// We test sets of integers in the range from 0, inclusive, to bit_count,
/// exclusive.
using My_set_traits = sets::Int_set_traits<Bitset_value, 0, bit_count>;

using Interval_t = sets::Interval<My_set_traits>;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define AEQ(x, y) ASSERT_EQ(x, y)
#define ANE(x, y) ASSERT_NE(x, y)
#define ATRUE(x) ASSERT_TRUE(x)
#define AFALSE(x) ASSERT_FALSE(x)
// NOLINTEND(cppcoreguidelines-macro-usage)

/// Assert that it1 and it2 are either both positioned at the end, or point to
/// the same value.
void assert_equal_iterators(
    auto it1, auto it2, auto end1, auto end2,
    [[maybe_unused]] const std::source_location &source_location =
        std::source_location::current()) {
  MY_SCOPED_TRACE(source_location);
  if (it1 == end1) {
    AEQ(it2, end2);
  } else {
    ANE(it2, end2);
    AEQ(*it1, *it2);
  }
}

/// Exercise set operations that are common to interval sets and boundary
/// sets.
///
/// @tparam Set_t Set type to test. It may be either a boundary
/// set or an interval set.
///
/// @tparam Bitset_set_t Bitset to compare with. Either both Set_t and
/// Bitset_set_t should be boundary sets, or both should be interval
/// sets.
///
/// @param set The set to test
///
/// @param bitset_set Expected contents of set, in the form of an
/// Bitset_interval_container.
template <class Bitset_set_t, class Set_t>
void test_one_boundary_or_interval_set(const Bitset_set_t &bitset_set,
                                       const Set_t &set) {
  using Iterator_t = typename Set_t::Iterator_t;

  AEQ(set.size(), bitset_set.size());

  // Iterators and operator[]
  {
    auto set_it = set.begin();
    auto bitset_it = bitset_set.begin();
    int i = 0;
    while (true) {
      // Compare iterators
      assert_equal_iterators(set_it, bitset_it, set.end(), bitset_set.end());

      // Keep loop condition here because we want to compare end iterators
      // above.
      if (set_it == set.end()) break;

      // For non-end, random_access iterators, compare results of operator[]
      if constexpr (std::random_access_iterator<Iterator_t>) {
        AEQ(set[i], bitset_set[i]);
        AEQ(set[i], *set_it);
      }

      // Advance position
      ++set_it;
      ++bitset_it;
      ++i;
    }
  }

  // front/back and operator[]
  if (!bitset_set.empty()) {
    AEQ(set.front(), bitset_set.front());
    AEQ(set.front(), *bitset_set.begin());

    if constexpr (std::bidirectional_iterator<Iterator_t>) {
      AEQ(set.back(), bitset_set.back());
      AEQ(set.back(), *std::ranges::prev(bitset_set.end()));
    }
    if constexpr (std::random_access_iterator<Iterator_t>) {
      AEQ(set.front(), set[0]);
      AEQ(set.front(), bitset_set[0]);
      AEQ(set.back(), set[set.size() - 1]);
      AEQ(set.back(), bitset_set[set.size() - 1]);
    }
  }

  // contains_element
  for (Bitset_value v = 0; v <= Set_t::Set_traits_t::max_exclusive(); ++v) {
    MY_SCOPED_TRACE("v=", v);
    ASSERT_EQ(sets::contains_element(set, v),
              sets::contains_element(bitset_set, v));
    ASSERT_EQ(sets::contains_element(bitset_set, v),
              bitset_set.contains_element(v));
  }
}

template <class Cont_t>
void test_decode([[maybe_unused]] std::string_view format_name,
                 const strconv::Is_format auto &format, const auto &bitset_set,
                 const auto &set, int &out_size) {
  MY_SCOPED_TRACE(format_name);
  // Encode
  auto str = strconv::throwing::encode(format, set);
  auto bitset_str = strconv::throwing::encode(format, bitset_set);
  AEQ(str, bitset_str);
  // Decode
  Cont_t cont;
  auto ret = strconv::decode(format, str, cont);
  ATRUE(ret.is_ok()) << strconv::throwing::encode_debug(ret);
  // Compare
  ATRUE(sets::is_equal(cont, set)) << strconv::throwing::concat_debug(
      "cont='", cont, "' set='", set, "' bitset_set='", bitset_set, "'");
  out_size += str.size();
}

int text_size{};
int binary_size{};
int binary_fixint_size{};

template <class Cont_t>
void test_decode(const auto &bitset_set, const auto &set) {
  test_decode<Cont_t>("text", strconv::Boundary_set_text_format{}, bitset_set,
                      set, text_size);
  test_decode<Cont_t>("binary", strconv::Binary_format{}, bitset_set, set,
                      binary_size);
  test_decode<Cont_t>("binary_fixint", strconv::Fixint_binary_format{},
                      bitset_set, set, binary_fixint_size);
  test_decode_prefix(set, strconv::Binary_format{});
  test_decode_prefix(set, strconv::Fixint_binary_format{});
}

/// Exercise set operations that are common to interval sets and boundary sets.
///
/// @tparam Set_t Type of the set. It may be either a boundary set
/// or an interval set.
///
/// @tparam Bitset_set_t Type of the int set.
///
/// @param set The set to test
///
/// @param bitset_set Expected contents of set, in the form of an
/// Bitset_interval_container.
template <class Bitset_set_t, class Set_t>
void test_one_boundary_set(const Bitset_set_t &bitset_set, const Set_t &set) {
  for (Bitset_value v = 0; v <= Set_t::Set_traits_t::max_exclusive(); ++v) {
    MY_SCOPED_TRACE("v=", v);
    assert_equal_iterators(set.upper_bound(v), bitset_set.upper_bound(v),
                           set.end(), bitset_set.end());
  }

  for (Bitset_value v = 0; v <= Set_t::Set_traits_t::max_exclusive(); ++v) {
    // upper_bound(v)
    assert_equal_iterators(set.upper_bound(v), bitset_set.upper_bound(v),
                           set.end(), bitset_set.end());

    // lower_bound(v)
    assert_equal_iterators(set.lower_bound(v), bitset_set.lower_bound(v),
                           set.end(), bitset_set.end());

    auto bitset_it = bitset_set.begin();
    for (auto it = set.begin(); it != set.end(); ++it) {
      // upper_bound(it, v)
      assert_equal_iterators(bitset_set.upper_bound(bitset_it, v),
                             set.upper_bound(it, v), bitset_set.end(),
                             set.end());

      // lower_bound(it, v)
      assert_equal_iterators(bitset_set.lower_bound(bitset_it, v),
                             set.lower_bound(it, v), bitset_set.end(),
                             set.end());

      ++bitset_it;
    }
    AEQ(bitset_it, bitset_set.end());
  }

  // decode
  test_decode<
      sets::throwing::Map_boundary_container<typename Set_t::Set_traits_t>>(
      bitset_set, set);
}

/// Exercise (read-only) set operations that are specific to interval
/// sets.
///
/// @tparam Set_t Type of the set. It may be either a boundary set or an
/// interval set.
///
/// @tparam Bitset_set_t Type of the int set.
///
/// @param set The set to test
///
/// @param bitset_set Expected contents of set, in the form of an
/// Bitset_interval_container.
template <class Bitset_set_t, class Set_t>
void test_one_interval_set(const Bitset_set_t &bitset_set, const Set_t &set) {
  ASSERT_EQ(set.size() * 2, set.boundaries().size());

  // decode
  test_decode<
      sets::throwing::Map_interval_container<typename Set_t::Set_traits_t>>(
      bitset_set, set);
}

template <sets::Is_interval_set Other_set_t>
auto make_bitset_container(const Other_set_t &other_set) {
  return sets::Bitset_interval_container<
      Other_set_t::Set_traits_t::max_exclusive()>(other_set);
}
template <sets::Is_boundary_set Other_set_t>
auto make_bitset_container(const Other_set_t &other_set) {
  return sets::Bitset_boundary_container<
      Other_set_t::Set_traits_t::max_exclusive()>(other_set);
}
auto make_bitset_container(const int &rhs) { return rhs; }
auto make_bitset_container(const Interval_t &rhs) { return rhs; }
auto do_make_bitset_container = [](const auto &cont) {
  return make_bitset_container(cont);
};

/// Exercise container operations (read/write) that are common to interval sets
/// and boundary sets.
///
/// @tparam Cont_t Type of the set. It may be either a boundary container or an
/// interval container.
///
/// @param cont The container to test.
template <class Cont_t>
void test_one_boundary_or_interval_container(const Cont_t &cont) {
  constexpr Bitset_value max_exclusive = Cont_t::Set_traits_t::max_exclusive();

  for (Bitset_value value = 0; value != max_exclusive; ++value) {
    progress(2, "VALUE: ", value, "\n");
    if (sets::contains_element(cont, value)) {
      test_inplace_operation("insert existing element", inplace_insert_lambda,
                             do_make_bitset_container, cont, value,
                             is_equal_lambda, contains_lambda, 0);
      test_inplace_operation("remove existing element", inplace_remove_lambda,
                             do_make_bitset_container, cont, value,
                             is_subset_lambda, does_not_contain_lambda, 0);
    } else {
      test_inplace_operation("insert non-existing element",
                             inplace_insert_lambda, do_make_bitset_container,
                             cont, value, is_superset_lambda, contains_lambda,
                             0);
      test_inplace_operation("remove non-existing element",
                             inplace_remove_lambda, do_make_bitset_container,
                             cont, value, is_equal_lambda,
                             does_not_contain_lambda, 0);
    }
  }
}

template <class Bitset_cont_t, class Cont_t>
void test_one_interval_container(const Bitset_cont_t &bitset_cont,
                                 const Cont_t &cont) {
  constexpr Bitset_value max_exclusive =
      Bitset_cont_t::Set_traits_t::max_exclusive();
  // Inplace operations with interval as RHS
  for (Bitset_value start = 0; start != max_exclusive; ++start) {
    progress(2, "START:", start, "\n");
    for (Bitset_value inclusive_end = start; inclusive_end != max_exclusive;
         ++inclusive_end) {
      auto exclusive_end = inclusive_end + 1;
      progress(2, "EXCLUSIVE_END: ", exclusive_end, "\n");
      auto iv = Interval_t::throwing_make(start, exclusive_end);
      test_inplace_operation("inplace_union", inplace_union_lambda,
                             do_make_bitset_container, cont, iv,
                             is_superset_lambda, is_superset_lambda, 0);
      test_inplace_operation("inplace_subtract", inplace_subtract_lambda,
                             do_make_bitset_container, cont, iv,
                             is_subset_lambda, is_disjoint_lambda, 0);
      test_inplace_operation("inplace_intersect", inplace_intersect_lambda,
                             do_make_bitset_container, cont, iv,
                             is_subset_lambda, is_subset_lambda, 0);
    }
  }
  ASSERT_EQ(volume(cont), bitset_cont.volume());
}

template <class Bitset_cont_t, class Cont_t>
void test_one_boundary_container(const Bitset_cont_t &bitset_cont,
                                 const Cont_t &cont) {
  constexpr Bitset_value max_exclusive =
      Bitset_cont_t::Set_traits_t::max_exclusive();
  // Inplace operations with interval as RHS
  for (Bitset_value start = 0; start != max_exclusive; ++start) {
    for (Bitset_value inclusive_end = start; inclusive_end != max_exclusive;
         ++inclusive_end) {
      auto exclusive_end = inclusive_end + 1;
      auto iv = Interval_t::throwing_make(start, exclusive_end);
      // Without hint
      test_inplace_operation("inplace_union", boundary_inplace_union_lambda,
                             do_make_bitset_container, cont, iv,
                             is_superset_lambda, is_superset_lambda, 0);
      test_inplace_operation("inplace_subtract",
                             boundary_inplace_subtract_lambda,
                             do_make_bitset_container, cont, iv,
                             is_subset_lambda, is_disjoint_lambda, 0);
      test_inplace_operation("inplace_intersect",
                             boundary_inplace_intersect_lambda,
                             do_make_bitset_container, cont, iv,
                             is_subset_lambda, is_subset_lambda, 0);
      // With hint
      for (int i = 0; i <= bitset_cont.ssize(); ++i) {
        test_inplace_operation("inplace_union",
                               make_boundary_inplace_union_hint(i),
                               do_make_bitset_container, cont, iv,
                               is_superset_lambda, is_superset_lambda, 0);
        test_inplace_operation("inplace_subtract",
                               make_boundary_inplace_subtract_hint(i),
                               do_make_bitset_container, cont, iv,
                               is_subset_lambda, is_disjoint_lambda, 0);
        // There is no inplace_intersect(..., hint) function
      }
    }
  }
}

template <class Cont1_t, class Cont2_t>
void test_two_boundary_or_interval_containers(const Cont1_t &cont1,
                                              const Cont2_t &cont2) {
  // Decoding the comma-separated concatenation of the string representations
  // should give the union. This exercises the logic that allows out-of-order
  // and overlapping intervals.
  if constexpr (Cont1_t::has_fast_insertion) {
    auto str = strconv::throwing::concat_text(cont1, ",", cont2);
    Cont1_t out;
    auto ret = strconv::decode_text(str, out);
    ASSERT_TRUE(ret.is_ok());
    ASSERT_EQ(out, sets::make_union_view(cont1, cont2));
  }
}

#undef AEQ
#undef ANE
#undef ATRUE
#undef AFALSE

/// Invoke all the test functions that test operations with one operand.
///
/// @param bitset_interval_cont Interval container represented as a bitset.
///
/// @param bitset_interval_cont Interval container to test.
void test_one(auto &bitset_interval_cont, auto &interval_cont) {
  {
    MY_SCOPED_TRACE("intervals");
    test_one_set<Test_complement::yes>(bitset_interval_cont, interval_cont);
    // Test set properties, i.e., read-only operations, which hold for
    // both boundaries and intervals. For example, iteration over the set.
    test_one_boundary_or_interval_set(bitset_interval_cont, interval_cont);
    // Test set properties, i.e., read-only operations, which are
    // specific to intervals. For example, boundaries() == 2 * size().
    test_one_interval_set(bitset_interval_cont, interval_cont);
    // Test generic container properties, i.e., write operations.
    test_one_container(interval_cont);
    // Test container properties that are common to boundary containers and
    // interval containers.
    test_one_boundary_or_interval_container(interval_cont);
    // Test container properties specific to interval containers.
    test_one_interval_container(bitset_interval_cont, interval_cont);
  }
  {
    MY_SCOPED_TRACE("boundaries");
    auto &boundary_cont = interval_cont.boundaries();
    auto &bitset_boundary_cont = bitset_interval_cont.boundaries();
    test_one_set<Test_complement::yes>(bitset_boundary_cont, boundary_cont);
    // Test set properties, i.e., read-only operations, which hold for
    // both boundaries and intervals. For example, iteration over the set.
    test_one_boundary_or_interval_set(bitset_boundary_cont, boundary_cont);
    // Test set properties, i.e., read-only operations, which are
    // specific to boundaries. For example, upper_bound/lower_bound.
    test_one_boundary_set(bitset_boundary_cont, boundary_cont);
    // Test generic container properties, i.e., write operations.
    test_one_container(boundary_cont);
    // Test container properties that are common to boundary containers and
    // interval containers.
    test_one_boundary_or_interval_container(boundary_cont);
    // Test boundary properties specific to boundary containers.
    test_one_boundary_container(bitset_boundary_cont, boundary_cont);
  }
}

/// Invoke all the test functions that test operations with two operands.
///
/// @param bitset_interval_cont1 Left-hand-size interval container, represented
/// as a bitset.
///
/// @param bitset_interval_cont2 Right-hand-size 8nterval container, represented
/// as a bitset.
///
/// @param interval_cont1 Left-hand-size interval container to test.
///
/// @param interval_cont2 Right-hand-side interval container to test.
void test_two(auto &bitset_interval_cont1, auto &bitset_interval_cont2,
              auto &interval_cont1, auto &interval_cont2) {
  {
    MY_SCOPED_TRACE("intervals");
    // Test set properties, i.e., read-only operations.
    test_binary_predicates(bitset_interval_cont1, bitset_interval_cont2,
                           interval_cont1, interval_cont2);
    // Test container properties, i.e., write operations.
    test_two_containers<Is_throwing::yes>(do_make_bitset_container,
                                          interval_cont1, interval_cont2);
    test_two_boundary_or_interval_containers(interval_cont1, interval_cont2);
  }
  {
    MY_SCOPED_TRACE("boundaries");
    auto &boundary_cont1 = interval_cont1.boundaries();
    auto &bitset_boundary_cont1 = bitset_interval_cont1.boundaries();
    auto &boundary_cont2 = interval_cont2.boundaries();
    auto &bitset_boundary_cont2 = bitset_interval_cont2.boundaries();
    // Test set properties, i.e., read-only operations.
    test_binary_predicates(bitset_boundary_cont1, bitset_boundary_cont2,
                           boundary_cont1, boundary_cont2);
    // Test container properties, i.e., write operations.
    test_two_containers<Is_throwing::yes>(do_make_bitset_container,
                                          boundary_cont1, boundary_cont2);
    test_two_boundary_or_interval_containers(boundary_cont1, boundary_cont2);
  }
}

/// Given a bitmap, construct the corresponding Bitset_interval_container.
/// Invoke all test functions with one operand. Then, iterate over all bitmaps
/// representing a set, construct the corresponding Bitset_interval_container,
/// and invoke all test functions with two operands.
///
/// @tparam Interval_cont1_t Type of left-hand-side container to test.
///
/// @tparam Interval_cont2_t Type of right-hand-side container to test.
///
/// @param bitset1 Bitmap representing the left-hand-side set.
template <class Interval_cont1_t, class Interval_cont2_t>
void exhaustive_test_for_bitset(const Bitset_storage &bitset1) {
  using Set_traits_t = typename Interval_cont1_t::Set_traits_t;
  constexpr int max_exclusive = Set_traits_t::max_exclusive();
  using Bitset_interval_cont_t = sets::Bitset_interval_container<max_exclusive>;

  Bitset_interval_cont_t bitset_interval_cont1(bitset1);
  MY_SCOPED_TRACE("bitset1=", bitset1, "=", bitset_interval_cont1);
  progress(1, "SET1: ", bitset1, " = ", bitset_interval_cont1, "\n");
  Interval_cont1_t interval_cont1(bitset_interval_cont1);
  test_one(bitset_interval_cont1, interval_cont1);

  for (Bitset_storage bitset2 = right_start;
       bitset2 < bitset_mask(max_exclusive); ++bitset2) {
    Bitset_interval_cont_t bitset_interval_cont2(bitset2);
    progress(2, "SET1: ", bitset1, " = ", bitset_interval_cont1,
             " SET2: ", bitset2, " = ", bitset_interval_cont2, "\n");
    MY_SCOPED_TRACE("bitset2=", bitset2, "=", bitset_interval_cont2);
    Interval_cont2_t interval_cont2(bitset_interval_cont2);
    test_two(bitset_interval_cont1, bitset_interval_cont2, interval_cont1,
             interval_cont2);
  }
}

// Exhaustively test all combinations of two sets
template <class Interval_cont1_t, class Interval_cont2_t>
void exhaustive_test() {
  test_two_set_types<Interval_cont1_t, Interval_cont2_t>();

  // Use a thread per left-hand set, just to make the test faster
  std::vector<std::thread> threads;
  for (Bitset_storage bitset = left_start; bitset < bitset_mask(bit_count);
       ++bitset) {
    auto func = [=] {
      exhaustive_test_for_bitset<Interval_cont1_t, Interval_cont2_t>(bitset);
    };
    if (use_multithreading)
      threads.emplace_back(func);
    else
      func();
  }
  for (auto &thread : threads) thread.join();

  progress(1, "text_size=", text_size, " binary_size=", binary_size,
           " binary_fixint_size=", binary_fixint_size, "\n");
}

#define DEFINE_SCENARIO(type1, type2)                                 \
  TEST(LibsSetsIntervalsExhaustive, type1##type2) {                   \
    exhaustive_test<                                                  \
        sets::throwing::type1##_interval_container<My_set_traits>,    \
        sets::throwing::type2##_interval_container<My_set_traits>>(); \
  }

DEFINE_SCENARIO(Map, Map)
DEFINE_SCENARIO(Map, Vector)
DEFINE_SCENARIO(Vector, Map)
DEFINE_SCENARIO(Vector, Vector)

#undef DEFINE_SCENARIO

}  // namespace
