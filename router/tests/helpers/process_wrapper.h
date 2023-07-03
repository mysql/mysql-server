/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>

using mysql_harness::Path;

// test performance tweaks
// shorter timeout -> faster test execution, longer timeout -> increased test
// stability
static constexpr auto kDefaultExpectOutputTimeout =
    std::chrono::milliseconds(1000);

// wait-timeout should be less than infinite, and long enough that even with
// valgrind we properly pass the tests
static constexpr auto kDefaultWaitForExitTimeout = std::chrono::seconds(30);

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
  ~ProcessWrapper() { stop_output_reader_thread(); }

  void stop_output_reader_thread() {
    output_reader_stop_ = true;
    if (output_reader_.joinable()) {
      output_reader_.join();
    }
  }

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
   */
  std::string get_full_output() {
    std::lock_guard<std::mutex> output_lock(output_mtx_);
    return execute_output_raw_;
  }

  /** @brief returns the content of the process app logfile as a string
   *
   * @param file_name name of the logfile, use "" for default filename that
   * given process is using
   * @param file_path path to the logfile, use "" for default path that the
   * component test is using
   * @param lines_limit maximum numbers of lines that should be returned; if 0
   * return all lines; if greater than 0 only return limit/2 beginning lines and
   * limit/2 ending lines
   */
  std::string get_logfile_content(const std::string &file_name = "",
                                  const std::string &file_path = "",
                                  size_t lines_limit = 0) const;

  /**
   * get the current output of the process.
   *
   * doesn't check if there is new content.
   */
  std::string get_current_output() const {
    std::lock_guard<std::mutex> output_lock(output_mtx_);
    return execute_output_raw_;
  }

  /** @brief Returns the exit code of the process.
   *
   *  Must always be called after wait_for_exit(),
   *  otherwise it throws runtime_error
   *
   * @returns exit code of the process
   */
  mysql_harness::ProcessLauncher::exit_status_type native_exit_code() {
    if (!exit_status_) {
      throw std::runtime_error(
          "RouterComponentTest::Command_handle: exit_code() called without "
          "wait_for_exit()!");
    }
    return *exit_status_;
  }

  int exit_code() {
    if (!exit_status_) {
      throw std::runtime_error(
          "RouterComponentTest::Command_handle: exit_code() called without "
          "wait_for_exit()!");
    }

    if (auto code = exit_status_->exited()) {
      return *code;
    } else {
      throw std::runtime_error("signal or so.");
    }
  }

  bool has_exit_code() const { return exit_status_.has_value(); }

  /** @brief Waits for the process to exit, while reading its output and
   * autoresponding to prompts.
   *
   * If the process did not finish yet, it waits the given number of
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

  mysql_harness::ProcessLauncher::exit_status_type native_wait_for_exit(
      std::chrono::milliseconds timeout = kDefaultWaitForExitTimeout);

  /** @brief Returns process PID
   *
   * @returns PID of the process
   */
  mysql_harness::ProcessLauncher::process_id_type get_pid() const {
    return launcher_.get_pid();
  }

  std::string get_command_line() { return launcher_.get_cmd_line(); }
  std::string executable() { return launcher_.executable(); }

  int kill();

  mysql_harness::ProcessLauncher::exit_status_type native_kill();

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

  bool output_contains(const std::string &str, bool regex = false) const;

  using OutputResponder = std::function<std::string(const std::string &)>;

  void wait_for_sync_point_result(stdx::expected<void, std::error_code> v) {
    wait_for_sync_point_result_ = std::move(v);
  }

  [[nodiscard]] stdx::expected<void, std::error_code>
  wait_for_sync_point_result() const {
    return wait_for_sync_point_result_;
  }

 private:
  ProcessWrapper(
      const std::string &app_cmd, const std::vector<std::string> &args,
      const std::vector<std::pair<std::string, std::string>> &env_vars,
      bool include_stderr, OutputResponder &output_responder);

 protected:
  /** @brief read() output from child until timeout expires, optionally
   * autoresponding to prompts
   *
   * @param timeout timeout in milliseconds
   * @param autoresponder_enabled autoresponder is enabled if true (default)
   * @retval true if at least one byte was read
   */
  bool read_and_autorespond_to_output(std::chrono::milliseconds timeout,
                                      bool autoresponder_enabled = true);

  /** @brief write() predefined responses on found predefined patterns
   *
   * @param cmd_output buffer containing output to be scanned for triggers and
   * possibly autoresponded to
   */
  void autorespond_to_matching_lines(const std::string_view &cmd_output);

  /** @brief write() a predefined response if a predefined pattern is matched
   *
   * @param line line of output that will trigger a response, if matched
   * @retval true if an autoresponse was sent
   */
  bool autorespond_on_matching_pattern(const std::string &line);

  mysql_harness::ProcessLauncher
      launcher_;  // <- this guy's destructor takes care of
                  // killing the spawned process
  std::string execute_output_raw_;
  std::string last_line_read_;
  OutputResponder output_responder_;
  std::optional<mysql_harness::ProcessLauncher::exit_status_type> exit_status_;

  std::string logging_dir_;
  std::string logging_file_;

  std::atomic<bool> output_reader_stop_{false};
  std::thread output_reader_;
  mutable std::mutex output_mtx_;

  friend class ProcessManager;

  stdx::expected<void, std::error_code> wait_for_sync_point_result_{};
};  // class ProcessWrapper

#endif  // _PROCESS_WRAPPER_H_
