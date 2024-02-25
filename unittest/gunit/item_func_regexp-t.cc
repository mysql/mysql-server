/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
   */

#include "my_config.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
#include <limits>

#include "sql/item_regexp_func.h"
#include "sql/parse_tree_items.h"
#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/item_utils.h"
#include "unittest/gunit/mock_parse_tree.h"
#include "unittest/gunit/test_utils.h"

namespace item_func_regexp_unittest {

using my_testing::Mock_pt_item_list;
using my_testing::Server_initializer;
using std::string;

class ItemFuncRegexpTest : public ::testing::Test {
 protected:
  void SetUp() override { initializer.SetUp(); }
  void TearDown() override { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;

  template <typename Item_type>
  void test_print(const char *expected,
                  std::initializer_list<const char *> args) {
    auto items = new (thd()->mem_root) Mock_pt_item_list(args);
    Item *item = new Item_type(POS(), items);
    Parse_context pc(thd(), thd()->lex->query_block);
    item->itemize(&pc, &item);
    item->fix_fields(thd(), nullptr);
    String buf;
    item->print(thd(), &buf, QT_ORDINARY);
    EXPECT_STREQ(expected, buf.c_ptr_safe());
  }
};

TEST_F(ItemFuncRegexpTest, Print) {
  test_print<Item_func_regexp_instr>("regexp_instr('abc','def')",
                                     {"abc", "def"});
  test_print<Item_func_regexp_like>("regexp_like('abc','def')", {"abc", "def"});
  test_print<Item_func_regexp_replace>("regexp_replace('ab','c','d')",
                                       {"ab", "c", "d"});
  test_print<Item_func_regexp_substr>("regexp_substr('x','y')", {"x", "y"});
}

static bool is_control_character(char c) {
  // This set of control characters is by no means exhaustive. It's based solely
  // on experimentation of which characters can't be used in a single-character
  // pattern in this particular regexp library (ICU).
  static const std::vector<char> control_chars = {
      '.', '$', '|', '^', '(', ')', '*', '+', '?', '[', '\\', '{', '}'};
  return std::find(control_chars.begin(), control_chars.end(), c) !=
         control_chars.end();
}

Item *make_binary_literal(THD *thd, const string &s) {
  POS pos;  // We expect this object to be copied.
  LEX_STRING lex_string = {const_cast<char *>(s.c_str()), s.length()};
  return new (thd->mem_root) PTI_text_literal_underscore_charset(
      pos, false, &my_charset_bin, lex_string);
}

TEST_F(ItemFuncRegexpTest, BinaryCharset) {
  static const int max_char = std::numeric_limits<unsigned char>::max();
  for (int i = 0; i <= max_char; ++i) {
    string subject;
    subject = char(i);
    for (int j = 0; j <= max_char; ++j) {
      string pattern;
      if (is_control_character(char(j))) pattern = '\\';
      pattern += char(j);

      Item *subject_item = make_binary_literal(thd(), subject);
      Item *pattern_item = make_binary_literal(thd(), pattern);
      auto rlike = make_resolved<Item_func_regexp_like>(thd(), subject_item,
                                                        pattern_item);
      if (i == j)
        EXPECT_EQ(1, rlike->val_int())
            << "They're equal(" << i
            << "), they should match via regexp matching.";
      else
        EXPECT_EQ(0, rlike->val_int())
            << "They're not equal(" << i << " vs " << j
            << "), they should not match via regexp matching.";
    }
  }
}

/*
  Benchmark performance of LIKE vs REGEXP.
  LIKE uses my_wildcmp which calls my_wildcmp_8bit_impl.
  REGEXP uses the ICU regexp matcher, where most of the time is spent in
    RegexMatcher::MatchChunkAt, ucase_toFullFolding and
    CaseFoldingUCharIterator::next
  It turns out ICU is orders of magnitute slower, esp. if there is no match.
 */
static const char *substrings[2] = {
    "Folder=RootObject="
    "Operating System-WinWinServerLocalAdminis-trator1-ABCDEFGHI01-adminimm",
    "tasktype=ReconcileTask"};

static std::string make_like_pattern() {
  return std::string("%") + substrings[0] + "%" + substrings[1] + "%";
}

static std::string make_rlike_pattern() {
  return std::string(".*") + substrings[0] + ".*" + substrings[1] + ".*";
}

static std::string make_matched_string() {
  return std::string("hello") + substrings[0] + "42" + substrings[1] +
         "goodbye";
}

CHARSET_INFO *init_collation(const char *name) {
  MY_CHARSET_LOADER loader;
  return my_collation_get_by_name(&loader, name, MYF(0));
}

Item_string *make_string_item(const std::string *str) {
  CHARSET_INFO *cs = init_collation("ascii_general_ci");
  return new Item_string(str->data(), str->length(), cs);
}

class LikeVSRlikeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    initializer.SetUp();
    dummy_subject_item = make_string_item(&dummy_string);
    match_subject_item = make_string_item(&match_string);
    like_pattern_item = make_string_item(&like_pattern);
    rlike_pattern_item = make_string_item(&rlike_pattern);
  }
  void TearDown() override { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  const std::string dummy_string = "this is a dummy string";
  const std::string like_pattern = make_like_pattern();
  const std::string rlike_pattern = make_rlike_pattern();
  const std::string match_string = make_matched_string();

  Server_initializer initializer;
  Item_string *dummy_subject_item;
  Item_string *match_subject_item;
  Item_string *like_pattern_item;
  Item_string *rlike_pattern_item;
};

// "dummy text"    LIKE "pattern with three %"
// "matching text" LIKE "pattern with three %"
TEST_F(LikeVSRlikeTest, SimpleLike) {
  Item_func_like *like_item =
      new Item_func_like(dummy_subject_item, like_pattern_item);
  EXPECT_EQ(0, like_item->fix_fields(thd(), nullptr));
  EXPECT_EQ(0, like_item->val_int());

  like_item = new Item_func_like(match_subject_item, like_pattern_item);
  EXPECT_EQ(0, like_item->fix_fields(thd(), nullptr));
  EXPECT_EQ(1, like_item->val_int());
}

// "dummy text"    REGEXP "pattern with three .*"
// "matching text" REGEXP "pattern with three .*"
TEST_F(LikeVSRlikeTest, SimpleRLike) {
  Item *like_item = make_resolved<Item_func_regexp_like>(
      thd(), dummy_subject_item, rlike_pattern_item);
  EXPECT_EQ(0, like_item->val_int());
  like_item = make_resolved<Item_func_regexp_like>(thd(), match_subject_item,
                                                   rlike_pattern_item);
  EXPECT_EQ(1, like_item->val_int());
}

// Benchmark LIKE which does not match.
static void BM_LikeNoMatch(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  std::string dummy_string = "this is a dummy string";
  std::string like_pattern = make_like_pattern();
  Item_func_like *like_item = new Item_func_like(
      make_string_item(&dummy_string), make_string_item(&like_pattern));
  like_item->fix_fields(initializer.thd(), nullptr);

  StartBenchmarkTiming();
  longlong num_matches = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    num_matches += like_item->val_int();
  }
  StopBenchmarkTiming();
  EXPECT_EQ(0, num_matches);

