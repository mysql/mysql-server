/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
    rtr_.append("substr", std::make_unique<MockRequestHandler>(
                              [&](http::base::Request &req [[maybe_unused]]) {
                                substr_is_matched_ = true;
                              }));
    rtr_.append("^/prefix", std::make_unique<MockRequestHandler>(
                                [&](http::base::Request &req [[maybe_unused]]) {
                                  prefix_is_matched_ = true;
                                }));
    rtr_.append("/suffix$", std::make_unique<MockRequestHandler>(
                                [&](http::base::Request &req [[maybe_unused]]) {
                                  suffix_is_matched_ = true;
                                }));
    rtr_.append("^/exact$", std::make_unique<MockRequestHandler>(
                                [&](http::base::Request &req [[maybe_unused]]) {
                                  exact_is_matched_ = true;
                                }));
    rtr_.append("^/r[eE]gex$",
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
}  // namespace

BENCHMARK(RegexMatchStdRegex)
BENCHMARK(RegexMatchStdRegexSimplified)
BENCHMARK(RegexMatchICUFind)
BENCHMARK(RegexMatchICUFindSimplified)

int main(int argc, char *argv[]) {
  init_test_logger();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
