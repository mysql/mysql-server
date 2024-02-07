/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <chrono>
#include <thread>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core_dumper.h"
#include "core_finder.h"
#include "my_config.h"  // HAVE_ASAN & HAVE_UBSAN
#include "mysql/harness/filesystem.h"
#include "mysql/harness/signal_handler.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysqlrouter/mock_server_rest_client.h"
#include "mysqlrouter/rest_client.h"
#include "process_launcher.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"
#include "router_test_helpers.h"
#include "scope_guard.h"
#include "test/temp_directory.h"

namespace {
constexpr const int abrt_status{
#ifdef _WIN32
    0x00000102  // STATUS_TIMEOUT
#else
    SIGABRT
#endif
};

#if !defined(HAVE_ASAN) && !defined(HAVE_UBSAN) && !defined(HAVE_TSAN)

constexpr const int segv_status{
#ifdef _WIN32
    static_cast<int>(0xC0000005)  // STATUS_ACCESS_VIOLATION
#else
    SIGSEGV
#endif
};

#endif

}  // namespace

class RouterStacktraceTest : public RouterComponentTest {};

// TS_1_1
TEST_F(RouterStacktraceTest, help_has_core_file) {
  TempDirectory tmp_dir;

  // --core-file is added automatically by router_spawner()
  auto &r = router_spawner()
                .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
                .spawn({"--help"});

  SCOPED_TRACE("// wait for the exit");
  r.native_wait_for_exit();

  // Usage
  EXPECT_THAT(r.get_full_output(), ::testing::HasSubstr("[--core-file"));

  // Options
  EXPECT_THAT(r.get_full_output(), ::testing::HasSubstr("  --core-file"));
}

// TS_1_2
TEST_F(RouterStacktraceTest, bootstrap_with_core_file) {
  auto mock_port = port_pool_.get_next_available();

  // using a non-bootstrap script makes sure the bootstrap fails early, and
  // still checks that --core-file is accepted.
  launch_mysql_server_mock(get_data_dir().str() + "/my_port.js", mock_port,
                           EXIT_SUCCESS, false, 0, 0, get_data_dir().str());

  TempDirectory tmp_dir;
  // --core-file is added automatically by router_spawner()
  auto &r =
      router_spawner()
          .with_core_dump(false)  // avoid the automatic --core-file
          .expected_exit_code(
              ExitStatus{ExitStatus::exited_t{},
                         mysql_harness::SignalHandler::HARNESS_ABORT_EXIT})
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .output_responder([](const std::string &) { return "password\n"; })
          .spawn({
              "--bootstrap",
              "username@127.0.0.1:" + std::to_string(mock_port),  //
              "--directory", tmp_dir.name(),                      //
              "--core-file",                                      //
              "--report-host=dont.query.dns",                     //
          });

  SCOPED_TRACE("// wait for the exit");
  r.native_wait_for_exit();

  // as the mock-server doesn't run a bootstrap script, the SQL will fail.
  EXPECT_THAT(r.get_full_output(),
              ::testing::HasSubstr("Error executing MySQL query"));
}

// TS_1_2
TEST_F(RouterStacktraceTest, crash_me_bootstrap) {
  auto mock_port = port_pool_.get_next_available();
  auto mock_http_port = port_pool_.get_next_available();

  launch_mysql_server_mock(
      get_data_dir().str() + "/bootstrap_exec_time_2_seconds.js", mock_port,
      EXIT_SUCCESS, true, mock_http_port, 0, get_data_dir().str());

  TempDirectory tmp_dir;
  // --core-file is added automatically by router_spawner()
  auto &r =
      router_spawner()
          .with_core_dump(false)  // avoid the automatic --core-file
          .expected_exit_code(
              ExitStatus{ExitStatus::terminated_t{}, abrt_status})
          .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
          .output_responder([](const std::string &) { return "somepass\n"; })
          .spawn({
              "--bootstrap", "127.0.0.1:" + std::to_string(mock_port),  //
              "--directory", tmp_dir.name(),                            //
              "--core-file",                                            //
              "--report-host=dont.query.dns",                           //
          });

  SCOPED_TRACE("// wait until mock-server is blocked for 2 seconds");
  MockServerRestClient client(mock_http_port);
  int blocked{};

  using namespace std::chrono_literals;

  using clock_type = std::chrono::steady_clock;

  auto now = clock_type::now();
  auto end_time = now + 5s;

  do {
    try {
      blocked = client.get_int_global("blocked");
      if (blocked) break;
    } catch (const std::exception &e) {
      std::cerr << e.what() << "\n";
    }

    std::this_thread::sleep_for(100ms);
  } while (clock_type::now() < end_time);

  ASSERT_TRUE(blocked);

  SCOPED_TRACE("// aborting bootstrapping router");
  r.send_shutdown_event(mysql_harness::ProcessLauncher::ShutdownEvent::ABRT);

  SCOPED_TRACE("// wait for the exit");
  r.native_wait_for_exit();

  auto core_file_name = CoreFinder(r.executable(), r.get_pid()).core_name();
  // remove the core-file if it exists at the end of the test.
  Scope_guard exit_guard([core_file_name]() {
    if (core_file_name.empty() ||
        !mysql_harness::Path(core_file_name).exists()) {
      return;
    }

    std::error_code ec;
    stdx::filesystem::remove(core_file_name, ec);
  });

  if (core_file_name.empty()) {
    GTEST_SKIP() << "CoreFinder doesn't know how to locate the core-file";
  }

  if (!mysql_harness::Path(core_file_name).exists()) {
    // MacOS has cores usually disabled.
    GTEST_SKIP() << core_file_name << " does not exist";
  }

  auto dump_res = CoreDumper(r.executable(), r.get_pid()).dump(core_file_name);
  if (!dump_res) {
    GTEST_SKIP() << "CoreDumper failed with: " << dump_res.error();
  }

  // if we get a stacktrace it should mention something with mysqlrouter.
  EXPECT_THAT(dump_res.value(), ::testing::HasSubstr("mysqlrouter"));
}

