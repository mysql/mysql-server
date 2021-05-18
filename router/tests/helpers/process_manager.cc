/*
  Copyright (c) 2019, 2021, Oracle and/or its affiliates.

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

#include "process_manager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <thread>

#ifndef _WIN32
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#else
#define USE_STD_REGEX
#include <WinSock2.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>
#endif

#include <fcntl.h>

#include "dim.h"
#include "mysql/harness/net_ts/buffer.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/local.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/string_view.h"
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "mysqlrouter/utils.h"
#include "process_launcher.h"
#include "random_generator.h"

#ifdef USE_STD_REGEX
#include <regex>
#else
#include <regex.h>
#endif

#include <gtest/gtest.h>  // FAIL

#define EXPECT_NO_ERROR(x) \
  EXPECT_THAT((x), ::testing::Truly([](auto const &v) { return bool(v); }))

#define ASSERT_NO_ERROR(x) \
  ASSERT_THAT((x), ::testing::Truly([](auto const &v) { return bool(v); }))

using mysql_harness::Path;
using mysql_harness::ProcessLauncher;
Path ProcessManager::origin_dir_;
Path ProcessManager::data_dir_;
Path ProcessManager::plugin_dir_;
Path ProcessManager::mysqlrouter_exec_;
Path ProcessManager::mysqlserver_mock_exec_;

using namespace std::chrono_literals;

#ifdef _WIN32

template <class Clock>
stdx::expected<ProcessManager::notify_socket_t, std::error_code> accept_until(
    ProcessManager::wait_socket_t &sock,
    typename Clock::time_point const &end_time) {
  using clock_type = Clock;
  do {
    auto accept_res = sock.accept();
    if (!accept_res) {
      const auto ec = accept_res.error();
      const std::error_code ec_pipe_listening{ERROR_PIPE_LISTENING,  // 536
                                              std::system_category()};

      if (ec != ec_pipe_listening) {
        return accept_res.get_unexpected();
      }

      // nothing is connected yet, sleep a bit an retry.

      std::this_thread::sleep_for(100ms);
    } else {
      return accept_res;
    }
  } while (clock_type::now() < end_time);

  return stdx::make_unexpected(make_error_code(std::errc::timed_out));
}

stdx::expected<void, std::error_code> ProcessManager::wait_for_notified(
    wait_socket_t &sock, const std::string &expected_notification,
    std::chrono::milliseconds timeout) {
  using clock_type = std::chrono::system_clock;
  const auto start_time = clock_type::now();
  const auto end_time = start_time + timeout;

  sock.native_non_blocking(true);

  auto accept_res = accept_until<clock_type>(sock, end_time);
  if (!accept_res) {
    return accept_res.get_unexpected();
  }

  auto accepted = std::move(accept_res.value());

  // make the read non-blocking.
  const auto non_block_res = accepted.native_non_blocking(true);
  if (!non_block_res) {
    return non_block_res.get_unexpected();
  }

  const size_t BUFF_SIZE = 512;
  std::array<char, BUFF_SIZE> buff;

  do {
    const auto read_res =
        net::read(accepted, net::buffer(buff), net::transfer_at_least(1));
    if (!read_res) {
      if (read_res.error() !=
          std::error_code{ERROR_NO_DATA, std::system_category()}) {
        return read_res.get_unexpected();
      }

      // there was no data. Wait a bit and try again.
      std::this_thread::sleep_for(10ms);
    } else {
      const auto bytes_read = read_res.value();

      if (bytes_read >= expected_notification.size()) {
        if (stdx::string_view(expected_notification) ==
            stdx::string_view(buff.data(), expected_notification.size())) {
          return {};
        }
      } else {
        // too short
        std::cerr << __LINE__ << ": too short" << std::endl;
      }
    }

    // either not matched, or no data yet.
  } while (clock_type::now() < end_time);

  return stdx::make_unexpected(make_error_code(std::errc::timed_out));
}

#else
stdx::expected<void, std::error_code> ProcessManager::wait_for_notified(
    wait_socket_t &sock, const std::string &expected_notification,
    std::chrono::milliseconds timeout) {
  const size_t BUFF_SIZE = 512;
  std::array<char, BUFF_SIZE> buff;

  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  while (true) {
    std::array<pollfd, 1> fds = {{{sock.native_handle(), POLLIN, 0}}};
    const auto poll_res =
        net::impl::poll::poll(fds.data(), fds.size(), timeout);
    if (!poll_res) {
      return poll_res.get_unexpected();
    }

    const auto read_res =
        net::read(sock, net::buffer(buff), net::transfer_at_least(1));
    if (!read_res) {
      return read_res.get_unexpected();
    } else {
      const auto bytes_read = read_res.value();

      if (bytes_read >= expected_notification.size()) {
        if (stdx::string_view(expected_notification) ==
            stdx::string_view(buff.data(), expected_notification.size())) {
          return {};
        }
      } else {
        // too short
        std::cerr << __LINE__ << ": too short" << std::endl;
      }
    }
  }
}

#endif

stdx::expected<void, std::error_code> ProcessManager::wait_for_notified_ready(
    wait_socket_t &sock, std::chrono::milliseconds timeout) {
  return wait_for_notified(sock, "READY=1", timeout);
}

stdx::expected<void, std::error_code>
ProcessManager::wait_for_notified_stopping(wait_socket_t &sock,
                                           std::chrono::milliseconds timeout) {
  return wait_for_notified(
      sock, "STOPPING=1\nSTATUS=Router shutdown in progress\n", timeout);
}

static std::string generate_notify_socket_path(const std::string &tmp_dir) {
  const std::string unique_id =
      mysql_harness::RandomGenerator().generate_identifier(
          12, mysql_harness::RandomGenerator::AlphabetLowercase);

#ifdef _WIN32
  (void)tmp_dir;
  return std::string("\\\\.\\pipe\\") + unique_id;
#else
  Path result(tmp_dir);
  result.append(unique_id);

  return result.str();
#endif
}

ProcessWrapper &ProcessManager::launch_command(
    const std::string &command, const std::vector<std::string> &params,
    int expected_exit_code, bool catch_stderr,
    std::vector<std::pair<std::string, std::string>> env_vars) {
  ProcessWrapper process(command, params, env_vars, catch_stderr);

  processes_.emplace_back(std::move(process), expected_exit_code);

  return std::get<0>(processes_.back());
}

ProcessWrapper &ProcessManager::launch_command(
    const std::string &command, const std::vector<std::string> &params,
    int expected_exit_code, bool catch_stderr,
    std::chrono::milliseconds wait_notified_ready) {
  if (command.empty())
    throw std::logic_error("path to launchable executable must not be empty");

  std::vector<std::pair<std::string, std::string>> env_vars;

  net::io_context io_ctx;

  ProcessManager::wait_socket_t notify_socket{io_ctx};

  if (wait_notified_ready >= 0ms) {
    const std::string socket_node =
        generate_notify_socket_path(get_test_temp_dir_name());

    EXPECT_NO_ERROR(notify_socket.open());
    EXPECT_NO_ERROR(notify_socket.bind({socket_node}));

    env_vars.emplace_back("NOTIFY_SOCKET", socket_node);
  }

  auto &result = launch_command(command, params, expected_exit_code,
                                catch_stderr, env_vars);

  if (wait_notified_ready >= 0ms) {
    EXPECT_TRUE(wait_for_notified_ready(notify_socket, wait_notified_ready));
  }

  return result;
}

static std::vector<std::string> build_exec_args(
    const std::string &mysqlrouter_exec, bool with_sudo) {
  std::vector<std::string> args;

  if (with_sudo) {
    args.emplace_back("sudo");
    args.emplace_back("--non-interactive");
  }

  if (getenv("WITH_VALGRIND")) {
    const auto valgrind_exe = getenv("VALGRIND_EXE");
    args.emplace_back(valgrind_exe ? valgrind_exe : "valgrind");
    args.emplace_back("--error-exitcode=77");
    args.emplace_back("--quiet");
#if 0
    args.emplace_back("--leak-check=full");
    args.emplace_back("--show-leak-kinds=all");
    // when debugging mem-leaks reported by ASAN, it can help to use valgrind
    // instead and enable these options.
    args.emplace_back("--errors-for-leak-kinds=all");
#endif
  }

  args.emplace_back(mysqlrouter_exec);

  return args;
}

ProcessWrapper &ProcessManager::launch_router(
    const std::vector<std::string> &params, int expected_exit_code /*= 0*/,
    bool catch_stderr /*= true*/, bool with_sudo /*= false*/,
    std::chrono::milliseconds wait_for_notify_ready /*= 5s*/) {
  std::vector<std::string> args =
      build_exec_args(mysqlrouter_exec_.str(), with_sudo);

  // 1st argument is special - it needs to be passed as "command" to
  // launch_command()
  std::string cmd = args.at(0);
  args.erase(args.begin());
  std::copy(params.begin(), params.end(), std::back_inserter(args));

  auto &router = launch_command(cmd, args, expected_exit_code, catch_stderr,
                                wait_for_notify_ready);
  router.logging_dir_ = logging_dir_.name();
  router.logging_file_ = "mysqlrouter.log";

  return router;
}

