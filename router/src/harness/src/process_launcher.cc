/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "process_launcher.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>    // fprintf()
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
#include <cstring>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;

namespace mysql_harness {

// performance tweaks
constexpr auto kWaitPidCheckInterval = std::chrono::milliseconds(10);
constexpr auto kTerminateWaitInterval = std::chrono::seconds(10);

ProcessLauncher::~ProcessLauncher() {
  if (is_alive) {
    try {
      close();
    } catch (const std::exception &e) {
      fprintf(stderr, "Can't stop the alive process %s: %s\n", cmd_line.c_str(),
              e.what());
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
      ok = TerminateProcess(pi.hProcess, 0);
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

std::string cmdline_from_args(const char *const *args) {
  std::string s;

  for (auto arg = args; *arg != nullptr; ++arg) {
    if (!s.empty()) s.push_back(' ');
    s.append(win32::cmdline_quote_arg(*arg));
  }

  return s;
}

}  // namespace win32

void ProcessLauncher::start() {
  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&child_out_rd, &child_out_wr, &saAttr, 0)) {
    throw std::system_error(last_error_code(), "Failed to create child_out_rd");
  }

  if (!SetHandleInformation(child_out_rd, HANDLE_FLAG_INHERIT, 0))
    throw std::system_error(last_error_code(), "Failed to create child_out_rd");

  // force non blocking IO in Windows
  // DWORD mode = PIPE_NOWAIT;
  // BOOL res = SetNamedPipeHandleState(child_out_rd, &mode, NULL, NULL);

  if (!CreatePipe(&child_in_rd, &child_in_wr, &saAttr, 0))
    throw std::system_error(last_error_code(), "Failed to create child_in_rd");

  if (!SetHandleInformation(child_in_wr, HANDLE_FLAG_INHERIT, 0))
    throw std::system_error(last_error_code(), "Failed to created child_in_wr");

  std::string arguments = win32::cmdline_from_args(args);

  si.cb = sizeof(STARTUPINFO);
  if (redirect_stderr) si.hStdError = child_out_wr;
  si.hStdOutput = child_out_wr;
  si.hStdInput = child_in_rd;
  si.dwFlags |= STARTF_USESTDHANDLES;

  // as CreateProcess may/will modify the arguments (split filename and args
  // with a \0) keep a copy of it for error-reporting.
  std::string create_process_arguments = arguments;
  BOOL bSuccess =
      CreateProcess(NULL,                               // lpApplicationName
                    &create_process_arguments.front(),  // lpCommandLine
                    NULL,                               // lpProcessAttributes
                    NULL,                               // lpThreadAttributes
                    TRUE,                               // bInheritHandles
                    CREATE_NEW_PROCESS_GROUP,           // dwCreationFlags
                    NULL,                               // lpEnvironment
                    NULL,                               // lpCurrentDirectory
                    &si,                                // lpStartupInfo
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

uint64_t ProcessLauncher::get_pid() const { return (uint64_t)pi.hProcess; }

int ProcessLauncher::wait(std::chrono::milliseconds timeout) {
  DWORD dwExit = 0;
  BOOL get_ret{FALSE};
  if ((get_ret = GetExitCodeProcess(pi.hProcess, &dwExit))) {
    if (dwExit == STILL_ACTIVE) {
      auto wait_ret = WaitForSingleObject(pi.hProcess, timeout.count());
      switch (wait_ret) {
        case WAIT_OBJECT_0:
          get_ret = GetExitCodeProcess(pi.hProcess, &dwExit);
          break;
        case WAIT_TIMEOUT:
          throw std::system_error(
              std::make_error_code(std::errc::timed_out),
              std::string("Timed out waiting " +
                          std::to_string(timeout.count()) +
                          " ms for the process '" + cmd_line + "' to exit"));
        case WAIT_FAILED:
          throw std::system_error(last_error_code());
        default:
          throw std::runtime_error(
              "Unexpected error while waiting for the process '" + cmd_line +
              "' to finish: " + std::to_string(wait_ret));
      }
    }
  }
  if (get_ret == FALSE) {
    auto ec = last_error_code();
    if (ec != std::error_code(ERROR_INVALID_HANDLE,
                              std::system_category()))  // not closed already?
      throw std::system_error(ec);
    else
      dwExit = 128;  // Invalid handle
  }
  return dwExit;
}

int ProcessLauncher::close() {
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

int ProcessLauncher::read(char *buf, size_t count,
                          std::chrono::milliseconds timeout) {
  DWORD dwBytesRead;
  DWORD dwBytesAvail;

  // at least 1ms, but max 100ms
  auto std_interval = std::min(100ms, std::max(timeout / 10, 1ms));

  do {
    // check if there is data in the pipe before issuing a blocking read
    BOOL bSuccess =
        PeekNamedPipe(child_out_rd, NULL, 0, NULL, &dwBytesAvail, NULL);

    if (!bSuccess) {
      auto ec = last_error_code();
      if (ec == std::error_code(ERROR_NO_DATA, std::system_category()) ||
          ec == std::error_code(ERROR_BROKEN_PIPE, std::system_category())) {
        return EOF;
      } else {
        throw std::system_error(last_error_code());
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

  BOOL bSuccess = ReadFile(child_out_rd, buf, count, &dwBytesRead, NULL);

  if (bSuccess == FALSE) {
    auto ec = last_error_code();
    if (ec == std::error_code(ERROR_NO_DATA, std::system_category()) ||
        ec == std::error_code(ERROR_BROKEN_PIPE, std::system_category())) {
      return EOF;
    } else {
      throw std::system_error(ec);
    }
  }

  return dwBytesRead;
}

int ProcessLauncher::write(const char *buf, size_t count) {
  DWORD dwBytesWritten;

  BOOL bSuccess = WriteFile(child_in_wr, buf, count, &dwBytesWritten, NULL);
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

uint64_t ProcessLauncher::get_fd_write() const { return (uint64_t)child_in_wr; }

uint64_t ProcessLauncher::get_fd_read() const { return (uint64_t)child_out_rd; }

#else

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

    execvp(cmd_line.c_str(), const_cast<char *const *>(args));
    // if exec returns, there is an error.
    auto ec = last_error_code();
    fprintf(stderr, "%s could not be executed: %s (errno %d)\n",
            cmd_line.c_str(), ec.message().c_str(), ec.value());

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

int ProcessLauncher::close() {
  int result = 0;
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
        result = wait(kTerminateWaitInterval);
      } catch (const std::system_error &e) {
        if (e.code() != std::errc::no_such_process) {
          std::error_code ec2 = send_shutdown_event(ShutdownEvent::KILL);
          if (ec2 != std::errc::no_such_process) {
            throw std::system_error(ec2);
          }
        }
        result = wait();
      }
    }
  }

  if (fd_out[0] != -1) ::close(fd_out[0]);
  if (fd_in[1] != -1) ::close(fd_in[1]);

  fd_out[0] = -1;
  fd_in[1] = -1;
  is_alive = false;

  return result;
}

void ProcessLauncher::end_of_write() {
  if (fd_in[1] != -1) ::close(fd_in[1]);
  fd_in[1] = -1;
}

int ProcessLauncher::read(char *buf, size_t count,
                          std::chrono::milliseconds timeout) {
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

  int res = select(fd_out[0] + 1, &set, NULL, NULL, &timeout_tv);
  if (res < 0) throw std::system_error(last_error_code(), "select()");
  if (res == 0) return 0;

  if ((n = (int)::read(fd_out[0], buf, count)) >= 0) return n;

  throw std::system_error(last_error_code(), "read");
}

int ProcessLauncher::write(const char *buf, size_t count) {
  int n;
  if ((n = (int)::write(fd_in[1], buf, count)) >= 0) return n;

  auto ec = last_error_code();
  if (ec == std::errc::broken_pipe) return 0;

  throw std::system_error(ec, "write");
}

uint64_t ProcessLauncher::get_pid() const {
  static_assert(sizeof(pid_t) <= sizeof(uint64_t),
                "sizeof(pid_t) > sizeof(uint64_t)");
  return (uint64_t)childpid;
}

int ProcessLauncher::wait(const std::chrono::milliseconds timeout) {
  using namespace std::chrono_literals;
  auto wait_time = timeout;
  do {
    int status;

    pid_t ret = ::waitpid(childpid, &status, WNOHANG);

    if (ret == 0) {
      auto sleep_for = std::min(wait_time, kWaitPidCheckInterval);
      if (sleep_for.count() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for));
        wait_time -= sleep_for;
      } else {
        throw std::system_error(std::make_error_code(std::errc::timed_out),
                                std::string("Timed out waiting ") +
                                    std::to_string(timeout.count()) +
                                    " ms for the process " +
                                    std::to_string(childpid) + " to exit");
      }
    } else if (ret == -1) {
      throw std::system_error(
          last_error_code(),
          std::string("waiting for process '" + cmd_line + "' failed"));
    } else {
      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        std::string msg;
        std::array<char, 1024> b;
        int n;
        while ((n = read(b.data(), b.size(), 100ms)) > 0) {
          msg.append(b.data(), n);
        }
        throw std::runtime_error(std::string("Process '" + cmd_line +
                                             "' got signal " +
                                             std::to_string(WTERMSIG(status))) +
                                 ":\n" + msg);
      } else {
        // it neither exited, not received a signal.
        throw std::runtime_error(
            std::string("Process '" + cmd_line + "' ... no idea"));
      }
    }
  } while (true);
}

uint64_t ProcessLauncher::get_fd_write() const { return (uint64_t)fd_in[1]; }

uint64_t ProcessLauncher::get_fd_read() const { return (uint64_t)fd_out[0]; }

#endif

int ProcessLauncher::kill() { return close(); }

}  // end of namespace mysql_harness
