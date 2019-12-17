/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _PROCESS_MANAGER_H_
#define _PROCESS_MANAGER_H_

#include "process_wrapper.h"

#include <gmock/gmock.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "router_test_helpers.h"
#include "temp_dir.h"

using mysql_harness::Path;

/** @brief maximum number of parameters that can be passed to the launched
 * process */
constexpr size_t kMaxLaunchedProcessParams{30};

/** @class ProcessManager
 *
 * Manages collecion of the processes
 * Enables creating, shutting down etc.
 *
 **/
class ProcessManager {
 public:
  /**
   * set origin path.
   */
  static void set_origin(const Path &dir);

 protected:
  virtual ~ProcessManager() {}

  /**
   * shutdown all managed processes.
   */
  void shutdown_all();

  /**
   * ensures all processes exited and checks for crashes.
   */
  void ensure_clean_exit();

  /**
   * ensures given process exited with expected return value and checks for
   * crashes.
   */
  void check_exit_code(
      ProcessWrapper &process, int expected_exit_code = EXIT_SUCCESS,
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout);

  /**
   * ensures given port is ready for accepting connections, prints some debug
   * data otherwise.
   *
   * @param process       process that should be listening on that port
   * @param port          TCP port number to check
   * @param timeout       maximum timeout to wait for the port
   * @param hostname      name/IP address of the network host to check
   */
  void check_port_ready(
      ProcessWrapper &process, uint16_t port,
      std::chrono::milliseconds timeout = kDefaultPortReadyTimeout,
      const std::string &hostname = "127.0.0.1");

  /**
   * ensures given port is NOT ready for accepting connections, prints some
   * debug data otherwise.
   *
   * @param process       process that should be listening on that port
   * @param port          TCP port number to check
   * @param timeout       maximum timeout to wait for the port
   * @param hostname      name/IP address of the network host to check
   */
  void check_port_not_ready(
      ProcessWrapper &process, uint16_t port,
      std::chrono::milliseconds timeout = kDefaultPortReadyTimeout,
      const std::string &hostname = "127.0.0.1");

  /** @brief Launches the MySQLRouter process.
   *
   * @param   params vector<string> containing command line parameters to pass
   * to process
   * @param expected_exit_code expected exit-code for ensure_clean_exit()
   * @param   catch_stderr bool flag indicating if the process' error output
   * stream should be included in the output caught from the process
   * @param   with_sudo    bool flag indicating if the process' should be
   * execute with sudo priviledges
   *
   * @returns handle to the launched proccess
   */
  ProcessWrapper &launch_router(const std::vector<std::string> &params,
                                int expected_exit_code = 0,
                                bool catch_stderr = true,
                                bool with_sudo = false);

  /** @brief Launches the MySQLServerMock process.
   *
   * @param json_file  path to the json file containing expected queries
   * definitions
   * @param   port       number of the port where the mock server will accept
   * the client connections
   * @param expected_exit_code expected exit-code for ensure_clean_exit()
   * @param debug_mode if true all the queries and result get printed on the
   *                     standard output
   * @param http_port  port number where the http_server module of the mock
   * server will accept REST client requests
   * @param x_port  port number where the mock server will accept x client
   *                  connections
   * @param module_prefix base-path for javascript modules used by the tests
   * @param bind_address listen address for the mock server to bind to
   *
   * @returns handle to the launched proccess
   */
  ProcessWrapper &launch_mysql_server_mock(
      const std::string &json_file, unsigned port, int expected_exit_code = 0,
      bool debug_mode = false, uint16_t http_port = 0, uint16_t x_port = 0,
      const std::string &module_prefix = "",
      const std::string &bind_address = "0.0.0.0");

  /** @brief Launches a process.
   *
   * @param command       path to executable
   * @param params        array of commanline parameters to pass to the
   * executable
   * @param catch_stderr  if true stderr will also be captured (combined with
   * stdout)
   *
   * @returns handle to the launched proccess
   */
  ProcessWrapper &launch_command(const std::string &command,
                                 const std::vector<std::string> &params,
                                 int expected_exit_code = 0,
                                 bool catch_stderr = true);

  /** @brief Gets path to the directory containing testing data
   *         (conf files, json files).
   */
  const Path &get_data_dir() const { return data_dir_; }

  /** @brief Gets path to the directory used as log output directory
   */
  Path get_logging_dir() const { return Path(logging_dir_.name()); }

  /** @brief returns a map with default [DEFAULT] section parameters
   *
   * @return default parameters for [DEFAULT] section
   */
  std::map<std::string, std::string> get_DEFAULT_defaults() const;

  /** @brief create config file
   *
   * @param directory directory in which the config file will be created
   * @param sections text to follow [DEFAULT] section (typically all other
   * sections)
   * @param default_section [DEFAULT] section parameters
   * @param name config file name
   * @param extra_defaults addional parameters to add to [DEFAULT]
   *
   * @return path to the created file
   */
  std::string create_config_file(
      const std::string &directory, const std::string &sections = "",
      const std::map<std::string, std::string> *default_section = nullptr,
      const std::string &name = "mysqlrouter.conf",
      const std::string &extra_defaults = "") const;

  // returns full path to the file
  std::string create_state_file(const std::string &dir_name,
                                const std::string &content);

  static const Path &get_origin() { return origin_dir_; }

  const Path &get_mysqlrouter_exec() const { return mysqlrouter_exec_; }

  /**
   * get Path to mysql_server_mock inside the build-dir.
   *
   * valid after SetUp() got called.
   */
  const Path &get_mysqlserver_mock_exec() const {
    return mysqlserver_mock_exec_;
  }

  void set_mysqlrouter_exec(const Path &path) { mysqlrouter_exec_ = path; }

 protected:
  /** @brief returns a [DEFAULT] section as string
   *
   * @param params map of [DEFAULT] section parameters
   * @returns [DEFAULT] section text
   */
  std::string make_DEFAULT_section(
      const std::map<std::string, std::string> *params) const;

 private:
  void get_params(const std::string &command,
                  const std::vector<std::string> &params_vec,
                  const char *out_params[kMaxLaunchedProcessParams]) const;

  void check_port(bool should_be_ready, ProcessWrapper &process, uint16_t port,
                  std::chrono::milliseconds timeout,
                  const std::string &hostname);

  static Path origin_dir_;
  static Path data_dir_;
  static Path plugin_dir_;
  static Path mysqlrouter_exec_;
  static Path mysqlserver_mock_exec_;

  TempDirectory logging_dir_;

  std::list<std::tuple<ProcessWrapper, int>> processes_;
};

#endif  // _PROCESS_MANAGER_H_
