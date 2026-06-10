// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.er
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

#include <gtest/gtest.h>                      // TEST
#include <chrono>                             // duration
#include <random>                             // random_device
#include <string>                             // string
#include <thread>                             // thread
#include "bitset_interval_container.h"        // Bitset_interval_container
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/sets/sets.h"                  // Int_set_traits
#include "set_assertions.h"                   // assert_equal_sets
#include "test_one_container.h"               // test_one_container
#include "test_one_set.h"                     // test_one_set
#include "test_two_containers.h"              // test_two_containers
#include "test_two_set_types.h"               // test_two_set_types

// Test strategy
// =============
//
// Purpose
// -------
//
// Test set operations for nested sets, checking correctness for all the member
// functions and free function predicates/operations.
//
// Approach
// --------
//
// We define a class of sets. Each scenario is defined by two sets drawn from
// the class (so that we can test operations that depend on two operand sets).
// We test all possible such scenarios, i.e., all ways to draw two sets from the
// class.
//
// Structure of tested sets
// ------------------------
//
// To test set operations on nested sets, we use the following set structure:
//
// Nested(int, Nested(std::string, Interval(int)))
//
// I.e.:
// - An outer level nested set where the keys are integers and the values are
//   middle level sets.
// - Middle level nested sets where the keys are strings and the values are the
//   inner level sets.
// - Inner level interval sets where the elements are integers.
//
// This data structure represents a set of 3-tuples (a, b, c), where a is an
// integer, b is a string and c is an integer.
//
// Values of tested sets
// ---------------------
//
// We choose three "base sets": base set A of integers for the first component,
// base set B of strings for the second component, and a base set C of integers
// for the third component. Given the three base sets, we test all possible sets
// of 3-tuples with components drawn from those base sets. Thus, the number of
// possible 3-tuples is |A| * |B| * |C|, and the number of possible sets is
// 2^(|A| * |B| * |C|). The number of possible scenarios given by choosing two
// such sets is then (2^(|A| * |B| * |C|)) ^ 2 = 2^(2 * |A| * |B| * |C|).
//
// We can run the order of one million scenarios in reasonable time. Therefore,
// we need |A| * |B| * |C| <= 10. The following combinations of values for |A|,
// |B|, |C| satisfy that: (2, 2, 2), (1, 2, 4), (1, 4, 2), (2, 1, 4), (2, 4, 1),
// (4, 1, 2), (4, 2, 1), (1, 3, 3), (3, 1, 3), (3, 3, 1), (10, 1, 1), (1, 10,
// 1), (10, 1, 1). It is unlikely to have bugs that impact 10-element sets but
// not, say, 8-element sets, so to we replace 10 by 8 in the last 3
// combinations. We for each such triple, we test all possible scenarios having
// the given number of elements in A, B, and C respectively.
//
// Verifying the results of set operations
// ---------------------------------------
//
// To know the expected result of a set operation `op` on two such nested sets,
// we define a homomorphism `H` from the nested sets to interval sets, such that
// H(op(X, Y)) = op(H(X), H(Y)). Then, we compute the result of the operation on
// the nested sets, and map that result to the corresponding interval set; and
// we map X and Y to the corresponding interval sets and compute the operation
// on those; we expect that the two resulting interval sets are equal.
//
// This tests that the operation on nested sets is correct, assuming that the
// operation on interval sets is correct. We test the correctness of interval
// sets in another test, intervals_exhaustive.

