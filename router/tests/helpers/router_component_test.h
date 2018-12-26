/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _ROUTER_COMPONENT_TEST_H_
#define _ROUTER_COMPONENT_TEST_H_

#include "process_launcher.h"
#include "router_test_helpers.h"

#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <streambuf>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#else
#define NOMINMAX
#endif

using mysql_harness::Path;

/** @brief maximum number of parameters that can be passed to the launched
 * process */
const size_t MAX_PARAMS{30};

/** @class RouterComponentTest
 *
 * Base class for the MySQLRouter component-like tests.
 * Enables creating processes, intercepting their output, writing to input, etc.
 *
 **/
class RouterComponentTest {
  // test performance tweaks
  // shorter timeout -> faster test execution, longer timeout -> increased test
  // stability
  static constexpr unsigned kDefaultExpectOutputTimeout = 1000;

  static constexpr size_t kReadBufSize = 1024;

 public:
  // wait-timeout should be less than infinite, and long enough that even with
  // valgrind we properly pass the tests
  static constexpr unsigned kDefaultWaitForExitTimeout = 10 * 1000;

 protected:
  RouterComponentTest();
  virtual ~RouterComponentTest();

  static void rewrite_js_to_tracefile(
      const std::string &infile_name, const std::string &outfile_name,
      const std::map<std::string, std::string> &env_vars);

  /** @class CommandHandle
   *
   * Object of this class gets return from launch_* method and can be
   * use to manipulate launched process (get the output, exit code,
   * inject input, etc.)
   *
   **/
  class CommandHandle {
   public:
    /** @brief Checks if the process wrote the specified string to its output.
     *
     * This function loops read()ing child process output, until either the
     * expected output appears, or until timeout is reached. While reading, it
     * also calls autoresponder to react to any prompts issued by the child
     * process.
     *
     * @param str         Expected output string
     * @param regex       True if str is a regex pattern
     * @param timeout_ms  Timeout in milliseconds, to wait for the output
     * @return Returns bool flag indicating if the specified string appeared
     *                 in the process' output.
     */
    bool expect_output(const std::string &str, bool regex = false,
                       unsigned timeout_ms = kDefaultExpectOutputTimeout);

    /** @brief Returns the full output that was produced the process till moment
     *         of calling this method.
     *  TODO: this description does not match what the code does, this needs to
     * be fixed.
     */
    std::string get_full_output() {
      while (read_and_autorespond_to_output(0)) {
      }
      return execute_output_raw_;
    }

    /**
     * get the current output of the process.
     *
     * doesn't check if there is new content.
     */
    std::string get_current_output() const { return execute_output_raw_; }

    /** @brief Register the response that should be written to the process'
     * input descriptor when the given string appears on it output while
     * executing expect_output().
     *
     * @param query     string that should trigger writing the response
     * @param response  string that should get written
     */
    void register_response(const std::string &query,
                           const std::string &response) {
      output_responses_[query] = response;
    }

    /** @brief Returns the exit code of the process.
     *
     *  Must always be called after wait_for_exit(),
     *  otherwise it throws runtime_error
     *
     * @returns exit code of the process
     */
    int exit_code() {
      if (!exit_code_set_) {
        throw std::runtime_error(
            "RouterComponentTest::Command_handle: exit_code() called without "
            "wait_for_exit()!");
      }
      return exit_code_;
    }

    /** @brief Waits for the process to exit, while reading its output and
     * autoresponding to prompts
     *
     *  If the process did not finish yet, it waits the given number of
     * milliseconds. If the timeout expired, it throws runtime_error. In case of
     * failure, it throws system_error.
     *
     * @param timeout_ms maximum amount of time to wait for the process to
     * finish
     * @throws std::runtime_error on timeout, std::system_error on failure
     * @returns exit code of the process
     */
    int wait_for_exit(unsigned timeout_ms = kDefaultWaitForExitTimeout) {
      // wait_for_exit() is a convenient short name, but a little unclear with
      // respect to what this function actually does
      return wait_for_exit_while_reading_and_autoresponding_to_output(
          timeout_ms);
    }

