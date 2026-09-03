// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string>

#include "../../src/common/wildcard_match.h"

using opentelemetry::sdk::common::WildcardMatch;

TEST(WildcardMatch, exact_match)
{
  ASSERT_TRUE(WildcardMatch("hello", "hello"));
}

TEST(WildcardMatch, exact_mismatch)
{
  ASSERT_FALSE(WildcardMatch("hello", "world"));
}

TEST(WildcardMatch, empty_pattern_empty_text)
{
  ASSERT_TRUE(WildcardMatch("", ""));
}

TEST(WildcardMatch, empty_pattern_nonempty_text)
{
  ASSERT_FALSE(WildcardMatch("", "hello"));
}

TEST(WildcardMatch, star_matches_empty)
{
  ASSERT_TRUE(WildcardMatch("*", ""));
}

TEST(WildcardMatch, star_matches_everything)
{
  ASSERT_TRUE(WildcardMatch("*", "anything"));
}

TEST(WildcardMatch, question_mark_single_char)
{
  ASSERT_TRUE(WildcardMatch("h?llo", "hello"));
  ASSERT_TRUE(WildcardMatch("h?llo", "hallo"));
}

TEST(WildcardMatch, question_mark_requires_char)
{
  ASSERT_FALSE(WildcardMatch("h?llo", "hllo"));
}

TEST(WildcardMatch, star_prefix)
{
  ASSERT_TRUE(WildcardMatch("*.library", "noisy.library"));
  ASSERT_FALSE(WildcardMatch("*.library", "noisy.lib"));
}

TEST(WildcardMatch, star_suffix)
{
  ASSERT_TRUE(WildcardMatch("io.opentelemetry.*", "io.opentelemetry.contrib.jdbc"));
  ASSERT_TRUE(WildcardMatch("io.opentelemetry.*", "io.opentelemetry."));
  ASSERT_FALSE(WildcardMatch("io.opentelemetry.*", "io.opentelemetry"));
}

TEST(WildcardMatch, star_middle)
{
  ASSERT_TRUE(WildcardMatch("io.*.contrib", "io.opentelemetry.contrib"));
  ASSERT_TRUE(WildcardMatch("io.*.contrib", "io..contrib"));
  ASSERT_FALSE(WildcardMatch("io.*.contrib", "io.opentelemetry.contri"));
}

TEST(WildcardMatch, multiple_stars)
{
  ASSERT_TRUE(WildcardMatch("*.*.*", "a.b.c"));
  ASSERT_TRUE(WildcardMatch("**", "anything"));
  ASSERT_TRUE(WildcardMatch("a*b*c", "abc"));
  ASSERT_TRUE(WildcardMatch("a*b*c", "aXbYc"));
  ASSERT_FALSE(WildcardMatch("a*b*c", "aXbY"));
}

TEST(WildcardMatch, question_and_star_combined)
{
  ASSERT_TRUE(WildcardMatch("?*", "a"));
  ASSERT_TRUE(WildcardMatch("?*", "ab"));
  ASSERT_FALSE(WildcardMatch("?*", ""));
}

TEST(WildcardMatch, trailing_star)
{
  ASSERT_TRUE(WildcardMatch("hello*", "hello"));
  ASSERT_TRUE(WildcardMatch("hello*", "helloworld"));
}

TEST(WildcardMatch, pattern_longer_than_text)
{
  ASSERT_FALSE(WildcardMatch("hello_world", "hello"));
}

TEST(WildcardMatch, only_stars)
{
  ASSERT_TRUE(WildcardMatch("***", ""));
  ASSERT_TRUE(WildcardMatch("***", "anything"));
}
