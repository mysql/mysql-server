/*
Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

#include "gtest/gtest.h"

#include "mysql/harness/tls_reload_component.h"
#include "test/helpers.h"

namespace {

using namespace std::chrono_literals;

constexpr auto kAsyncTimeout = 10s;
constexpr auto kNoUnexpectedCallbackDelay = 100ms;

class TlsReloadComponentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    component_.stop();
    component_.init();
  }

  void TearDown() override { component_.stop(); }

  mysql_harness::TlsReloadComponent &component_{
      mysql_harness::TlsReloadComponent::get_instance()};
};

TEST_F(TlsReloadComponentTest, RunsEveryOwnerDespiteCallbackFailure) {
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<unsigned> waiting_callback_calls{2};

  const auto failed = component_.register_callback(
      "failed", [&]() -> mysql_harness::TlsReloadComponent::Result {
        if (--waiting_callback_calls == 0) cv.notify_one();
        return stdx::unexpected(std::string{"failed"});
      });
  const auto successful = component_.register_callback("successful", [&] {
    if (--waiting_callback_calls == 0) cv.notify_one();
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  std::unique_lock<std::mutex> lock{mutex};
  EXPECT_TRUE(cv.wait_for(lock, kAsyncTimeout,
                          [&] { return waiting_callback_calls.load() == 0; }));
  component_.unregister_callback(failed);
  component_.unregister_callback(successful);
}

TEST_F(TlsReloadComponentTest, RunsEveryOwnerDespiteCallbackException) {
  std::mutex mutex;
  std::condition_variable cv;
  bool completed{false};

  const auto standard_exception = component_.register_callback(
      "standard exception", []() -> mysql_harness::TlsReloadComponent::Result {
        throw std::runtime_error{"expected test exception"};
      });
  const auto unknown_exception = component_.register_callback(
      "unknown exception",
      []() -> mysql_harness::TlsReloadComponent::Result { throw 1; });
  const auto successful = component_.register_callback("successful", [&] {
    std::lock_guard<std::mutex> lock{mutex};
    completed = true;
    cv.notify_one();
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  std::unique_lock<std::mutex> lock{mutex};
  EXPECT_TRUE(cv.wait_for(lock, kAsyncTimeout, [&] { return completed; }));
  component_.unregister_callback(standard_exception);
  component_.unregister_callback(unknown_exception);
  component_.unregister_callback(successful);
}

TEST_F(TlsReloadComponentTest, RequestDuringActivePassSchedulesSecondPass) {
  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<unsigned> calls{0};
  const auto id = component_.register_callback("owner", [&] {
    const auto call = ++calls;
    if (call == 1) {
      for (unsigned ndx = 0; ndx < 10; ++ndx) component_.request_reload();
    }
    if (call == 2) cv.notify_one();
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  std::unique_lock<std::mutex> lock{mutex};
  EXPECT_TRUE(
      cv.wait_for(lock, kAsyncTimeout, [&] { return calls.load() >= 2; }));
  lock.unlock();
  std::this_thread::sleep_for(kNoUnexpectedCallbackDelay);
  EXPECT_EQ(2U, calls.load());
  component_.unregister_callback(id);
}

TEST_F(TlsReloadComponentTest, RequestsBeforeWorkerStartsAreCoalesced) {
  component_.stop();

  std::mutex mutex;
  std::condition_variable cv;
  std::atomic<unsigned> calls{0};
  const auto id = component_.register_callback("owner", [&] {
    ++calls;
    cv.notify_one();
    return mysql_harness::TlsReloadComponent::Result{};
  });

  for (unsigned ndx = 0; ndx < 10; ++ndx) component_.request_reload();
  component_.init();

  std::unique_lock<std::mutex> lock{mutex};
  ASSERT_TRUE(
      cv.wait_for(lock, kAsyncTimeout, [&] { return calls.load() == 1; }));
  lock.unlock();
  std::this_thread::sleep_for(kNoUnexpectedCallbackDelay);
  EXPECT_EQ(1U, calls.load());
  component_.unregister_callback(id);
}

TEST_F(TlsReloadComponentTest, RequestDoesNotWaitForActiveCallback) {
  std::mutex mutex;
  std::condition_variable cv;
  bool started{false};
  bool release{false};
  std::atomic<unsigned> calls{0};
  const auto id = component_.register_callback("owner", [&] {
    ++calls;
    std::unique_lock<std::mutex> lock{mutex};
    started = true;
    cv.notify_all();
    cv.wait(lock, [&] { return release; });
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  {
    std::unique_lock<std::mutex> lock{mutex};
    ASSERT_TRUE(cv.wait_for(lock, kAsyncTimeout, [&] { return started; }));
  }

  const auto started_at = std::chrono::steady_clock::now();
  for (unsigned ndx = 0; ndx < 100; ++ndx) component_.request_reload();
  EXPECT_LT(std::chrono::steady_clock::now() - started_at, 500ms);

  {
    std::lock_guard<std::mutex> lock{mutex};
    release = true;
  }
  cv.notify_all();
  component_.unregister_callback(id);
}

TEST_F(TlsReloadComponentTest, UnregisteredCallbackIsNotCalled) {
  std::atomic<unsigned> calls{0};
  const auto removed = component_.register_callback("removed", [&] {
    ++calls;
    return mysql_harness::TlsReloadComponent::Result{};
  });
  component_.unregister_callback(removed);

  std::mutex mutex;
  std::condition_variable cv;
  bool complete{false};
  const auto active = component_.register_callback("active", [&] {
    std::lock_guard<std::mutex> lock{mutex};
    complete = true;
    cv.notify_one();
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  std::unique_lock<std::mutex> lock{mutex};
  ASSERT_TRUE(cv.wait_for(lock, kAsyncTimeout, [&] { return complete; }));
  EXPECT_EQ(0U, calls.load());
  component_.unregister_callback(active);
}

TEST_F(TlsReloadComponentTest, UnregisterWaitsForActiveCallback) {
  std::mutex mutex;
  std::condition_variable cv;
  bool callback_started{false};
  bool release_callback{false};
  bool unregister_started{false};
  bool unregister_complete{false};
  std::atomic<unsigned> calls{0};

  const auto id = component_.register_callback("active", [&] {
    ++calls;
    std::unique_lock<std::mutex> lock{mutex};
    callback_started = true;
    cv.notify_all();
    cv.wait(lock, [&] { return release_callback; });
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  {
    std::unique_lock<std::mutex> lock{mutex};
    ASSERT_TRUE(
        cv.wait_for(lock, kAsyncTimeout, [&] { return callback_started; }));
  }

  std::thread unregister_thread{[&] {
    {
      std::lock_guard<std::mutex> lock{mutex};
      unregister_started = true;
      cv.notify_all();
    }
    component_.unregister_callback(id);
    {
      std::lock_guard<std::mutex> lock{mutex};
      unregister_complete = true;
      cv.notify_all();
    }
  }};

  {
    std::unique_lock<std::mutex> lock{mutex};
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&] { return unregister_started; }));
    EXPECT_FALSE(cv.wait_for(lock, kNoUnexpectedCallbackDelay,
                             [&] { return unregister_complete; }));
    release_callback = true;
  }
  cv.notify_all();
  unregister_thread.join();

  EXPECT_EQ(1U, calls.load());
  // A request after unregistering must not revive the removed callback.
  component_.request_reload();
  std::this_thread::sleep_for(kNoUnexpectedCallbackDelay);
  EXPECT_EQ(1U, calls.load());
}

TEST_F(TlsReloadComponentTest, StopWaitsForActiveCallback) {
  std::mutex mutex;
  std::condition_variable started_cv;
  std::condition_variable release_cv;
  bool started{false};
  bool release{false};
  const auto id = component_.register_callback("active", [&] {
    std::unique_lock<std::mutex> lock{mutex};
    started = true;
    started_cv.notify_one();
    release_cv.wait(lock, [&] { return release; });
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.request_reload();
  {
    std::unique_lock<std::mutex> lock{mutex};
    ASSERT_TRUE(
        started_cv.wait_for(lock, kAsyncTimeout, [&] { return started; }));
  }

  std::thread resetter{[&] { component_.stop(); }};
  {
    std::lock_guard<std::mutex> lock{mutex};
    release = true;
  }
  release_cv.notify_one();
  resetter.join();
  component_.unregister_callback(id);
}

TEST_F(TlsReloadComponentTest, RequestsAfterStopAreIgnored) {
  std::atomic<unsigned> calls{0};
  component_.stop();
  component_.request_reload();
  const auto id = component_.register_callback("owner", [&] {
    ++calls;
    return mysql_harness::TlsReloadComponent::Result{};
  });

  component_.init();
  std::this_thread::sleep_for(kNoUnexpectedCallbackDelay);

  EXPECT_EQ(0U, calls.load());
  component_.unregister_callback(id);
}

}  // namespace

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
