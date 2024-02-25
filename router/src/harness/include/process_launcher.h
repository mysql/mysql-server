/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef _PROCESS_LAUNCHER_H_
#define _PROCESS_LAUNCHER_H_

#include <chrono>
#include <cstdint>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#ifdef UNICODE
# #undef UNICODE
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "harness_export.h"

#include "exit_status.h"
#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {
#ifdef _WIN32
namespace win32 {
// reverse of CommandLineToArgv()
HARNESS_EXPORT std::string cmdline_quote_arg(const std::string &arg);
HARNESS_EXPORT std::string cmdline_from_args(
    const std::string &executable_path, const std::vector<std::string> &args);
}  // namespace win32
#endif

/** an alive, spawned process
 *
 * @todo
 *   refactor ProcessLauchner and SpawnedProcess into:
 *   - ProcessLauncher having only the spawn/launch() method and no state
 *   - Process as a thin wrapper around 'pid' and operators on it
 *   - SpawnedProcess being a Process with stdin/stdout/stderr
 *   - a way to declare ownership over the 'pid' (if owned, kill pid in
 * destructor) For now, this mostly exists to make the move-constructor of
 * ProcessLauncher easier to implement.
 */
class HARNESS_EXPORT SpawnedProcess {
 public:
  SpawnedProcess(std::string pexecutable_path, std::vector<std::string> pargs,
                 std::vector<std::pair<std::string, std::string>> penv_vars,
                 bool predirect_stderr = true)
      : executable_path{std::move(pexecutable_path)},
        args{std::move(pargs)},
        env_vars{std::move(penv_vars)},
#ifdef _WIN32
        child_in_rd{INVALID_HANDLE_VALUE},
        child_in_wr{INVALID_HANDLE_VALUE},
        child_out_rd{INVALID_HANDLE_VALUE},
        child_out_wr{INVALID_HANDLE_VALUE},
        pi{},
        si{},
#else
        childpid{-1},
        fd_in{-1, -1},
        fd_out{-1, -1},
#endif
        redirect_stderr{predirect_stderr} {
  }

  SpawnedProcess(const SpawnedProcess &) = default;

  virtual ~SpawnedProcess() = default;

#ifdef _WIN32
  using handle_type = HANDLE;
  using id_type = DWORD;
#else
  using handle_type = pid_t;
  using id_type = pid_t;
#endif

  std::string get_cmd_line() const;

  std::string executable() const { return executable_path; }

 protected:
  const std::string executable_path;
  const std::vector<std::string> args;
  const std::vector<std::pair<std::string, std::string>> env_vars;
#ifdef _WIN32
  HANDLE child_in_rd;
  HANDLE child_in_wr;
  HANDLE child_out_rd;
  HANDLE child_out_wr;
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
#else
  pid_t childpid;
  int fd_in[2];
  int fd_out[2];
#endif
  bool redirect_stderr;
};

// Launches a process as child of current process and exposes the stdin &
// stdout of the child process (implemented thru pipelines) so the client of
// this class can read from the child's stdout and write to the child's
// stdin. For usage, see unit tests.
//
class HARNESS_EXPORT ProcessLauncher : public SpawnedProcess {
#ifdef _WIN32
  /*
   * After ProcessLauncher sends all data to remote process, it closes the
   * handle to notify the remote process that no more data will be sent.
   *
   * Since you cannot close the same handle more than once, store
   * information if handle should be closed in child_in_wr_closed.
   */
  bool child_in_wr_closed = false;
#endif

 public:
  using exit_status_type = ExitStatus;

  /**
   * Creates a new process and launch it.
   * If redirect_stderr is true, the child's stderr is redirected to the
   * same stream than child's stdout.
   */
  ProcessLauncher(std::string pexecutable_path, std::vector<std::string> pargs,
                  std::vector<std::pair<std::string, std::string>> penv_vars,
                  bool predirect_stderr = true)
      : SpawnedProcess(std::move(pexecutable_path), std::move(pargs),
                       std::move(penv_vars), predirect_stderr),
        is_alive{false} {}

  // copying a Process results in multiple destructors trying
  // to kill the same alive process. Disable it.
  ProcessLauncher(const ProcessLauncher &) = delete;
  ProcessLauncher operator=(const ProcessLauncher &) = delete;

  ProcessLauncher(ProcessLauncher &&rhs)
      : SpawnedProcess(rhs), is_alive(std::move(rhs.is_alive)) {
    // make sure destructor on the other object doesn't try to kill
    // the process-id we just moved

    rhs.is_alive = false;
  }

  ~ProcessLauncher() override;

  /** Launches the child process, and makes pipes available for read/write.
   */
  void start();

  /**
   * Read up to a 'count' bytes from the stdout of the child process.
   * This method blocks until the amount of bytes is read or specified
   * timeout expires.
   * @param buf already allocated buffer where the read data will be stored.
   * @param count the maximum amount of bytes to read.
   * @param timeout timeout (in milliseconds) for the read to complete
   * @return the real number of bytes read.
   * Returns an shcore::Exception in case of error when reading.
   */
  int read(char *buf, size_t count, std::chrono::milliseconds timeout);

  /**
   * Writes several butes into stdin of child process.
   * Returns an shcore::Exception in case of error when writing.
   */
  int write(const char *buf, size_t count);

  /**
   * Kills the child process and returns process' exit code.
   */
  exit_status_type kill();

  using process_handle_type = SpawnedProcess::handle_type;
  using process_id_type = SpawnedProcess::id_type;

  /**
   * Returns the child process id.
   */
  process_id_type get_pid() const;

  /**
   * Returns the child process handle.
   */
  process_handle_type get_process_handle() const;

  /**
   * get exit-code.
   */
  stdx::expected<exit_status_type, std::error_code> exit_code();

  /**
   * Wait for the child process to exists and returns its exit code.
   * If the child process is already dead, wait() just returns.
   *
   * @returns the exit code of the process.
   * @throws std::runtime_error if process exited with a signal
   */

  int wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

  exit_status_type native_wait(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

  /**
   * Closes pipe to process' STDIN in order to notify the process that all
   * data was sent.
   */
  void end_of_write();

  enum class ShutdownEvent {
    TERM,  // clean shutdown (ie. SIGTERM on Unix)
    KILL,  // immediate (and abrupt) shutdown (ie. SIGKILL on Unix)
    ABRT   // try to generate a stacktrace
  };
  /**
   * Sends a shutdown event to child process (SIGTERM on Unix, Ctrl+C on
   * Win)
   *
   * @param event type of shutdown event
   * @return std::error_code indicating success/failure
   */
  std::error_code send_shutdown_event(
      ShutdownEvent event = ShutdownEvent::TERM) const noexcept;

 private:
  /**
   * Closes child process and returns process' exit code.
   *
   * @throws std::system_error if sending signal to child process fails
   * @throws std::runtime_error if waiting for process to change state fails
   *
   * @return process exit code.
   */
  exit_status_type close();

  std::mutex fd_in_mtx_;
  std::mutex fd_out_mtx_;

  bool is_alive;
};

}  // end of namespace mysql_harness

#endif  // _PROCESS_LAUNCHER_H_
