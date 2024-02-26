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
#include "mysqlrouter/uri.h"

#include <exception>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using mysqlrouter::URI;
using mysqlrouter::URIAuthority;
using mysqlrouter::URIError;
using mysqlrouter::URIPath;
using mysqlrouter::URIQuery;
using ::testing::StrEq;

class URISimpleTests : public ::testing::Test {};

// default constructor tests

TEST_F(URISimpleTests, Constructor) {
  URI u;
  ASSERT_TRUE(u.scheme.empty());
  ASSERT_TRUE(u.host.empty());
  ASSERT_EQ(u.port, 0);
  ASSERT_TRUE(u.username.empty());
  ASSERT_TRUE(u.password.empty());
  ASSERT_TRUE(u.path.empty());
  ASSERT_TRUE(u.query.empty());
  ASSERT_TRUE(u.fragment.empty());
}

// parsing

struct URIParseGoodTestData {
  struct {
    const char *uri;
  } input;

  struct {
    const char *scheme;
    const char *host;
    uint64_t port;
    const char *username;
    const char *password;
    URIPath path;
    URIQuery query;
    const char *fragment;
  } expected;
};

// pretty printer for the test data
::std::ostream &operator<<(::std::ostream &os,
                           const URIParseGoodTestData &data) {
  return os << data.input.uri;
}

class URIParseGoodTests
    : public ::testing::Test,
      public ::testing::WithParamInterface<URIParseGoodTestData> {};

TEST_P(URIParseGoodTests, ParseURIconstruct) {
  auto test_param = GetParam();
  URI u;

  ASSERT_NO_THROW(u = URI(test_param.input.uri));

  EXPECT_THAT(u.scheme, StrEq(test_param.expected.scheme));
  EXPECT_THAT(u.host, StrEq(test_param.expected.host));
  EXPECT_EQ(u.port, test_param.expected.port);
  EXPECT_THAT(u.username, StrEq(test_param.expected.username));
  EXPECT_THAT(u.password, StrEq(test_param.expected.password));
  EXPECT_EQ(u.path, test_param.expected.path);
  EXPECT_EQ(u.query, test_param.expected.query);
  EXPECT_THAT(u.fragment, StrEq(test_param.expected.fragment));
}

TEST_P(URIParseGoodTests, ParseURIseturi) {
  auto test_param = GetParam();
  URI u;

  ASSERT_NO_THROW(u = URI("ham://foo:bar@host/path?key=value#frag"));
  ASSERT_NO_THROW(u.set_uri(test_param.input.uri));

  EXPECT_THAT(u.scheme, StrEq(test_param.expected.scheme));
  EXPECT_THAT(u.host, StrEq(test_param.expected.host));
  EXPECT_EQ(u.port, test_param.expected.port);
  EXPECT_THAT(u.username, StrEq(test_param.expected.username));
  EXPECT_THAT(u.password, StrEq(test_param.expected.password));
  EXPECT_EQ(u.path, test_param.expected.path);
  EXPECT_EQ(u.query, test_param.expected.query);
  EXPECT_THAT(u.fragment, StrEq(test_param.expected.fragment));
}

