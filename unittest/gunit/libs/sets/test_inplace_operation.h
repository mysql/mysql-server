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

#ifndef UNITTEST_LIBS_SETS_TEST_INPLACE_OPERATIONS_H
#define UNITTEST_LIBS_SETS_TEST_INPLACE_OPERATIONS_H

#include <gtest/gtest.h>                      // ASSERT_TRUE
#include "mysql/debugging/my_scoped_trace.h"  // MY_SCOPED_TRACE
#include "mysql/sets/sets.h"                  // make_union_view
#include "test_one_set.h"                     // test_one_set

namespace unittest::libs::sets::detail {

/// Return true if Lhs_t is a boundary container and an inplace binary operation
/// given an Rhs_t&& set should be able to steal any new boundaries from the rhs
/// set, rather than allocate.
///
/// This is true if both sets are boundary containers, and both have Storage_t
/// type members, and their Storage_t type members are equal, and either both
/// support "fast insertion" (i.e., are node-based), or the lhs is empty.
///
/// @param empty_lhs true if the object is empty.
template <class Lhs_t, class Rhs_t>
bool shall_expect_stealing_operation_on_boundary_container(bool empty_lhs) {
  if constexpr (mysql::sets::Is_boundary_container<Lhs_t> &&
                mysql::sets::Is_boundary_container<Rhs_t>) {
    if constexpr (
        requires { typename Lhs_t::Storage_t; } &&
        requires { typename Rhs_t::Storage_t; }) {
      if (std::same_as<typename Lhs_t::Storage_t, typename Rhs_t::Storage_t>) {
        return Lhs_t::has_fast_insertion || empty_lhs;
      }
    }
  }
  return false;
}

/// Return true if Lhs_t, Rhs_t are boundary containers for which
/// shall_expect_stealing_operation_on_boundary_container returns true, or they
/// are interval containers and the same holds on their boundary containers.
template <class Lhs_t, class Rhs_t>
bool shall_expect_stealing_operation(bool empty_lhs) {
  if constexpr (mysql::sets::Is_interval_container<Lhs_t> &&
                mysql::sets::Is_interval_container<Rhs_t>) {
    return shall_expect_stealing_operation_on_boundary_container<
        mysql::sets::Interval_set_boundary_set_type<Lhs_t>,
        mysql::sets::Interval_set_boundary_set_type<Rhs_t>>(empty_lhs);
  } else {
    return shall_expect_stealing_operation_on_boundary_container<Lhs_t, Rhs_t>(
        empty_lhs);
  }
}

}  // namespace unittest::libs::sets::detail

