/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#include "process_launcher.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>    // fprintf()
#include <cstring>   // memset
#include <iterator>  // std::distance
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>  // this_thread::sleep_for

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <cstdlib>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "exit_status.h"
#include "mysql/harness/stdx/expected.h"
#include "scope_guard.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

#ifndef _WIN32
extern char **environ;
#endif

namespace mysql_harness {

// performance tweaks
constexpr auto kTerminateWaitInterval = std::chrono::seconds(10);
#ifndef _WIN32
/** @brief maximum number of parameters that can be passed to the launched
 * process */
constexpr auto kWaitPidCheckInterval = std::chrono::milliseconds(10);
#endif

std::string SpawnedProcess::get_cmd_line() const {
  std::string result = executable_path;
  for (const auto &arg : args) {
    result += " " + arg;
  }

  return result;
}

ProcessLauncher::~ProcessLauncher() {
  if (is_alive) {
    try {
      close();
    } catch (const std::exception &e) {
      fprintf(stderr, "Can't stop the alive process %s: %s\n",
              executable_path.c_str(), e.what());
    }
  }
}

static std::error_code last_error_code() noexcept {
#ifdef _WIN32
  return std::error_code{static_cast<int>(GetLastError()),
                         std::system_category()};
#else
  return std::error_code{errno, std::generic_category()};
#endif
}

std::error_code ProcessLauncher::send_shutdown_event(
    ShutdownEvent event /* = ShutdownEvent::TERM */) const noexcept {
  bool ok{false};
#ifdef _WIN32
  switch (event) {
    case ShutdownEvent::TERM:
      ok = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId);
      break;
    case ShutdownEvent::KILL:
      ok = TerminateProcess(pi.hProcess, 0x0);
      break;
    case ShutdownEvent::ABRT:
      ok = TerminateProcess(pi.hProcess, STATUS_TIMEOUT);
      break;
  }
#else
  switch (event) {
    case ShutdownEvent::TERM:
      ok = ::kill(childpid, SIGTERM) == 0;
      break;
    case ShutdownEvent::KILL:
      ok = ::kill(childpid, SIGKILL) == 0;
      break;
    case ShutdownEvent::ABRT:
      ok = ::kill(childpid, SIGABRT) == 0;
      break;
  }
#endif

  return ok ? std::error_code{} : last_error_code();
}

#ifdef _WIN32

namespace win32 {
// reverse of CommandLineToArgv()
std::string cmdline_quote_arg(const std::string &arg) {
  if (!arg.empty() && (arg.find_first_of(" \t\n\v\"") == arg.npos)) {
    // no need to quote it
    return arg;
  }

  std::string out("\"");

  for (auto it = arg.begin(); it != arg.end(); ++it) {
    // backslashes are special at the end of the line
    //
    // foo\bar  -> "foo\\bar"
    // foobar\  -> "foobar\\"
    // foobar\\ -> "foobar\\\\"
    // foobar\" -> "foobar\""

    auto no_backslash_it = std::find_if(
        it, arg.end(), [](const auto &value) { return value != '\\'; });

    const size_t num_backslash = std::distance(it, no_backslash_it);
    // move past the backslashes
    it = no_backslash_it;

    if (it == arg.end()) {
      // one-or-more backslash to the end
      //
      // escape all backslash
      out.append(num_backslash * 2, '\\');

      // we are at the end, get out
      break;
    }

    if (*it == '"') {
      // one-or-more backslash before "
      // escape all backslash and "
      out.append(num_backslash * 2 + 1, '\\');
    } else {
      // zero-or-more backslash before non-special char|end
      // don't escape
      out.append(num_backslash, '\\');
    }
    out.push_back(*it);
  }

  out.push_back('"');

  return out;
}

std::string cmdline_from_args(const std::string &executable_path,
                              const std::vector<std::string> &args) {
  std::string s = win32::cmdline_quote_arg(executable_path);

  for (const auto &arg : args) {
    s.append(" ").append(win32::cmdline_quote_arg(arg));
  }

  return s;
}

}  // namespace win32