namespace {

using namespace mysql;

// Normally 0. Set higher values for more progress information, for debugging.
constexpr int progress_level = 0;

// Normally true, which makes it faster on multicore CPUs. Set to false for
// debugging.
constexpr bool use_multithreading = true;

// Normally 0. Set to N to start testing the N'th left-hand-side set, for
// debugging.
constexpr int left_start = 0;

// Normally 0. Set to N to start testing the N'th right-hand-side set, for
// debugging.
constexpr int right_start = 0;

// Only run the test for this amount of time (seconds).
constexpr int timeout_sec = 300;

template <class... Args_t>
void progress(int level, Args_t &&...args) {
  if (level <= progress_level) {
    std::cout << strconv::throwing::concat_debug(std::forward<Args_t>(args)...)
              << std::flush;
  }
}

using namespace unittest::libs::sets;

using Outer_t = int;
using Middle_t = std::string;
using Inner_t = int;
using Tuple_t = std::tuple<int, std::string, int>;

constexpr int max_elements = 10;
constexpr int max_elements_per_component = 8;
Outer_t outer_elements[max_elements_per_component] = {2,  3,  5,  7,
                                                      11, 13, 17, 19};
Middle_t middle_elements[max_elements_per_component] = {
    "one", "two", "three", "four", "five", "six", "seven", "eight"};
Inner_t inner_elements[max_elements_per_component] = {0, 1, 2, 3, 4, 5, 6, 7};

using Bitset_t = sets::Bitset_interval_container<max_elements>;

using Int_traits_t = sets::Int_set_traits<int64_t>;
using Bitset_traits_t = Bitset_t::Set_traits_t;
struct String_traits_t
    : public sets::Ordered_set_traits_interface<String_traits_t, std::string> {
  static auto cmp_impl(const std::string &a, const std::string &b) {
    return a <=> b;
  }
};

using Interval_t = sets::Interval<Bitset_traits_t>;

class Tester_base {
 public:
  Tester_base() = default;
  Tester_base(const Tester_base &) = default;
  Tester_base(Tester_base &&) = default;
  Tester_base &operator=(const Tester_base &) = default;
  Tester_base &operator=(Tester_base &&) = default;
  virtual ~Tester_base() = default;
  virtual int get_iterations() = 0;
  virtual void test() = 0;
};

/// The logic to iterate over all sets and invoke procedures that test them.
///
/// @tparam outer_count Size of prefix of outer_elements from which the elements
/// of the first component are drawn.
///
/// @tparam middle_count Size of prefix of middle_elements from which the
/// elements of the second component are drawn.
///
/// @tparam inner_count Size of prefix of inner_elements from which the
/// elements of the third component are drawn.
template <class Nested_set1_t, class Nested_set2_t, int outer_count,
          int middle_count, int inner_count>
class Tester : public Tester_base {
 public:
  static constexpr int element_count = outer_count * middle_count * inner_count;

  static constexpr int subset_count = bitset_mask(element_count);

  static constexpr int iterations = subset_count * subset_count;

  /// Encode a nested set of nested sets of interval sets, as a single interval
  /// set.
  ///
  /// Given a nested set of the form Nested(int, Nested(string, Interval(int))),
  /// where:
  ///
  /// - the first component in each element is one of the first outer_count
  ///   elements of outer_elements;
  ///
  /// - the second component in each element is one of the first middle_count
  ///   elements of middle_elements; and
  ///
  /// - the third component in each element is one of the inner_count first
  ///   elements of inner_elements;
  ///
  /// maps each element (outer_elements[A], middle_elements[B],
  /// inner_elements[C]) of the nested set to the element `C + middle_count * (B
  /// + outer_count * A)` of the interval set.
  ///
  /// @param nested_set The input nested set.
  ///
  /// @param[out] / The output interval set.
  static void nested_set_to_interval_set(const auto &nested_set,
                                         auto &interval_set) {
    for (int outer_i = 0; outer_i < outer_count; ++outer_i) {
      auto outer_it = nested_set.find(outer_elements[outer_i]);
      if (outer_it == nested_set.end()) continue;
      decltype(auto) middle_pair = *outer_it;
      const auto &middle_set = middle_pair.second;
      for (int middle_i = 0; middle_i < middle_count; ++middle_i) {
        auto middle_it = middle_set.find(middle_elements[middle_i]);
        if (middle_it == middle_set.end()) continue;
        decltype(auto) inner_pair = *middle_it;
        const auto &inner_set = inner_pair.second;
        int offset = outer_i;
        offset *= middle_count;
        offset += middle_i;
        offset *= inner_count;
        for (auto &&interval : inner_set) {
          interval_set.inplace_union(Interval_t::throwing_make(
              offset + interval.start(), offset + interval.exclusive_end()));
        }
      }
    }
  }

  /// Decode an interval set of the form produced by nested_set_to_interval_set,
  /// back into a nested set.
  ///
  /// @param interval_set The input interval set.
  ///
  /// @param[out] nested_set The output nested set.
  static void interval_set_to_nested_set(const auto &interval_set,
                                         auto &nested_set) {
    for (auto interval : interval_set) {
      for (auto element = interval.start(); element != interval.exclusive_end();
           ++element) {
        auto tmp = element;
        int inner_i = tmp % inner_count;
        tmp /= inner_count;
        int middle_i = tmp % middle_count;
        tmp /= middle_count;
        assert(tmp < outer_count);
        int outer_i = tmp;
        auto ret = nested_set.insert(outer_elements[outer_i],
                                     middle_elements[middle_i],
                                     inner_elements[inner_i]);
        ASSERT_OK(ret);
      }
    }
  }

