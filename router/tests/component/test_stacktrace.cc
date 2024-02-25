/*
Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "my_config.h"  // HAVE_ASAN & HAVE_UBSAN
#include "mysql/harness/filesystem.h"
#include "process_launcher.h"
#include "process_manager.h"
#include "router_component_test.h"

mysql_harness::Path g_origin_path;

class StacktraceTest : public RouterComponentTest {};

TEST_F(StacktraceTest, spawn_missing_args) {
  auto &proc =
      spawner(g_origin_path.join("signal_me").str())
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .expected_exit_code(EXIT_FAILURE)
          .spawn({});

  SCOPED_TRACE("// wait for the process to exit");
  EXPECT_NO_THROW(proc.native_wait_for_exit());  // timeout throws
}

TEST_F(StacktraceTest, spawn_signal_0) {
  auto &proc =
      spawner(g_origin_path.join("signal_me").str())
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .expected_exit_code(EXIT_SUCCESS)
          .spawn({"0"});

  SCOPED_TRACE("// wait for the process to exit");
  EXPECT_NO_THROW(proc.native_wait_for_exit());  // timeout throws
}

TEST_F(StacktraceTest, spawn_signal_abrt) {
  auto executable = g_origin_path.join("signal_me").str();

  auto &proc =
      spawner(executable)
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
#ifdef _WIN32
          // Abort terminates ... and returns an exit code of 3
          .expected_exit_code(ExitStatus{ExitStatus::terminated_t{},
                                         static_cast<int>(STATUS_BREAKPOINT)})
#else
          .expected_exit_code(ExitStatus{ExitStatus::terminated_t{}, SIGABRT})
#endif
          .spawn({std::to_string(SIGABRT)});

  SCOPED_TRACE("// wait for the process to exit");
  EXPECT_NO_THROW(proc.native_wait_for_exit());  // timeout throws
}

// we skip that one when ASAN, UBSAN or TSAN is used as it marks them as failed
// seeing ABORT signal
#if !defined(HAVE_ASAN) && !defined(HAVE_UBSAN) && !defined(HAVE_TSAN)

TEST_F(StacktraceTest, spawn_signal_segv) {
  auto executable = g_origin_path.join("signal_me").str();
  auto &proc =
      spawner(executable)
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
#ifdef _WIN32
          // Abort terminates ... and returns an exit code of 3
          .expected_exit_code(
              ExitStatus{ExitStatus::terminated_t{},
                         static_cast<int>(STATUS_ACCESS_VIOLATION)})
#else
          .expected_exit_code(ExitStatus{ExitStatus::terminated_t{}, SIGSEGV})
#endif
          .spawn({std::to_string(SIGSEGV)});

  SCOPED_TRACE("// wait for the process to exit");
  EXPECT_NO_THROW(proc.native_wait_for_exit());  // timeout throws
}

#endif

int main(int argc, char *argv[]) {
  g_origin_path = mysql_harness::Path(argv[0]).dirname();

  ProcessManager::set_origin(g_origin_path);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