void ProcessLauncher::start() {
  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = nullptr;

  if (!CreatePipe(&child_out_rd, &child_out_wr, &saAttr, 0)) {
    throw std::system_error(last_error_code(), "Failed to create child_out_rd");
  }

  if (!SetHandleInformation(child_out_rd, HANDLE_FLAG_INHERIT, 0))
    throw std::system_error(last_error_code(), "Failed to create child_out_rd");

  // force non blocking IO in Windows
  // DWORD mode = PIPE_NOWAIT;
  // BOOL res = SetNamedPipeHandleState(child_out_rd, &mode, nullptr, nullptr);

  if (!CreatePipe(&child_in_rd, &child_in_wr, &saAttr, 0))
    throw std::system_error(last_error_code(), "Failed to create child_in_rd");

  if (!SetHandleInformation(child_in_wr, HANDLE_FLAG_INHERIT, 0))
    throw std::system_error(last_error_code(), "Failed to created child_in_wr");

  std::string arguments = win32::cmdline_from_args(executable_path, args);

  // as CreateProcess may/will modify the arguments (split filename and args
  // with a \0) keep a copy of it for error-reporting.
  std::string create_process_arguments = arguments;

  DWORD dwCreationFlags = 0;

  std::vector<TCHAR> env_block;

  if (auto parent_env = GetEnvironmentStrings()) {
    // copy the whole parent-env-block to the current env.
    //
    // ... but first the length of the env-block must be calculated.
    size_t parent_env_len = 0;
    auto cur_env = parent_env;

    // env-block is terminated by an empty-string (\0)
    for (auto cur_len = strlen(cur_env); cur_len != 0;
         cur_len = strlen(cur_env)) {
      cur_len += 1;  // env-var terminator (\0)

      parent_env_len += cur_len;

      // skip the current variable.
      cur_env += cur_len;
    }

    // copy the parent-env-block to the child-env, without the terminating \0
    std::copy(parent_env, parent_env + parent_env_len,
              std::back_insert_iterator(env_block));

    FreeEnvironmentStrings(parent_env);
  }

#ifdef UNICODE
  dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
#endif

  for (const auto &[key, val] : env_vars) {
    std::copy(key.begin(), key.end(), std::back_insert_iterator(env_block));
    env_block.push_back('=');
    std::copy(val.begin(), val.end(), std::back_insert_iterator(env_block));

    env_block.push_back(0);  // terminate env-var
  }
  env_block.push_back(0);  // terminate env-block

  // The code below makes sure the process we are launching only inherits the
  // in/out pipes FDs
  SIZE_T size = 0;
  // figure out the size needed for a 1-elem list
  if (InitializeProcThreadAttributeList(nullptr, 1, 0, &size) == FALSE &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    throw std::system_error(last_error_code(),
                            "Failed to InitializeProcThreadAttributeList() "
                            "when launching a process " +
                                arguments);
  }

  // allocate the memory for the list
  auto attribute_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
      HeapAlloc(GetProcessHeap(), 0, size));
  if (attribute_list == nullptr) {
    throw std::system_error(
        last_error_code(),
        "Failed to HeapAlloc() when launchin a process " + arguments);
  }
  Scope_guard clean_attribute_list_guard(
      [&]() { DeleteProcThreadAttributeList(attribute_list); });

  if (InitializeProcThreadAttributeList(attribute_list, 1, 0, &size) == FALSE) {
    throw std::system_error(last_error_code(),
                            "Failed to InitializeProcThreadAttributeList() 2 "
                            "when launching a process " +
                                arguments);
  }

  // fill up the list
  HANDLE handles_to_inherit[] = {child_out_wr, child_in_rd};
  if (UpdateProcThreadAttribute(attribute_list, 0,
                                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                handles_to_inherit, sizeof(handles_to_inherit),
                                nullptr, nullptr) == FALSE) {
    throw std::system_error(
        last_error_code(),
        "Failed to UpdateProcThreadAttribute() when launching a process " +
            arguments);
  }

  // prepare the process' startup parameters structure
  si.cb = sizeof(STARTUPINFO);
  if (redirect_stderr) si.hStdError = child_out_wr;
  si.hStdOutput = child_out_wr;
  si.hStdInput = child_in_rd;
  si.dwFlags |= STARTF_USESTDHANDLES;
  STARTUPINFOEX si_ex;
  ZeroMemory(&si_ex, sizeof(si_ex));
  si_ex.StartupInfo = si;
  si_ex.StartupInfo.cb = sizeof(si_ex);
  si_ex.lpAttributeList = attribute_list;

  dwCreationFlags |= EXTENDED_STARTUPINFO_PRESENT;
  dwCreationFlags |= CREATE_NEW_PROCESS_GROUP;

  // launch the process
  BOOL bSuccess =
      CreateProcess(nullptr,                            // lpApplicationName
                    &create_process_arguments.front(),  // lpCommandLine
                    nullptr,                            // lpProcessAttributes
                    nullptr,                            // lpThreadAttributes
                    TRUE,                               // bInheritHandles
                    dwCreationFlags,                    //
                    env_block.data(),                   // lpEnvironment
                    nullptr,                            // lpCurrentDirectory
                    &si_ex.StartupInfo,                 // lpStartupInfo
                    &pi);                               // lpProcessInformation

  if (!bSuccess) {
    throw std::system_error(last_error_code(),
                            "Failed to start process " + arguments);
  } else {
    is_alive = true;
  }

  CloseHandle(child_out_wr);
  CloseHandle(child_in_rd);

  // DWORD res1 = WaitForInputIdle(pi.hProcess, 100);
  // res1 = WaitForSingleObject(pi.hThread, 100);
}

