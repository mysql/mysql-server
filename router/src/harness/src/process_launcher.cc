/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace mysql_harness {

// performance tweaks
constexpr unsigned kWaitPidCheckInterval = 10;
constexpr auto kTerminateWaitInterval = std::chrono::seconds(10);

ProcessLauncher::~ProcessLauncher() {
  if (is_alive) {
    try {
      close();
    } catch (std::exception &e) {
      fprintf(stderr, "Can't stop the alive process %s: %s\n", cmd_line.c_str(),
              e.what());
    }
  }
}

std::error_code ProcessLauncher::send_shutdown_event(
    ShutdownEvent event /* = ShutdownEvent::TERM */) const noexcept {
#ifdef _WIN32
  bool ok = false;  // need to initialize to avoid -Werror=maybe-uninitialized
  switch (event) {
    case ShutdownEvent::TERM:
      ok = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId);
      break;
    case ShutdownEvent::KILL:
      ok = TerminateProcess(pi.hProcess, 0);
      break;
  }

  return ok ? std::error_code{}
            : std::error_code(GetLastError(), std::system_category());
#else
  bool ok = false;  // need to initialize to avoid -Werror=maybe-uninitialized
  switch (event) {
    case ShutdownEvent::TERM:
      ok = ::kill(childpid, SIGTERM) == 0;
      break;
    case ShutdownEvent::KILL:
      ok = ::kill(childpid, SIGKILL) == 0;
      break;
  }

  return ok ? std::error_code{}
            : std::error_code(errno, std::system_category());
#endif
}

#ifdef _WIN32

void ProcessLauncher::start() {
  SECURITY_ATTRIBUTES saAttr;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&child_out_rd, &child_out_wr, &saAttr, 0))
    report_error("Failed to create child_out_rd");

  if (!SetHandleInformation(child_out_rd, HANDLE_FLAG_INHERIT, 0))
    report_error("Failed to create child_out_rd");

  // force non blocking IO in Windows
  DWORD mode = PIPE_NOWAIT;
  // BOOL res = SetNamedPipeHandleState(child_out_rd, &mode, NULL, NULL);

  if (!CreatePipe(&child_in_rd, &child_in_wr, &saAttr, 0))
    report_error("Failed to create child_in_rd");

  if (!SetHandleInformation(child_in_wr, HANDLE_FLAG_INHERIT, 0))
    report_error("Failed to created child_in_wr");

  // Create Process
  std::string s = this->cmd_line;
  const char **pc = args;
  while (*++pc != NULL) {
    s += " ";
    s += *pc;
  }
  char *sz_cmd_line = (char *)malloc(s.length() + 1);
  if (!sz_cmd_line)
    report_error(
        "Cannot assign memory for command line in ProcessLauncher::start");
  _tcscpy(sz_cmd_line, s.c_str());

  BOOL bSuccess = FALSE;

  ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

  ZeroMemory(&si, sizeof(STARTUPINFO));
  si.cb = sizeof(STARTUPINFO);
  if (redirect_stderr) si.hStdError = child_out_wr;
  si.hStdOutput = child_out_wr;
  si.hStdInput = child_in_rd;
  si.dwFlags |= STARTF_USESTDHANDLES;

  bSuccess = CreateProcess(NULL,                      // lpApplicationName
                           sz_cmd_line,               // lpCommandLine
                           NULL,                      // lpProcessAttributes
                           NULL,                      // lpThreadAttributes
                           TRUE,                      // bInheritHandles
                           CREATE_NEW_PROCESS_GROUP,  // dwCreationFlags
                           NULL,                      // lpEnvironment
                           NULL,                      // lpCurrentDirectory
                           &si,                       // lpStartupInfo
                           &pi);                      // lpProcessInformation

  if (!bSuccess)
    report_error(("Failed to start process " + s).c_str());
  else
    is_alive = true;

  CloseHandle(child_out_wr);
  CloseHandle(child_in_rd);

  // DWORD res1 = WaitForInputIdle(pi.hProcess, 100);
  // res1 = WaitForSingleObject(pi.hThread, 100);
  free(sz_cmd_line);
}

uint64_t ProcessLauncher::get_pid() const { return (uint64_t)pi.hProcess; }

int ProcessLauncher::wait(unsigned int timeout_ms) {
  DWORD dwExit = 0;
  BOOL get_ret{FALSE};
  if (get_ret = GetExitCodeProcess(pi.hProcess, &dwExit)) {
    if (dwExit == STILL_ACTIVE) {
      auto wait_ret = WaitForSingleObject(pi.hProcess, timeout_ms);
      if (wait_ret == 0) {
        get_ret = GetExitCodeProcess(pi.hProcess, &dwExit);
      } else {
        throw std::runtime_error("Error waiting for process exit: " +
                                 std::to_string(wait_ret));
      }
    }
  }
  if (get_ret == FALSE) {
    DWORD dwError = GetLastError();
    if (dwError != ERROR_INVALID_HANDLE)  // not closed already?
      report_error(NULL);
    else
      dwExit = 128;  // Invalid handle
  }
  return dwExit;
}

