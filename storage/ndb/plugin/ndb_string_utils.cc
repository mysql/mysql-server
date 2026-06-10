/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef TEST_NDB_STRING_UTILS

#include <string>
#include <string_view>
#include <vector>

#include <NdbTap.hpp>

#include "storage/ndb/plugin/ndb_string_conv.h"
#include "storage/ndb/plugin/ndb_string_split.h"
#include "storage/ndb/plugin/ndb_string_trim.h"

using ndbcluster::from_chars_to;
using ndbcluster::split_range;
using ndbcluster::trim;

// Tests for ndb_string_trim.h
static void test_ndb_string_trim() {
  OK(trim("").empty());
  OK(trim("  ").empty());
  OK(trim("  abc  ") == "abc");
  OK(trim("abc") == "abc");
  OK(trim("  abc") == "abc");
  OK(trim("abc  ") == "abc");
  OK(trim("\t\n abc \t\n", " \t\n") == "abc");
}

// Sub-tests for ndb_string_split.h
static void test_split_basic_cases() {
  std::vector<std::string_view> out;

  out.clear();
  for (auto part : split_range("x,y")) {
    out.push_back(part);
  }
  OK(out.size() == 2);
  OK(out[0] == "x");
  OK(out[1] == "y");

  // Empty input -> one empty token
  out.clear();
  for (auto p : split_range("")) {
    out.push_back(p);
  }
  OK(out.size() == 1);
  OK(out[0].empty());

  // No delimiter present
  out.clear();
  for (auto p : split_range("abc")) {
    out.push_back(p);
  }
  OK(out.size() == 1);
  OK(out[0] == "abc");

  // Single character token
  out.clear();
  for (auto p : split_range("a")) {
    out.push_back(p);
  }
  OK(out.size() == 1);
  OK(out[0] == "a");

  // Leading delimiter -> first token empty
  out.clear();
  for (auto p : split_range(",abc", ',')) {
    out.push_back(p);
  }
  OK(out.size() == 2);
  OK(out[0].empty());
  OK(out[1] == "abc");

  // Trailing delimiter -> last token empty
  out.clear();
  for (auto p : split_range("abc,", ',')) {
    out.push_back(p);
  }
  OK(out.size() == 2);
  OK(out[0] == "abc");
  OK(out[1].empty());

  // Consecutive delimiters -> empty middle token
  out.clear();
  for (auto p : split_range("a,,b", ',')) {
    out.push_back(p);
  }
  OK(out.size() == 3);
  OK(out[0] == "a");
  OK(out[1].empty());
  OK(out[2] == "b");

  // Only delimiters -> only empty tokens
  out.clear();
  for (auto p : split_range(",,,", ',')) {
    out.push_back(p);
  }
  OK(out.size() == 4);
  OK(out[0].empty() && out[1].empty() && out[2].empty() && out[3].empty());

  // Single delimiter -> two empty tokens
  out.clear();
  for (auto p : split_range(",", ',')) {
    out.push_back(p);
  }
  OK(out.size() == 2);
  OK(out[0].empty() && out[1].empty());

  // Custom delimiter
  out.clear();
  for (auto p : split_range("a;b;c", ';')) {
    out.push_back(p);
  }
  OK(out.size() == 3);
  OK(out[0] == "a");
  OK(out[1] == "b");
  OK(out[2] == "c");
}

static void test_split_whitespace_behavior() {
  std::vector<std::string_view> out;
  for (auto p : split_range(" a , b ,  c ", ',')) {
    out.push_back(p);
  }
  OK(out.size() == 3);
  OK(out[0] == " a ");
  OK(out[1] == " b ");
  OK(out[2] == "  c ");
}

static void test_split_trim_composition() {
  std::vector<std::string> trimmed;
  for (auto p : split_range(" 1 ,  2 ,3 ", ',')) {
    trimmed.emplace_back(trim(p));
  }
  OK(trimmed.size() == 3);
  OK(trimmed[0] == "1");
  OK(trimmed[1] == "2");
  OK(trimmed[2] == "3");
}

