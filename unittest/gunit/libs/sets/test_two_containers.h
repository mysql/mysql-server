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

#ifndef UNITTEST_LIBS_SETS_TEST_TWO_CONTAINERS_H
#define UNITTEST_LIBS_SETS_TEST_TWO_CONTAINERS_H

#include <gtest/gtest.h>                          // ASSERT_TRUE
#include <algorithm>                              // move
#include "mysql/debugging/my_scoped_trace.h"      // MY_SCOPED_TRACE
#include "mysql/debugging/unittest_assertions.h"  // ASSERT_OK
#include "set_assertions.h"                       // assert_equal_sets
#include "test_inplace_operation.h"               // test_inplace_operation

namespace unittest::libs::sets {

// NOLINTNEXTLINE(performance-enum-size): silence clang-tidy's pointless hint
enum class Is_throwing { no, yes };

/// Exercise container operations with two operands of any set type.
///
/// User should pass two containers to test, and a function that computes a
/// "truth" from a container (a better word than "truth" would be "reference",
/// but that is overloaded as a C++ term already). The truth is another
/// container type supporting the same set of operations. Each operation is
/// computed first on the two containers to test, and then on truths computed
/// from the two containers to test. The results are then compared. It is not
/// required that the tested containers and the truths are compatible - they may
/// have different categories. But we test that any operation applied on the two
/// tested sets has the same result as the operation applied on the truths.
///
/// @tparam is_throwing If yes, test copy constructors and copy assignment
/// operators, and also expect that the `assign` member with copy semantics
/// returns void. Otherwise, don't test copy constructors and copy assignment,
/// and assume that the `assign` member with copy semantics return
/// mysql::utils::Return_status.
///
/// @tparam Cont1_t Type of the left-hand-side set.
///
/// @tparam Cont2_t Type of the right-hand-side set. It should have the same
/// category and traits as Cont1_t.
///
/// @param make_truth Function that creates a truth from one of
/// the given containers.
///
/// @param cont1 Left-hand-size container to test.
///
/// @param cont2 Right-hand-size container to test.
template <Is_throwing is_throwing, class Cont1_t, class Cont2_t>
void test_two_containers(const auto &make_truth, const Cont1_t &cont1,
                         const Cont2_t &cont2) {
  constexpr bool same_source_and_dest = std::same_as<Cont1_t, Cont2_t>;
  MY_SCOPED_TRACE("test_two_containers");

  {
    MY_SCOPED_TRACE("constructor taking only a Memory_resource");
    Cont1_t tmp(mysql::allocators::Memory_resource{});  // Test!
    assert_equal_sets(tmp, mysql::sets::make_empty_set_view_like<Cont1_t>());
  }

  // Idiomatic copy semantics only possible with throwing interface.
  if constexpr (is_throwing == Is_throwing::yes) {
    {
      MY_SCOPED_TRACE("copy constructor");
      Cont2_t src;
      assign_nocheck(src, cont2);  // Prepare src
      Cont1_t dst(src);            // Test!
      src.clear();  // clear src so the test fails if src and dst share
                    // anything.
      assert_equal_sets(dst, cont2);
    }

    {
      MY_SCOPED_TRACE("copy constructor with Memory_resource");
      Cont2_t src;
      assign_nocheck(src, cont2);                              // Prepare src
      Cont1_t dst(src, mysql::allocators::Memory_resource{});  // Test!
      src.clear();  // clear src so the test fails if src and dst share
                    // anything.
      assert_equal_sets(dst, cont2);
    }

    {
      MY_SCOPED_TRACE("copy assignment operator");
      Cont1_t dst(cont1);
      {
        Cont2_t src;
        assign_nocheck(src, cont2);  // Prepare src
        dst = src;                   // Test!
      }
      assert_equal_sets(dst, cont2);
    }
  }

  // Idiomatic move semantics possible with throwing interface, or if the source
  // and target types are equal.
  if constexpr (is_throwing == Is_throwing::yes || same_source_and_dest) {
    {
      MY_SCOPED_TRACE("move constructor");
      Cont2_t tmp;
      {
        Cont2_t src;
        assign_nocheck(src, cont2);   // Prepare src
        Cont1_t dst(std::move(src));  // Test!
        assert_equal_sets(dst, cont2);
        // Ensure the test fails if src and dst share anything: tmp will use the
        // same data as dst, and we test tmp after src has expired.
        assign_nocheck(tmp, std::move(dst));
      }
      assert_equal_sets(tmp, cont2);
    }

    {
      MY_SCOPED_TRACE("move assignment operator");
      Cont1_t dst;
      assign_nocheck(dst, cont1);  // Prepare dst
      {
        Cont2_t src;
        assign_nocheck(src, cont2);  // Prepare src
        dst = std::move(src);        // Test!
      }
      // Ensure that the test fails if src and dst share anything, by checking
      // this after src expires.
      assert_equal_sets(dst, cont2);
    }
  }

  {
    MY_SCOPED_TRACE("`assign` member with copy semantics");
    Cont1_t dst;
    assign_nocheck(dst, cont1);  // Prepare dst
    {
      Cont2_t src;
      assign_nocheck(src, cont2);  // Prepare src
      // dst.assign returns void if the interface is throwing; otherwise status
      if constexpr (is_throwing == Is_throwing::yes) {
        dst.assign(cont2);  // Test!
      } else {
        auto ret = dst.assign(cont2);  // Test!
        ASSERT_OK(ret);
      }
    }
    assert_equal_sets(dst, cont2);
  }

  {
    MY_SCOPED_TRACE("`assign` member with move semantics");
    Cont1_t dst;
    assign_nocheck(dst, cont1);  // Prepare dst
    {
      Cont2_t src;
      assign_nocheck(src, cont2);  // Prepare src
      // dst.assign returns void if the class is throwing or the source and
      // target types are equal; otherwise return status.
      if constexpr (is_throwing == Is_throwing::yes || same_source_and_dest) {
        dst.assign(std::move(src));  // Test!
      } else {
        auto ret = dst.assign(std::move(src));  // Test!
        ASSERT_OK(ret);
      }
    }
    assert_equal_sets(dst, cont2);
  }

  if constexpr (is_throwing == Is_throwing::yes &&
                !mysql::sets::Is_interval_set<Cont1_t>) {
    {
      MY_SCOPED_TRACE("Constructor taking iterators");
      Cont1_t tmp;
      {
        Cont2_t src;
        assign_nocheck(src, cont2);           // Prepare src
        Cont1_t dst(src.begin(), src.end());  // Test!
        assert_equal_sets(dst, cont2);
        // Ensure the test fails if src and dst share anything: tmp will use the
        // same data as dst, and we test tmp after src has expired.
        assign_nocheck(tmp, std::move(dst));
      }
      assert_equal_sets(tmp, cont2);
    }

    {
      MY_SCOPED_TRACE("Constructor taking iterators and Memory_resource");
      Cont1_t tmp;
      {
        Cont2_t src;
        assign_nocheck(src, cont2);  // Prepare src
        Cont1_t dst(src.begin(), src.end(),
                    mysql::allocators::Memory_resource{});  // Test!
        assert_equal_sets(dst, cont2);
        // Ensure the test fails if src and dst share anything: tmp will use the
        // same data as dst, and we test tmp after src has expired.
        assign_nocheck(tmp, std::move(dst));
      }
      assert_equal_sets(tmp, cont2);
    }
  }

  if constexpr (same_source_and_dest) {
    MY_SCOPED_TRACE("std::swap");
    Cont2_t src;
    assign_nocheck(src, cont2);  // Prepare src
    Cont1_t dst;
    assign_nocheck(dst, cont1);  // Prepare dst
    std::swap(src, dst);         // Test!
    {
      MY_SCOPED_TRACE("checking cont1");
      assert_equal_sets(src, cont1);
    }
    {
      MY_SCOPED_TRACE("checking cont2");
      assert_equal_sets(dst, cont2);
    }
  }

  // Inplace operations
  {
    MY_SCOPED_TRACE("inplace operations");
    test_inplace_operation("union", inplace_union_lambda, make_truth, cont1,
                           cont2, is_superset_lambda, is_superset_lambda,
                           make_union_view_lambda);
    test_inplace_operation("intersection", inplace_intersect_lambda, make_truth,
                           cont1, cont2, is_subset_lambda, is_subset_lambda,
                           make_intersection_view_lambda);
    test_inplace_operation("subtraction", inplace_subtract_lambda, make_truth,
                           cont1, cont2, is_subset_lambda, is_disjoint_lambda,
                           make_subtraction_view_lambda);
  }
}

}  // namespace unittest::libs::sets

#endif  // ifndef UNITTEST_LIBS_SETS_TEST_TWO_CONTAINERS_H
