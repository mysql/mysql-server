/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

/**
 * Test of HttpUri.
 */

#include <gmock/gmock.h>

#include "http/base/uri.h"

using ConanicalizeTestParam = std::tuple<const std::string,  // test-name
                                         const std::string,  // input
                                         const std::string   // output
                                         >;

class ConanicalizeTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ConanicalizeTestParam> {};
/**
 * @test ensure PasswordFrontent behaves correctly.
 */
TEST_P(ConanicalizeTest, ensure) {
  EXPECT_EQ(http::base::http_uri_path_canonicalize(std::get<1>(GetParam())),
            std::get<2>(GetParam()));
}

// cleanup test-names to satisfy googletest's requirements
static std::string sanitise(const std::string &name) {
  std::string out{name};

  for (auto &c : out) {
    if (!isalnum(c)) {
      c = '_';
    }
  }

  return out;
}

INSTANTIATE_TEST_SUITE_P(
    Spec, ConanicalizeTest,
    ::testing::Values(
        std::make_tuple("canonical case, single slash", "/", "/"),
        std::make_tuple("canonical case, no trailing slash", "/a", "/a"),
        std::make_tuple("canonical case", "/a/", "/a/"),
        std::make_tuple("no escape root, no trailing slash", "/..", "/"),
        std::make_tuple("no escape root", "/../", "/"),
        std::make_tuple("no escape root, no leading slash", "..", "/"),
        std::make_tuple("double-slash is ignored", "//", "/"),
        std::make_tuple("empty", "", "/"),
        std::make_tuple("single dot", "/./", "/"),
        std::make_tuple("single dot, no trailing slash", "/.", "/"),
        std::make_tuple("one up", "/a/../", "/"),
        std::make_tuple("same level", "/a/./", "/a/")),
    [](testing::TestParamInfo<ConanicalizeTestParam> param_info) {
      return sanitise(std::get<0>(param_info.param));
    });

struct UriStringParam {
  UriStringParam(const std::string &param_uri) : uri{param_uri} {}
  UriStringParam(const std::string &param_uri,
                 const std::string &param_uri_result)
      : uri{param_uri}, reported_back_uri{param_uri_result} {}

  std::string get_result_uri() {
    if (reported_back_uri.empty()) return uri;

    return reported_back_uri;
  }

  std::string uri;
  std::string reported_back_uri;
};

struct UrlParam {
  UriStringParam uri;
  std::string scheme;
  std::string host;
  int32_t port;  // int32, because we need whole range of uint16_t + -1
  std::string path{};
  std::string query{};
  std::string fragment{};
};

class UrlParsingTest : public ::testing::Test,
                       public ::testing::WithParamInterface<UrlParam> {};

TEST_P(UrlParsingTest, parse_and_verify) {
  auto p = GetParam();

  http::base::Uri u{p.uri.uri};

  EXPECT_EQ(p.scheme, u.get_scheme());
  EXPECT_EQ(p.host, u.get_host());
  EXPECT_EQ(p.port, u.get_port());
  EXPECT_EQ(p.path, u.get_path());
  EXPECT_EQ(p.query, u.get_query());
  EXPECT_EQ(p.fragment, u.get_fragment());
}

TEST_P(UrlParsingTest, move_path) {
  const std::string k_path{"/some_apth"};
  auto p = GetParam();

  http::base::Uri u{p.uri.uri};

  u.set_path(k_path);

  ASSERT_EQ(p.scheme, u.get_scheme());
  ASSERT_EQ(p.host, u.get_host());
  ASSERT_EQ(p.port, u.get_port());
  ASSERT_EQ(k_path, u.get_path());
  ASSERT_EQ(p.query, u.get_query());
  ASSERT_EQ(p.fragment, u.get_fragment());
}

TEST_P(UrlParsingTest, move_path_query) {
  const std::string k_path{"/some_path"};
  const std::string k_query{"some_query=1"};
  auto p = GetParam();

  http::base::Uri u{p.uri.uri};

  u.set_path(k_path);
  u.set_query(k_query);

  ASSERT_EQ(p.scheme, u.get_scheme());
  ASSERT_EQ(p.host, u.get_host());
  ASSERT_EQ(p.port, u.get_port());
  ASSERT_EQ(k_path, u.get_path());
  ASSERT_EQ(k_query, u.get_query());
  ASSERT_EQ(p.fragment, u.get_fragment());
}