std::vector<std::string> ProcessManager::mysql_server_mock_cmdline_args(
    const std::string &json_file, uint16_t port, uint16_t http_port,
    uint16_t x_port, const std::string &module_prefix /* = "" */,
    const std::string &bind_address /*= "0.0.0.0"*/) {
  std::vector<std::string> server_params{
      "--filename",     json_file,             //
      "--port",         std::to_string(port),  //
      "--bind-address", bind_address,
  };

  server_params.emplace_back("--module-prefix");
  if (module_prefix.empty()) {
    server_params.emplace_back(get_data_dir().str());
  } else {
    server_params.emplace_back(module_prefix);
  }

  if (http_port > 0) {
    server_params.emplace_back("--http-port");
    server_params.emplace_back(std::to_string(http_port));
  }

  if (x_port > 0) {
    server_params.emplace_back("--xport");
    server_params.emplace_back(std::to_string(x_port));
  }

  return server_params;
}

ProcessWrapper &ProcessManager::launch_mysql_server_mock(
    const std::vector<std::string> &server_params, int expected_exit_code,
    std::chrono::milliseconds wait_for_notify_ready /*= 5s*/) {
  return launch_command(mysqlserver_mock_exec_.str(), server_params,
                        expected_exit_code, true, wait_for_notify_ready);
}