  enum class Subtract_can_fail { no, yes };
  template <Subtract_can_fail subtract_can_fail>
  static void test_intersection_and_subtraction(auto &nested_set,
                                                auto... elements) {
    Nested_set1_t intersected;
    auto ret = intersected.assign(nested_set);
    ASSERT_OK(ret);
    intersected.inplace_intersect(elements...);

    Nested_set1_t subtracted;
    ret = subtracted.assign(nested_set);
    ASSERT_OK(ret);
    if constexpr (subtract_can_fail == Subtract_can_fail::yes) {
      ret = subtracted.inplace_subtract(elements...);
      ASSERT_OK(ret);
    } else {
      subtracted.inplace_subtract(elements...);
    }

    ASSERT_TRUE(sets::is_disjoint(intersected, subtracted));
    ASSERT_EQ(sets::make_union_view(intersected, subtracted), nested_set);

    auto v0 = sets::volume(nested_set);
    auto v1 = sets::volume(intersected);
    auto v2 = sets::volume(subtracted);
    ASSERT_EQ(v0, v1 + v2);
  }

  static void test_one_nested_container(const Bitset_t &bitset [[maybe_unused]],
                                        const Nested_set1_t &nested_set) {
    using Interval_t = typename Nested_set1_t::Mapped_t::Mapped_t::Interval_t;

    // Iterate over all possible set elements, and test the
    // single-element-versions of all inplace operations.
    for (int outer_i = 0; outer_i != outer_count; ++outer_i) {
      auto outer_e = outer_elements[outer_i];
      test_intersection_and_subtraction<Subtract_can_fail::no>(nested_set,
                                                               outer_e);

      for (int middle_i = 0; middle_i != middle_count; ++middle_i) {
        auto middle_e = middle_elements[middle_i];
        test_intersection_and_subtraction<Subtract_can_fail::no>(
            nested_set, outer_e, middle_e);

        for (int inner_i = 0; inner_i != inner_count; ++inner_i) {
          auto inner_e = inner_elements[inner_i];
          auto inner_iv = Interval_t::throwing_make(inner_e, inner_e + 1);
          test_intersection_and_subtraction<Subtract_can_fail::yes>(
              nested_set, outer_e, middle_e, inner_iv);

          Nested_set1_t singleton_set;
          auto ret = singleton_set.insert(outer_e, middle_e, inner_e);
          ASSERT_OK(ret);
          ASSERT_EQ(sets::volume(singleton_set), 1);

          // 3-arg inplace_union, and insert
          {
            // inplace_union
            Nested_set1_t nested_set1;  // copy of nested_set
            ret = nested_set1.assign(nested_set);
            ASSERT_OK(ret);
            ret = nested_set1.inplace_union(outer_e, middle_e, inner_iv);
            ASSERT_OK(ret);

            // insert
            Nested_set1_t nested_set2;  // copy of nested_set
            ret = nested_set2.assign(nested_set);
            ASSERT_OK(ret);
            ret = nested_set2.insert(outer_e, middle_e, inner_e);
            ASSERT_OK(ret);

            ASSERT_EQ(nested_set1, nested_set2);

            if (sets::contains_element(nested_set, outer_e, middle_e,
                                       inner_e)) {
              assert_equal_sets(nested_set1, nested_set);
            } else {
              assert_equal_sets(nested_set1, sets::make_union_view(
                                                 nested_set, singleton_set));
              ASSERT_EQ(sets::volume_difference(nested_set1, nested_set), 1.0);
            }
          }

          // 3-arg inplace_subtraction, and remove
          {
            // inplace_subtract
            Nested_set1_t nested_set1;  // copy of nested_set
            ret = nested_set1.assign(nested_set);
            ASSERT_OK(ret);
            ret = nested_set1.inplace_subtract(outer_e, middle_e, inner_iv);
            ASSERT_OK(ret);

            // remove
            Nested_set1_t nested_set2;  // copy of nested_set
            ret = nested_set2.assign(nested_set);
            ASSERT_OK(ret);
            ret = nested_set2.remove(outer_e, middle_e, inner_e);
            ASSERT_OK(ret);

            ASSERT_EQ(nested_set1, nested_set2);

            if (sets::contains_element(nested_set, outer_e, middle_e,
                                       inner_e)) {
              assert_equal_sets(nested_set1, sets::make_subtraction_view(
                                                 nested_set, singleton_set));
              ASSERT_EQ(sets::volume_difference(nested_set1, nested_set), -1.0);
            } else {
              assert_equal_sets(nested_set1, nested_set);
            }
          }

          // 3-arg inplace_intersection
          {
            Nested_set1_t nested_set1;  // copy of nested_set
            ret = nested_set1.assign(nested_set);
            ASSERT_OK(ret);
            // inplace_intersect with single element cannot fail
            nested_set1.inplace_intersect(outer_e, middle_e, inner_iv);
            if (sets::contains_element(nested_set, outer_e, middle_e,
                                       inner_e)) {
              assert_equal_sets(nested_set1, singleton_set);
              ASSERT_EQ(sets::volume(nested_set1), 1.0);
            } else {
              ASSERT_TRUE(nested_set1.empty());
            }
          }
        }
      }
    }
  }

