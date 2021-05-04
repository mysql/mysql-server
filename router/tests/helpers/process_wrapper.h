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

#ifndef _PROCESS_WRAPPER_H_
#define _PROCESS_WRAPPER_H_

#include "process_launcher.h"
#include "router_test_helpers.h"

#include <cstring>

using mysql_harness::Path;

// test performance tweaks
// shorter timeout -> faster test execution, longer timeout -> increased test
// stability
static constexpr auto kDefaultExpectOutputTimeout =
    std::chrono::milliseconds(1000);

// wait-timeout should be less than infinite, and long enough that even with
// valgrind we properly pass the tests
static constexpr auto kDefaultWaitForExitTimeout =
    std::chrono::milliseconds(10000);

static constexpr size_t kReadBufSize = 1024;

/** @class ProcessWrapper
 *
 * Object of this class gets return from launch_* method and can be
 * use to manipulate launched process (get the output, exit code,
 * inject input, etc.)
 *
 **/
class ProcessWrapper {
 public:
  /** @brief Checks if the process wrote the specified string to its output.
   *
   * This function loops read()ing child process output, until either the
   * expected output appears, or until timeout is reached. While reading, it
   * also calls autoresponder to react to any prompts issued by the child
   * process.
   *
   * @param str      Expected output string
   * @param regex    True if str is a regex pattern
   * @param timeout  timeout in milliseconds, to wait for the output
   * @return Returns bool flag indicating if the specified string appeared
   *                 in the process' output.
   */
  bool expect_output(
      const std::string &str, bool regex = false,
      std::chrono::milliseconds timeout = kDefaultExpectOutputTimeout);

  /** @brief Returns the full output that was produced the process till moment
   *         of calling this method.
   *  TODO: this description does not match what the code does, this needs to
   * be fixed.
   */
  std::string get_full_output() {
    while (read_and_autorespond_to_output(std::chrono::milliseconds(0))) {
    }
    return execute_output_raw_;
  }

  /** @brief returns the content of the process app logfile as a string
   *
   * @param file_name name of the logfile, use "" for default filename that
   * given process is using
   * @param file_path path to the logfile, use "" for default path that the
   * component test is using
   */
  std::string get_full_logfile(const std::string &file_name = "",
                               const std::string &file_path = "") const {
    const std::string path = file_path.empty() ? logging_dir_ : file_path;
    const std::string name = file_name.empty() ? logging_file_ : file_name;

    if (name.empty()) return "";

    return get_file_output(name, path);
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
   * @param timeout maximum amount of time to wait for the process to
   * finish
   * @throws std::runtime_error on timeout, std::system_error on failure
   * @returns exit code of the process
   */
  int wait_for_exit(
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout);

  /** @brief Returns process PID
   *
   * @returns PID of the process
   */
  uint64_t get_pid() const { return launcher_.get_pid(); }

  std::string get_command_line() { return launcher_.get_cmd_line(); }

  int kill();

  std::error_code send_shutdown_event(
      mysql_harness::ProcessLauncher::ShutdownEvent event =
          mysql_harness::ProcessLauncher::ShutdownEvent::TERM) const noexcept {
    return launcher_.send_shutdown_event(event);
  }

  /** @brief Initiate Router shutdown
   *
   * @returns shutdown event delivery success/failure
   */
  std::error_code send_clean_shutdown_event() const {
    return launcher_.send_shutdown_event();
  }

  std::string get_logfile_path() const {
    return logging_dir_ + "/" + logging_file_;
  }

  void set_logging_path(const std::string &logging_dir,
                        const std::string &logging_file) {
    logging_dir_ = logging_dir;
    logging_file_ = logging_file;
  }

 private:
  ProcessWrapper(
      const std::string &app_cmd, const std::vector<std::string> &args,
      const std::vector<std::pair<std::string, std::string>> &env_vars,
      bool include_stderr)
      : launcher_(app_cmd.c_str(), args, env_vars, include_stderr) {
    launcher_.start();
  }

 protected:
  bool output_contains(const std::string &str, bool regex = false) const;

  /** @brief read() output from child until timeout expires, optionally
   * autoresponding to prompts
   *
   * @param timeout timeout in milliseconds
   * @param autoresponder_enabled autoresponder is enabled if true (default)
   * @returns true if at least one byte was read
   */
  bool read_and_autorespond_to_output(std::chrono::milliseconds timeout,
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
      std::chrono::milliseconds timeout);

  mysql_harness::ProcessLauncher
      launcher_;  // <- this guy's destructor takes care of
                  // killing the spawned process
  std::string execute_output_raw_;
  std::string last_line_read_;
  std::map<std::string, std::string> output_responses_;
  int exit_code_;
  bool exit_code_set_{false};

  std::string logging_dir_;
  std::string logging_file_;

  friend class ProcessManager;
};  // class ProcessWrapper

#endif  // _PROCESS_WRAPPER_H_
