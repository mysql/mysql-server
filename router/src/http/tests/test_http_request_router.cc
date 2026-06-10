/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "http_request_router.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unicode/regex.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

#include <regex>

#include "harness_assert.h"
#include "http/server/request.h"
#include "test/helpers.h"  // init_test_logger()
#include "unittest/gunit/benchmark.h"

class MockRequestHandler : public http::base::RequestHandler {
 public:
  explicit MockRequestHandler(std::function<void(http::base::Request &)> cb)
      : cb_(std::move(cb)) {}
  void handle_request(http::base::Request &req) override { cb_(req); }

 private:
  std::function<void(http::base::Request &)> cb_;
};

class HttpRequestRouterTest : public ::testing::Test {
 public:
  void SetUp() override {
    rtr_.register_regex_handler(
        "", "substr",
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              substr_is_matched_ = true;
            }));
    rtr_.register_regex_handler(
        "", "^/prefix",
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              prefix_is_matched_ = true;
            }));
    rtr_.register_regex_handler(
        "", "/suffix$",
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              suffix_is_matched_ = true;
            }));
    rtr_.register_regex_handler(
        "", "^/exact$",
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              exact_is_matched_ = true;
            }));
    rtr_.register_regex_handler(
        "", "^/r[eE]gex$",
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              regex_is_matched_ = true;
            }));

    rtr_.set_default_route(std::make_unique<MockRequestHandler>(
        [&](http::base::Request &req [[maybe_unused]]) {
          default_is_matched_ = true;
        }));
  }

 protected:
  HttpRequestRouter rtr_;

  bool substr_is_matched_{false};
  bool prefix_is_matched_{false};
  bool exact_is_matched_{false};
  bool suffix_is_matched_{false};
  bool regex_is_matched_{false};
  bool default_is_matched_{false};
};

TEST_F(HttpRequestRouterTest, route_substr) {
  http::server::ServerRequest req{
      nullptr, 0, http::base::method::Get, "/foo/substr/baz", {}};

  rtr_.route(req);

  EXPECT_TRUE(substr_is_matched_);
  EXPECT_FALSE(exact_is_matched_);
  EXPECT_FALSE(prefix_is_matched_);
  EXPECT_FALSE(suffix_is_matched_);
  EXPECT_FALSE(regex_is_matched_);
  EXPECT_FALSE(default_is_matched_);
}

TEST_F(HttpRequestRouterTest, route_exact) {
  http::server::ServerRequest req{
      nullptr, 0, http::base::method::Get, "/exact", {}};

  rtr_.route(req);

  EXPECT_FALSE(substr_is_matched_);
  EXPECT_TRUE(exact_is_matched_);
  EXPECT_FALSE(prefix_is_matched_);
  EXPECT_FALSE(suffix_is_matched_);
  EXPECT_FALSE(regex_is_matched_);
  EXPECT_FALSE(default_is_matched_);
}

TEST_F(HttpRequestRouterTest, route_prefix) {
  http::server::ServerRequest req{
      nullptr, 0, http::base::method::Get, "/prefix/baz", {}};

  rtr_.route(req);

  EXPECT_FALSE(substr_is_matched_);
  EXPECT_FALSE(exact_is_matched_);
  EXPECT_TRUE(prefix_is_matched_);
  EXPECT_FALSE(suffix_is_matched_);
  EXPECT_FALSE(regex_is_matched_);
  EXPECT_FALSE(default_is_matched_);
}

TEST_F(HttpRequestRouterTest, route_suffix) {
  http::server::ServerRequest req{
      nullptr, 0, http::base::method::Get, "/some/suffix", {}};

  rtr_.route(req);

  EXPECT_FALSE(substr_is_matched_);
  EXPECT_FALSE(exact_is_matched_);
  EXPECT_FALSE(prefix_is_matched_);
  EXPECT_TRUE(suffix_is_matched_);
  EXPECT_FALSE(regex_is_matched_);
  EXPECT_FALSE(default_is_matched_);
}

TEST_F(HttpRequestRouterTest, route_regex) {
  http::server::ServerRequest req{
      nullptr, 0, http::base::method::Get, "/rEgex", {}};

  rtr_.route(req);

  EXPECT_FALSE(substr_is_matched_);
  EXPECT_FALSE(exact_is_matched_);
  EXPECT_FALSE(prefix_is_matched_);
  EXPECT_FALSE(suffix_is_matched_);
  EXPECT_TRUE(regex_is_matched_);
  EXPECT_FALSE(default_is_matched_);
}

