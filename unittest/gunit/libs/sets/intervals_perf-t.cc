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

#include <gtest/gtest.h>      // TEST
#include <algorithm>          // reverse
#include <array>              // array
#include <chrono>             // high_resolution_clock
#include <numeric>            // iota
#include <random>             // mt19937
#include "mysql/sets/sets.h"  // Map_boundary_container
#include "sql/rpl_gtid.h"     // Gtid_set

namespace {

using namespace mysql;

// ==== random_order_insert ====
//
// Test how the data structures behave when appending random values near the
// end, keeping a bounded number of gaps. The workload is constructed as
// follows:
//
// 1. Insert a random permutation of the numbers from 1 to N.
// 2. Insert a random permutation of the numbers from N+1 to 2*N.
// 3. Insert a random permutation of the numbers from 2*N+1 to 3*N.
//    ...
//
// This is intended to simulate the progression of gtid_executed in a situation
// where transactions are assigned a GTID in on order and added to gtid_executed
// in a slightly different order, such as on a replica using
// replica-preserve-commit-order=0.

/// Fill the range from begin to end with a random permutation of the numbers
/// from offset to (end-begin)-1, inclusive, using the given random seed.
void generate_random_permutation(auto generator, auto begin, auto end,
                                 auto offset) {
  std::iota(begin, end, offset);
  std::shuffle(begin, end, generator);
}

/// Fill a container the numbers from 0 to container.size()-1, then shuffle each
/// size-N chunk randomly, where N is the given chunk_size.
void generate_random_permutation_chunks(int64_t seed, auto &container,
                                        std::size_t chunk_size) {
  std::mt19937 generator(seed);
  std::iota(container.begin(), container.end(), 1);
  for (auto pos = container.begin(); pos != container.end();
       pos += chunk_size) {
    std::shuffle(pos, pos + chunk_size, generator);
  }
  std::reverse(container.begin(), container.end());
}

/// Insert a random permutation of the values from 0 to N - 1 into the
/// interval container.
int64_t test_insert(auto insert, auto size, auto values) {
  int64_t cumulative_size = 0;
  for (const auto &value : values) {
    insert(value);
    cumulative_size += size();
  }
  return cumulative_size;
}

/// Run test_random_order_insert multiple times, with the number of elements
/// inserted being the powers of two up to 65536, and the number of iterations
/// such that the total inser
///
/// @param seed Random seed
///
/// @param insert Function to insert a value.
void test_random_order_insert(int64_t seed, auto insert, auto clear,
                              auto size) {
  constexpr int max_level = 15;
  constexpr std::size_t element_count = std::size_t(1) << max_level;
  std::array<int, element_count> values;
  // NOLINTNEXTLINE(modernize-use-ranges): we can use ranges when all compilers
  // we need to build under support them.
  std::ranges::fill(values.begin(), values.end(), 0);

  // Pre-heat the container.
  generate_random_permutation_chunks(seed, values, values.size());
  test_insert(insert, size, values);

  for (int disorder_level = 0; disorder_level <= max_level; ++disorder_level) {
    clear();
    std::size_t chunk_size = std::size_t(1) << disorder_level;
    generate_random_permutation_chunks(seed, values, chunk_size);
    auto start_time = std::chrono::high_resolution_clock::now();
    int64_t cumulative_size = test_insert(insert, size, values);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto delta_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    auto delta_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time);
    std::cout << disorder_level << ": " << delta_us.count() << " us, "
              << (cumulative_size / element_count) << " elems on avg, "
              << (delta_ns.count() / element_count) << " ns/insertion\n";
  }
}

using Int_traits = sets::Int_set_traits<int64_t>;

/// Run the random_order_insert for a throwing container.
void test_random_order_insert(auto &cont) {
  test_random_order_insert(
      1, /*insert=*/[&](auto val) { return cont.insert(val); },
      /*clear=*/[&] { cont.clear(); },
      /*size=*/[&] { return cont.size(); });
}

TEST(LibsSetsIntervalsPerformance, InsertThrowingMap) {
  sets::throwing::Map_interval_container<Int_traits> cont;
  test_random_order_insert(cont);
}

// Test to use std::multimap instead of std::map. Given how the map is used by
// Boundary_container, map and multimap are equivalent. So we test whether there
// is any difference in performance characteristics. (So far we did not observe
// any difference.)
TEST(LibsSetsIntervalsPerformance, InsertThrowingMultiMap) {
  sets::Interval_container<
      sets::throwing::Boundary_container<sets::throwing::Map_boundary_storage<
          Int_traits, sets::Map_for_set_traits<std::multimap, Int_traits>>>>
      cont;
  test_random_order_insert(cont);
}

TEST(LibsSetsIntervalsPerformance, InsertThrowingVector) {
  sets::throwing::Vector_interval_container<Int_traits> cont;
  test_random_order_insert(cont);
}

TEST(LibsSetsIntervalsPerformance, InsertNonthrowingMap) {
  sets::Map_interval_container<Int_traits> cont;
  test_random_order_insert(cont);
}

TEST(LibsSetsIntervalsPerformance, InsertNonThrowingMultiMap) {
  sets::Interval_container<
      sets::throwing::Boundary_container<sets::throwing::Map_boundary_storage<
          Int_traits, sets::Map_for_set_traits<std::multimap, Int_traits>>>>
      cont;
  test_random_order_insert(cont);
}

