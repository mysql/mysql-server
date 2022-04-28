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

#include "my_config.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "item.h"
#include "item_cmpfunc.h"
#include "sql_class.h"
#include "test_utils.h"

namespace item_func_regex_unittest {

using my_testing::Server_initializer;

static const char *substrings[2] = {
    "Folder=RootObject="
    "Operating System-WinWinServerLocalAdminis-trator1-ABCDEFGHI01-adminimm",
    "tasktype=ReconcileTask"};

static const char *wild_like = "%";
static const char *wild_rlike = ".*";
static const char escape[] = "\\";

static std::string make_like_pattern() {
  return std::string(wild_like) + substrings[0] + wild_like + substrings[1] +
         wild_like;
}

static std::string make_rlike_pattern() {
  return std::string(wild_rlike) + substrings[0] + wild_rlike + substrings[1] +
         wild_rlike;
}

static std::string make_matched_string() {
  return std::string("hello") + substrings[0] + "42" + substrings[1] +
         "goodbye";
}

CHARSET_INFO *init_collation(const char *name) {
  MY_CHARSET_LOADER loader;
  my_charset_loader_init_mysys(&loader);
  return my_collation_get_by_name(&loader, name, MYF(0));
}

Item_string *make_string_item(std::string *str) {
  CHARSET_INFO *cs = init_collation("ascii_general_ci");
  return new Item_string(str->c_str(), str->length(), cs);
}

class LikeVSRlikeTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    initializer.SetUp();

    dummy_string = "this is a dummy string";
    like_pattern = make_like_pattern();
    rlike_pattern = make_rlike_pattern();
    match_string = make_matched_string();

    dummy_subject_item = make_string_item(&dummy_string);
    match_subject_item = make_string_item(&match_string);
    like_pattern_item = make_string_item(&like_pattern);
    rlike_pattern_item = make_string_item(&rlike_pattern);
    it_escape = new Item_string(STRING_WITH_LEN(escape), &my_charset_latin1);
  }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  std::string dummy_string;
  std::string like_pattern;
  std::string rlike_pattern;
  std::string match_string;

  Server_initializer initializer;
  Item_string *dummy_subject_item;
  Item_string *match_subject_item;
  Item_string *like_pattern_item;
  Item_string *rlike_pattern_item;
  Item_string *it_escape;
};

// "dummy text"    LIKE "pattern with three %"
// "matching text" LIKE "pattern with three %"
TEST_F(LikeVSRlikeTest, SimpleLike) {
  Item_func_like *like_item = new Item_func_like(
      dummy_subject_item, like_pattern_item, it_escape, false);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  EXPECT_EQ(0, like_item->val_int());

  like_item = new Item_func_like(match_subject_item, like_pattern_item,
                                 it_escape, false);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  EXPECT_EQ(1, like_item->val_int());
}

// "dummy text"    REGEXP "pattern with three .*"
// "matching text" REGEXP "pattern with three .*"
TEST_F(LikeVSRlikeTest, SimpleRLike) {
  POS pos;
  Item *like_item =
      new Item_func_regex(pos, dummy_subject_item, rlike_pattern_item);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  EXPECT_EQ(0, like_item->val_int());
  like_item->cleanup();

  like_item = new Item_func_regex(pos, match_subject_item, rlike_pattern_item);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  EXPECT_EQ(1, like_item->val_int());
  like_item->cleanup();
}

/*
  To benchmark:
  make item_func_rlike-t
  ./unittest/gunit/item_func_rlike-t --disable-tap-output

  googletest reports total time in milliseconds,
  and we do num_iterations = 1000000;
  so translation to ns/iter is trivial.
 */

// Do each algorithm this many times. Increase value for benchmarking!
// static const size_t num_iterations = 1000000;
static const size_t num_iterations = 1;

TEST_F(LikeVSRlikeTest, BM_LikeNoMatch) {
  Item_func_like *like_item = new Item_func_like(
      dummy_subject_item, like_pattern_item, it_escape, false);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  for (size_t i = 0; i < num_iterations; ++i) {
    EXPECT_EQ(0, like_item->val_int());
  }
}

TEST_F(LikeVSRlikeTest, BM_LikeWithMatch) {
  Item_func_like *like_item = new Item_func_like(
      match_subject_item, like_pattern_item, it_escape, false);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  for (size_t i = 0; i < num_iterations; ++i) {
    EXPECT_EQ(1, like_item->val_int());
  }
}

TEST_F(LikeVSRlikeTest, BM_RlikeNoMatch) {
  POS pos;
  Item *like_item =
      new Item_func_regex(pos, dummy_subject_item, rlike_pattern_item);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  for (size_t i = 0; i < num_iterations; ++i) {
    EXPECT_EQ(0, like_item->val_int());
  }
  like_item->cleanup();
}

TEST_F(LikeVSRlikeTest, BM_RlikeWithMatch) {
  POS pos;
  Item *like_item =
      new Item_func_regex(pos, match_subject_item, rlike_pattern_item);
  EXPECT_EQ(0, like_item->fix_fields(thd(), NULL));
  for (size_t i = 0; i < num_iterations; ++i) {
    EXPECT_EQ(1, like_item->val_int());
  }
  like_item->cleanup();
}

} // namespace item_func_regex_unittest