namespace unittest::libs::sets {

/// Test that one of the inplace_union/inplace_intersect/inplace_subtract
/// operations works as expected.
///
/// @tparam Cont_t Type of the container to operate on.
///
/// @tparam Make_view_t Type of right-hand-side operand.
///
/// @param operation_name String name of the operation.
///
/// @param inplace_operation Two-argument callable to execute the inplace
/// operation.
///
/// @param make_truth Function that produces a truth container from
/// a Cont_t object.
///
/// @param lhs Container to operate on.
///
/// @param rhs Right hand side for Cont_t: can be a boundary container,
/// interval, or boundary.
///
/// @param lhs_relation Relation that should hold between the result and lhs:
/// for union/intersect/subtract, this should be
/// is_superset/is_subset/is_subset, respectively.
///
/// @param rhs_relation Relation that should hold between the result and rhs:
/// for union/intersect/subtract, this should be
/// is_superset/is_subset/is_disjoint, respectively.
///
/// @param make_view The function to construct a view over the result of the
/// operation: one of
/// make_union_view/make_intersection_view/make_subtraction_view. Or, in case
/// the operand types do not allow a view over them, just pass any `int`.
template <class Cont_t, class Rhs_t, class Make_view_t = int>
void test_inplace_operation(
    [[maybe_unused]] const std::string_view &operation_name,
    const auto &inplace_operation, const auto &make_truth, const Cont_t &lhs,
    const Rhs_t &rhs, const auto &lhs_relation, const auto &rhs_relation,
    const Make_view_t &make_view) {
  MY_SCOPED_TRACE(operation_name);

  // Compute "truth_result" - what we get when executing the inplace operation
  // on "truth_lhs" and "truth_rhs".
  auto truth_lhs = make_truth(lhs);
  auto truth_rhs = make_truth(rhs);
  auto truth_result = make_truth(lhs);
  inplace_operation(truth_result, truth_rhs);

  // The relations hold for truth_result
  ASSERT_TRUE(lhs_relation(truth_result, truth_lhs));
  ASSERT_TRUE(rhs_relation(truth_result, truth_rhs));

  auto check_result = [&](const Cont_t &result) {
    auto debug_info = [&] {
      return mysql::strconv::throwing::concat_text("lhs=", lhs, " rhs=", rhs,
                                                   " result=", result);
    };
    // The relations hold for result
    ASSERT_TRUE(lhs_relation(result, lhs)) << debug_info();
    ASSERT_TRUE(rhs_relation(result, rhs)) << debug_info();

    // Compute "converted_result", and assert that converted_result and and
    // truth_result are equal.
    auto converted_result = make_truth(result);
    assert_equal_sets(truth_result, converted_result);
    // The relations hold for converted_result
    ASSERT_TRUE(lhs_relation(converted_result, truth_lhs)) << debug_info();
    ASSERT_TRUE(rhs_relation(converted_result, truth_rhs)) << debug_info();
  };

  {
    MY_SCOPED_TRACE(
        "inplace operation with lvalue reference RHS (normal case)");
    // Compute "result" - what we get when executing the inplace operation on
    // "lhs" and "rhs".
    Cont_t result;
    assign_nocheck(result, lhs);
    inplace_operation(result, rhs);
    check_result(result);
  }

  {
    MY_SCOPED_TRACE(
        "inplace operation with rvalue reference RHS (move "
        "semantics/stealing)");
    // Copy of rhs
    Rhs_t rhs2;
    assign_nocheck(rhs2, rhs);

    // Define `result`, with a memory resource that counts the allocations.
    int alloc_counter = 0;
    mysql::allocators::Memory_resource counting_memory_resource(
        [&alloc_counter](std::size_t size) {
          ++alloc_counter;
          return std::malloc(size);
        },
        [](void *ptr) { std::free(ptr); });
    Cont_t result(counting_memory_resource);

    // Copy lhs to `result`
    assign_nocheck(result, lhs);

    // Read the counter value before the operation and check if lhs was empty
    // before the operation.
    int counter_before = alloc_counter;
    bool was_empty = lhs.empty();

    // Execute the operation with move-semantics and check the result.
    inplace_operation(result, std::move(rhs2));
    check_result(result);

    // If we can expect that the operation is able to steal from the rhs (based
    // on the Lhs and Rhs types and the emptiness of the lhs), assert that no
    // allocations occurred.
    if (detail::shall_expect_stealing_operation<Cont_t, Rhs_t>(was_empty)) {
      ASSERT_EQ(alloc_counter, counter_before);
    }
  }

  if constexpr (!std::same_as<Make_view_t, int>) {
    auto view = make_view(lhs, rhs);
    auto converted_view = make_truth(view);
    static_assert(mysql::sets::Is_compatible_set<decltype(view), Cont_t>);
    static_assert(mysql::sets::Is_compatible_set<decltype(converted_view),
                                                 decltype(truth_result)>);

    // Results are equal to the corresponding views.
    assert_equal_sets(truth_result, converted_view);

    // Test the set properties of the view
    constexpr auto test_complement =
        mysql::sets::Is_boundary_set<Cont_t> ||
                mysql::sets::Is_interval_set<Cont_t>
            ? Test_complement::yes
            : Test_complement::no;
    test_one_set<test_complement>(truth_result, view);
  }
}

// Helper template lambdas, passed to test_inplace_operation.
//
// If the free functions such as mysql::sets::make_union_view are passed
// directly as arguments to test_inplace_operation, the compiler can't deduce
// their type. But when we wrap them in lambdas, it can. I am not sure why but
// one works and not the other.
inline auto inplace_insert_lambda = [](auto &cont, const auto &rhs) {
  return cont.insert(rhs);
};
inline auto inplace_remove_lambda = [](auto &cont, const auto &rhs) {
  return cont.remove(rhs);
};
inline auto inplace_union_lambda = [](auto &cont, auto &&rhs) {
  return cont.inplace_union(std::forward<decltype(rhs)>(rhs));
};
inline auto inplace_intersect_lambda = [](auto &cont, auto &&rhs) {
  return cont.inplace_intersect(std::forward<decltype(rhs)>(rhs));
};
inline auto inplace_subtract_lambda = [](auto &cont, auto &&rhs) {
  return cont.inplace_subtract(std::forward<decltype(rhs)>(rhs));
};
inline auto boundary_inplace_union_lambda = [](auto &cont, const auto &rhs) {
  return cont.inplace_union(rhs.start(), rhs.exclusive_end());
};
inline auto boundary_inplace_intersect_lambda = [](auto &cont,
                                                   const auto &rhs) {
  return cont.inplace_intersect(rhs.start(), rhs.exclusive_end());
};
inline auto boundary_inplace_subtract_lambda = [](auto &cont, const auto &rhs) {
  return cont.inplace_subtract(rhs.start(), rhs.exclusive_end());
};
inline auto make_boundary_inplace_union_hint(int position) {
  return [=](auto &cont, const auto &rhs) {
    auto it = std::next(cont.begin(), position);
    return cont.inplace_union(it, rhs.start(), rhs.exclusive_end());
  };
}
inline auto make_boundary_inplace_subtract_hint(int position) {
  return [=](auto &cont, const auto &rhs) {
    auto it = std::next(cont.begin(), position);
    return cont.inplace_subtract(it, rhs.start(), rhs.exclusive_end());
  };
}
inline auto make_union_view_lambda = [](const auto &lhs, const auto &rhs) {
  return mysql::sets::make_union_view(lhs, rhs);
};
inline auto make_intersection_view_lambda = [](const auto &lhs,
                                               const auto &rhs) {
  return mysql::sets::make_intersection_view(lhs, rhs);
};
inline auto make_subtraction_view_lambda = [](const auto &lhs,
                                              const auto &rhs) {
  return mysql::sets::make_subtraction_view(lhs, rhs);
};
inline auto is_subset_lambda = [](const auto &lhs, const auto &rhs) {
  return mysql::sets::is_subset(lhs, rhs);
};
inline auto is_superset_lambda = [](const auto &lhs, const auto &rhs) {
  return mysql::sets::is_superset(lhs, rhs);
};
inline auto is_disjoint_lambda = [](const auto &lhs, const auto &rhs) {
  return mysql::sets::is_disjoint(lhs, rhs);
};
inline auto is_equal_lambda = [](const auto &lhs, const auto &rhs) {
  return mysql::sets::is_equal(lhs, rhs);
};
inline auto contains_lambda = [](const auto &lhs, const auto &rhs) {
  return mysql::sets::contains_element(lhs, rhs);
};
inline auto does_not_contain_lambda = [](const auto &lhs, const auto &rhs) {
  return !mysql::sets::contains_element(lhs, rhs);
};

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_TEST_INPLACE_OPERATIONS_H
