/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/signal_handler.h"

#include <condition_variable>
#include <csignal>
#include <fstream>
#include <future>  // promise
#include <iostream>
#include <mutex>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#include <sys/resource.h>  // rlimit
#include <unistd.h>
#endif

#include "config.h"  // HAVE_SYS_PRCTL_H

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "harness_assert.h"
#include "my_config.h"  // HAVE_ASAN
#include "my_stacktrace.h"
#include "my_thread.h"                 // my_thread_self_setname
#include "mysql/harness/filesystem.h"  // Path
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/process_state_component.h"

IMPORT_LOG_FUNCTIONS()

namespace {

#ifdef _WIN32
void write_minidump(int signum) {
  my_safe_printf_stderr("Application got fatal signal: 0x%x\n", signum);
  my_write_core(signum);
}

LONG WINAPI exception_filter_minidump(EXCEPTION_POINTERS *exp) {
  __try {
    // set the exception pointers for my_write_core();
    my_set_exception_pointers(exp);
    write_minidump(exp->ExceptionRecord->ExceptionCode);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    fputs("exception_filter() failed.\n", stderr);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI exception_filter_print_stacktrace(EXCEPTION_POINTERS *exp) {
  __try {
    my_safe_printf_stderr("Application got fatal signal: 0x%lx\n",
                          exp->ExceptionRecord->ExceptionCode);
    my_print_stacktrace(nullptr, 0);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    fputs("exception_filter() failed.\n", stderr);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

BOOL WINAPI ctrl_c_handler(DWORD ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
    // user pressed Ctrl+C or we got Ctrl+Break request

    mysql_harness::ProcessStateComponent::get_instance()
        .request_application_shutdown();

    return TRUE;  // don't pass this event to further handlers
  } else {
    // some other event
    return FALSE;  // let the default Windows handler deal with it
  }
}

#endif

#if !defined(_WIN32)
/**
 * check if RLIMIT_CORE is ignored.
 */
bool rlimit_core_is_ignored() {
#if defined(__linux__)
  mysql_harness::Path core_pattern_path("/proc/sys/kernel/core_pattern");
  if (core_pattern_path.exists()) {
    std::ifstream ifs(core_pattern_path.str());

    std::string core_pattern;
    std::getline(ifs, core_pattern);

    // if core-pattern redirects to a pipe, the rlimit_core is ignored.
    if (!core_pattern.empty() && core_pattern[0] == '|') return true;
  }
#endif

  return false;
}
#endif
}  // namespace

namespace mysql_harness {

SignalHandler::~SignalHandler() {
  if (signal_thread_.joinable()) {
#ifndef _WIN32
    // as the signal thread is blocked on sigwait(), interrupt it with a SIGTERM
    pthread_kill(signal_thread_.native_handle(), SIGTERM);
#endif
    signal_thread_.join();
  }

#ifdef _WIN32
  unregister_ctrl_c_handler();
#endif
}

void SignalHandler::block_all_nonfatal_signals() {
#ifndef _WIN32
  sigset_t ss;
  sigfillset(&ss);
  // we can't block those signals globally and rely on our handler thread, as
  // these are only received by the offending thread itself.
  // see "man signal" for more details
  for (const auto &sig : kFatalSignals) {
    sigdelset(&ss, sig);
  }
  if (0 != pthread_sigmask(SIG_SETMASK, &ss, nullptr)) {
    throw std::system_error({errno, std::generic_category()},
                            "pthread_sigmask() failed");
  }
#endif
}

void SignalHandler::register_ignored_signals_handler() {
#if !defined(_WIN32)
  struct sigaction sa;
  (void)sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = SIG_IGN;

  for (const auto &sig : kIgnoredSignals) {
    (void)sigaction(sig, &sa, nullptr);
  }
#endif
}

void SignalHandler::register_fatal_signal_handler(bool dump_core) {
#if !defined(_WIN32)

  if (dump_core) {
#ifdef HAVE_SYS_PRCTL_H
    /* inform kernel that process is dumpable */
    (void)prctl(PR_SET_DUMPABLE, 1);
#endif

    // enable core-dumps up to the hard-limit.
    if (!rlimit_core_is_ignored()) {
      struct rlimit rl;
      rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;

      (void)setrlimit(RLIMIT_CORE, &rl);

      if (0 == getrlimit(RLIMIT_CORE, &rl)) {
        if (rl.rlim_cur == 0) {
          // as it is early logging, no WARN prefix will be generated.
          log_warning(
              "NOTE: core-file requested, but resource-limits say core-files "
              "are disabled for this process ('ulimit -c' is '0')");
        }
      }
    }
  }

  // enable a crash handler on POSIX systems if not built with ASAN|TSAN
#if !defined(HAVE_ASAN) && !defined(HAVE_TSAN)
  struct sigaction sa;
  (void)sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND | SA_NODEFER;

#if defined(HAVE_STACKTRACE)
  my_init_stacktrace();
#endif  // HAVE_STACKTRACE

  if (!dump_core) {
    sa.sa_handler = [](int sig) {
      my_safe_printf_stderr("Application got fatal signal: %d\n", sig);
#if defined(HAVE_STACKTRACE)
      my_print_stacktrace(nullptr, 0);
#endif
      _Exit(HARNESS_FAILURE_EXIT);  // exit with failure
    };
  } else {
    sa.sa_handler = [](int sig) {
      my_safe_printf_stderr("Application got fatal signal: %d\n", sig);
#if defined(HAVE_STACKTRACE)
      my_print_stacktrace(nullptr, 0);
#endif  // HAVE_STACKTRACE
      // raise the signal again to get a core file.
      my_write_core(sig);
    };
  }

  for (auto sig : kFatalSignals) {
    (void)sigaction(sig, &sa, nullptr);
  }
#endif  // HAVE_ASAN

#else
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

  UINT mode = SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX;
  SetErrorMode(mode);

  if (dump_core) {
    SetUnhandledExceptionFilter(exception_filter_minidump);
  } else {
    SetUnhandledExceptionFilter(exception_filter_print_stacktrace);
  }
#endif
}

#ifdef _WIN32
void SignalHandler::register_ctrl_c_handler() {
  if (!SetConsoleCtrlHandler(ctrl_c_handler, TRUE)) {
    std::cerr << "Could not install Ctrl+C handler, exiting.\n";
    exit(1);
  }
}

void SignalHandler::unregister_ctrl_c_handler() {
  SetConsoleCtrlHandler(ctrl_c_handler, FALSE);  // remove
}
#endif

void SignalHandler::spawn_signal_handler_thread() {
#ifndef _WIN32
  std::promise<void> signal_handler_thread_setup_done;
  signal_thread_ = std::thread([this] {
    my_thread_self_setname("sig handler");

    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);
    sigaddset(&ss, SIGTERM);
    sigaddset(&ss, SIGHUP);
    sigaddset(&ss, SIGUSR1);

    int sig = 0;
    while (true) {
      sig = 0;
      if (0 == sigwait(&ss, &sig)) {
        if (sig == SIGUSR1) {
          signal_thread_ready_.serialize_with_cv([](auto &ready, auto &cv) {
            ready = true;

            cv.notify_one();
          });
          sigdelset(&ss, SIGUSR1);
        } else {
          auto handler =
              sig_handlers_([sig](auto &handlers) -> std::function<void(int)> {
                auto it = handlers.find(sig);
                if (it == handlers.end()) return {};

                return it->second;
              });

          // if a handler was found, call it.
          if (handler) handler(sig);

          // TERM and INT are only handled once and terminate the loop.
          if (sig == SIGTERM || sig == SIGINT) break;
        }
      } else {
        // man sigwait() says, it should only fail if we provided invalid
        // signals.
        harness_assert_this_should_not_execute();
      }
    }
  });

  // wait until the signal handler is setup
  signal_thread_ready_.wait([this](auto ready) {
    if (!ready) pthread_kill(signal_thread_.native_handle(), SIGUSR1);

    return ready;
  });
#endif
}

}  // namespace mysql_harness
