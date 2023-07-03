/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "sql/join_optimizer/compare_access_paths.h"
#include "unittest/gunit/test_utils.h"

TEST(CompareAccessPathsTest, FuzzyComparison) {
  double fuzz_factor = 1.01;
  EXPECT_EQ(FuzzyComparison(1.0, 1.0, fuzz_factor),
            FuzzyComparisonResult::IDENTICAL);
  EXPECT_EQ(FuzzyComparison(1.0, 1.02, fuzz_factor),
            FuzzyComparisonResult::FIRST_BETTER);
  EXPECT_EQ(FuzzyComparison(1.02, 1.0, fuzz_factor),
            FuzzyComparisonResult::SECOND_BETTER);
  EXPECT_EQ(FuzzyComparison(1.0, 1.005, fuzz_factor),
            FuzzyComparisonResult::FIRST_SLIGHTLY_BETTER);
  EXPECT_EQ(FuzzyComparison(1.005, 1.0, fuzz_factor),
            FuzzyComparisonResult::SECOND_SLIGHTLY_BETTER);

  // x is significantly less (better) than y if fuzz_factor * x < y.
  // Verify that for x = 1.0 we switch from better to slightly better
  // around y = 1.01.
  EXPECT_EQ(FuzzyComparison(1.0, 1.0101, fuzz_factor),
            FuzzyComparisonResult::FIRST_BETTER);
  EXPECT_EQ(FuzzyComparison(1.0, 1.0099, fuzz_factor),
            FuzzyComparisonResult::FIRST_SLIGHTLY_BETTER);

  // Exchanging x and y.
  EXPECT_EQ(FuzzyComparison(1.0101, 1.0, fuzz_factor),
            FuzzyComparisonResult::SECOND_BETTER);
  EXPECT_EQ(FuzzyComparison(1.0099, 1.0, fuzz_factor),
            FuzzyComparisonResult::SECOND_SLIGHTLY_BETTER);
}

TEST(CompareAccessPathsTest, CompareAccessPaths) {
  // We need the test server since the LogicalOrderings constructor takes
  // a pointer to THD.
  my_testing::Server_initializer m_initializer;
  m_initializer.SetUp();
  THD *thd = m_initializer.thd();
  LogicalOrderings orderings(thd);
  std::string trace;
  orderings.Build(thd, &trace);
  OrderingSet obsolete_orderings;

  AccessPath a;
  // Discrete/categorical cost dimensions (non-fuzzy comparison).
  a.parameter_tables = 0b111;
  a.ordering_state = 0;
  a.safe_for_rowid = AccessPath::Safety::UNSAFE;
  // Numerical cost dimensions (fuzzy comparison).
  a.set_num_output_rows(100.0);
  a.cost = 100.0;
  a.init_cost = 100.0;
  a.init_once_cost = 0.0;
  static_assert(std::is_trivially_copy_constructible<AccessPath>::value);
  AccessPath b = a;

  EXPECT_EQ(CompareAccessPaths(orderings, a, b, obsolete_orderings),
            PathComparisonResult::IDENTICAL);
  EXPECT_EQ(CompareAccessPaths(orderings, b, a, obsolete_orderings),
            PathComparisonResult::IDENTICAL);

  // Domination from a single discrete dimension (the parameter tables of one
  // path is a subset of the parameter tables of the other path).
  b.parameter_tables = 0b001;
  EXPECT_EQ(CompareAccessPaths(orderings, a, b, obsolete_orderings),
            PathComparisonResult::SECOND_DOMINATES);
  EXPECT_EQ(CompareAccessPaths(orderings, b, a, obsolete_orderings),
            PathComparisonResult::FIRST_DOMINATES);

  // Different strengths caused by categorical dimensions.
  a.safe_for_rowid = AccessPath::Safety::SAFE;
  EXPECT_EQ(CompareAccessPaths(orderings, a, b, obsolete_orderings),
            PathComparisonResult::DIFFERENT_STRENGTHS);
  EXPECT_EQ(CompareAccessPaths(orderings, b, a, obsolete_orderings),
            PathComparisonResult::DIFFERENT_STRENGTHS);

  // Fuzzily identical, neither path dominates.
  a = b;
  a.cost = 100.5;
  b.init_cost = 100.5;
  EXPECT_EQ(CompareAccessPaths(orderings, a, b, obsolete_orderings),
            PathComparisonResult::IDENTICAL);
  EXPECT_EQ(CompareAccessPaths(orderings, b, a, obsolete_orderings),
            PathComparisonResult::IDENTICAL);

  // Fuzzily identical, but one path dominates (slightly).
  b.cost = 100.0;
  b.init_cost = 100.0;
  a.cost = 99.5;
  a.init_cost = 99.5;
  EXPECT_EQ(CompareAccessPaths(orderings, a, b, obsolete_orderings),
            PathComparisonResult::FIRST_DOMINATES);
  EXPECT_EQ(CompareAccessPaths(orderings, b, a, obsolete_orderings),
            PathComparisonResult::SECOND_DOMINATES);

  // Different strenghts in numerical dimensions.
  a.cost = 100.0;
  b.cost = 95.0;
  a.init_cost = 95.0;
  b.init_cost = 100.0;
  EXPECT_EQ(CompareAccessPaths(orderings, a, b, obsolete_orderings),
            PathComparisonResult::DIFFERENT_STRENGTHS);
  EXPECT_EQ(CompareAccessPaths(orderings, b, a, obsolete_orderings),
            PathComparisonResult::DIFFERENT_STRENGTHS);
}