  static void test_one(const Bitset_t &bitset1, const auto &nested_set1) {
    {
      Bitset_t bitset1a;
      nested_set_to_interval_set(nested_set1, bitset1a);
      assert_equal_sets(bitset1, bitset1a);
    }
    test_one_set<Test_complement::no>(bitset1, nested_set1);
    test_one_container(nested_set1);
    test_one_nested_container(bitset1, nested_set1);
  }

  static void test_two(const Bitset_t &bitset1, const Bitset_t &bitset2,
                       const Nested_set1_t &nested_set1,
                       const Nested_set2_t &nested_set2) {
    auto do_make_bitset = [](const sets::Is_nested_set auto &nested_set) {
      Bitset_t bitset;
      nested_set_to_interval_set(nested_set, bitset);
      return bitset;
    };
    {
      Bitset_t bitset1a;
      nested_set_to_interval_set(nested_set1, bitset1a);
      assert_equal_sets(bitset1, bitset1a);
    }
    {
      Bitset_t bitset2a;
      nested_set_to_interval_set(nested_set2, bitset2a);
      // intentional
      // NOLINTNEXTLINE(readability-suspicious-call-argument)
      assert_equal_sets(bitset2, bitset2a);
    }

    test_binary_predicates(bitset1, bitset2, nested_set1, nested_set2);
    test_two_containers<Is_throwing::no>(do_make_bitset, nested_set1,
                                         nested_set2);
  }

  static void exhaustive_test_for_bitset(Bitset_storage bitset_storage1) {
    MY_SCOPED_TRACE("bitset_storage1=", bitset_storage1);
    Bitset_t bitset1{bitset_storage1};
    Nested_set1_t nested_set1;
    interval_set_to_nested_set(bitset1, nested_set1);
    progress(2, outer_count, "/", middle_count, "/", inner_count,
             "SET1: ", bitset_storage1, " = ", nested_set1, "\n");
    MY_SCOPED_TRACE("set1=", nested_set1);

    test_one(bitset1, nested_set1);

    for (Bitset_storage bitset_storage2 = right_start;
         bitset_storage2 < bitset_mask(element_count); ++bitset_storage2) {
      MY_SCOPED_TRACE("bitset_storage2=", bitset_storage2);
      Bitset_t bitset2{bitset_storage2};
      Nested_set2_t nested_set2;
      interval_set_to_nested_set(bitset2, nested_set2);
      progress(3, outer_count, "/", middle_count, "/", inner_count,
               "  SET1: ", bitset_storage1, " SET2: ", bitset_storage2, " = ",
               nested_set2, "\n");
      MY_SCOPED_TRACE("set2=", nested_set2);

      test_two(bitset1, bitset2, nested_set1, nested_set2);
    }
  }

  // Exhaustively test all combinations of two sets
  static void exhaustive_test() {
    progress(1, outer_count, "/", middle_count, "/", inner_count, "\n");

    MY_SCOPED_TRACE("outer_count=", outer_count, " middle_count=", middle_count,
                    " inner_count=", inner_count);

    // Set this 'true' to run a thread per bitset1 value; 'false' to execute
    // sequentially.
    // Use a thread per left-hand set to make the test faster
    std::vector<std::thread> threads;

    for (Bitset_storage bitset_storage = left_start;
         bitset_storage < bitset_mask(element_count); ++bitset_storage) {
      auto func = [=] { exhaustive_test_for_bitset(bitset_storage); };
      if (use_multithreading)
        threads.emplace_back(func);
      else
        func();
    }
    for (auto &thread : threads) thread.join();
  }

