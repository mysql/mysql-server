/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_HELPERS_INCLUDED
#define MYSQL_HARNESS_HELPERS_INCLUDED

// Standard headers
#include <algorithm>
#include <list>
#include <string>
#include <vector>

// Third-party headers
#include <gtest/gtest.h>

#include "mysql/harness/loader.h"

template <typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &v) {
  out << "{";
  for (auto &&elem : v) out << " " << elem;
  out << " }";
  return out;
}

template <typename T>
std::ostream &operator<<(std::ostream &out, const std::list<T> &v) {
  out << "{";
  for (auto &&elem : v) out << " " << elem;
  out << " }";
  return out;
}

template <typename A, typename B>
std::ostream &operator<<(std::ostream &out, const std::pair<A, B> &p) {
  return out << p.first << ":" << p.second;
}

template <typename SeqCont1, typename SeqCont2>
::testing::AssertionResult AssertSetEqual(const char *seq1_expr,
                                          const char *seq2_expr,
                                          const SeqCont1 &seq1,
                                          const SeqCont2 &seq2) {
  std::vector<typename SeqCont1::value_type> c1(seq1.begin(), seq1.end());
  std::vector<typename SeqCont2::value_type> c2(seq2.begin(), seq2.end());
  std::sort(c1.begin(), c1.end());
  std::sort(c2.begin(), c2.end());

  // Check for elements that are in the first range but not in the second.
  std::vector<typename SeqCont2::value_type> c1_not_c2;
  std::set_difference(c1.begin(), c1.end(), c2.begin(), c2.end(),
                      std::back_inserter(c1_not_c2));
  if (c1_not_c2.size() > 0) {
    auto result = ::testing::AssertionFailure();
    result << seq1_expr << " had elements not in " << seq2_expr << ": ";
    for (auto elem : c1_not_c2) result << elem << " ";
    return result;
  }

  // Check for elements that are in the second range but not in the first.
  std::vector<typename SeqCont2::value_type> c2_not_c1;
  std::set_difference(c2.begin(), c2.end(), c1.begin(), c1.end(),
                      std::back_inserter(c2_not_c1));
  if (c2_not_c1.size() > 0) {
    auto result = ::testing::AssertionFailure();
    result << seq2_expr << " had elements not in " << seq1_expr << ": ";
    for (auto elem : c2_not_c1) result << elem << " ";
    return result;
  }

  return ::testing::AssertionSuccess();
}

#define EXPECT_SETEQ(S1, S2) EXPECT_PRED_FORMAT2(AssertSetEqual, S1, S2)

::testing::AssertionResult AssertLoaderSectionAvailable(
    const char *loader_expr, const char *section_expr,
    mysql_harness::Loader *loader, const std::string &section_name);

#define EXPECT_SECTION_AVAILABLE(S, L) \
  EXPECT_PRED_FORMAT2(AssertLoaderSectionAvailable, L, S)

/**
 * Just register logger with DIM for unit tests (unlike init_log(), which also
 * initializes it)
 */
void register_test_logger();

/**
 * Register + init logger for unit tests
 *
 * Creates application ("main") logger, which will write all messages to the
 * console. Almost all of our code relies on the fact of "main" logger being
 * initialized, so it is necessary to provide one for unit tests. Also, some
 * unit tests analyze log output, and expect that output to exist on stderr.
 */
void init_test_logger(const std::list<std::string> &additional_log_domains = {},
                      const std::string &log_folder = "",
                      const std::string &log_filename = "");

#endif /* MYSQL_HARNESS_HELPERS_INCLUDED */