struct InvalidOptions {
  std::string name;

  std::string option;
};

class RouterStacktraceInvalidOptionTest
    : public RouterStacktraceTest,
      public ::testing::WithParamInterface<InvalidOptions> {};

// TS_1_3
TEST_P(RouterStacktraceInvalidOptionTest, core_file_invalid_value_fails) {
  TempDirectory tmp_dir;

  // --core-file is added automatically by router_spawner()
  auto &r = router_spawner()
                .with_core_dump(false)  // avoid the automatic --core-file
                .expected_exit_code(ExitStatus{
                    ExitStatus::exited_t{},
                    mysql_harness::SignalHandler::HARNESS_ABORT_EXIT})
                .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
                .spawn({"--core-file=" + GetParam().option});

  SCOPED_TRACE("// wait for the exit");
  r.native_wait_for_exit();

  EXPECT_THAT(r.get_full_output(),
              ::testing::HasSubstr("needs to be one of: ['0', '1']"));
}

static const InvalidOptions invalid_options[] = {
    {"minus_1", "-1"},
    {"2", "2"},
    {"abc", "abc"},
};

INSTANTIATE_TEST_SUITE_P(Spec, RouterStacktraceInvalidOptionTest,
                         ::testing::ValuesIn(invalid_options),
                         [](auto info) { return info.param.name; });

// we skip those when ASAN, UBSAN and TSAN is used as it marks them as failed
// seeing ABORT signal
#if !defined(HAVE_ASAN) && !defined(HAVE_UBSAN) && !defined(HAVE_TSAN)

// TS_3_1
TEST_F(RouterStacktraceTest, crash_me_via_rest_signal_abort) {
  TempDirectory tmp_dir;

  auto http_port = port_pool_.get_next_available();

  auto writer =
      config_writer(tmp_dir.name())
          .section("rest_signal", {})
          .section("http_server", {{"bind_address", "127.0.0.1"},
                                   {"port", std::to_string(http_port)}});

  // --core-file is added automatically by router_spawner()
  auto &r = router_spawner()
                .expected_exit_code(
                    ExitStatus{ExitStatus::terminated_t{}, segv_status})
                .spawn({"-c", writer.write()});

  SCOPED_TRACE("// aborting router");
  {
    IOContext io_ctx;
    auto resp =
        RestClient(io_ctx, "127.0.0.1", http_port)
            .request_sync(HttpMethod::Get,
                          std::string(rest_api_basepath) + "/signal/abort");

    EXPECT_FALSE(resp);  // router should crash.
  }

  SCOPED_TRACE("// wait for the exit");
  ASSERT_NO_THROW(r.native_wait_for_exit());

  auto core_file_name = CoreFinder(r.executable(), r.get_pid()).core_name();
  // remove the core-file if it exists at the end of the test.
  Scope_guard exit_guard([core_file_name]() {
    if (core_file_name.empty() ||
        !mysql_harness::Path(core_file_name).exists()) {
      return;
    }

    std::error_code ec;
    stdx::filesystem::remove(core_file_name, ec);
  });

  if (core_file_name.empty()) {
    GTEST_SKIP() << "CoreFinder doesn't know how to locate the core-file";
  }

  if (!mysql_harness::Path(core_file_name).exists()) {
    // MacOS has cores usually disabled.
    GTEST_SKIP() << core_file_name << " does not exist";
  }

  auto dump_res = CoreDumper(r.executable(), r.get_pid()).dump(core_file_name);
  if (!dump_res) {
    GTEST_SKIP() << "CoreDumper failed with: " << dump_res.error();
  }

  EXPECT_THAT(
      dump_res.value(),
      ::testing::AnyOf(
          ::testing::HasSubstr("Access violation - code "),         // cdb
          ::testing::HasSubstr("Program terminated with signal "),  // gdb
          ::testing::HasSubstr("stop reason = signal SIG")          // lldb
          ));
}