int ProcessLauncher::close() {
  // note: report_error() throws std::system_error

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
        if (send_shutdown_event(ShutdownEvent::KILL)) report_error(NULL);

        // wait again, if that fails not much we can do
        if (WaitForSingleObject(pi.hProcess, wait_timeout) != WAIT_OBJECT_0) {
          report_error(NULL);
        }
      }
    }
  } else {
    if (is_alive) report_error(NULL);
  }

  if (!CloseHandle(pi.hProcess)) report_error(NULL);
  if (!CloseHandle(pi.hThread)) report_error(NULL);

  if (!CloseHandle(child_out_rd)) report_error(NULL);
  if (!child_in_wr_closed && !CloseHandle(child_in_wr)) report_error(NULL);

  is_alive = false;
  return 0;
}

int ProcessLauncher::read(char *buf, size_t count, unsigned timeout_ms) {
  DWORD dwBytesRead;
  DWORD dwBytesAvail;

  // at least 1ms
  auto std_interval_ms = std::max(timeout_ms / 10U, 1U);

  do {
    // check if there is data in the pipe before issuing a blocking read
    BOOL bSuccess =
        PeekNamedPipe(child_out_rd, NULL, 0, NULL, &dwBytesAvail, NULL);

    if (!bSuccess) {
      DWORD dwCode = GetLastError();
      if (dwCode == ERROR_NO_DATA || dwCode == ERROR_BROKEN_PIPE)
        return EOF;
      else
        report_error(NULL);
    }

    // we got data, let's read it
    if (dwBytesAvail != 0) break;

    if (timeout_ms == 0) {
      // no data and time left to wait
      //

      return 0;
    }

    auto interval_ms = std::min(timeout_ms, std_interval_ms);

    // just wait the whole timeout and try again
    auto sleep_time = std::chrono::milliseconds(interval_ms);

    std::this_thread::sleep_for(sleep_time);

    timeout_ms -= interval_ms;
  } while (true);

  BOOL bSuccess = ReadFile(child_out_rd, buf, count, &dwBytesRead, NULL);

  if (bSuccess == FALSE) {
    DWORD dwCode = GetLastError();
    if (dwCode == ERROR_NO_DATA || dwCode == ERROR_BROKEN_PIPE)
      return EOF;
    else
      report_error(NULL);
  }

  return dwBytesRead;
}