  initializer.TearDown();
}
BENCHMARK(BM_LikeNoMatch)

// Benchmark LIKE with match.
static void BM_LikeWithMatch(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  std::string match_string = make_matched_string();
  std::string like_pattern = make_like_pattern();
  Item_func_like *like_item = new Item_func_like(
      make_string_item(&match_string), make_string_item(&like_pattern));
  like_item->fix_fields(initializer.thd(), nullptr);

  StartBenchmarkTiming();
  longlong num_matches = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    num_matches += like_item->val_int();
  }
  StopBenchmarkTiming();
  EXPECT_EQ(num_iterations, num_matches);

  initializer.TearDown();
}
BENCHMARK(BM_LikeWithMatch)

// Benchmark REGEXP which does not match.
static void BM_RlikeNoMatch(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  std::string dummy_string = "this is a dummy string";
  std::string rlike_pattern = make_rlike_pattern();
  Item *like_item = make_resolved<Item_func_regexp_like>(
      initializer.thd(), make_string_item(&dummy_string),
      make_string_item(&rlike_pattern));
  StartBenchmarkTiming();
  longlong num_matches = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    num_matches += like_item->val_int();
  }
  StopBenchmarkTiming();
  EXPECT_EQ(0, num_matches);

  initializer.TearDown();
}
BENCHMARK(BM_RlikeNoMatch)

// Benchmark REGEXP with match.
static void BM_RlikeWithMatch(size_t num_iterations) {
  StopBenchmarkTiming();

  my_testing::Server_initializer initializer;
  initializer.SetUp();

  std::string match_string = make_matched_string();
  std::string rlike_pattern = make_rlike_pattern();
  Item *like_item = make_resolved<Item_func_regexp_like>(
      initializer.thd(), make_string_item(&match_string),
      make_string_item(&rlike_pattern));
  StartBenchmarkTiming();
  longlong num_matches = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    num_matches += like_item->val_int();
  }
  StopBenchmarkTiming();
  EXPECT_EQ(num_iterations, num_matches);

  initializer.TearDown();
}
BENCHMARK(BM_RlikeWithMatch)

}  // namespace item_func_regexp_unittest
