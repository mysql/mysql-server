/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/executor.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

static_assert(net::is_executor<net::system_executor>::value,
              "net::system_executor MUST be an executor");

using namespace std::chrono_literals;
constexpr auto kRetryTimeout = 1s;

// a service MUST inherit from execution_context::service
class MockService : public net::execution_context::service {
 public:
  MockService(net::execution_context &owner)
      : net::execution_context::service(owner) {}

  // a service MUST have a key_type that is_same<> itself
  using key_type = MockService;

  void shutdown() noexcept override {}

  // a method to check it works and can be called
  bool ping() const { return true; }
};

/*
 * if a service doesn't exist in the context, make_service creates it
 */
TEST(TestExecutor, make_service) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::make_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));
}

/*
 * make_service throws, if a service of the same type already exists in the
 * context
 */
TEST(TestExecutor, make_service_dup_throws) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::make_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));

  EXPECT_THROW(net::make_service<MockService>(ctx),
               net::service_already_exists);
}

/*
 * has_service returns false if a service doesn't exist
 */
TEST(TestExecutor, has_service_not) {
  net::execution_context ctx;

  EXPECT_FALSE(net::has_service<MockService>(ctx));
}

/*
 * calling use_service if a service doesn't exists, creates it
 */
TEST(TestExecutor, use_service) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::use_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));
}

/*
 * calling use_service if it already exists, doesn't throw
 */
TEST(TestExecutor, use_service_dup_no_throws) {
  net::execution_context ctx;
  EXPECT_FALSE(net::has_service<MockService>(ctx));

  auto &svc = net::use_service<MockService>(ctx);
  EXPECT_TRUE(svc.ping());

  EXPECT_TRUE(net::has_service<MockService>(ctx));

  EXPECT_NO_THROW(net::use_service<MockService>(ctx));
}

template <class Func>
bool retry_for(Func &&func, std::chrono::milliseconds timeout) {
  using clock_type = std::chrono::steady_clock;

  auto start = clock_type::now();
  const auto end_time = start + timeout;

  do {
    if (func()) return true;
    std::this_thread::sleep_for(10ms);
  } while (clock_type::now() <= end_time);

  return false;
}

TEST(TestSystemExecutor, defer_default_context) {
  std::atomic<int> done{};

  // net::defer runs in another thread.
  net::defer([&]() { done = 1; });

  // wait for 'done' to become 1
  EXPECT_TRUE(retry_for([&]() { return done == 1; }, kRetryTimeout));
}

TEST(TestSystemExecutor, defer_system_executor) {
  net::system_executor ex;

  std::atomic<int> done{};

  // net::defer runs in another thread.
  net::defer(ex, [&]() { done = 1; });

  // wait for 'done' to become 1
  EXPECT_TRUE(retry_for([&]() { return done == 1; }, kRetryTimeout));

  // and a 2nd task
  net::defer(ex, [&]() { done = 2; });
  EXPECT_TRUE(retry_for([&]() { return done == 2; }, kRetryTimeout));
}

TEST(TestSystemExecutor, stopped_no_work) {
  net::system_executor ex;

  ASSERT_FALSE(ex.context().stopped());
}

TEST(TestSystemExecutor, stopped_with_work) {
  // there is only one system-context for _all_ tests.
  net::system_executor ex;

  ASSERT_FALSE(ex.context().stopped());

  std::atomic<int> done{};

  // net::defer runs in another thread.
  net::defer(ex, [&]() { done = 1; });

  // wait for 'done' to become 1
  EXPECT_TRUE(retry_for([&]() { return done == 1; }, kRetryTimeout));

  // the executor shouldn't stop itself.
  ASSERT_FALSE(ex.context().stopped());
}

/*
 * THIS MUST BE THE LAST TEST.
 *
 * there is no way to restart the net::system_context's execution-thread once it
 * is stopped
 */
TEST(TestSystemExecutor, stop) {
  // there is only one system-context for _all_ tests.
  net::system_executor ex;

  ASSERT_FALSE(ex.context().stopped());

  std::atomic<int> done{};

  // net::defer runs in another thread.
  net::defer(ex, [&]() { done = 1; });

  // wait for 'done' to become 1
  EXPECT_TRUE(retry_for([&]() { return done == 1; }, kRetryTimeout));

  // the executor shouldn't stop itself.
  ASSERT_FALSE(ex.context().stopped());

  ex.context().stop();

  ASSERT_TRUE(ex.context().stopped());

  // net::defer runs in another thread.
  net::defer(ex, [&]() { done = 2; });

  // should timeout as the 'defer' will not be executed (within the time we
  // wait)
  EXPECT_FALSE(retry_for([&]() { return done == 2; }, kRetryTimeout));

  ex.context().join();
}

TEST(TestSystemExecutor, stopped_after_other_test_stopped) {
  // there is only one system-context for _all_ tests.
  net::system_executor ex;

  ASSERT_TRUE(ex.context().stopped());
}

TEST(TestSystemExecutor, compare) {
  net::system_executor ex1;
  net::system_executor ex2;

  // two system-executors are equal
  ASSERT_THAT(ex1, ::testing::Eq(ex2));
  ASSERT_THAT(ex1, ::testing::Not(::testing::Ne(ex2)));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