ProcessWrapper &ProcessManager::launch_mysql_server_mock(
    const std::string &json_file, unsigned port, int expected_exit_code,
    bool debug_mode, uint16_t http_port, uint16_t x_port,
    const std::string &module_prefix /* = "" */,
    const std::string &bind_address /*= "127.0.0.1"*/,
    std::chrono::milliseconds wait_for_notify_ready /*= 5s*/) {
  if (mysqlserver_mock_exec_.str().empty())
    throw std::logic_error("path to mysql-server-mock must not be empty");

  auto server_params = mysql_server_mock_cmdline_args(
      json_file, port, http_port, x_port, module_prefix, bind_address);

  if (debug_mode) {
    server_params.emplace_back("--verbose");
  }

  return launch_mysql_server_mock(server_params, expected_exit_code,
                                  wait_for_notify_ready);
}

std::map<std::string, std::string> ProcessManager::get_DEFAULT_defaults()
    const {
  return {
      {"logging_folder", logging_dir_.name()},
      {"plugin_folder", plugin_dir_.str()},
      {"runtime_folder", origin_dir_.str()},
      {"config_folder", origin_dir_.str()},
      {"data_folder", origin_dir_.str()},
  };
}

std::string ProcessManager::make_DEFAULT_section(
    const std::map<std::string, std::string> *params) const {
  auto l = [params](const char *key) -> std::string {
    return (params->count(key))
               ? std::string(key) + " = " + params->at(key) + "\n"
               : "";
  };

  return params ? std::string("[DEFAULT]\n") + l("logging_folder") +
                      l("plugin_folder") + l("runtime_folder") +
                      l("config_folder") + l("data_folder") +
                      l("keyring_path") + l("master_key_path") +
                      l("master_key_reader") + l("master_key_writer") +
                      l("dynamic_state") + l("pid_file") + "\n"
                : std::string("[DEFAULT]\n") +
                      "logging_folder = " + logging_dir_.name() + "\n" +
                      "plugin_folder = " + plugin_dir_.str() + "\n" +
                      "runtime_folder = " + origin_dir_.str() + "\n" +
                      "config_folder = " + origin_dir_.str() + "\n" +
                      "data_folder = " + origin_dir_.str() + "\n\n";
}