#endif  // !defined(HAVE_ASAN) && !defined(HAVE_UBSAN)

TEST_F(RouterStacktraceTest, crash_me_via_event) {
  TempDirectory tmp_dir;

  auto writer =
      config_writer(tmp_dir.name())
          .section("routing:some",
                   {
                       {"bind_port",
                        std::to_string(port_pool_.get_next_available())},
                       {"destinations", "127.0.0.1:3306"},
                       {"routing_strategy", "round-robin"},
                   });

  // --core-file is added automatically by router_spawner()
  auto &r = router_spawner()
                .expected_exit_code(
                    ExitStatus{ExitStatus::terminated_t{}, abrt_status})
                .spawn({"-c", writer.write()});

  SCOPED_TRACE("// aborting router");
  r.send_shutdown_event(mysql_harness::ProcessLauncher::ShutdownEvent::ABRT);

  SCOPED_TRACE("// wait for the exit");
  ASSERT_NO_THROW(r.native_wait_for_exit());

  auto core_file_name = CoreFinder(r.executable(), r.get_pid()).core_name();
  // remove the core-file if it exists at the end of the test.
  Scope_guard exit_guard([core_file_name]() {
    if (core_file_name.empty() ||
        !mysql_harness::Path(core_file_name).exists()) {
      return;
    }

    std::error_code ec;
    stdx::filesystem::remove(core_file_name, ec);
  });

  if (core_file_name.empty()) {
    GTEST_SKIP() << "CoreFinder doesn't know how to locate the core-file";
  }

  if (!mysql_harness::Path(core_file_name).exists()) {
    // MacOS has cores usually disabled.
    GTEST_SKIP() << core_file_name << " does not exist";
  }

  auto dump_res = CoreDumper(r.executable(), r.get_pid()).dump(core_file_name);
  if (!dump_res) {
    GTEST_SKIP() << "CoreDumper failed with: " << dump_res.error();
  }

  EXPECT_THAT(
      dump_res.value(),
      ::testing::AnyOf(
          ::testing::HasSubstr("Access violation - code "),         // cdb
          ::testing::HasSubstr("Program terminated with signal "),  // gdb
          ::testing::HasSubstr("stop reason = signal SIG")          // lldb
          ));
}

#if !defined(HAVE_ASAN) && !defined(HAVE_UBSAN) && !defined(HAVE_TSAN)

// TS_3_1
TEST_F(RouterStacktraceTest, crash_me_core_file_1) {
  TempDirectory tmp_dir;

  auto http_port = port_pool_.get_next_available();
  auto writer =
      config_writer(tmp_dir.name())
          .section("rest_signal", {})
          .section("http_server", {{"bind_address", "127.0.0.1"},
                                   {"port", std::to_string(http_port)}});

  auto &r = router_spawner()
                .with_core_dump(false)  // avoid the automatic --core-file
                .expected_exit_code(
                    ExitStatus{ExitStatus::terminated_t{}, segv_status})
                .spawn({"-c", writer.write(), "--core-file", "1"});

  SCOPED_TRACE("// aborting router");
  {
    IOContext io_ctx;
    auto resp =
        RestClient(io_ctx, "127.0.0.1", http_port)
            .request_sync(HttpMethod::Get,
                          std::string(rest_api_basepath) + "/signal/abort");

    EXPECT_FALSE(resp);  // router should crash.
  }

  SCOPED_TRACE("// wait for the exit");
  ASSERT_NO_THROW(r.native_wait_for_exit());

  auto core_file_name = CoreFinder(r.executable(), r.get_pid()).core_name();
  // remove the core-file if it exists at the end of the test.
  Scope_guard exit_guard([core_file_name]() {
    if (core_file_name.empty()) return;

    std::error_code ec;
    stdx::filesystem::remove(core_file_name, ec);
  });

  if (core_file_name.empty()) {
    GTEST_SKIP() << "CoreFinder doesn't know how to locate the core-file";
  }

  if (!mysql_harness::Path(core_file_name).exists()) {
    // MacOS has cores usually disabled.
    GTEST_SKIP() << core_file_name << " does not exist";
  }

  auto dump_res = CoreDumper(r.executable(), r.get_pid()).dump(core_file_name);
  if (!dump_res) {
    GTEST_SKIP() << "CoreDumper failed with: " << dump_res.error();
  }

  EXPECT_THAT(
      dump_res.value(),
      ::testing::AnyOf(
          ::testing::HasSubstr("Access violation - code "),         // cdb
          ::testing::HasSubstr("Program terminated with signal "),  // gdb
          ::testing::HasSubstr("stop reason = signal SIG")          // lldb
          ));
}