ProcessLauncher::process_id_type ProcessLauncher::get_pid() const {
  return pi.dwProcessId;
}

ProcessLauncher::process_handle_type ProcessLauncher::get_process_handle()
    const {
  return pi.hProcess;
}

stdx::expected<ProcessLauncher::exit_status_type, std::error_code>
ProcessLauncher::exit_code() {
  DWORD dwExit = 0;

  const BOOL ret = GetExitCodeProcess(pi.hProcess, &dwExit);
  if (ret == 0) {
    return stdx::unexpected(last_error_code());
  } else if (dwExit == STILL_ACTIVE) {
    return stdx::unexpected(std::make_error_code(std::errc::timed_out));
  }

  using ret_type =
      stdx::expected<ProcessLauncher::exit_status_type, std::error_code>;

  return ret_type{std::in_place, exit_status_type::native_t{}, dwExit};
}

ProcessLauncher::exit_status_type ProcessLauncher::native_wait(
    std::chrono::milliseconds timeout) {
  auto exit_res = exit_code();
  if (exit_res) return *exit_res;

  auto ec = exit_res.error();
  if (ec != std::errc::timed_out) throw std::system_error(ec);

  auto wait_ret = WaitForSingleObject(pi.hProcess, timeout.count());
  switch (wait_ret) {
    case WAIT_OBJECT_0:
      break;
    case WAIT_TIMEOUT:
      throw std::system_error(std::make_error_code(std::errc::timed_out));
    case WAIT_FAILED:
      throw std::system_error(last_error_code());
    default:
      throw std::runtime_error(
          "Unexpected error while waiting for the process '" + executable_path +
          "' to finish: " + std::to_string(wait_ret));
  }

  // try again.
  exit_res = exit_code();
  if (exit_res) return *exit_res;

  ec = exit_res.error();
  if (ec == std::error_code(ERROR_INVALID_HANDLE, std::system_category())) {
    return 128;  // Invalid handle
  }

  throw std::system_error(ec);
}

ProcessLauncher::exit_status_type ProcessLauncher::close() {
  DWORD dwExit;
  if (GetExitCodeProcess(pi.hProcess, &dwExit)) {
    if (dwExit == STILL_ACTIVE) {
      send_shutdown_event(ShutdownEvent::TERM);

      DWORD wait_timeout =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              kTerminateWaitInterval)
              .count();
      if (WaitForSingleObject(pi.hProcess, wait_timeout) != WAIT_OBJECT_0) {
        // use the big hammer if that did not work
        if (send_shutdown_event(ShutdownEvent::KILL))
          throw std::system_error(last_error_code());

        // wait again, if that fails not much we can do
        if (WaitForSingleObject(pi.hProcess, wait_timeout) != WAIT_OBJECT_0) {
          throw std::system_error(last_error_code());
        }
      }
    }
  } else {
    if (is_alive) throw std::system_error(last_error_code());
  }

  if (!CloseHandle(pi.hProcess)) throw std::system_error(last_error_code());
  if (!CloseHandle(pi.hThread)) throw std::system_error(last_error_code());

  if (!CloseHandle(child_out_rd)) throw std::system_error(last_error_code());
  if (!child_in_wr_closed && !CloseHandle(child_in_wr))
    throw std::system_error(last_error_code());

  is_alive = false;
  return 0;
}

