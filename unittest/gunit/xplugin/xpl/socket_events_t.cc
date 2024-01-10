/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <thread>  // NOLINT(build/c++11)

#include "plugin/x/src/ngs/socket_events.h"

namespace ngs {
namespace test {

class Socket_events_task_suite : public ::testing::Test {
 public:
  Socket_events m_sut;
};

TEST_F(Socket_events_task_suite, loop_doesnt_block_when_no_events) {
  m_sut.loop();
}

TEST_F(Socket_events_task_suite, execute_loop_until_no_events) {
  std::atomic<uint64_t> execution_count{4};
  m_sut.add_timer(10, [&execution_count]() { return --execution_count; });
  m_sut.loop();
  ASSERT_EQ(0, execution_count);
}

TEST_F(Socket_events_task_suite,
       break_loop_is_queued_and_ignores_active_events) {
  std::atomic<uint64_t> execution_count{0};

  m_sut.break_loop();
  m_sut.add_timer(10, [&execution_count]() {
    ++execution_count;
    return true;
  });
  m_sut.loop();
  ASSERT_EQ(0, execution_count);
}

TEST_F(Socket_events_task_suite, break_loop_from_thread) {
  std::atomic<uint64_t> execution_count{0};

  std::thread break_thread{[this, &execution_count]() {
    while (execution_count.load() < 10) {
      std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }
    m_sut.break_loop();
  }};

  m_sut.add_timer(10, [&execution_count]() {
    ++execution_count;
    return true;
  });
  m_sut.loop();
  ASSERT_LT(0, execution_count.load());
  break_thread.join();
}

// "Socket_events::break_loop" calls "net::io_context::stop".
// The "stop" method doesn't work when its called from active thread.
// This behavior is not consitent with "asio::io_context::stop".
// still the test-case is optional because X Plugin doesn't
// calls the stop from active thread.
//
// The test is going to be disabled until proper behavior
// of io_context::stop is implemnted.
TEST_F(Socket_events_task_suite,
       DISABLED_break_loop_from_thread_always_active) {
  std::atomic<uint64_t> execution_count{0};

  std::thread break_thread{[this, &execution_count]() {
    while (execution_count.load() < 10) {
      std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }
    m_sut.break_loop();
  }};

  m_sut.add_timer(0, [&execution_count]() {
    ++execution_count;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
  });
  m_sut.loop();
  ASSERT_LT(0, execution_count.load());
  break_thread.join();
}

}  // namespace test
}  // namespace ngs
