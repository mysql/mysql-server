/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "mrs/http/url.h"

using namespace mrs;
using namespace testing;
using namespace std::string_literals;

TEST(HttpUrl, append) {
  auto uri_empty = HttpUri::parse("http://first.url/path1/path2");
  auto uri_notempty =
      HttpUri::parse("http://first.url/path1/path2?something=1");
  ASSERT_TRUE(http::Url::append_query(uri_empty, "new=2"));
  ASSERT_TRUE(http::Url::append_query(uri_notempty, "new=2"));

  ASSERT_EQ("http://first.url/path1/path2?new=2", uri_empty.join());
  ASSERT_EQ("http://first.url/path1/path2?something=1&new=2",
            uri_notempty.join());
}

TEST(HttpUrl, append_failed) {
  auto uri = HttpUri::parse("http://first.url/path1/path2");
  EXPECT_FALSE(http::Url::append_query(uri, "new=string1 tring2_etc"));

  EXPECT_EQ("http://first.url/path1/path2", uri.join());
}

TEST(HttpUrl, append_escapeed_raw) {
  auto uri = HttpUri::parse("http://first.url/path1/path2");
  EXPECT_TRUE(http::Url::append_query(
      uri, ("new="s + http::Url::escape_uri("string1 string2_etc")).c_str()));

  EXPECT_EQ("http://first.url/path1/path2?new=string1%20string2_etc",
            uri.join());
}

TEST(HttpUrl, append_escapeed) {
  auto uri = HttpUri::parse("http://first.url/path1/path2");
  EXPECT_TRUE(
      http::Url::append_query_parameter(uri, "new", "string1 string2_etc"));

  EXPECT_EQ("http://first.url/path1/path2?new=string1%20string2_etc",
            uri.join());
}

TEST(HttpUrl, get_escapeed) {
  auto uri =
      HttpUri::parse("http://first.url/path1/path2?new=string1%20string2_etc");
  http::Url http_uri(uri);

  ASSERT_EQ("string1 string2_etc", http_uri.get_query_parameter("new"));
}
