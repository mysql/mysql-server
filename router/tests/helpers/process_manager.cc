/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates.

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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
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
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "mysqlrouter/utils.h"
#include "process_launcher.h"
#include "random_generator.h"
#include "socket_operations.h"

#ifdef USE_STD_REGEX
#include <regex>
#else
#include <regex.h>
#endif

#include <gtest/gtest.h>  // FAIL

using mysql_harness::Path;
using mysql_harness::ProcessLauncher;
using mysql_harness::socket_t;
Path ProcessManager::origin_dir_;
Path ProcessManager::data_dir_;
Path ProcessManager::plugin_dir_;
Path ProcessManager::mysqlrouter_exec_;
Path ProcessManager::mysqlserver_mock_exec_;

using namespace std::chrono_literals;

#ifdef _WIN32

notify_socket_t ProcessManager::create_notify_socket(const std::string &name) {
  return CreateNamedPipe(TEXT(name.c_str()), PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
                         PIPE_UNLIMITED_INSTANCES, 1024 * 16, 1024 * 16,
                         NMPWAIT_USE_DEFAULT_WAIT, NULL);
}

void ProcessManager::close_notify_socket(notify_socket_t socket) {
  if (socket != INVALID_HANDLE_VALUE) CloseHandle(socket);
}

bool ProcessManager::wait_for_notified(notify_socket_t sock,
                                       const std::string &expected_notification,
                                       std::chrono::milliseconds timeout) {
  DWORD len{0};
  const size_t BUFF_SIZE = 512;
  std::array<char, BUFF_SIZE> buff;

  if (!ConnectNamedPipe(sock, NULL)) {
    if ((GetLastError() != ERROR_PIPE_LISTENING) &&
        (GetLastError() != ERROR_NO_DATA)) {
      return false;
    }
  }

  std::shared_ptr<void> notify_socket_close_guard(
      nullptr, [&](void *) { DisconnectNamedPipe(sock); });

  const auto start_time = std::chrono::system_clock::now();
  while (true) {
    DWORD numRead = 1;
    if (!ReadFile(sock, &buff.front(), BUFF_SIZE, &len, NULL)) {
      if ((GetLastError() != ERROR_PIPE_LISTENING) &&
          (GetLastError() != ERROR_NO_DATA)) {
        return false;
      }
    }
    if ((len > 0) && (strncmp(expected_notification.c_str(), buff.data(),
                              static_cast<size_t>(len)) == 0)) {
      return true;
    } else {
      const auto current_time = std::chrono::system_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              current_time - start_time) >= timeout) {
        return false;
      }
    }
  }

  return false;
}

#else
notify_socket_t ProcessManager::create_notify_socket(
    const std::string &name, int type /*= SOCK_DGRAM */) {
  struct sockaddr_un sock_unix;

  auto *sock_ops = mysql_harness::SocketOperations::instance();

  const auto socket_res = sock_ops->socket(AF_UNIX, type, 0);
  if (!socket_res) {
    throw std::system_error(socket_res.error(), "socket() failed");
  }

  sock_unix.sun_family = AF_UNIX;
  std::strncpy(sock_unix.sun_path, name.c_str(), name.size() + 1);

  const auto bind_res =
      sock_ops->bind(socket_res.value(), (struct sockaddr *)&sock_unix,
                     static_cast<socklen_t>(sizeof(sock_unix)));
  if (!bind_res && !name.empty()) {
    throw std::system_error(bind_res.error(), "bind() failed");
  }

  return socket_res.value();
}

void ProcessManager::close_notify_socket(notify_socket_t socket) {
  auto *sock_ops = mysql_harness::SocketOperations::instance();
  if (socket != mysql_harness::kInvalidSocket) {
    sock_ops->close(socket);
  }
}

bool ProcessManager::wait_for_notified(notify_socket_t sock,
                                       const std::string &expected_notification,
                                       std::chrono::milliseconds timeout) {
  const size_t BUFF_SIZE = 512;
  std::array<char, BUFF_SIZE> buff;
  auto *sock_ops = mysql_harness::SocketOperations::instance();
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  while (true) {
    const auto has_data_result = sock_ops->has_data(sock, timeout);
    if (!has_data_result) break;
    if (!has_data_result.value()) break;

    const auto read_res = sock_ops->read(sock, buff.data(), buff.size());
    if (read_res && (strncmp(expected_notification.c_str(), buff.data(),
                             read_res.value()) == 0)) {
      return true;
    }
  }

  return false;
}

#endif

bool ProcessManager::wait_for_notified_ready(
    notify_socket_t sock, std::chrono::milliseconds timeout) {
  return wait_for_notified(sock, "READY=1", timeout);
}

bool ProcessManager::wait_for_notified_stopping(
    notify_socket_t sock, std::chrono::milliseconds timeout) {
  return wait_for_notified(
      sock, "STOPPING=1\nSTATUS=Router shutdown in progress\n", timeout);
}

static std::string generate_notify_socket_path(const std::string &tmp_dir) {
  const std::string unique_id =
      mysql_harness::RandomGenerator().generate_identifier(
          12, mysql_harness::RandomGenerator::AlphabetLowercase);

#ifdef _WIN32
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

#ifdef _WIN32
  HANDLE notify_socket{INVALID_HANDLE_VALUE};
#else
  socket_t notify_socket{mysql_harness::kInvalidSocket};
#endif
  std::shared_ptr<void> notify_socket_close_guard(
      nullptr, [&](void *) { close_notify_socket(notify_socket); });

  if (wait_notified_ready >= 0ms) {
    const std::string socket_node =
        generate_notify_socket_path(get_test_temp_dir_name());
    notify_socket = create_notify_socket(socket_node);
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

ProcessWrapper &ProcessManager::launch_mysql_server_mock(
    const std::string &json_file, unsigned port, int expected_exit_code,
    bool debug_mode, uint16_t http_port, uint16_t x_port,
    const std::string &module_prefix /* = "" */,
    const std::string &bind_address /*= "127.0.0.1"*/,
    std::chrono::milliseconds wait_for_notify_ready /*= 5s*/) {
  if (mysqlserver_mock_exec_.str().empty())
    throw std::logic_error("path to mysql-server-mock must not be empty");

  std::vector<std::string> server_params(
      {"--filename=" + json_file, "--port=" + std::to_string(port),
       "--bind-address=" + bind_address,
       "--http-port=" + std::to_string(http_port),
       "--module-prefix=" +
           (!module_prefix.empty() ? module_prefix : get_data_dir().str())});

  if (debug_mode) {
    server_params.emplace_back("--verbose");
  }

  if (x_port > 0) {
    server_params.emplace_back("--xport=" + std::to_string(x_port));
  }

  return launch_command(mysqlserver_mock_exec_.str(), server_params,
                        expected_exit_code, true, wait_for_notify_ready);
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
    } catch (const std::exception &e) {
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