TEST_P(UrlParsingTest, move_data) {
  auto p = GetParam();

  http::base::Uri u_parsed{p.uri.uri};
  http::base::Uri u;

  u.set_scheme(u_parsed.get_scheme());
  u.set_host(u_parsed.get_host());
  u.set_port(u_parsed.get_port());
  u.set_path(u_parsed.get_path());
  u.set_query(u_parsed.get_query());
  u.set_fragment(u_parsed.get_fragment());

  ASSERT_EQ(p.scheme, u.get_scheme());
  ASSERT_EQ(p.host, u.get_host());
  ASSERT_EQ(p.port, u.get_port());
  ASSERT_EQ(p.path, u.get_path());
  ASSERT_EQ(p.query, u.get_query());
  ASSERT_EQ(p.fragment, u.get_fragment());
}

TEST_P(UrlParsingTest, move_path_query_fragment) {
  const std::string k_path{"/some_path"};
  const std::string k_query{"some_query=1"};
  const std::string k_fragment{"some_fragment=1"};
  auto p = GetParam();

  http::base::Uri u{p.uri.uri};

  u.set_path(k_path);
  u.set_query(k_query);
  u.set_fragment(k_fragment);

  ASSERT_EQ(p.scheme, u.get_scheme());
  ASSERT_EQ(p.host, u.get_host());
  ASSERT_EQ(p.port, u.get_port());
  ASSERT_EQ(k_path, u.get_path());
  ASSERT_EQ(k_query, u.get_query());
  ASSERT_EQ(k_fragment, u.get_fragment());
}

TEST_P(UrlParsingTest, uri_join_before_after_overide) {
  const std::string k_path{"/some_path"};
  const std::string k_query{"some_query=1"};
  const std::string k_fragment{"some_fragment=1"};
  auto p = GetParam();
  auto make_uri_without_path = [](auto &p) {
    if (p.scheme.empty()) return std::string{"/"};
    if (p.port >= 0)
      return p.scheme + "://" + p.host + ":" + std::to_string(p.port);
    return p.scheme + "://" + p.host;
  };

  http::base::Uri u{p.uri.uri};

  ASSERT_EQ(p.uri.get_result_uri(), u.join());
  u.set_path("");
  u.set_query("");
  u.set_fragment("");

  ASSERT_EQ(p.scheme, u.get_scheme());
  ASSERT_EQ(p.host, u.get_host());
  ASSERT_EQ(p.port, u.get_port());
  ASSERT_TRUE(u.get_path().empty());
  ASSERT_TRUE(u.get_query().empty());
  ASSERT_TRUE(u.get_fragment().empty());
  ASSERT_EQ(make_uri_without_path(p), u.join());
}

INSTANTIATE_TEST_SUITE_P(
    InstantiateUriParsing, UrlParsingTest,
    ::testing::Values(
        UrlParam{{"http://[::1]"}, "http", "[::1]", -1},
        UrlParam{{"http://[1::1]:2100"}, "http", "[1::1]", 2100},
        UrlParam{
            {"http://[1::1]:2100/path1"}, "http", "[1::1]", 2100, "/path1"},
        UrlParam{{"http://127.0.0.1"}, "http", "127.0.0.1", -1},
        UrlParam{{"https://127.0.0.2:2000"}, "https", "127.0.0.2", 2000},
        UrlParam{
            {"ftp://127.0.0.3:2001/path"}, "ftp", "127.0.0.3", 2001, "/path"},
        UrlParam{{"ftp://127.0.0.3:2001/path?query=1"},
                 "ftp",
                 "127.0.0.3",
                 2001,
                 "/path",
                 "query=1"},
        UrlParam{{"ftp://127.0.0.3:2001/path?query=1#fragment=2"},
                 "ftp",
                 "127.0.0.3",
                 2001,
                 "/path",
                 "query=1",
                 "fragment=2"},
        UrlParam{{"ftp://127.0.0.3:2001/path1/"
                  "path2?query1=1&query2=2#fragment1=1&fragment2=2"},
                 "ftp",
                 "127.0.0.3",
                 2001,
                 "/path1/path2",
                 "query1=1&query2=2",
                 "fragment1=1&fragment2=2"},
        UrlParam{{"/path1/"}, "", "", -1, "/path1/"},
        UrlParam{
            {"/path1/path2?query1=1"}, "", "", -1, "/path1/path2", "query1=1"},
        UrlParam{
            {"/svc/func/"
             "move_json?a=%5b%22aaaa%22,20,30,%7b%22field1%22:%22value1%22%"
             "7D%5D",
             "/svc/func/"
             "move_json?a=%5b%22aaaa%22%2c20%2c30%2c%7b%22field1%22%3a"
             "%22value1%22%7d%5d"},
            "",
            "",
            -1,
            "/svc/func/move_json",
            "a=%5b%22aaaa%22%2c20%2c30%2c%7b%22field1%22%3a%22value1%22%"
            "7d%5d"}));

TEST(UrlParsingTest, reproduce) { mysqlrouter::URI u{"BEB://B:///"}; }

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
