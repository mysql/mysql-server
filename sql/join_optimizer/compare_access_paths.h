/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_JOIN_OPTIMIZER_COMPARE_ACCESS_PATHS_H
#define SQL_JOIN_OPTIMIZER_COMPARE_ACCESS_PATHS_H

#include <assert.h>
#include <stdint.h>

#include <cmath>

#include "sql/join_optimizer/access_path.h"
#include "sql/join_optimizer/interesting_orders.h"

enum class FuzzyComparisonResult : uint32_t {
  IDENTICAL = 0,
  FIRST_BETTER = 1,
  SECOND_BETTER = 2,
  FIRST_SLIGHTLY_BETTER = 4,
  SECOND_SLIGHTLY_BETTER = 8
};

// Compare x and y with a given fuzz factor under the assumption that the
// lesser value is preferred. If the relative difference between x and y
// is small (x and y are fuzzily identical) we still return information
// about which one is slightly better.
inline FuzzyComparisonResult FuzzyComparison(double x, double y,
                                             double fuzz_factor) {
  assert(std::isfinite(x));
  assert(std::isfinite(y));
  assert(x >= 0);
  assert(y >= 0);

  if (fuzz_factor * x < y) {
    return FuzzyComparisonResult::FIRST_BETTER;
  } else if (fuzz_factor * y < x) {
    return FuzzyComparisonResult::SECOND_BETTER;
  } else {  // Fuzzily identical
    if (x < y) {
      return FuzzyComparisonResult::FIRST_SLIGHTLY_BETTER;
    } else if (y < x) {
      return FuzzyComparisonResult::SECOND_SLIGHTLY_BETTER;
    } else {
      return FuzzyComparisonResult::IDENTICAL;
    }
  }
}

enum class PathComparisonResult {
  FIRST_DOMINATES,
  SECOND_DOMINATES,
  DIFFERENT_STRENGTHS,
  IDENTICAL,
};

PathComparisonResult CompareAccessPaths(const LogicalOrderings &orderings,
                                        const AccessPath &a,
                                        const AccessPath &b,
                                        OrderingSet obsolete_orderings);

#endif  // SQL_JOIN_OPTIMIZER_COMPARE_ACCESS_PATHS_H
