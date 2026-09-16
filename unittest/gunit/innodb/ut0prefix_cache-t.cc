/* Copyright (c) 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <string_view>

#include <gtest/gtest.h>

#include "my_config.h"
#include "storage/innobase/include/univ.i"
#include "ut0prefix_cache.h"

TEST(ut0prefix_cache, consumes_matching_prefix) {
  ut::prefix_cache<char, int> cache(4);
  ASSERT_TRUE(cache.insert("abc", 7));

  std::string_view buffer = "abcdef";
  int *value = cache.find_value_for_prefix_of(buffer);

  ASSERT_NE(nullptr, value);
  ASSERT_EQ(7, *value);
  ASSERT_EQ("def", buffer);
}

#ifdef UNIV_DEBUG
TEST(ut0prefix_cache, asserts_prefix_free) {
  ut::prefix_cache<char, int> cache(4);
  ASSERT_TRUE(cache.insert("ab", 1));
  ASSERT_DEATH(cache.insert("abc", 2), "");
  ASSERT_DEATH(cache.insert("a", 3), "");
}
#endif /* UNIV_DEBUG */

TEST(ut0prefix_cache, misses_when_no_cached_prefix_matches) {
  ut::prefix_cache<char, int> cache(4);
  ASSERT_TRUE(cache.insert("abc", 7));

  std::string_view buffer = "abx";
  int *value = cache.find_value_for_prefix_of(buffer);

  ASSERT_EQ(nullptr, value);
  ASSERT_EQ("abx", buffer);
}

TEST(ut0prefix_cache, rejects_duplicate_insert) {
  ut::prefix_cache<char, int> cache(4);

  ASSERT_TRUE(cache.insert("abc", 7));
  ASSERT_FALSE(cache.insert("abc", 9));

  std::string_view buffer = "abc!";
  int *value = cache.find_value_for_prefix_of(buffer);

  ASSERT_NE(nullptr, value);
  ASSERT_EQ(7, *value);
}

TEST(ut0prefix_cache, evicts_least_recently_used_entry) {
  ut::prefix_cache<char, int> cache(2);

  ASSERT_TRUE(cache.insert("ab", 1));
  ASSERT_TRUE(cache.insert("cd", 2));

  std::string_view touch = "ab!";
  int *touched = cache.find_value_for_prefix_of(touch);
  ASSERT_NE(nullptr, touched);
  ASSERT_EQ(1, *touched);

  ASSERT_TRUE(cache.insert("ef", 3));

  std::string_view evicted = "cd!";
  ASSERT_EQ(nullptr, cache.find_value_for_prefix_of(evicted));

  std::string_view kept_old = "ab?";
  int *old_value = cache.find_value_for_prefix_of(kept_old);
  ASSERT_NE(nullptr, old_value);
  ASSERT_EQ(1, *old_value);

  std::string_view kept_new = "ef?";
  int *new_value = cache.find_value_for_prefix_of(kept_new);
  ASSERT_NE(nullptr, new_value);
  ASSERT_EQ(3, *new_value);
}