TEST_F(HttpRequestRouterTest, route_default) {
  http::server::ServerRequest req{
      nullptr, 0, http::base::method::Get, "/default", {}};

  rtr_.route(req);

  EXPECT_FALSE(substr_is_matched_);
  EXPECT_FALSE(exact_is_matched_);
  EXPECT_FALSE(prefix_is_matched_);
  EXPECT_FALSE(suffix_is_matched_);
  EXPECT_FALSE(regex_is_matched_);
  EXPECT_TRUE(default_is_matched_);
}

class HttpRequestRouterDirectMatchTest : public ::testing::Test {
 public:
  using UriPathMatcher = ::http::base::UriPathMatcher;

  bool matches(const UriPathMatcher &pm1, const UriPathMatcher &pm2) {
    const auto path_key1 =
        HttpRequestRouter::RouteDirectMatcher::path_key_from_matcher(pm1);
    const auto path_key2 =
        HttpRequestRouter::RouteDirectMatcher::path_key_from_matcher(pm2);

    // this should never be the case
    EXPECT_FALSE((path_key1 < path_key2) && (path_key2 < path_key1));

    return !(path_key1 < path_key2) && !(path_key2 < path_key1);
  }
};

TEST_F(HttpRequestRouterDirectMatchTest, compare_direct_paths) {
  using PM = ::http::base::UriPathMatcher;

  // 1:1 match
  EXPECT_TRUE(matches(PM{"/service/db", false, false},
                      PM{"/service/db", false, false}));

  // 1:1 match (with allowed id and trailing slash)
  EXPECT_TRUE(
      matches(PM{"/service", false, false}, PM{"/service", true, true}));

  // no match different path elements
  EXPECT_FALSE(matches(PM{"/service/path1", false, false},
                       PM{"/service/path2", false, false}));

  // no match different path lengths
  EXPECT_FALSE(matches(PM{"/service/path1/path2", false, false},
                       PM{"/service/path1", false, false}));

  // match because id allowed
  EXPECT_TRUE(matches(PM{"/service/a/b/c/123", false, false},
                      PM{"/service/a/b/c", true, false}));

  // no match because id not allowed
  EXPECT_FALSE(matches(PM{"/service/a/b/c/123", false, false},
                       PM{"/service/a/b/c", false, false}));

  // paths matching, parameters ignored
  EXPECT_TRUE(
      matches(PM{"/debug?do=start", false, false}, {"/debug", false, false}));

  // match because trailing slash allowed
  EXPECT_TRUE(
      matches(PM{"/service/", false, false}, PM{"/service", false, true}));

  // no match because trailing slash not allowed
  EXPECT_FALSE(
      matches(PM{"/service/", false, false}, PM{"/service", false, false}));
}

TEST_F(HttpRequestRouterDirectMatchTest, multiple_handlers_and_wildcard) {
  HttpRequestRouter rtr;
  size_t handler_metadata_used{0}, handler_custom_used{0},
      handler_wildcard_used{0};

  // register paths:
  // /svc/db/ob/_metadata
  // /svc/db/ob/custom
  // /svc/db/ob[/*]
  rtr.register_direct_match_handler(
      "", {"/svc/db/ob/_metadata", false, false},
      std::make_unique<MockRequestHandler>(
          [&](http::base::Request &req [[maybe_unused]]) {
            handler_metadata_used++;
          }));
  rtr.register_direct_match_handler(
      "", {"/svc/db/ob/custom", false, false},
      std::make_unique<MockRequestHandler>(
          [&](http::base::Request &req [[maybe_unused]]) {
            handler_custom_used++;
          }));
  rtr.register_direct_match_handler(
      "", {"/svc/db/ob", true, false},
      std::make_unique<MockRequestHandler>(
          [&](http::base::Request &req [[maybe_unused]]) {
            handler_wildcard_used++;
          }));

  // route a request for each
  {
    http::server::ServerRequest req{
        nullptr, 0, http::base::method::Get, "/svc/db/ob/_metadata", {}};
    rtr.route(req);
  }

  {
    http::server::ServerRequest req{
        nullptr, 0, http::base::method::Get, "/svc/db/ob/custom", {}};
    rtr.route(req);
  }

  {
    http::server::ServerRequest req{
        nullptr, 0, http::base::method::Get, "/svc/db/ob/1", {}};
    rtr.route(req);
  }

  // check that expected handlers were used
  EXPECT_EQ(1, handler_metadata_used);
  EXPECT_EQ(1, handler_custom_used);
  EXPECT_EQ(1, handler_wildcard_used);
}