static void test_split_iterators_semantics() {
  // Iterator semantics: empty -> one empty token, begin()!=end()
  {
    auto r = split_range("");
    OK(r.begin() != r.end());
    OK((*r.begin()).empty());
  }

  // Iterator pre-increment vs post-increment produce same sequence
  {
    std::vector<std::string_view> pre, post;
    auto r1 = split_range("a,b");
    for (auto it = r1.begin();  // NOLINT(modernize-loop-convert)
         it != r1.end(); ++it) {
      pre.push_back(*it);
    }
    auto r2 = split_range("a,b");
    for (auto it = r2.begin();  // NOLINT(modernize-loop-convert)
         it != r2.end(); it++) {
      post.push_back(*it);
    }
    OK(pre.size() == 2);
    OK(post.size() == 2);
    OK(pre[0] == "a" && pre[1] == "b");
    OK(post[0] == "a" && post[1] == "b");
  }
}

static void test_split_token_views_correctness() {
  // Token view correctness: views refer into original storage
  std::string s = "xx,a,bb,";
  std::vector<std::string_view> out;
  for (auto p : split_range(s)) {
    out.push_back(p);
  }
  OK(out.size() == 4);
  for (auto v : out) {
    OK(v.data() >= s.data());
    OK(v.data() <= s.data() + s.size());
  }
}

static void test_split_large_and_range_reuse() {
  // Large input: many small tokens
  const char *delimiter = "";
  std::string s;
  for (int i = 0; i < 100; ++i) {
    s += delimiter;
    delimiter = ",";
    s += "x";
  }
  size_t count = 0;
  for (auto p : split_range(s)) {
    (void)p;
    ++count;
  }
  OK(count == 100);

  // Range value semantics: iterate the same range twice
  auto range = split_range("x,y");
  size_t c1 = 0;
  for (auto p : range) {
    (void)p;
    ++c1;
  }
  size_t c2 = 0;
  for (auto p : range) {
    (void)p;
    ++c2;
  }
  OK(c1 == 2 && c2 == 2);
}

// Tests for ndb_string_split.h
static void test_ndb_string_split() {
  test_split_basic_cases();
  test_split_whitespace_behavior();
  test_split_trim_composition();
  test_split_iterators_semantics();
  test_split_token_views_correctness();
  test_split_large_and_range_reuse();
}

// Tests for ndb_string_conv.h
static void test_ndb_string_conv() {
  // Success cases
  {
    unsigned int v = 0;
    OK(from_chars_to("1", v));
    OK(v == 1);
    OK(from_chars_to("10", v));
    OK(v == 10);
    OK(from_chars_to("0", v));
    OK(v == 0);
    // Using string_view argument
    OK(from_chars_to(std::string_view{"37"}, v));
    OK(v == 37);
  }

  // Full consumption required
  {
    unsigned int v = 0xdeadbeU;
    OK(!from_chars_to("123x", v));
    // v remains unmodified on failure
    OK(v == 0xdeadbeU);
  }

  // Negative number for unsigned should fail
  {
    unsigned int v = 7U;
    OK(!from_chars_to("-1", v));
    OK(v == 7U);
  }

  // Overflow should fail (use a very large number)
  {
    unsigned int v = 0;
    // Assuming 32-bit unsigned, 20 nines will overflow
    OK(!from_chars_to("99999999999999999999", v));
  }

  // Empty string should fail
  {
    unsigned int v = 0;
    OK(!from_chars_to("", v));
  }

  // Whitespace is not consumed by from_chars; caller should trim first.
  {
    unsigned int v = 0;
    OK(!from_chars_to(" 42 ", v));
    // After trim it should succeed
    OK(from_chars_to(trim(" 42 "), v));
    OK(v == 42U);
  }
}

TAPTEST(NdbStringUtils) {
  test_ndb_string_trim();
  test_ndb_string_split();
  test_ndb_string_conv();
  return 1;
}

#endif  // TEST_NDB_STRING_UTILS
