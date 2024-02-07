/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_SIGNAL_HANDLER_INCLUDED
#define MYSQL_HARNESS_SIGNAL_HANDLER_INCLUDED

#include "harness_export.h"

#include <array>
#include <csignal>
#include <functional>
#include <map>
#include <thread>

#include "mysql/harness/stdx/monitor.h"

namespace mysql_harness {

class HARNESS_EXPORT SignalHandler {
 public:
  // mimick the MYSQLD_RESTART_EXIT values from sql/sql_const.h
  static constexpr const int HARNESS_SUCCESS_EXIT{0};
  static constexpr const int HARNESS_ABORT_EXIT{1};
  static constexpr const int HARNESS_FAILURE_EXIT{2};
  static constexpr const int HARNESS_RESTART_EXIT{16};

#ifndef _WIN32
  static constexpr const std::array kFatalSignals{SIGSEGV, SIGABRT, SIGBUS,
                                                  SIGILL,  SIGFPE,  SIGTRAP};

  static constexpr const std::array kIgnoredSignals{SIGPIPE};
#endif
  SignalHandler() = default;

  ~SignalHandler();

  void register_ignored_signals_handler();

  void block_all_nonfatal_signals();

  /**
   * register a handler for fatal signals.
   *
   * @param dump_core dump core of fatal signal
   */
  void register_fatal_signal_handler(bool dump_core);

#ifdef _WIN32
  // register Ctrl-C handler.
  void register_ctrl_c_handler();

  // unregister Ctrl-C handler.
  void unregister_ctrl_c_handler();
#endif

  /**
   * add signal handler for a signal
   *
   * @param signum signal number
   * @param f signal handler
   */
  void add_sig_handler(int signum, std::function<void(int, std::string)> f) {
    sig_handlers_([signum, f](auto &handlers) {
      handlers.emplace(signum, std::move(f));
    });
  }

  void remove_sig_handler(int signum) {
    sig_handlers_([signum](auto &handlers) { handlers.erase(signum); });
  }

  void spawn_signal_handler_thread();

 private:
  // signal handlers per signal number.
  Monitor<std::map<int, std::function<void(int, std::string)>>> sig_handlers_{
      {}};

  WaitableMonitor<bool> signal_thread_ready_{false};

  // sigwait thread
  std::thread signal_thread_;
};

}  // namespace mysql_harness
#endif
