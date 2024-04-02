/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "helper/http/url.h"

using namespace helper;
using namespace testing;
using namespace std::string_literals;

using HttpUri = ::http::base::Uri;
using Url = helper::http::Url;

// TODO(lkotula): Moce some to HTTP URI test (Shouldn't be in review)
// TODO(lkotula): append_query, remove and use the baseclass of Uri ? (Shouldn't
// be in review)

TEST(HttpUrl, append) {
  HttpUri uri_empty{"http://first.url/path1/path2"};
  HttpUri uri_notempty{"http://first.url/path1/path2?something=1"};
  Url::append_query_parameter(uri_empty, "new", "2");
  Url::append_query_parameter(uri_notempty, "new", "2");

  ASSERT_EQ("http://first.url/path1/path2?new=2", uri_empty.join());
  ASSERT_EQ("http://first.url/path1/path2?new=2&something=1",
            uri_notempty.join());
}

TEST(HttpUrl, spaces_in_path) {
  HttpUri uri1a("http://first.url/path1%20/path2");
  HttpUri uri2a("http://first.url/path1/path2%20");

  ASSERT_EQ("/path1%20/path2", uri1a.get_path());
  ASSERT_EQ("/path1/path2%20", uri2a.get_path());
}

TEST(HttpUrl, spaces_in_query_are_escaped) {
  HttpUri uri{"http://first.url/path1/path2"};
  Url::append_query_parameter(uri, "new", "string1 tring2_etc");

  EXPECT_EQ("http://first.url/path1/path2?new=string1%20tring2_etc",
            uri.join());
}

TEST(HttpUrl, append_escapeed_raw) {
  HttpUri uri{"http://first.url/path1/path2"};
  Url::append_query_parameter(uri, "new=", "string1 string2_etc");

  EXPECT_EQ("http://first.url/path1/path2?new%3d=string1%20string2_etc",
            uri.join());
}

TEST(HttpUrl, append_escapeed) {
  HttpUri uri{"http://first.url/path1/path2"};
  Url::append_query_parameter(uri, "new", "string1 string2_etc");

  EXPECT_EQ("http://first.url/path1/path2?new=string1%20string2_etc",
            uri.join());
}

TEST(HttpUrl, get_escapeed) {
  HttpUri uri{"http://first.url/path1/path2?new=string1%20string2_etc"};
  Url http_uri(std::move(uri));

  ASSERT_EQ("string1 string2_etc", http_uri.get_query_parameter("new"));
}
