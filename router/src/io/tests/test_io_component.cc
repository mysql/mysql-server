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

/**
 * @file
 *
 * plugin interface of the io-component.
 */

#include "mysqlrouter/io_component.h"

#include <limits>
#include <system_error>

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/time.h>
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/loader.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"

constexpr const char defaultIoBackend[] = "poll";

class IoComponentTest : public ::testing::Test {
 public:
  void TearDown() override {
    // if test fails, reset the instance anyway to not interfere with the next
    // test
    IoComponent::get_instance().reset();
  }
};

TEST_F(IoComponentTest, run_without_init) {
  auto &io_comp = IoComponent::get_instance();

  io_comp.run();
}

TEST_F(IoComponentTest, reset_without_init) {
  auto &io_comp = IoComponent::get_instance();

  io_comp.reset();
}

TEST_F(IoComponentTest, init_reinit_reset) {
  auto &io_comp = IoComponent::get_instance();

  SCOPED_TRACE("// init once");
  EXPECT_TRUE(io_comp.init(1, defaultIoBackend));
  SCOPED_TRACE("// init again");
  EXPECT_EQ(
      io_comp.init(1, defaultIoBackend),
      stdx::unexpected(make_error_code(IoComponentErrc::already_initialized)));
  io_comp.reset();
}

// test that the io-component can be re-initialized with the same values
TEST_F(IoComponentTest, init_reset_reinit) {
  auto &io_comp = IoComponent::get_instance();

  SCOPED_TRACE("// init once");
  EXPECT_TRUE(io_comp.init(1, defaultIoBackend));
  io_comp.reset();
  SCOPED_TRACE("// init again");
  EXPECT_TRUE(io_comp.init(1, defaultIoBackend));
  io_comp.reset();
}

// test that run leaves as there is no work assigned to it.
TEST_F(IoComponentTest, init_run_reset_no_work) {
  auto &io_comp = IoComponent::get_instance();

  SCOPED_TRACE("// init once");
  EXPECT_TRUE(io_comp.init(1, defaultIoBackend));
  SCOPED_TRACE("// run");
  io_comp.run();
  SCOPED_TRACE("// reset");
  io_comp.reset();
}

// test that run leaves as there is no work assigned to it.
TEST_F(IoComponentTest, init_run_reset_some_work) {
  auto &io_comp = IoComponent::get_instance();

  SCOPED_TRACE("// init once");
  EXPECT_TRUE(io_comp.init(1, defaultIoBackend));
  SCOPED_TRACE("// run");
  io_comp.run();
  SCOPED_TRACE("// reset");
  io_comp.reset();
}

// test unknown backends
TEST_F(IoComponentTest, init_unknown_backend) {
  auto &io_comp = IoComponent::get_instance();

  SCOPED_TRACE("// init once");
  EXPECT_EQ(
      io_comp.init(1, "unknown_backend"),
      stdx::unexpected(make_error_code(IoComponentErrc::unknown_backend)));
}

#if defined(__linux__)
// test if failing to spawn too many io-threads is handled properly
//
// testing this scenario is:
//
// - safe on Linux as the thread-per-process limits can be adjusted
// - intrusive everywhere else as forward example on windows the
//   "can't spawn more threads" depends on the available memory.
//   If 16G of RAM are available, then the thread-creation fails
//   once all 16G have been used, which doesn't work well with other
//   tests running in parallel in a CI system.
//
#define INIT_TOO_MANY_THREADS_TEST_NAME init_too_many_threads
#else
#define INIT_TOO_MANY_THREADS_TEST_NAME DISABLED_init_too_many_threads
#endif

// Test temporary disabled. The reason is faulty implementation
// of std::error_code on older platforms.
// Example: EL7 puts new things into stdc++ static library
// and this doesn't work well when application puts code into
// shared-libraries.
// Generic error category gets an instance in each shared-library
// and std::error_code compares the category by address.
// This causes that the same errors, are not matched because
// they have different instances of the same category
// (were created in different shared_liblaries).
TEST_F(IoComponentTest, DISABLED_init_too_many_threads) {
  auto &io_comp = IoComponent::get_instance();

#if defined(__linux__)
  // on Linux the test can fail faster by reducing the max num of threads.
  //
  // macosx has RLIMIT_NPROC, but it isn't used as "max-threads"
  SCOPED_TRACE("// get current thread limit");
  struct rlimit orig_rlim = {};
  ASSERT_EQ(getrlimit(RLIMIT_NPROC, &orig_rlim), 0);

  auto rlim = orig_rlim;
  rlim.rlim_cur = 4;

  SCOPED_TRACE("// set to a lower thread limit to trigger the error quickly");
  ASSERT_EQ(setrlimit(RLIMIT_NPROC, &rlim), 0);
#endif

  // either the test runs out of threads or it runs out of notify
  // filedescriptors

  SCOPED_TRACE("// trigger the 'can't spawn-threads'");
  EXPECT_THAT(
      io_comp.init(std::numeric_limits<size_t>::max(), defaultIoBackend),
      ::testing::AnyOf(
          stdx::unexpected(
              make_error_condition(std::errc::resource_unavailable_try_again)),
          stdx::unexpected(
              make_error_condition(std::errc::too_many_files_open)),
          // windows WSAENOBUFS
          stdx::unexpected(std::error_code(10055, std::system_category()))));

#if defined(__linux__)
  SCOPED_TRACE("// reset the thread limit");
  EXPECT_EQ(setrlimit(RLIMIT_NPROC, &orig_rlim), 0);
#endif
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