URIParseGoodTestData uri_test_data[] = {
    // just a scheme, everything else is empty
    {{
         "ham:",
     },
     {"ham", "", 0, "", "", URIPath(), URIQuery(), ""}},

    // uppercase, translates to lowercase
    {
        {
            "HAM:",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // check all allowed chars
    {
        {
            "h123-+.:",
        },
        {"h123-+.", "", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // no host, no trailing space
    {
        {
            "ham://",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},

    },
    // no host, absolute path
    {
        {
            "ham:/foo",
        },
        {"ham", "", 0, "", "", URIPath({"foo"}), URIQuery(), ""},
    },

    // no host, no trailing space
    {
        {
            "ham:///",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // no values, just sep's
    {
        {
            "ham://:@:/",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // no values, just sep's
    {
        {
            "ham://:/",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},

    },
    // no values, just sep's
    {
        {
            "ham://:@/",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // host, no trailing space
    {
        {
            "ham://spam.example.com",
        },
        {"ham", "spam.example.com", 0, "", "", URIPath(), URIQuery(), ""},

    },
    // host, with trailing space
    {
        {
            "ham://spam.example.com/",
        },
        {"ham", "spam.example.com", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // host, port-separator, but no value
    {
        {
            "ham://spam.example.com:/",
        },
        {"ham", "spam.example.com", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // host, port-separator, value
    {
        {
            "ham://spam.example.com:80/",
        },
        {"ham", "spam.example.com", 80, "", "", URIPath(), URIQuery(), ""},
    },

    // host, userinfo empty
    {
        {
            "ham://@spam.example.com:80/",
        },
        {"ham", "spam.example.com", 80, "", "", URIPath(), URIQuery(), ""},
    },

    // host, userinfo empty, pw empty, with sep
    {
        {
            "ham://:@spam.example.com:80/",
        },
        {"ham", "spam.example.com", 80, "", "", URIPath(), URIQuery(), ""},
    },

    // host, userinfo, no pw
    {
        {
            "ham://scott@spam.example.com:80/",
        },
        {"ham", "spam.example.com", 80, "scott", "", URIPath(), URIQuery(), ""},
    },

    // host, userinfo, no pw
    {
        {
            "ham://scott:@spam.example.com:80/",
        },
        {"ham", "spam.example.com", 80, "scott", "", URIPath(), URIQuery(), ""},
    },

    // host, userinfo, pw
    {
        {
            "ham://scott:tiger@spam.example.com:80/",
        },
        {"ham", "spam.example.com", 80, "scott", "tiger", URIPath(), URIQuery(),
         ""},
    },

    // no host, userinfo, pw
    {
        {
            "ham://scott:tiger@/",
        },
        {"ham", "", 0, "scott", "tiger", URIPath(), URIQuery(), ""},
    },

    // no host, no user, pw
    {
        {
            "ham://:tiger@/",
        },
        {"ham", "", 0, "", "tiger", URIPath(), URIQuery(), ""},
    },

    // ipv4
    {
        {
            "ham://1.2.3.4",
        },
        {"ham", "1.2.3.4", 0, "", "", URIPath(), URIQuery(), ""},
    },
    // ipv4, trailing slash
    {
        {
            "ham://1.2.3.4/",
        },
        {"ham", "1.2.3.4", 0, "", "", URIPath(), URIQuery(), ""},
    },
    // ipv4, port
    {
        {
            "ham://1.2.3.4:82",
        },
        {"ham", "1.2.3.4", 82, "", "", URIPath(), URIQuery(), ""},
    },

    // ipv6, loopback address, compressed
    {
        {
            "ham://[::1]",
        },
        {"ham", "::1", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // ipv6, unspecified address
    {
        {
            "ham://[::]",
        },
        {"ham", "::", 0, "", "", URIPath(), URIQuery(), ""},
    },
    // ipv6, full length
    {
        {
            "ham://[ABCD:EF01:2345:6789:ABCD:EF01:2345:6789]",
        },
        {"ham", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", 0, "", "", URIPath(),
         URIQuery(), ""},
    },

    // ipv6, abbreviated
    {
        {
            "ham://[2001:DB8:0:0:8:800:200C:417A]",
        },
        {"ham", "2001:DB8:0:0:8:800:200C:417A", 0, "", "", URIPath(),
         URIQuery(), ""},
    },
    // ipv6, abbreviated, compressed
    {
        {
            "ham://[2001:DB8::8:800:200C:417A]",
        },
        {"ham", "2001:DB8::8:800:200C:417A", 0, "", "", URIPath(), URIQuery(),
         ""},
    },
    // IPv6, ipv4 inside ipv6, compresses
    {
        {
            "ham://[::13.1.68.3]",
        },
        {"ham", "::13.1.68.3", 0, "", "", URIPath(), URIQuery(), ""},
    },
    // IPv6 with zoneinfo
    {
        {
            "ham://[::1"
            "%25"
            "foo]",
        },
        {"ham", "::1%foo", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // ipv6, trailing slash
    {
        {
            "ham://[::1]/",
        },
        {"ham", "::1", 0, "", "", URIPath(), URIQuery(), ""},
    },
    // ipv6, port
    {
        {
            "ham://[::1]:81",
        },
        {"ham", "::1", 81, "", "", URIPath(), URIQuery(), ""},
    },
    // ipv6, port, trailing slash
    {
        {
            "ham://[::1]:81/",
        },
        {"ham", "::1", 81, "", "", URIPath(), URIQuery(), ""},
    },

    // fragment
    {
        {
            "ham:///#fragment",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), "fragment"},
    },

    // fragment
    {
        {
            "ham:///#fragment",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), "fragment"},

    },
    // fragment ... with all allowed extra chars
    {
        {
            "ham:///#fragment?@:/",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), "fragment?@:/"},
    },

    // empty fragment
    {
        {
            "ham:///#",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},
    },

    // query string
    {
        {
            "ham:///?foo=bar",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery({{"foo", "bar"}}), ""},
    },

    // query string, empty value
    {
        {
            "ham:///?foo=",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery({{"foo", ""}}), ""},
    },

    // query string, empty value, trailing &
    {
        {
            "ham:///?foo=&",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery({{"foo", ""}}), ""},
    },

    // query string, trailing &
    {
        {
            "ham:///?foo=bar&",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery({{"foo", "bar"}}), ""},
    },

    // query string, multiple values
    {
        {
            "ham:///?foo=bar&fuz=baz",
        },
        {"ham", "", 0, "", "", URIPath(),
         URIQuery({{"foo", "bar"}, {"fuz", "baz"}}), ""},
    },

    // query string, pct-encoding for key and value
    {
        {
            "ham:///?foo%3d=bar%26&",
        },
        {"ham", "", 0, "", "", URIPath(),
         URIQuery({
             {"foo=", "bar&"},
         }),
         ""},
    },

    // path
    {
        {
            "ham:///foo/",
        },
        {"ham", "", 0, "", "", URIPath({"foo"}), URIQuery(), ""},
    },

    // path, no trailing slash
    {
        {
            "ham:///foo",
        },
        {"ham", "", 0, "", "", URIPath({"foo"}), URIQuery(), ""},
    },

    // double slash
    {
        {
            "ham:///foo//bar",
        },
        {"ham", "", 0, "", "", URIPath({"foo", "bar"}), URIQuery(), ""},
    },

    // empty host, empty path, query
    {
        {
            "ham://?foo=bar",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery({{"foo", "bar"}}), ""},
    },

    // empty host, empty path, fragment
    {
        {
            "ham://#fragment",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), "fragment"},
    },

    {
        {
            "mailto:foo@example.org",
        },
        {"mailto", "", 0, "", "", URIPath({"foo@example.org"}), URIQuery(), ""},
    },

    {
        {
            "urn:example:animal:ferret:nose",
        },
        {"urn", "", 0, "", "", URIPath({"example:animal:ferret:nose"}),
         URIQuery(), ""},
    },

    {
        // pct-encoded reg-name
        {
            "ham://foo.%2f/",
        },
        {"ham", "foo./", 0, "", "", URIPath(), URIQuery(), ""},
    },

    {
        // pct-encoded reg-name
        {
            "ham://foo.%2fbar/",
        },
        {"ham", "foo./bar", 0, "", "", URIPath(), URIQuery(), ""},
    },

    {
        // pct-encoded reg-name
        {
            "ham://foo.%2f%2fbar/",
        },
        {"ham", "foo.//bar", 0, "", "", URIPath(), URIQuery(), ""},
    },
    {
        // pct-encoded userinfo
        {
            "ham://user:fo%40@/",
        },
        {"ham", "", 0, "user", "fo@", URIPath(), URIQuery(), ""},
    },
    {
        // pct-encoded userinfo, check if %3a [:] isn't decoded too early
        {
            "ham://user%3a:fo%40@/",
        },
        {"ham", "", 0, "user:", "fo@", URIPath(), URIQuery(), ""},
    },
    {
        // pct-encoded userinfo, leading pct-enc is ok
        {
            "ham://%40user:fo%40bar%40@/",
        },
        {"ham", "", 0, "@user", "fo@bar@", URIPath(), URIQuery(), ""},
    },
    {
        // pct-encoded path, trailing pct-enc
        {
            "ham:///fo%2f",
        },
        {"ham", "", 0, "", "", URIPath({"fo/"}), URIQuery(), ""},
    },

    {
        // pct-encoded reg-name
        {
            "s:v%88",
        },
        {"s", "", 0, "", "",
         URIPath({"v"
                  "\x88"}),
         URIQuery(), ""},
    },

    {
        // fuzzer hang
        {
            "ham:o/scott:tiger@spam.example.com:80",
        },
        {"ham", "", 0, "", "",
         URIPath({"o", "scott:tiger@spam.example.com:80"}), URIQuery(), ""},
    },

    {
        // path-empty with query
        {
            "ham:?",
        },
        {"ham", "", 0, "", "", URIPath(), URIQuery(), ""},
    },
    {
        {"w://7.7.3.7."},
        {
            "w",
            "7.7.3.7.",
            0,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
    },
};

INSTANTIATE_TEST_SUITE_P(URITests, URIParseGoodTests,
                         ::testing::ValuesIn(uri_test_data));

// should all throw
struct URITestFailData {
  struct {
    const char *uri;
  } input;

  struct {
    const char *exception_text;
  } expected;
};

// pretty printer for the test data
::std::ostream &operator<<(::std::ostream &os, const URITestFailData &data) {
  return os << data.input.uri;
}

class URIParseThrowingTests
    : public ::testing::Test,
      public ::testing::WithParamInterface<URITestFailData> {};

TEST_P(URIParseThrowingTests, FailsParseURI) {
  // call GetParam outside the ASSERT to actually make it work
  URITestFailData test_data = GetParam();

  try {
    auto u = URI(test_data.input.uri);

    FAIL() << "should have failed";
  } catch (URIError &e) {
    EXPECT_THAT(e.what(), StrEq(test_data.expected.exception_text));
  } catch (...) {
    FAIL() << "expected to throw URIError, got unexpected exception";
  }
}

URITestFailData uri_test_fail_data[] = {
    {
        // no trailing :
        {
            "ham",
        },
        {"invalid URI: expected colon after scheme at position 3 for: ham"},
    },
    {
        // invalid chars
        {
            "ham$$:",
        },
        {"invalid URI: expected colon after scheme at position 3 for: ham$$:"},
    },
    {
        // initial char has to be a ALPHA
        {
            "1ham:",
        },
        {"invalid URI: no scheme at position 0 for: 1ham:"},
    },
    {
        // IPv6 has to be valid
        {
            "ham://[user@bar]:3306",
        },
        {"invalid URI: expected to find IPv6 address, but failed at position 7 "
         "for: ham://[user@bar]:3306"},
    },
    {
        {
            "ham://[",
        },
        {"invalid URI: expected to find IPv6 address, but failed at position 7 "
         "for: ham://["},
    },
    {
        // no =, no value
        {
            "ham:///?foo",
        },
        {"invalid URI: query-string part doesn't contain '='"},
    },
    {
        // port too large
        {
            "ham://:99999",
        },
        {"invalid URI: invalid port: impossible port number for: ham://:99999"},
    },
    {
        // port too large (stoul)
        {
            "ham://"
            ":999999999999999999999999999999999999999999999999999999999999999",
        },
        {"invalid URI: invalid port: impossible port number for: "
         "ham://"
         ":999999999999999999999999999999999999999999999999999999999999999"},
    },
    {
        // invalid scheme
        {
            "ham//",
        },
        {"invalid URI: expected colon after scheme at position 3 for: ham//"},
    },
    {
        // IPv6 validation, too may colons
        {
            "ham://[:::1]",
        },
        {"invalid URI: expected to find a ']' at position 9 for: ham://[:::1]"},
    },
    {
        // IPv6 validation, only one colon
        {
            "ham://[::1::1]",
        },
        {"invalid URI: expected to find a ']' at position 10 for: "
         "ham://[::1::1]"},
    },
    {
        // IPv6 validation, wrong sep
        {
            "ham://[::1%26foo]",
        },
        {"invalid URI: invalid pct-encoded value, expected %25 at position 11 "
         "for: ham://[::1%26foo]"},
    },
    {
        // IPv6 validation, wrong ipv4
        {
            "ham://[::1.1.1]",
        },
        {"invalid URI: expected to find a ']' at position 10 for: "
         "ham://[::1.1.1]"},
    },
#if 0
  {
    // TODO: IPvFuture validation
    { "ham://[v66.abc]", },
    { "invalid URI: expected to find a ']' at position 7 for: ham://[v66.abc]" },
  },
#endif
    {
        // host, broken pct-encoded
        {
            "ham://%",
        },
        {"invalid URI: unexpected characters at position 6 for: ham://%"},
    },
    {
        // host, broken pct-encoded
        {
            "ham://%a",
        },
        {"invalid URI: unexpected characters at position 6 for: ham://%a"},
    },
    {
        // path, broken pct-encoded
        {
            "ham:%a",
        },
        {"invalid URI: unexpected characters at position 4 for: ham:%a"},
    },
    {
        // fuzzer crash
        {
            "ham://[c::d:55%2555%25jm.examph55555C5I5%25",
        },
        {
            "invalid URI: expected to find a ']' at position 43 for: "
            "ham://[c::d:55%2555%25jm.examph55555C5I5%25",
        },
    },
    {
        // fuzzer crash
        {
            "hhu://[c::B",
        },
        {
            "invalid URI: expected to find a ']' at position 11 for: "
            "hhu://[c::B",
        },
    },
};

INSTANTIATE_TEST_SUITE_P(URITests, URIParseThrowingTests,
                         ::testing::ValuesIn(uri_test_fail_data));

// parsing

struct URItoStringTestData {
  struct {
    const char *scheme;
    const char *host;
    uint16_t port;
    const char *username;
    const char *password;
    URIPath path;
    URIQuery query;
    const char *fragment;
  } input;

  struct {
    const char *uri;
  } expected;
};

// pretty printer for the test data
::std::ostream &operator<<(::std::ostream &os,
                           const URItoStringTestData &data) {
  return os << data.input.scheme << "://" << data.input.username << ":"
            << data.input.password << "@" << data.input.host << ":"
            << data.input.port << "/"
            << "..."
            << "?"
            << "..."
            << "#" << data.input.fragment;
}

class URItoStringGoodTests
    : public ::testing::Test,
      public ::testing::WithParamInterface<URItoStringTestData> {};

TEST_P(URItoStringGoodTests, URItoString) {
  auto test_param = GetParam();
  URI u;

  u.scheme = test_param.input.scheme;
  u.host = test_param.input.host;
  u.port = test_param.input.port;
  u.username = test_param.input.username;
  u.password = test_param.input.password;
  u.path = test_param.input.path;
  u.query = test_param.input.query;
  u.fragment = test_param.input.fragment;

  std::stringstream ss;

  ss << u;

  EXPECT_THAT(ss.str(), StrEq(test_param.expected.uri));
}

URItoStringTestData uri_to_string_test_data[] = {
    {{
         "ham",
         "",
         0,
         "",
         "",
         URIPath(),
         URIQuery(),
         "",
     },
     {"ham:"}},
    {
        {
            "mailto",
            "",
            0,
            "",
            "",
            URIPath({"foo@example.org"}),
            URIQuery(),
            "",
        },
        {"mailto:foo@example.org"},
    },
    {
        {
            "http",
            "example.org",
            0,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://example.org"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://example.org:80"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://user@example.org:80"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://user:pw@example.org:80"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "",
            "pw",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://:pw@example.org:80"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p"}),
            URIQuery(),
            "",
        },
        {"http://user:pw@example.org:80/p"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p", "w"}),
            URIQuery(),
            "",
        },
        {"http://user:pw@example.org:80/p/w"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p", "w"}),
            URIQuery({{"k", "v"}}),
            "",
        },
        {"http://user:pw@example.org:80/p/w?k=v"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p", "w"}),
            URIQuery({{"k", "v"}, {"l", "m"}}),
            "",
        },
        {"http://user:pw@example.org:80/p/w?k=v&l=m"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p", "w"}),
            URIQuery({{"k", "v"}, {"l", "m"}}),
            "frag",
        },
        {"http://user:pw@example.org:80/p/w?k=v&l=m#frag"},
    },
    {
        {
            "http",
            "/",
            0,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://%2f"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "foo:bar",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://foo%3abar@example.org:80"},
    },
    // no need to encode the password's : as it is the 2nd sep
    {
        {
            "http",
            "example.org",
            80,
            "foo:bar",
            "p:w",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://foo%3abar:p:w@example.org:80"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p/w"}),
            URIQuery(),
            "",
        },
        {"http://user:pw@example.org:80/p%2fw"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p", "w"}),
            URIQuery({{"k=", "v&"}}),
            "",
        },
        {"http://user:pw@example.org:80/p/w?k%3d=v%26"},
    },
    {
        {
            "http",
            "example.org",
            80,
            "user",
            "pw",
            URIPath({"p", "w"}),
            URIQuery({{"k", "v"}, {"l", "m#"}}),
            "frag",
        },
        {"http://user:pw@example.org:80/p/w?k=v&l=m%23#frag"},
    },
    {
        {
            "http",
            "::1",
            0,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://[::1]"},
    },
    {
        {
            "http",
            "::1%lo",
            0,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://[::1%25lo]"},
    },
    {
        {
            "http",
            "::1",
            80,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"http://[::1]:80"},
    },
    {
        {
            "s",
            "",
            0,
            "",
            "",
            URIPath({"v\x88"}),
            URIQuery(),
            "",
        },
        {"s:v%88"},
    },
    {
        {
            "v",
            "",
            0,
            "",
            "v(",
            URIPath(),
            URIQuery(),
            "",
        },
        {"v://:v(@"},
    },
    {
        {
            "w",
            "7.7.3.7.",
            0,
            "",
            "",
            URIPath(),
            URIQuery(),
            "",
        },
        {"w://7.7.3.7."},
    },
};

INSTANTIATE_TEST_SUITE_P(URITests, URItoStringGoodTests,
                         ::testing::ValuesIn(uri_to_string_test_data));

// rootless

// should all throw
struct URIRootlessTestFailData {
  struct {
    const char *uri;
  } input;

  struct {
    const char *exception_text;
  } expected;
};

// pretty printer for the test data
::std::ostream &operator<<(::std::ostream &os,
                           const URIRootlessTestFailData &data) {
  return os << data.input.uri;
}

class URIRootlessThrowingTests
    : public ::testing::Test,
      public ::testing::WithParamInterface<URIRootlessTestFailData> {};

TEST_P(URIRootlessThrowingTests, FailsParseURI) {
  // call GetParam outside the ASSERT to actually make it work
  auto test_data = GetParam();

  try {
    auto u = URI(test_data.input.uri, false);

    FAIL() << "should have failed";
  } catch (URIError &e) {
    EXPECT_THAT(e.what(), StrEq(test_data.expected.exception_text));
  } catch (...) {
    FAIL() << "expected to throw URIError, got unexpected exception";
  }
}

URIRootlessTestFailData uri_rootless_test_fail_data[] = {
    {// looks like a URI with scheme: localhost, path: 1234
     {
         "localhost:1234",
     },
     {"invalid URI: neither authority nor path at position 14 for: "
      "localhost:1234"}},
};

INSTANTIATE_TEST_SUITE_P(URITests, URIRootlessThrowingTests,
                         ::testing::ValuesIn(uri_rootless_test_fail_data));

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