std::string ProcessManager::create_config_file(
    const std::string &directory, const std::string &sections,
    const std::map<std::string, std::string> *default_section,
    const std::string &name, const std::string &extra_defaults,
    bool enable_debug_logging) const {
  Path file_path = Path(directory).join(name);
  std::ofstream ofs_config(file_path.str());

  if (!ofs_config.good()) {
    throw(
        std::runtime_error("Could not create config file " + file_path.str()));
  }

  ofs_config << make_DEFAULT_section(default_section);
  ofs_config << extra_defaults << std::endl;
  ofs_config << sections << std::endl;
  if (enable_debug_logging) {
    ofs_config
        << "[logger]\nlevel = DEBUG\ntimestamp_precision=millisecond\n\n";
  }
  ofs_config.close();

  return file_path.str();
}

std::string ProcessManager::create_state_file(const std::string &dir_name,
                                              const std::string &content) {
  Path file_path = Path(dir_name).join("state.json");
  std::ofstream ofs_config(file_path.str());

  if (!ofs_config.good()) {
    throw(std::runtime_error("Could not create state file " + file_path.str()));
  }

  ofs_config << content;
  ofs_config.flush();
  ofs_config.close();

  return file_path.str();
}

void ProcessManager::shutdown_all() {
  // stop all the processes
  for (auto &proc : processes_) {
    std::get<0>(proc).send_shutdown_event();
  }
}

void ProcessManager::dump_all() {
  std::stringstream ss;
  for (auto &proc : processes_) {
    ss << "# Process: \n"
       << std::get<0>(proc).get_command_line() << "\n"
       << "PID:\n"
       << std::get<0>(proc).get_pid() << "\n"
       << "Console output:\n"
       << std::get<0>(proc).get_current_output() + "\n"
       << "Log content:\n"
       << std::get<0>(proc).get_full_logfile() + "\n";
  }

  FAIL() << ss.str();
}

void ProcessManager::ensure_clean_exit() {
  for (auto &proc : processes_) {
    try {
      check_exit_code(std::get<0>(proc), std::get<1>(proc));
    } catch (const std::exception &) {
      FAIL() << "PID: " << std::get<0>(proc).get_pid()
             << " didn't exit as expected";
    }
  }
}

void ProcessManager::check_exit_code(ProcessWrapper &process,
                                     int expected_exit_code,
                                     std::chrono::milliseconds timeout) {
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  int result{0};
  try {
    result = process.wait_for_exit(timeout);
  } catch (const std::exception &e) {
    FAIL() << "Process wait for exit failed. " << e.what();
  }

  ASSERT_EQ(expected_exit_code, result);
}

void ProcessManager::check_port(bool should_be_ready, ProcessWrapper &process,
                                uint16_t port,
                                std::chrono::milliseconds timeout,
                                const std::string &hostname) {
  bool ready = wait_for_port_ready(port, timeout, hostname);

// That creates a lot of noise in the logs so gets disabled for now.
#if 0
  // let's collect some more info
  std::string netstat_info;
  if (ready != should_be_ready) {
    auto &netstat = launch_command("netstat", {});
    check_exit_code(netstat);
    netstat_info = netstat.get_full_output();
  }
#endif

  ASSERT_EQ(ready, should_be_ready) << process.get_full_output() << "\n"
                                    << process.get_full_logfile() << "\n"
                                    << "port: " << std::to_string(port) << "\n"
#if 0
                                    << "netstat output: " << netstat_info
#endif
      ;
}

void ProcessManager::check_port_ready(ProcessWrapper &process, uint16_t port,
                                      std::chrono::milliseconds timeout,
                                      const std::string &hostname) {
  check_port(true, process, port, timeout, hostname);
}

void ProcessManager::check_port_not_ready(ProcessWrapper &process,
                                          uint16_t port,
                                          std::chrono::milliseconds timeout,
                                          const std::string &hostname) {
  check_port(false, process, port, timeout, hostname);
}

void ProcessManager::set_origin(const Path &dir) {
  using mysql_harness::Path;

  origin_dir_ = dir;
  if (origin_dir_.str().empty()) {
    throw std::runtime_error("Origin dir not set");
  }
  plugin_dir_ = mysql_harness::get_plugin_dir(origin_dir_.str());

  auto get_exe_path = [&](const std::string &name) -> Path {
    Path path(origin_dir_);
#ifdef _WIN32
    path.append(name + ".exe");
#else
    path.append(name);
#endif
    return path.real_path();
  };

  mysqlrouter_exec_ = get_exe_path("mysqlrouter");
  mysqlserver_mock_exec_ = get_exe_path("mysql_server_mock");

  data_dir_ = COMPONENT_TEST_DATA_DIR;
}
