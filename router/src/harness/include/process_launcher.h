/* Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef _PROCESS_LAUNCHER_H_
#define _PROCESS_LAUNCHER_H_

#include <chrono>
#include <cstdint>
#include <mutex>
#include <span>
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

class HARNESS_EXPORT Handle {
 public:
  Handle() = default;

  Handle(HANDLE hndl) : handle_(hndl) {}
  Handle(const Handle &) = delete;
  Handle(Handle &&other) : handle_(other.release()) {}
  Handle &operator=(const Handle &) = delete;
  Handle &operator=(Handle &&other) {
    handle_ = other.release();
    return *this;
  }

  ~Handle() { close(); }

  bool is_open() const { return handle_ != INVALID_HANDLE_VALUE; }

  stdx::expected<void, std::error_code> close();

  HANDLE native_handle() const { return handle_; }

  HANDLE release() { return std::exchange(handle_, INVALID_HANDLE_VALUE); }

  static stdx::expected<void, std::error_code> set_information(HANDLE hndl,
                                                               DWORD mask,
                                                               DWORD flags);

  stdx::expected<void, std::error_code> set_information(DWORD mask,
                                                        DWORD flags);

  stdx::expected<DWORD, std::error_code> wait_for_single_object(
      DWORD timeout_ms);

 private:
  HANDLE handle_{INVALID_HANDLE_VALUE};
};

class HARNESS_EXPORT FileHandle : public Handle {
 public:
  using Handle::Handle;

  stdx::expected<DWORD, std::error_code> write(
      const void *buf, DWORD buf_size, OVERLAPPED *overlapped = nullptr);

  stdx::expected<DWORD, std::error_code> read(void *buf, DWORD buf_size,
                                              OVERLAPPED *overlapped = nullptr);
};

class HARNESS_EXPORT PipeHandle : public FileHandle {
 public:
  using FileHandle::FileHandle;

  struct PeekResult {
    DWORD bytesRead;
    DWORD totalBytesAvail;
    DWORD bytesLeftThisMessage;
  };

  stdx::expected<PeekResult, std::error_code> peek(void *buf, DWORD buf_size);
};

class HARNESS_EXPORT ProcessHandle : public Handle {
 public:
  using Handle::Handle;

  stdx::expected<void, std::error_code> terminate(UINT exit_code) const;

  stdx::expected<DWORD, std::error_code> exit_code() const;
};

class HARNESS_EXPORT ThreadHandle : public Handle {
 public:
  stdx::expected<void, std::error_code> resume() const;
};

class HARNESS_EXPORT JobObject {
 public:
  JobObject() = default;

  JobObject(Handle hndl) : handle_(std::move(hndl)) {}

  static stdx::expected<JobObject, std::error_code> create();

  stdx::expected<void, std::error_code> set_information(
      JOBOBJECTINFOCLASS info_class, void *info, DWORD info_size);

  stdx::expected<void, std::error_code> assign_process(HANDLE process);

  bool is_open() const { return handle_.is_open(); }

 private:
  Handle handle_;
};

class HARNESS_EXPORT Process {
 public:
  Process() = default;

  Process(ProcessHandle process_hndl, ThreadHandle thread_hndl,
          DWORD process_id, DWORD thread_id)
      : process_handle_(std::move(process_hndl)),
        thread_handle_(std::move(thread_hndl)),
        process_id_(process_id),
        thread_id_(thread_id) {}

  static stdx::expected<Process, std::error_code> create(
      const char *app_name, char *cmd_line, SECURITY_ATTRIBUTES *process_attrs,
      SECURITY_ATTRIBUTES *thread_attrs, BOOL inherit_handles,
      DWORD creation_flags, void *env, const char *current_dir,
      STARTUPINFO *startup_info);

  ProcessHandle &process_handle() { return process_handle_; }
  const ProcessHandle &process_handle() const { return process_handle_; }

  ThreadHandle &thread_handle() { return thread_handle_; }
  const ThreadHandle &thread_handle() const { return thread_handle_; }

  DWORD process_id() const { return process_id_; }
  DWORD thread_id() const { return thread_id_; }

 private:
  ProcessHandle process_handle_;
  ThreadHandle thread_handle_;
  DWORD process_id_{};
  DWORD thread_id_{};
};

class HARNESS_EXPORT Pipe {
 public:
  Pipe(PipeHandle rd, PipeHandle wr) : rd_(std::move(rd)), wr_(std::move(wr)) {}

  static stdx::expected<Pipe, std::error_code> create(
      SECURITY_ATTRIBUTES *sec_attrs, DWORD sz);

  PipeHandle &read_handle() { return rd_; }
  PipeHandle &write_handle() { return wr_; }

 private:
  PipeHandle rd_;
  PipeHandle wr_;
};

class ThreadAttributeList {
 public:
  ThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST attr_list)
      : attr_list_(attr_list) {}

  ThreadAttributeList(const ThreadAttributeList &) = delete;
  ThreadAttributeList &operator=(const ThreadAttributeList &) = delete;

  ThreadAttributeList(ThreadAttributeList &&other)
      : attr_list_(std::exchange(other.attr_list_, nullptr)) {}
  ThreadAttributeList &operator=(ThreadAttributeList &other) {
    attr_list_ = std::exchange(other.attr_list_, nullptr);

    return *this;
  }

  ~ThreadAttributeList() {
    if (attr_list_ != nullptr) DeleteProcThreadAttributeList(attr_list_);
  }

  static stdx::expected<ThreadAttributeList, std::error_code> create(
      DWORD count);

  stdx::expected<void, std::error_code> update(DWORD flags, DWORD_PTR attribute,
                                               void *value, size_t value_size,
                                               void **prev_value,
                                               size_t *return_size) const;

  LPPROC_THREAD_ATTRIBUTE_LIST get() const { return attr_list_; }

 private:
  LPPROC_THREAD_ATTRIBUTE_LIST attr_list_;
};

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
      : executable_path(std::move(pexecutable_path)),
        args(std::move(pargs)),
        env_vars(std::move(penv_vars)),
        redirect_stderr(predirect_stderr) {}

  SpawnedProcess(const SpawnedProcess &) = delete;
  SpawnedProcess &operator=(const SpawnedProcess &) = delete;

  SpawnedProcess(SpawnedProcess &&) = default;
  SpawnedProcess &operator=(SpawnedProcess &&) = default;

  virtual ~SpawnedProcess() = default;

#ifdef _WIN32
  using handle_type = HANDLE;
  using id_type = DWORD;
#else
  using handle_type = pid_t;
  using id_type = pid_t;
#endif

  [[nodiscard]] std::string get_cmd_line() const;

  [[nodiscard]] std::string executable() const { return executable_path; }

 protected:
  std::string executable_path;
  std::vector<std::string> args;
  std::vector<std::pair<std::string, std::string>> env_vars;
#ifdef _WIN32
  win32::PipeHandle child_in_wr;
  win32::PipeHandle child_out_rd;
  win32::Process process_;
#else
  pid_t childpid{-1};
  int fd_in[2]{-1, -1};
  int fd_out[2]{-1, -1};
#endif
  bool redirect_stderr;
};

// Launches a process as child of current process and exposes the stdin &
// stdout of the child process (implemented thru pipelines) so the client of
// this class can read from the child's stdout and write to the child's
// stdin. For usage, see unit tests.
//
class HARNESS_EXPORT ProcessLauncher : public SpawnedProcess {
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
                       std::move(penv_vars), predirect_stderr) {}

  // copying a Process results in multiple destructors trying
  // to kill the same alive process. Disable it.
  ProcessLauncher(const ProcessLauncher &) = delete;
  ProcessLauncher operator=(const ProcessLauncher &) = delete;

  ProcessLauncher(ProcessLauncher &&rhs) noexcept
      : SpawnedProcess(std::move(rhs)),
        is_alive(std::exchange(rhs.is_alive, false)) {}

  ~ProcessLauncher() override;

  /** Launches the child process, and makes pipes available for read/write.
   */
  void start(bool use_std_io_handlers = false);

  void start(bool use_stdout_handler, bool use_stdin_handler);

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

  bool is_alive{false};
};

}  // end of namespace mysql_harness

#endif  // _PROCESS_LAUNCHER_H_