int ProcessLauncher::write(const char *buf, size_t count) {
  DWORD dwBytesWritten;
  BOOL bSuccess = FALSE;
  bSuccess = WriteFile(child_in_wr, buf, count, &dwBytesWritten, NULL);
  if (!bSuccess) {
    if (GetLastError() != ERROR_NO_DATA)  // otherwise child process just died.
      report_error(NULL);
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

void ProcessLauncher::report_error(const char *msg, const char *prefix) {
  DWORD dwCode = GetLastError();
  LPTSTR lpMsgBuf;

  if (msg != NULL) {
    throw std::system_error(dwCode, std::generic_category(), msg);
  } else {
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, dwCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&lpMsgBuf, 0, NULL);
    std::string msgerr;
    if (prefix != nullptr) {
      msgerr += std::string(prefix) + "; ";
    }
    msgerr += "SystemError: ";
    msgerr += lpMsgBuf;
    msgerr += "with error code %d" + std::to_string(dwCode) + ".";
    throw std::system_error(dwCode, std::generic_category(), msgerr);
  }
}

uint64_t ProcessLauncher::get_fd_write() const { return (uint64_t)child_in_wr; }

uint64_t ProcessLauncher::get_fd_read() const { return (uint64_t)child_out_rd; }

#else

void ProcessLauncher::start() {
  if (pipe(fd_in) < 0) {
    report_error(NULL, "ProcessLauncher::start() pipe(fd_in)");
  }
  if (pipe(fd_out) < 0) {
    report_error(NULL, "ProcessLauncher::start() pipe(fd_out)");
  }

  // Ignore broken pipe signal
  signal(SIGPIPE, SIG_IGN);

  childpid = fork();
  if (childpid == -1) {
    report_error(NULL, "ProcessLauncher::start() fork()");
  }

  if (childpid == 0) {
#ifdef LINUX
    prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif

    ::close(fd_out[0]);
    ::close(fd_in[1]);
    while (dup2(fd_out[1], STDOUT_FILENO) == -1) {
      if (errno == EINTR)
        continue;
      else
        report_error(NULL, "ProcessLauncher::start() dup2()");
    }

    if (redirect_stderr) {
      while (dup2(fd_out[1], STDERR_FILENO) == -1) {
        if (errno == EINTR)
          continue;
        else
          report_error(NULL, "ProcessLauncher::start() dup2()");
      }
    }
    while (dup2(fd_in[0], STDIN_FILENO) == -1) {
      if (errno == EINTR)
        continue;
      else
        report_error(NULL, "ProcessLauncher::start() dup2()");
    }

    fcntl(fd_out[1], F_SETFD, FD_CLOEXEC);
    fcntl(fd_in[0], F_SETFD, FD_CLOEXEC);

    execvp(cmd_line.c_str(), const_cast<char *const *>(args));
    // if exec returns, there is an error.
    int my_errno = errno;
    fprintf(stderr, "%s could not be executed: %s (errno %d)\n",
            cmd_line.c_str(), strerror(my_errno), my_errno);

    // we need to identify an ENOENT and since some programs return 2 as
    // exit-code we need to return a non-existent code, 128 is a general
    // convention used to indicate a failure to execute another program in a
    // subprocess
    if (my_errno == 2) my_errno = 128;

    exit(my_errno);
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
        result = wait(static_cast<unsigned int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                kTerminateWaitInterval)
                .count()));
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

int ProcessLauncher::read(char *buf, size_t count, unsigned timeout_ms) {
  int n;
  fd_set set;
  struct timeval timeout;
  memset(&timeout, 0x0, sizeof(timeout));
  timeout.tv_sec = static_cast<decltype(timeout.tv_sec)>(timeout_ms / 1000);
  timeout.tv_usec =
      static_cast<decltype(timeout.tv_usec)>((timeout_ms % 1000) * 1000);

  FD_ZERO(&set);
  FD_SET(fd_out[0], &set);

  int res = select(fd_out[0] + 1, &set, NULL, NULL, &timeout);
  if (res < 0) report_error(nullptr, "select()");
  if (res == 0) return 0;

  if ((n = (int)::read(fd_out[0], buf, count)) >= 0) return n;

  report_error(nullptr, "read");
  return -1;
}

int ProcessLauncher::write(const char *buf, size_t count) {
  int n;
  if ((n = (int)::write(fd_in[1], buf, count)) >= 0) return n;
  if (errno == EPIPE) return 0;
  report_error(NULL, "write");
  return -1;
}

void ProcessLauncher::report_error(const char *msg, const char *prefix) {
  char sys_err[64] = {'\0'};
  int errnum = errno;
  if (msg == NULL) {
  // we do this #ifdef dance because on unix systems strerror_r() will
  // generate a warning if we don't collect the result (warn_unused_result
  // attribute)
#if ((defined _POSIX_C_SOURCE && (_POSIX_C_SOURCE >= 200112L)) || \
     (defined _XOPEN_SOURCE && (_XOPEN_SOURCE >= 600))) &&        \
    !defined _GNU_SOURCE
    int r = strerror_r(errno, sys_err, sizeof(sys_err));
    (void)r;  // silence unused variable;
#elif defined(_GNU_SOURCE) && defined(__GLIBC__)
    const char *r = strerror_r(errno, sys_err, sizeof(sys_err));
    (void)r;  // silence unused variable;
#else
    strerror_r(errno, sys_err, sizeof(sys_err));
#endif

    std::string s = std::string(prefix) + "; " + std::string(sys_err) +
                    "with errno ." + std::to_string(errnum);
    throw std::system_error(errnum, std::generic_category(), s);
  } else {
    throw std::system_error(errnum, std::generic_category(), msg);
  }
}

uint64_t ProcessLauncher::get_pid() const {
  static_assert(sizeof(pid_t) <= sizeof(uint64_t),
                "sizeof(pid_t) > sizeof(uint64_t)");
  return (uint64_t)childpid;
}

int ProcessLauncher::wait(const unsigned int timeout_ms) {
  unsigned int wait_time = timeout_ms;
  do {
    int status;

    pid_t ret = ::waitpid(childpid, &status, WNOHANG);

    if (ret == 0) {
      auto sleep_for = std::min(wait_time, kWaitPidCheckInterval);
      if (sleep_for > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for));
        wait_time -= sleep_for;
      } else {
        throw std::system_error(
            ETIMEDOUT, std::generic_category(),
            std::string("Timed out waiting " + std::to_string(timeout_ms) +
                        " ms for the process " + std::to_string(childpid) +
                        " to exit"));
      }
    } else if (ret == -1) {
      throw std::system_error(
          errno, std::generic_category(),
          std::string("waiting for process " + std::to_string(childpid) +
                      " failed"));
    } else {
      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        throw std::runtime_error(
            std::string("Process " + std::to_string(childpid) + " got signal " +
                        std::to_string(WTERMSIG(status))));
      } else {
        // it neither exited, not received a signal.
        throw std::runtime_error(std::string(
            "Process " + std::to_string(childpid) + " ... not idea"));
      }
    }
  } while (true);
}

uint64_t ProcessLauncher::get_fd_write() const { return (uint64_t)fd_in[1]; }

uint64_t ProcessLauncher::get_fd_read() const { return (uint64_t)fd_out[0]; }

#endif

int ProcessLauncher::kill() { return close(); }

}  // end of namespace mysql_harness