namespace {
void RegexMatchICUFind(size_t iter) {
  UErrorCode status = U_ZERO_ERROR;

  auto matcher = std::make_unique<icu::RegexMatcher>(
      "^/foo/bar/buz(/([0-9]|[a-z]|[A-Z]|[-._~!$&'()*+,;=:@%]| "
      ")*/?)?$",
      0, status);

  while ((iter--) != 0) {
    // input must outlive ->find()
    icu::UnicodeString input("/foo/bar/buz/1", -1, US_INV);
    matcher->reset(input);
    if (matcher->find(0, status)) {
      // happy.
    } else {
      harness_assert_this_should_not_execute();
    }
  }
}

void RegexMatchICUFindSimplified(size_t iter) {
  UErrorCode status = U_ZERO_ERROR;

  auto matcher = std::make_unique<icu::RegexMatcher>(
      "^/foo/bar/buz(/([-0-9a-zA-Z._~!$&'()*+,;=:@% ])*/?)?$", 0, status);

  while ((iter--) != 0) {
    // input must outlive ->find()
    icu::UnicodeString input("/foo/bar/buz/1", -1, US_INV);
    matcher->reset(input);
    if (matcher->find(0, status)) {
      // happy.
    } else {
      harness_assert_this_should_not_execute();
    }
  }
}

void RegexMatchStdRegex(size_t iter) {
  std::regex re(
      "^/foo/bar/buz(/([0-9]|[a-z]|[A-Z]|[-._~!$&'()*+,;=:@%]| "
      ")*/?)?$");

  while ((iter--) != 0) {
    if (std::regex_search("/foo/bar/buz/1", re)) {
      // happy
    } else {
      harness_assert_this_should_not_execute();
    }
  }
}

void RegexMatchStdRegexSimplified(size_t iter) {
  std::regex re("^/foo/bar/buz(/([-0-9a-zA-Z._~!$&'()*+,;=:@% ])*/?)?$");

  while ((iter--) != 0) {
    if (std::regex_search("/foo/bar/buz/1", re)) {
      // happy
    } else {
      harness_assert_this_should_not_execute();
    }
  }
}

constexpr size_t kNumPaths{2000};

void RegexMatchMultipleMatchers(size_t iter) {
  HttpRequestRouter rtr;
  size_t handled_requests_counter = 0;
  const std::string k_path_id_or_query =
      "(/([0-9]|[a-z]|[A-Z]|[-._~!$&'()*+,;=:@%]| )*/?)?";

  for (size_t i = 0; i < kNumPaths; i++) {
    std::string path{"/svc/path/subpath" + std::to_string(i) +
                     k_path_id_or_query};
    rtr.register_regex_handler(
        "", "^" + path + "$",
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              handled_requests_counter++;
            }));
  }

  while ((iter--) != 0) {
    std::string path{"/svc/path/subpath" + std::to_string(iter % kNumPaths) +
                     "/id"};
    http::server::ServerRequest req{
        nullptr, 0, http::base::method::Get, path, {}};

    rtr.route(req);
  }
}

void DirectMatchMultipleMatchers(size_t iter) {
  HttpRequestRouter rtr;
  size_t handled_requests_counter = 0;

  for (size_t i = 0; i < kNumPaths; i++) {
    std::string path{"/svc/path/subpath" + std::to_string(i)};
    rtr.register_direct_match_handler(
        "", {path, true, true},
        std::make_unique<MockRequestHandler>(
            [&](http::base::Request &req [[maybe_unused]]) {
              handled_requests_counter++;
            }));
  }

  while ((iter--) != 0) {
    std::string path{"/svc/path/subpath" + std::to_string(iter % kNumPaths) +
                     "/id"};
    http::server::ServerRequest req{
        nullptr, 0, http::base::method::Get, path, {}};

    rtr.route(req);
  }
}

}  // namespace

BENCHMARK(RegexMatchStdRegex)
BENCHMARK(RegexMatchStdRegexSimplified)
BENCHMARK(RegexMatchICUFind)
BENCHMARK(RegexMatchICUFindSimplified)

BENCHMARK(RegexMatchMultipleMatchers)
BENCHMARK(DirectMatchMultipleMatchers)

int main(int argc, char *argv[]) {
  init_test_logger({}, "", "", "info");

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
