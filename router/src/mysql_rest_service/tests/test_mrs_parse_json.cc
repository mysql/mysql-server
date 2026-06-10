/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "helper/json/text_to.h"
#include "mrs/json/json_template_unnest.h"
#include "mrs/json/parse_file_sharing_options.h"
#include "mysql/harness/make_shared_ptr.h"

template <typename T>
using MakeSharedPtr = mysql_harness::MakeSharedPtr<T>;
using ParseFileSharingOptions = mrs::json::ParseFileSharingOptions;
using testing::Test;

class ParseParam {
 public:
  std::string json_input;
  size_t no_of_contents;
  size_t no_of_idexes;
  size_t no_of_redirects;
};

class ParseFileSharingOptsTests : public testing::TestWithParam<ParseParam> {};

TEST_P(ParseFileSharingOptsTests, parse_file_sharing_opts_empty) {
  const auto &p = GetParam();
  auto result =
      helper::json::text_to_handler<ParseFileSharingOptions>(p.json_input)
          .value_or(ParseFileSharingOptions::Result{});

  ASSERT_EQ(p.no_of_contents, result.default_static_content_.size());
  if (p.no_of_idexes > 0) {
    ASSERT_EQ(p.no_of_idexes, result.directory_index_directive_.value().size());
  } else {
    ASSERT_EQ(false, result.directory_index_directive_.has_value());
  }
  ASSERT_EQ(p.no_of_redirects, result.default_redirects_.size());
}

INSTANTIATE_TEST_SUITE_P(
    InstantiationParseFileSharingOptsTests, ParseFileSharingOptsTests,
    testing::Values(
        ParseParam{"", 0, 0, 0}, ParseParam{"{}", 0, 0, 0},
        ParseParam{"{\"defaultStaticContent\":{\"first\":\"some string\"}}", 1,
                   0, 0},
        ParseParam{
            "{\"defaultStaticContent\":{\"first\":\"some string\", \"second\": "
            "\"other\"}, \"directoryIndexDirective\":[\"value1\"]}",
            2, 1, 0},
        ParseParam{"{\"defaultStaticContent\":{\"first\":\"some string\", "
                   "\"second\": \"other\", \"third\":\"last one\"}, "
                   "\"directoryIndexDirective\":[\"value1\", \"value2\"], "
                   "\"defaultRedirects\":{\"R1\":\"f1\"}}",
                   3, 2, 1},
        ParseParam{
            "{\"defaultStaticContent\":{\"first\":\"some string\", "
            "\"second\": \"other\", \"third\":\"?\", \"fourth\":\"last one\"}, "
            "\"directoryIndexDirective\":[\"value1\", \"value2\", \"3\"], "
            "\"defaultRedirects\":{\"R1\":\"f1\", \"R2\":\"f2\"}}",
            4, 3, 2}));

TEST(ParseFileSharingOptsTest, validate_resulting_values) {
  const std::string k_input_document =
      "{\"defaultStaticContent\":{\"first\":\"some string\", "
      "\"second\": \"other\", \"third\":\"?\", \"fourth\":\"last one\"}, "
      "\"directoryIndexDirective\":[\"value1\", \"value2\", \"3\"], "
      "\"defaultRedirects\":{\"R1\":\"f1\", \"R2\":\"f2\"}}";
  auto json =
      helper::json::text_to_handler<ParseFileSharingOptions>(k_input_document);

  ASSERT_TRUE(json.has_value());

  // Check sizes
  ASSERT_EQ(4, json->default_static_content_.size());
  ASSERT_EQ(3, json->directory_index_directive_.value().size());
  ASSERT_EQ(2, json->default_redirects_.size());

  // Check keys
  ASSERT_EQ(1, json->default_static_content_.count("first"));
  ASSERT_EQ(1, json->default_static_content_.count("second"));
  ASSERT_EQ(1, json->default_static_content_.count("third"));
  ASSERT_EQ(1, json->default_static_content_.count("fourth"));

  ASSERT_EQ(1, json->default_redirects_.count("R1"));
  ASSERT_EQ(1, json->default_redirects_.count("R2"));

  // Check values
  ASSERT_EQ("some string", json->default_static_content_["first"]);
  ASSERT_EQ("other", json->default_static_content_["second"]);
  ASSERT_EQ("?", json->default_static_content_["third"]);
  ASSERT_EQ("last one", json->default_static_content_["fourth"]);

  ASSERT_EQ("f1", json->default_redirects_["R1"]);
  ASSERT_EQ("f2", json->default_redirects_["R2"]);

  ASSERT_EQ("value1", json->directory_index_directive_.value()[0]);
  ASSERT_EQ("value2", json->directory_index_directive_.value()[1]);
  ASSERT_EQ("3", json->directory_index_directive_.value()[2]);
}

TEST(ParseFileSharingOptsTest, validate_resulting_values_base64) {
  const std::string k_input_document =
      "{\"defaultStaticContent\":{\"first\":\"c29tZSBzdHJpbmc=\", "
      "\"second\": \"b3RoZXI=\", \"third\":\"Pw==\", "
      "\"fourth\":\"bGFzdCBvbmU=\"}, "
      "\"directoryIndexDirective\":[\"dmFsdWUx\", \"dmFsdWUy\", \"Mw==\"], "
      "\"defaultRedirects\":{\"R1\":\"ZjE=\", \"R2\":\"ZjI=\"}}";
  auto json =
      helper::json::text_to_handler<ParseFileSharingOptions>(k_input_document);
  ASSERT_TRUE(json.has_value());

  // Check sizes
  ASSERT_EQ(4, json->default_static_content_.size());
  ASSERT_EQ(3, json->directory_index_directive_.value().size());
  ASSERT_EQ(2, json->default_redirects_.size());

  // Check keys
  ASSERT_EQ(1, json->default_static_content_.count("first"));
  ASSERT_EQ(1, json->default_static_content_.count("second"));
  ASSERT_EQ(1, json->default_static_content_.count("third"));
  ASSERT_EQ(1, json->default_static_content_.count("fourth"));

  ASSERT_EQ(1, json->default_redirects_.count("R1"));
  ASSERT_EQ(1, json->default_redirects_.count("R2"));

  // Check values
  ASSERT_EQ("some string", json->default_static_content_["first"]);
  ASSERT_EQ("other", json->default_static_content_["second"]);
  ASSERT_EQ("?", json->default_static_content_["third"]);
  ASSERT_EQ("last one", json->default_static_content_["fourth"]);

  ASSERT_EQ("f1", json->default_redirects_["R1"]);
  ASSERT_EQ("f2", json->default_redirects_["R2"]);

  ASSERT_EQ("value1", json->directory_index_directive_.value()[0]);
  ASSERT_EQ("value2", json->directory_index_directive_.value()[1]);
  ASSERT_EQ("3", json->directory_index_directive_.value()[2]);
}