static void throw_system_error(const std::error_code ec) {
  if (ec == std::error_code(ERROR_INVALID_FUNCTION, std::system_category()) ||
      ec == std::error_code(ERROR_INVALID_HANDLE, std::system_category()) ||
      ec == std::error_code(ERROR_BAD_PIPE, std::system_category())) {
    throw std::system_error(
        std::error_code(ERROR_NOT_READY, std::system_category()));
  }

  throw std::system_error(ec);
}

int ProcessLauncher::read(char *buf, size_t count,
                          std::chrono::milliseconds timeout) {
  DWORD dwBytesRead;
  DWORD dwBytesAvail;

  // at least 1ms, but max 100ms
  auto std_interval = std::min(100ms, std::max(timeout / 10, 1ms));

  do {
    // check if there is data in the pipe before issuing a blocking read
    BOOL bSuccess = PeekNamedPipe(child_out_rd, nullptr, 0, nullptr,
                                  &dwBytesAvail, nullptr);

    if (!bSuccess) {
      auto ec = last_error_code();
      if (ec == std::error_code(ERROR_NO_DATA, std::system_category()) ||
          ec == std::error_code(ERROR_BROKEN_PIPE, std::system_category())) {
        return EOF;
      } else {
        throw_system_error(ec);
      }
    }

    // we got data, let's read it
    if (dwBytesAvail != 0) break;

    if (timeout.count() == 0) {
      // no data and time left to wait
      //

      return 0;
    }

    auto interval = std::min(timeout, std_interval);

    // just wait the whole timeout and try again
    std::this_thread::sleep_for(interval);

    timeout -= interval;
  } while (true);

  BOOL bSuccess = ReadFile(child_out_rd, buf, count, &dwBytesRead, nullptr);

  if (bSuccess == FALSE) {
    auto ec = last_error_code();
    if (ec == std::error_code(ERROR_NO_DATA, std::system_category()) ||
        ec == std::error_code(ERROR_BROKEN_PIPE, std::system_category())) {
      return EOF;
    } else {
      throw_system_error(ec);
    }
  }

  return dwBytesRead;
}

int ProcessLauncher::write(const char *buf, size_t count) {
  DWORD dwBytesWritten;

  BOOL bSuccess = WriteFile(child_in_wr, buf, count, &dwBytesWritten, nullptr);
  if (!bSuccess) {
    auto ec = last_error_code();
    if (ec !=
        std::error_code(
            ERROR_NO_DATA,
            std::system_category()))  // otherwise child process just died.
      throw std::system_error(ec);
  } else {
    // When child input buffer is full, this returns zero in NO_WAIT mode.
    return dwBytesWritten;
  }
  return 0;  // so the compiler does not cry
}

void ProcessLauncher::end_of_write() {
  CloseHandle(child_in_wr);
  child_in_wr_closed = true;
}

#else

static std::vector<const char *> get_params(
    const std::string &command, const std::vector<std::string> &params_vec) {
  std::vector<const char *> result;
  result.reserve(params_vec.size() +
                 2);  // 1 for command name and 1 for terminating NULL
  result.push_back(command.c_str());

  for (const auto &par : params_vec) {
    result.push_back(par.c_str());
  }
  result.push_back(nullptr);

  return result;
}

// converts vector of pairs to vector of strings "first=second"
static auto get_env_vars_vector(
    const std::vector<std::pair<std::string, std::string>> &env_vars) {
  std::vector<std::string> result;

  for (const auto &env_var : env_vars) {
    result.push_back(env_var.first + "=" + env_var.second);
  }

  return result;
}

static auto get_env_vars(const std::vector<std::string> &env_vars) {
  std::vector<const char *> result;

  size_t i{0};
  for (; environ[i] != nullptr; ++i)
    ;
  result.reserve(env_vars.size() + i + 1);

  // first copy all current process' env variables
  for (i = 0; environ[i] != nullptr; ++i) {
    result.push_back(environ[i]);
  }

  // now append all the env variables passed to the launcher
  for (i = 0; i < env_vars.size(); ++i) {
    result.push_back(env_vars[i].c_str());
  }
  result.push_back(nullptr);

  return result;
}