    /** @brief Returns process PID
     *
     * @returns PID of the process
     */
    uint64_t get_pid() const { return launcher_.get_pid(); }

    int kill() {
      try {
        return launcher_.kill();
      } catch (std::exception &e) {
        fprintf(stderr, "failed killing process %s: %s\n",
                launcher_.get_cmd_line().c_str(), e.what());
        return 1;
      }
    }

   private:
    CommandHandle(const std::string &app_cmd, const char **args,
                  bool include_stderr)
        : launcher_(app_cmd.c_str(), args, include_stderr) {
      launcher_.start();
    }

   protected:
    bool output_contains(const std::string &str, bool regex = false) const;

    /** @brief read() output from child until timeout expires, optionally
     * autoresponding to prompts
     *
     * @param timeout_ms timeout in milliseconds
     * @param autoresponder_enabled autoresponder is enabled if true (default)
     * @returns true if at least one byte was read
     */
    bool read_and_autorespond_to_output(unsigned timeout_ms,
                                        bool autoresponder_enabled = true);

    /** @brief write() predefined responses on found predefined patterns
     *
     * @param bytes_read buffer length
     * @param cmd_output buffer containig output to be scanned for triggers and
     * possibly autoresponded to
     */
    void autorespond_to_matching_lines(int bytes_read, char *cmd_output);

    /** @brief write() a predefined response if a predefined pattern is matched
     *
     * @param line line of output that will trigger a response, if matched
     * @returns true if an autoresponse was sent
     */
    bool autorespond_on_matching_pattern(const std::string &line);

    /** @brief see wait_for_exit() */
    int wait_for_exit_while_reading_and_autoresponding_to_output(
        unsigned timeout_ms);

    mysql_harness::ProcessLauncher
        launcher_;  // <- this guy's destructor takes care of
                    // killing the spawned process
    std::string execute_output_raw_;
    std::string last_line_read_;
    std::map<std::string, std::string> output_responses_;
    int exit_code_;
    bool exit_code_set_{false};

    friend class RouterComponentTest;
  };  // class CommandHandle

  /** @brief Gtest class SetUp, prepares the testcase.
   */
  virtual void SetUp();

  /** @brief Launches the MySQLRouter process.
   *
   * @param   params string containing command line parameters to pass to
   * process
   * @param   catch_stderr bool flag indicating if the process' error output
   * stream should be included in the output caught from the process
   * @param   with_sudo    bool flag indicating if the process' should be
   * execute with sudo priviledges
   *
   * @returns handle to the launched proccess
   */
  CommandHandle launch_router(const std::string &params,
                              bool catch_stderr = true,
                              bool with_sudo = false) const;

  /** @brief Launches the MySQLRouter process.
   *
   * @param   params vector<string> containing command line parameters to pass
   * to process
   * @param   catch_stderr bool flag indicating if the process' error output
   * stream should be included in the output caught from the process
   * @param   with_sudo    bool flag indicating if the process' should be
   * execute with sudo priviledges
   *
   * @returns handle to the launched proccess
   */
  CommandHandle launch_router(const std::vector<std::string> &params,
                              bool catch_stderr = true,
                              bool with_sudo = false) const;

  /** @brief Launches the MySQLServerMock process.
   *
   * @param   json_file  path to the json file containing expected queries
   * definitions
   * @param   port       number of the port where the mock server will accept
   * the client connections
   * @param   debug_mode if true all the queries and result get printed on the
   *                     standard output
   * @param   http_port  port number where the http_server module of the mock
   * server will accept REST client requests
   * @param   module_prefix base-path for javascript modules used by the tests
   *
   * @returns handle to the launched proccess
   */
  CommandHandle launch_mysql_server_mock(
      const std::string &json_file, unsigned port, bool debug_mode = false,
      uint16_t http_port = 0, const std::string &module_prefix = "") const;