// TS_2_2
TEST_F(RouterStacktraceTest, no_core_file) {
  TempDirectory tmp_dir;

  auto http_port = port_pool_.get_next_available();
  auto writer =
      config_writer(tmp_dir.name())
          .section("rest_signal", {})
          .section("http_server", {{"bind_address", "127.0.0.1"},
                                   {"port", std::to_string(http_port)}});

  auto &r = router_spawner()
                .with_core_dump(false)
#ifdef _WIN32
                .expected_exit_code(
                    ExitStatus{ExitStatus::terminated_t{}, segv_status})
#else
                .expected_exit_code(ExitStatus{
                    ExitStatus::exited_t{},
                    mysql_harness::SignalHandler::HARNESS_FAILURE_EXIT})
#endif
                .spawn({"-c", writer.write()});

  SCOPED_TRACE("// aborting router");
  {
    IOContext io_ctx;
    auto resp =
        RestClient(io_ctx, "127.0.0.1", http_port)
            .request_sync(HttpMethod::Get,
                          std::string(rest_api_basepath) + "/signal/abort");

    EXPECT_FALSE(resp);  // router should crash.
  }

  SCOPED_TRACE("// wait for the exit");
  ASSERT_NO_THROW(r.native_wait_for_exit());

  auto core_file_name = CoreFinder(r.executable(), r.get_pid()).core_name();
  // remove the core-file if it exists at the end of the test.
  Scope_guard exit_guard([core_file_name]() {
    if (core_file_name.empty() ||
        !mysql_harness::Path(core_file_name).exists()) {
      return;
    }

    std::error_code ec;
    stdx::filesystem::remove(core_file_name, ec);
  });

  if (!core_file_name.empty()) {
    EXPECT_FALSE(mysql_harness::Path(core_file_name).exists())
        << core_file_name;
  }

  SCOPED_TRACE("// console output has stacktrace");
#ifdef HAVE_EXT_BACKTRACE
  EXPECT_THAT(r.get_full_output(), ::testing::HasSubstr("signal_handler.cc"));
#else
  EXPECT_THAT(r.get_full_output(), ::testing::HasSubstr("my_print_stacktrace"));
#endif
}

// TS_2_2
TEST_F(RouterStacktraceTest, core_file_0) {
  TempDirectory tmp_dir;

  auto http_port = port_pool_.get_next_available();
  auto writer =
      config_writer(tmp_dir.name())
          .section("rest_signal", {})
          .section("http_server", {{"bind_address", "127.0.0.1"},
                                   {"port", std::to_string(http_port)}});

  auto &r = router_spawner()
                .with_core_dump(false)
#ifdef _WIN32
                .expected_exit_code(
                    ExitStatus{ExitStatus::terminated_t{}, segv_status})
#else
                .expected_exit_code(ExitStatus{
                    ExitStatus::exited_t{},
                    mysql_harness::SignalHandler::HARNESS_FAILURE_EXIT})
#endif
                .spawn({"-c", writer.write(), "--core-file", "0"});

  SCOPED_TRACE("// aborting router");
  {
    IOContext io_ctx;
    auto resp =
        RestClient(io_ctx, "127.0.0.1", http_port)
            .request_sync(HttpMethod::Get,
                          std::string(rest_api_basepath) + "/signal/abort");

    EXPECT_FALSE(resp);  // router should crash.
  }

  SCOPED_TRACE("// wait for the exit");
  ASSERT_NO_THROW(r.native_wait_for_exit());

  auto core_file_name = CoreFinder(r.executable(), r.get_pid()).core_name();
  // remove the core-file if it exists at the end of the test.
  Scope_guard exit_guard([core_file_name]() {
    if (core_file_name.empty() ||
        !mysql_harness::Path(core_file_name).exists()) {
      return;
    }

    std::error_code ec;
    stdx::filesystem::remove(core_file_name, ec);
  });

  if (!core_file_name.empty()) {
    EXPECT_FALSE(mysql_harness::Path(core_file_name).exists())
        << core_file_name;
  }

  SCOPED_TRACE("// console output has stacktrace");
#ifdef HAVE_EXT_BACKTRACE
  EXPECT_THAT(r.get_full_output(), ::testing::HasSubstr("signal_handler.cc"));
#else
  EXPECT_THAT(r.get_full_output(), ::testing::HasSubstr("my_print_stacktrace"));
#endif
}

#endif  // !defined(HAVE_ASAN) && !defined(HAVE_UBSAN)

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