void ProcessLauncher::start() {
  if (pipe(fd_in) < 0) {
    throw std::system_error(last_error_code(),
                            "ProcessLauncher::start() pipe(fd_in)");
  }
  if (pipe(fd_out) < 0) {
    throw std::system_error(last_error_code(),
                            "ProcessLauncher::start() pipe(fd_out)");
  }

  // Ignore broken pipe signal
  signal(SIGPIPE, SIG_IGN);

  childpid = fork();
  if (childpid == -1) {
    throw std::system_error(last_error_code(),
                            "ProcessLauncher::start() fork()");
  }

  if (childpid == 0) {
#ifdef LINUX
    prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

    ::close(fd_out[0]);
    ::close(fd_in[1]);
    while (dup2(fd_out[1], STDOUT_FILENO) == -1) {
      auto ec = last_error_code();
      if (ec == std::errc::interrupted) {
        continue;
      } else {
        throw std::system_error(ec, "ProcessLauncher::start() dup2()");
      }
    }

    if (redirect_stderr) {
      while (dup2(fd_out[1], STDERR_FILENO) == -1) {
        auto ec = last_error_code();
        if (ec == std::errc::interrupted) {
          continue;
        } else {
          throw std::system_error(ec, "ProcessLauncher::start() dup2()");
        }
      }
    }
    while (dup2(fd_in[0], STDIN_FILENO) == -1) {
      auto ec = last_error_code();
      if (ec == std::errc::interrupted) {
        continue;
      } else {
        throw std::system_error(ec, "ProcessLauncher::start() dup2()");
      }
    }

    fcntl(fd_out[1], F_SETFD, FD_CLOEXEC);
    fcntl(fd_in[0], F_SETFD, FD_CLOEXEC);

    // mark all FDs as CLOEXEC
    //
    // don't inherit any open FD to the spawned process.
    //
    // 3 should be STDERR_FILENO + 1
    // 255 should be large enough.
    for (int fd = 3; fd < 255; ++fd) {
      // it may fail (bad fd, ...)
      fcntl(fd, F_SETFD, FD_CLOEXEC);
    }

    const auto params_arr = get_params(executable_path, args);
    const auto env_vars_vect = get_env_vars_vector(env_vars);
    const auto env_vars_arr = get_env_vars(env_vars_vect);
    execve(executable_path.c_str(),
           const_cast<char *const *>(params_arr.data()),
           const_cast<char *const *>(env_vars_arr.data()));
    // if exec returns, there is an error.
    auto ec = last_error_code();
    fprintf(stderr, "%s could not be executed: %s (errno %d)\n",
            executable_path.c_str(), ec.message().c_str(), ec.value());

    if (ec == std::errc::no_such_file_or_directory) {
      // we need to identify an ENOENT and since some programs return 2 as
      // exit-code we need to return a non-existent code, 128 is a general
      // convention used to indicate a failure to execute another program in a
      // subprocess
      exit(128);
    } else {
      exit(ec.value());
    }
  } else {
    ::close(fd_out[1]);
    ::close(fd_in[0]);

    fd_out[1] = -1;
    fd_in[0] = -1;

    is_alive = true;
  }
}

ProcessLauncher::exit_status_type ProcessLauncher::close() {
  exit_status_type result = 0;
  if (is_alive) {
    // only try to kill the pid, if we started it. Not that we hurt someone
    // else.
    if (std::error_code ec1 = send_shutdown_event(ShutdownEvent::TERM)) {
      if (ec1 != std::errc::no_such_process) {
        throw std::system_error(ec1);
      }
    } else {
      try {
        // wait for it shutdown before using the big hammer
        result = native_wait(kTerminateWaitInterval);
      } catch (const std::system_error &e) {
        if (e.code() != std::errc::no_such_process) {
          std::error_code ec2 = send_shutdown_event(ShutdownEvent::ABRT);
          if (ec2 != std::errc::no_such_process) {
            throw std::system_error(ec2);
          }
          try {
            // wait for it shutdown before using the big hammer
            result = native_wait(kTerminateWaitInterval);
          } catch (const std::system_error &e) {
            std::error_code ec2 = send_shutdown_event(ShutdownEvent::KILL);
            if (ec2 != std::errc::no_such_process) {
              throw std::system_error(ec2);
            }
          }
          result = native_wait();
        }
      }
    }
  }

  std::lock_guard<std::mutex> fd_in_lock(fd_in_mtx_);
  std::lock_guard<std::mutex> fd_out_lock(fd_out_mtx_);

  if (fd_out[0] != -1) ::close(fd_out[0]);
  if (fd_in[1] != -1) ::close(fd_in[1]);

  fd_out[0] = -1;
  fd_in[1] = -1;
  is_alive = false;

  return result;
}