  /** @brief Launches a process.
   *
   * @param command       path to executable
   * @param params        space-separated list of commanline parameters to pass
   * to the executable
   * @param catch_stderr  if true stderr will also be captured (combined with
   * stdout)
   *
   * @returns handle to the launched proccess
   */
  CommandHandle launch_command(const std::string &command,
                               const std::string &params,
                               bool catch_stderr = true) const;

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
  CommandHandle launch_command(const std::string &command,
                               const std::vector<std::string> &params,
                               bool catch_stderr) const;

  /** @brief Removes non-empty directory recursively.
   *
   * @param dir name of the directory to remove
   *
   * @returns 0 on success, error code on failure
   */
  static int purge_dir(const std::string &dir);

  /** @brief Creates a temporary directory with partially-random name and
   * returns its path.
   *
   * @note This is a convenience proxy function to mysql_harness::get_tmp_dir(),
   * see documentation there for more details.
   *
   * @param name name to be used as a directory name prefix
   *
   * @return path to the created directory
   *
   * @throws std::runtime_error if operation failed
   */
  static std::string get_tmp_dir(const std::string &name = "router");

  /** @brief Probes if the selected TCP port is accepting the connections.
   *
   * @param port          TCP port number to check
   * @param timeout_msec  maximum timeout to wait for the port
   * @param hostname      name/IP address of the network host to check
   *
   * @returns true if the selected port accepts connections, false otherwise
   */
  bool wait_for_port_ready(unsigned port, unsigned timeout_msec,
                           const std::string &hostname = "127.0.0.1") const;

  /** @brief Gets path to the directory containing testing data
   *         (conf files, json files).
   */
  const Path &get_data_dir() const { return data_dir_; }

  /** @brief Gets path to the directory used as log output directory
   */
  const Path &get_logging_dir() const { return logging_dir_; }

  /** @brief replace the 'process.env.{id}' in the input stream
   *
   * @pre assumes the input stream is a JS(ON) document with 'process.env.{id}'
   * references.
   *
   * replaces all references of process.env.{id} with the "environment
   * variables" provided in env_vars, line-by-line
   */
  static void replace_process_env(
      std::istream &ins, std::ostream &outs,
      const std::map<std::string, std::string> &env_vars);

  /** @brief returns a map with default [DEFAULT] section parameters
   *
   * @return default parameters for [DEFAULT] section
   */
  std::map<std::string, std::string> get_DEFAULT_defaults() const;

  std::string create_config_file(
      const std::string &content = "",
      const std::map<std::string, std::string> *params = nullptr,
      const std::string &directory = get_tmp_dir("conf"),
      const std::string &name = "mysqlrouter.conf") const;

  void set_origin(const Path &origin) { origin_dir_ = origin; }

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

  /** @brief returns true if the selected file contains a string
   *          that is true for a given predicate
   *
   * @param file_path path to the file we want to serach
   * @param predicate predicate to test the file
   * @param sleep_time max time to wait for the entry in the file
   */
  bool find_in_file(
      const std::string &file_path,
      const std::function<bool(const std::string &)> &predicate,
      std::chrono::milliseconds sleep_time = std::chrono::milliseconds(5000));

  /** @brief returns the content of the router logfile as a string
   *
   * @param file_name name of the logfile
   * @param file_path path to the logfile, use "" for default path that the
   * component test is using
   */
  std::string get_router_log_output(
      const std::string &file_name = "mysqlrouter.log",
      const std::string &file_path = "");

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
                  const char *out_params[MAX_PARAMS]) const;

  bool real_find_in_file(
      const std::string &file_path,
      const std::function<bool(const std::string &)> &predicate,
      std::ifstream &in_file, std::ios::streampos &cur_pos);

  Path data_dir_;
  Path origin_dir_;
  Path plugin_dir_;
  Path logging_dir_;
  Path mysqlrouter_exec_;
  Path mysqlserver_mock_exec_;
};

#endif  // _ROUTER_COMPONENT_TEST_H_
