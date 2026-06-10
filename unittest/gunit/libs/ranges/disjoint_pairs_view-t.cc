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

#include <gtest/gtest.h>                  // TEST
#include <forward_list>                   // forward_list
#include <set>                            // set
#include <vector>                         // vector
#include "mysql/ranges/disjoint_pairs.h"  // Disjoint_pairs_view

namespace {

// ==== Requirements ====
//
// Disjoint_pairs_view should yield a sequence of pairs of elements over the
// underlying range.
//
// Disjoint_pairs_iterator should satisfy the iterator concept of the source
// (unless it is contiguous_iterator).
//
// Disjoint_pairs_iterator and Disjoint_pairs_view should be
// default-constructible.

static_assert(std::forward_iterator<mysql::ranges::Disjoint_pairs_iterator<
                  std::forward_list<int>::iterator>>);
static_assert(
    std::bidirectional_iterator<
        mysql::ranges::Disjoint_pairs_iterator<std::set<float>::iterator>>);
static_assert(
    std::random_access_iterator<
        mysql::ranges::Disjoint_pairs_iterator<std::vector<double>::iterator>>);

TEST(LibsRanges, DisjointPairsView) {
  std::vector<int> v{1, 2, 3, 4, 5, 6};
  // view is default-constructible...
  mysql::ranges::Disjoint_pairs_view<std::vector<int>> pair_view;
  // ... and can be assigned to
  pair_view = mysql::ranges::make_disjoint_pairs_view(v);

  std::vector<std::pair<int, int>> pair_vector{{1, 2}, {3, 4}, {5, 6}};
  auto pair_vector_it = pair_vector.begin();

  // iterating over the view works
  for (const auto pair : pair_view) {
    ASSERT_EQ(pair, *pair_vector_it);
    ++pair_vector_it;
  }

  // iterator is default-constructible...
  mysql::ranges::Disjoint_pairs_iterator<std::vector<int>::const_iterator> iter;
  // ... and can be assigned to.
  iter = pair_view.begin();
  // all the members of the view work as expected
  ASSERT_EQ(pair_view.end() - iter, 3);
  ASSERT_EQ(*iter, std::make_pair(1, 2));
  ASSERT_EQ(pair_view.front(), std::make_pair(1, 2));
  ASSERT_EQ(pair_view.front(), std::make_pair(1, 2));
  ASSERT_EQ(pair_view.back(), std::make_pair(5, 6));
  ASSERT_EQ(pair_view[1], std::make_pair(3, 4));
  ASSERT_EQ(pair_view.size(), 3);
  ASSERT_EQ(pair_view.ssize(), 3);
  ASSERT_EQ(pair_view.empty(), false);
  ASSERT_EQ(!pair_view, false);
  ASSERT_EQ((bool)pair_view, true);
}

}  // namespace