  void test() override { exhaustive_test(); }
  int get_iterations() override { return iterations; }
};  // class Tester

const auto start{std::chrono::steady_clock::now()};
int iterations = 0;

template <class Nested_set1_t, class Nested_set2_t>
void test_all_domains(double timeout) {
  test_two_set_types<Nested_set1_t, Nested_set2_t>();
  std::vector<Tester_base *> testers{
      new Tester<Nested_set1_t, Nested_set2_t, 2, 2, 2>,
      new Tester<Nested_set1_t, Nested_set2_t, 3, 3, 1>,
      new Tester<Nested_set1_t, Nested_set2_t, 3, 1, 3>,
      new Tester<Nested_set1_t, Nested_set2_t, 1, 3, 3>,
      new Tester<Nested_set1_t, Nested_set2_t, 4, 2, 1>,
      new Tester<Nested_set1_t, Nested_set2_t, 4, 1, 2>,
      new Tester<Nested_set1_t, Nested_set2_t, 2, 4, 1>,
      new Tester<Nested_set1_t, Nested_set2_t, 2, 1, 4>,
      new Tester<Nested_set1_t, Nested_set2_t, 1, 4, 2>,
      new Tester<Nested_set1_t, Nested_set2_t, 1, 4, 2>,
      new Tester<Nested_set1_t, Nested_set2_t, 8, 1, 1>,
      new Tester<Nested_set1_t, Nested_set2_t, 1, 8, 1>,
      new Tester<Nested_set1_t, Nested_set2_t, 1, 1, 8>};

  // Randomize the order of testers.
  std::random_device rd;  // a seed source for the random number engine
  auto seed = rd();
  MY_SCOPED_TRACE(seed);
  progress(2, "seed=", seed, "\n");
  std::mt19937 gen(seed);  // rng
  std::shuffle(testers.begin(), testers.end(), gen);

  // Test as many domains as we can within the timeout.
  for (auto &tester : testers) {
    if (iterations != 0) {
      const auto now{std::chrono::steady_clock::now()};
      const auto elapsed = std::chrono::duration<double>{now - start}.count();
      const auto average = elapsed / iterations;
      const auto estimated_one_more_tester =
          average * (iterations + tester->get_iterations());
      // If the estimated time to execute one more iteration would make us
      // exceed the timeout, skip.
      if (estimated_one_more_tester >= timeout) break;
    }
    tester->test();
    iterations += tester->get_iterations();
  }
  for (auto &tester : testers) delete tester;
}

#define NESTED_TYPE(Type_a, Type_b, Type_c) \
  sets::Type_a##_nested_container<          \
      Int_traits_t,                         \
      sets::Type_b##_nested_container<      \
          String_traits_t, sets::Type_c##_interval_container<Int_traits_t>>>

#define DEFINE_NESTED_TYPE(Type_a, Type_b, Type_c)     \
  struct Nested##Type_a##Type_b##Type_c##Class         \
      : public NESTED_TYPE(Type_a, Type_b, Type_c) {}; \
  using Nested##Type_a##Type_b##Type_c##Alias =        \
      NESTED_TYPE(Type_a, Type_b, Type_c);

DEFINE_NESTED_TYPE(Map, Map, Map)
DEFINE_NESTED_TYPE(Map, Map, Vector)

#define DEFINE_SCENARIO(Type_1, Type_2, timeout)                             \
  {                                                                          \
    MY_SCOPED_TRACE(#Type_1 " vs " #Type_2);                                 \
    progress(1, #Type_1, " vs ", #Type_2, "\n");                             \
    test_all_domains<Nested##Type_1##Alias, Nested##Type_2##Alias>(timeout); \
  }

TEST(LibsSetsNested, Exhaustive) {
  // Test using different data structures for the three levels of containers.
  // Give each scenario 1/4 of the total time.
  static constexpr int scenario_timeout_sec = timeout_sec / 4;

  DEFINE_SCENARIO(MapMapMap, MapMapMap, scenario_timeout_sec);
  DEFINE_SCENARIO(MapMapMap, MapMapVector, scenario_timeout_sec * 2);
  DEFINE_SCENARIO(MapMapVector, MapMapMap, scenario_timeout_sec * 3);
  DEFINE_SCENARIO(MapMapVector, MapMapVector, scenario_timeout_sec * 4);
}

#undef DEFINE_SCENARIO

}  // namespace
