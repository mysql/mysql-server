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

#include <gtest/gtest.h>                    // TEST
#include <iterator>                         // forward_iterator
#include <map>                              // std::map
#include <vector>                           // std::vector
#include "mysql/meta/is_specialization.h"   // Is_specialization
#include "mysql/ranges/flat_view.h"         // make_flat_view
#include "mysql/ranges/projection_views.h"  // make_mapped_view

namespace {

// ==== Basic tests for flat views ====
//
// A flat view provides a linear sequence over the innermost values of a nested
// structure. This test verifies basic properties of flat views:
//
// - The sequence has the expected size and elements
// - The iterator is a forward iterator

using namespace mysql;

struct My_unfold {
  template <std::ranges::range Range_t>
  static decltype(auto) unfold(const Range_t &range) {
    if constexpr (meta::Is_specialization<Range_t, std::map>)
      // Unfold std::map by picking the value.
      return ranges::make_mapped_view(range);
    else
      // Unfold other ranges as themselves.
      return range;
  }

  // Disable unfolding for std::string, by not returning a range.
  //
  // (Otherwise we would get the sequence 'a', 'b', 'f', 'o', 'o', ...)
  template <std::ranges::range String_t>
    requires meta::Is_specialization<String_t, std::basic_string>
  static void unfold(String_t) {}
};

using Inner_t = std::map<float, std::string>;
using Middle_t = std::vector<Inner_t>;
using Outer_t = std::map<int, Middle_t>;
static_assert(ranges::Can_unfold_with<Inner_t, My_unfold>);
static_assert(!ranges::Can_unfold_twice_with<Inner_t, My_unfold>);
static_assert(ranges::Can_unfold_twice_with<Middle_t, My_unfold>);
static_assert(ranges::Can_unfold_twice_with<Outer_t, My_unfold>);

TEST(LibsRangesFlatView, Basic) {
  Outer_t nested;

  // Create a structure like:
  // {
  //   1: [{1.2: "a", 1.3: "b"}, {}],
  //   2: [],
  //   3: [],
  //   4: [{}, {}, {0.1: "foo", 0.2: "bar", 0.3: "baz"}]
  // }
  nested.emplace(1, Middle_t{});
  {
    nested[1].emplace_back();
    {
      nested[1][0].emplace(1.2, "a");
      nested[1][0].emplace(1.3, "b");
    }
    nested[1].emplace_back();
  }
  nested.emplace(2, Middle_t{});
  nested.emplace(3, Middle_t{});
  nested.emplace(4, Middle_t{});
  {
    nested[4].emplace_back();
    nested[4].emplace_back();
    nested[4].emplace_back();
    {
      nested[4][2].emplace(0.1, "foo");
      nested[4][2].emplace(0.2, "bar");
      nested[4][2].emplace(0.3, "baz");
    }
  }
  std::string truth[] = {"a", "b", "foo", "bar", "baz"};

  // Flat view over the nested structure
  auto flat_view = ranges::make_flat_view<My_unfold>(nested);
  auto tester = [&truth](auto &fv) {
    ASSERT_EQ(fv.size(), 5);
    static_assert(
        std::forward_iterator<ranges::Range_iterator_type<decltype(fv)>>);
    ASSERT_TRUE(std::ranges::equal(fv, truth));
    auto it = fv.begin();
    // Cover the case where we have to compare the inner iterators to determine
    // that two flat view iterators are equal.
    ++it;
    ++it;
    ++it;
    ASSERT_EQ(std::ranges::next(fv.begin(), 3), it);
    ASSERT_NE(std::ranges::next(fv.begin(), 4), it);
  };

  // Flat view over flat view should work too (idempotent)
  auto flat_view2 = ranges::make_flat_view<My_unfold>(flat_view);
  tester(flat_view2);

  // Flat view over flat view over flat view should work too (idempotent)
  auto flat_view3 = ranges::make_flat_view<My_unfold>(flat_view2);
  tester(flat_view3);
}

TEST(LibsRangesFlatView, BasicEmpty) {
  // Same structure as above, but without the innermost elements.
  Outer_t empty;
  empty.emplace(1, Middle_t{});
  empty[1].emplace_back();
  empty[1].emplace_back();
  empty.emplace(2, Middle_t{});
  empty.emplace(3, Middle_t{});
  empty.emplace(4, Middle_t{});
  empty[4].emplace_back();
  empty[4].emplace_back();

  // Flat view over nested structure.
  auto empty_flat_view = ranges::make_flat_view<My_unfold>(empty);
  ASSERT_EQ(empty_flat_view.size(), 0);
  ASSERT_TRUE(empty_flat_view.empty());

  // Flat view over flat view should work too (idempotent)
  auto empty_flat_view2 = ranges::make_flat_view<My_unfold>(empty_flat_view);
  ASSERT_EQ(empty_flat_view2.size(), 0);
  ASSERT_TRUE(empty_flat_view2.empty());
}

}  // namespace