void ProcessLauncher::end_of_write() {
  std::lock_guard<std::mutex> fd_in_lock(fd_in_mtx_);

  if (fd_in[1] != -1) ::close(fd_in[1]);
  fd_in[1] = -1;
}

int ProcessLauncher::read(char *buf, size_t count,
                          std::chrono::milliseconds timeout) {
  std::lock_guard<std::mutex> fd_out_lock(fd_out_mtx_);
  if (fd_out[0] == -1) return 0;

  int n;
  fd_set set;
  struct timeval timeout_tv;
  memset(&timeout_tv, 0x0, sizeof(timeout_tv));
  timeout_tv.tv_sec =
      static_cast<decltype(timeout_tv.tv_sec)>(timeout.count() / 1000);
  timeout_tv.tv_usec = static_cast<decltype(timeout_tv.tv_usec)>(
      (timeout.count() % 1000) * 1000);

  FD_ZERO(&set);
  FD_SET(fd_out[0], &set);

  int res = select(fd_out[0] + 1, &set, nullptr, nullptr, &timeout_tv);
  if (res < 0) throw std::system_error(last_error_code(), "select()");
  if (res == 0) return 0;

  if ((n = (int)::read(fd_out[0], buf, count)) >= 0) return n;

  throw std::system_error(last_error_code(), "read");
}

int ProcessLauncher::write(const char *buf, size_t count) {
  int n;

  std::lock_guard<std::mutex> fd_in_lock(fd_in_mtx_);
  if (fd_in[1] == -1) return 0;

  if ((n = (int)::write(fd_in[1], buf, count)) >= 0) return n;

  auto ec = last_error_code();
  if (ec == std::errc::broken_pipe) return 0;

  throw std::system_error(ec, "write");
}

ProcessLauncher::process_handle_type ProcessLauncher::get_pid() const {
  static_assert(sizeof(pid_t) <= sizeof(uint64_t),
                "sizeof(pid_t) > sizeof(uint64_t)");
  return childpid;
}

ProcessLauncher::process_handle_type ProcessLauncher::get_process_handle()
    const {
  return get_pid();
}

stdx::expected<ProcessLauncher::exit_status_type, std::error_code>
ProcessLauncher::exit_code() {
  int status;

  const pid_t ret = ::waitpid(childpid, &status, WNOHANG);
  if (ret == 0) {
    return stdx::unexpected(std::make_error_code(std::errc::timed_out));
  } else if (ret == -1) {
    return stdx::unexpected(last_error_code());
  }

  using ret_type =
      stdx::expected<ProcessLauncher::exit_status_type, std::error_code>;

  return ret_type{std::in_place, exit_status_type::native_t{}, status};
}

ProcessLauncher::exit_status_type ProcessLauncher::native_wait(
    const std::chrono::milliseconds timeout) {
  using namespace std::chrono_literals;

  auto end_time = std::chrono::steady_clock::now() + timeout;

  do {
    const auto wait_res = exit_code();

    if (wait_res) {
      auto status = wait_res.value();

      if (status.exited()) {
        return status;
      } else if (auto sig = status.terminated()) {
        auto signum = *sig;

        // drain the stdout|stderr.
        std::string msg;
        std::array<char, 1024> b;
        int n;
        while ((n = read(b.data(), b.size(), 100ms)) > 0) {
          msg.append(b.data(), n);
        }

        throw std::runtime_error(std::string("Process '" + executable_path +
                                             "' got signal " +
                                             std::to_string(signum)) +
                                 ":\n" + msg);
      } else {
        // it neither exited, not received a signal.
        throw std::runtime_error(
            std::string("Process '" + executable_path + "' ... no idea"));
      }
    }

    const auto ec = wait_res.error();

    if (ec != std::errc::timed_out) throw std::system_error(ec);

    const auto now = std::chrono::steady_clock::now();
    if (now > end_time) break;

    std::this_thread::sleep_until(
        std::min(now + kWaitPidCheckInterval, end_time));
  } while (true);

  throw std::system_error(std::make_error_code(std::errc::timed_out));
}
#endif

int ProcessLauncher::wait(const std::chrono::milliseconds timeout) {
  auto wait_res = native_wait(timeout);

  if (auto code = wait_res.exited()) {
    return *code;
  }

  throw std::runtime_error("terminated?");
}

ProcessLauncher::exit_status_type ProcessLauncher::kill() { return close(); }

}  // end of namespace mysql_harness
