/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_EXIT_STATUS_H_
#define MYSQL_HARNESS_EXIT_STATUS_H_

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include <cstdlib>
#include <cstring>  // strsignal
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>

/**
 * exit status of processes.
 *
 * a process can report its exit status:
 *
 * - exited (via exit(num))
 * - terminated (via a signal, exception, ...)
 * - stopped or continued (via SIGSTOP, SIGCONT)
 */
class ExitStatus {
 public:
  struct terminated_t {};  // helper tag for a terminated exit-status
  struct stopped_t {};     // helper tag for a stopped exit-status
  struct continued_t {};   // helper tag for a continued exit-status
  struct native_t {};      // helper tag for a native exit-status
  struct exited_t {};      // helper tag for a exited exit-status

  /**
   * construct a exit-status of a exited process.
   */
  constexpr ExitStatus(int exit_code) : ExitStatus{exited_t{}, exit_code} {}

  constexpr ExitStatus(exited_t, int exit_code)
      : status_kind_{StatusKind::kExited}, status_{exit_code} {}

  constexpr ExitStatus(native_t, int native_exit_code)
      : ExitStatus{from_native(native_exit_code)} {}

  constexpr ExitStatus(terminated_t, int signum)
      : status_kind_{StatusKind::kSignalled}, status_{signum} {}

  constexpr ExitStatus(stopped_t, int signum)
      : status_kind_{StatusKind::kStopped}, status_{signum} {}

  constexpr ExitStatus(continued_t)
      : status_kind_{StatusKind::kContinued}, status_{0} {}

  /**
   * check if the status is a clean exit.
   *
   * if true, contains the exit-code.
   */
  constexpr std::optional<int> exited() const {
    if (status_kind_ != StatusKind::kExited) return {};

    return status_;
  }

  /**
   * check if the status is a terminated exit.
   *
   * if true, contains the signal number used to terminate the process.
   */
  constexpr std::optional<int> terminated() const {
    if (status_kind_ != StatusKind::kSignalled) return {};

    return status_;
  }

  /**
   * check if the status is a stopped process.
   *
   * if true, contains the signal number used to stop the process.
   */
  constexpr std::optional<int> stopped() const {
    if (status_kind_ != StatusKind::kStopped) return {};

    return status_;
  }

  /**
   * check if the status is continued process.
   */
  constexpr bool continued() const {
    return (status_kind_ == StatusKind::kContinued);
  }

  friend bool operator==(const ExitStatus &a, const ExitStatus &b) {
    return a.status_kind_ == b.status_kind_ && a.status_ == b.status_;
  }

 private:
  enum class StatusKind { kSignalled, kExited, kStopped, kContinued };

  StatusKind status_kind_;

  int status_;

  static constexpr ExitStatus from_native(int native_exit_code) {
#ifndef _WIN32
    if (WIFSIGNALED(native_exit_code)) {
      return {terminated_t{}, WTERMSIG(native_exit_code)};
    } else if (WIFEXITED(native_exit_code)) {
      return {exited_t{}, WEXITSTATUS(native_exit_code)};
    } else if (WIFSTOPPED(native_exit_code)) {
      return {stopped_t{}, WSTOPSIG(native_exit_code)};
    } else if (WIFCONTINUED(native_exit_code)) {
      return {continued_t{}};
    } else {
      // shouldn't happen
      return {exited_t{}, native_exit_code};
    }
#else
    // the lower-byte contains the exit-code, everything else is a NTSTATUS
    if (native_exit_code > 0xff || native_exit_code < 0) {
      return {terminated_t{}, native_exit_code};
    } else {
      return {exited_t{}, native_exit_code};
    }
#endif
  }
};

inline std::ostream &operator<<(std::ostream &os, const ExitStatus &st) {
  if (auto code = st.exited()) {
    os << "Exit(" << *code << ")";
  } else if (auto code = st.terminated()) {
#ifndef _WIN32
    os << "Terminated(signal=" << *code << ") " << strsignal(*code);
#else
    std::ostringstream hexed;
    hexed << std::showbase << std::hex << *code;

    os << "Terminated(exception=" << hexed.str() << ") "
       << std::system_category().message(*code);
#endif
  } else if (auto code = st.stopped()) {
#ifndef _WIN32
    os << "Stopped(signal=" << *code << ") " << strsignal(*code);
#else
    os << "Stopped(signal=" << *code << ")";
#endif
  } else if (st.continued()) {
    os << "Continued()";
  }
  return os;
}

#endif
