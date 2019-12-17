/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifdef USE_STD_REGEX
#include <regex>
#else
#include <regex.h>
#endif

#include <gtest/gtest.h>  // FAIL

using mysql_harness::Path;
using mysql_harness::ProcessLauncher;
Path ProcessManager::origin_dir_;
Path ProcessManager::data_dir_;
Path ProcessManager::plugin_dir_;
Path ProcessManager::mysqlrouter_exec_;
Path ProcessManager::mysqlserver_mock_exec_;

ProcessWrapper &ProcessManager::launch_command(
    const std::string &command, const std::vector<std::string> &params,
    int expected_exit_code, bool catch_stderr) {
  const char *params_arr[kMaxLaunchedProcessParams];
  get_params(command, params, params_arr);

  if (command.empty())
    throw std::logic_error("path to launchable executable must not be empty");

  ProcessWrapper process(command, params_arr, catch_stderr);

  processes_.emplace_back(std::move(process), expected_exit_code);

  return std::get<0>(processes_.back());
}

static std::vector<std::string> build_exec_args(
    const std::string &mysqlrouter_exec, bool with_sudo) {
  std::vector<std::string> args;

  if (with_sudo) {
    args.emplace_back("sudo");
    args.emplace_back("--non-interactive");
  }

  if (getenv("WITH_VALGRIND")) {
    args.emplace_back("valgrind");
    args.emplace_back("--error-exitcode=1");
    args.emplace_back("--quiet");
  }

  args.emplace_back(mysqlrouter_exec);

  return args;
}

ProcessWrapper &ProcessManager::launch_router(
    const std::vector<std::string> &params, int expected_exit_code,
    bool catch_stderr, bool with_sudo) {
  std::vector<std::string> args =
      build_exec_args(mysqlrouter_exec_.str(), with_sudo);

  // 1st argument is special - it needs to be passed as "command" to
  // launch_router()
  std::string cmd = args.at(0);
  args.erase(args.begin());
  std::copy(params.begin(), params.end(), std::back_inserter(args));

  auto &router = launch_command(cmd, args, expected_exit_code, catch_stderr);
  router.logging_dir_ = logging_dir_.name();
  router.logging_file_ = "mysqlrouter.log";

  return router;
}

ProcessWrapper &ProcessManager::launch_mysql_server_mock(
    const std::string &json_file, unsigned port, int expected_exit_code,
    bool debug_mode, uint16_t http_port, uint16_t x_port,
    const std::string &module_prefix /* = "" */
    ,
    const std::string &bind_address /*= "127.0.0.1"*/) {
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
                        expected_exit_code, true);
}

void ProcessManager::get_params(
    const std::string &command, const std::vector<std::string> &params_vec,
    const char *out_params[kMaxLaunchedProcessParams]) const {
  out_params[0] = command.c_str();

  size_t i = 1;
  for (const auto &par : params_vec) {
    if (i >= kMaxLaunchedProcessParams - 1) {
      throw std::runtime_error("Too many parameters passed to the MySQLRouter");
    }
    out_params[i++] = par.c_str();
  }
  out_params[i] = nullptr;
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
    const std::string &name, const std::string &extra_defaults) const {
  Path file_path = Path(directory).join(name);
  std::ofstream ofs_config(file_path.str());

  if (!ofs_config.good()) {
    throw(
        std::runtime_error("Could not create config file " + file_path.str()));
  }

  ofs_config << make_DEFAULT_section(default_section);
  ofs_config << extra_defaults << std::endl;
  ofs_config << sections << std::endl;
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
  ofs_config.close();

  return file_path.str();
}

void ProcessManager::shutdown_all() {
  // stop them all
  for (auto &proc : processes_) {
    std::get<0>(proc).send_shutdown_event();
  }
}

void ProcessManager::ensure_clean_exit() {
  for (auto &proc : processes_) {
    check_exit_code(std::get<0>(proc), std::get<1>(proc));
  }
}

void ProcessManager::check_exit_code(ProcessWrapper &process,
                                     int expected_exit_code,
                                     std::chrono::milliseconds timeout) {
  try {
    ASSERT_EQ(expected_exit_code, process.wait_for_exit(timeout))
        << process.get_command_line() << "\n"
        << "output: " << process.get_full_output() << "\n"
        << "log: " << process.get_full_logfile() << "\n";
  } catch (const std::exception &e) {
    FAIL() << process.get_command_line() << "\n"
           << e.what() << "\n"
           << "output: " << process.get_full_output() << "\n"
           << "log: " << process.get_full_logfile() << "\n";
  }
}

void ProcessManager::check_port(bool should_be_ready, ProcessWrapper &process,
                                uint16_t port,
                                std::chrono::milliseconds timeout,
                                const std::string &hostname) {
  bool ready = wait_for_port_ready(port, timeout, hostname);

  // let's collect some more info
  std::string netstat_info;
  if (ready != should_be_ready) {
    auto &netstat = launch_command("netstat", {});
    check_exit_code(netstat);
    netstat_info = netstat.get_full_output();
  }

  ASSERT_EQ(ready, should_be_ready) << process.get_full_output() << "\n"
                                    << process.get_full_logfile() << "\n"
                                    << "port: " << std::to_string(port) << "\n"
                                    << "netstat output: " << netstat_info;
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