TEST(LibsSetsIntervalsPerformance, InsertNonthrowingVector) {
  sets::Vector_interval_container<Int_traits> cont;
  test_random_order_insert(cont);
}

TEST(LibsSetsIntervalsPerformance, InsertLegacyGtidSet) {
  Tsid_map sm(nullptr);
  Gtid_set set(&sm, nullptr);
  gtid::Tsid tsid;
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
  set.ensure_sidno(sidno);
  test_random_order_insert(
      1, /*insert=*/[&](auto val) { set._add_gtid(sidno, val); },
      /*clear=*/[&] { set.clear(); },
      /*size=*/[&] { return set.get_n_intervals(sidno); });
}

// ==== overlapping_union ====
//
// This computes the union of two sets, where the first one is a contiguous
// segment followed by a region of alternating values inside and outside the
// set; and the second set begins with such alternating values inside and
// outside the set, followed by a contiguous region, such that in the union
// of the two, each contiguous segment "eats up" the alternating values. Here is
// a graphical representation of the sets:
//
// set1: ____________________....................
// set2: ....................____________________
//
// This is a case where the optimizations in Union_view makes it
// logarithmic-time, whereas legacy Gtid_set is linear-time.

/// Construct the sets, and then compute their union a large number of times.
///
/// @param insert1 Function object to insert into the first set.
///
/// @param insert2 Function object to insert into the second set.
///
/// @param test Function object to return the number of elements in the union.
///
/// @return The total number of elements in each union computed.
size_t test_overlapping_union(auto insert1, auto insert2, auto test) {
  constexpr int count = 2'000;
  for (int i = 0; i < count; ++i) {
    insert1(1 + i);
    insert2(count + 1 + i);
  }
  for (int i = 0; i < count; i += 2) {
    insert1(count + 1 + i);
    insert2(1 + i);
  }
  size_t ret{0};
  for (int i = 0; i < count; ++i) {
    ret += test();
  }
  return ret;
}

/// Run the test using a Union_view to compute the result.
///
/// @param cont1 Interval_container for the first set.
///
/// @param cont2 Interval_container for the second set.
void test_overlapping_union_view(auto &cont1, auto &cont2) {
  auto union_view = sets::make_union_view(cont1, cont2);
  test_overlapping_union(
      /*insert1=*/[&](auto val) { cont1.insert(val); },
      /*insert2=*/[&](auto val) { cont2.insert(val); },
      /*test=*/[&] { return union_view.size(); });
}

/// Run the test using Interval_container::inplace_union to compute the result.
///
/// @param cont1 Interval_container for the first set.
///
/// @param cont2 Interval_container for the second set.
template <class Cont_t>
void test_overlapping_union_inplace(Cont_t &cont1, Cont_t &cont2) {
  Cont_t result;
  test_overlapping_union(
      /*insert1=*/[&](auto val) { cont1.insert(val); },
      /*insert2=*/[&](auto val) { cont2.insert(val); },
      /*test=*/
      [&] {
        result = cont1;
        result.inplace_union(cont2);
        return result.size();
      });
}

TEST(LibsSetsIntervalsPerformance, OverlappingUnionMapView) {
  sets::throwing::Map_interval_container<Int_traits> cont1;
  sets::throwing::Map_interval_container<Int_traits> cont2;
  test_overlapping_union_view(cont1, cont2);
}

TEST(LibsSetsIntervalsPerformance, OverlappingUnionVectorView) {
  sets::throwing::Vector_interval_container<Int_traits> cont1;
  sets::throwing::Vector_interval_container<Int_traits> cont2;
  test_overlapping_union_view(cont1, cont2);
}

TEST(LibsSetsIntervalsPerformance, OverlappingUnionMapInplace) {
  sets::throwing::Map_interval_container<Int_traits> cont1;
  sets::throwing::Map_interval_container<Int_traits> cont2;
  test_overlapping_union_inplace(cont1, cont2);
}

TEST(LibsSetsIntervalsPerformance, OverlappingUnionVectorInplace) {
  sets::throwing::Vector_interval_container<Int_traits> cont1;
  sets::throwing::Vector_interval_container<Int_traits> cont2;
  test_overlapping_union_inplace(cont1, cont2);
}

TEST(LibsSetsIntervalsPerformance, OverlappingUnionLegacyGtidSet) {
  Tsid_map sm(nullptr);
  Gtid_set set1(&sm, nullptr);
  Gtid_set set2(&sm, nullptr);
  Gtid_set result(&sm, nullptr);
  gtid::Tsid tsid;
  ASSERT_TRUE(tsid.from_cstring("d3a98502-756b-4b08-bdd2-a3d3938ba90f") > 0);
  rpl_sidno sidno = sm.add_tsid(tsid);
  set1.ensure_sidno(sidno);
  set2.ensure_sidno(sidno);
  result.ensure_sidno(sidno);
  test_overlapping_union(
      /*insert1=*/[&](auto val) { set1._add_gtid(sidno, val); },
      /*insert2=*/[&](auto val) { set2._add_gtid(sidno, val); },
      /*test=*/
      [&] {
        result.clear();
        result.add_gtid_set(&set1);
        result.add_gtid_set(&set2);
        return result.get_n_intervals(sidno);
      });
}

}  // namespace
