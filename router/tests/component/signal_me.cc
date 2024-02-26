/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include <charconv>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <string_view>
#include <system_error>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#endif

#include "my_stacktrace.h"  // my_write_core()

#ifdef _WIN32
static void signal_handler(int signum) {
  my_safe_printf_stderr("%d: %s: exception 0x%x\n", __LINE__, __func__, signum);
  my_write_core(signum);
}

LONG WINAPI exception_filter(EXCEPTION_POINTERS *exp) {
  __try {
    my_set_exception_pointers(exp);
    signal_handler(exp->ExceptionRecord->ExceptionCode);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    fputs("*boom*\n", stderr);
  }

  return EXCEPTION_EXECUTE_HANDLER;
}

void init_signal_handler() {
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

  UINT mode = SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX;
  SetErrorMode(mode);

  SetUnhandledExceptionFilter(exception_filter);
}
#else
void init_signal_handler() {}
#endif

int main(int argc, char **argv) {
  if (argc != 2) {
    puts("expected argc==2");
    return EXIT_FAILURE;
  }

  std::string_view arg_1(argv[1]);

  int signum;

  auto res = std::from_chars(arg_1.data(), arg_1.data() + arg_1.size(), signum);

  if (res.ec != std::errc{}) {
    puts("expected first arg to be decimal in range 32-bit signed integer");
    return EXIT_FAILURE;
  }

#ifndef _WIN32
#ifdef __linux__
  prctl(PR_SET_DUMPABLE, 1);
#endif

  struct rlimit rl;
  getrlimit(RLIMIT_CORE, &rl);

  // raise the core-limit to the max.
  rl.rlim_cur = rl.rlim_max;

  setrlimit(RLIMIT_CORE, &rl);
#endif

  init_signal_handler();

  switch (signum) {
    case 0:
      break;
#ifdef _WIN32
    case SIGSEGV:
      RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0,
                     NULL);
      break;
    case SIGINT:
    case SIGABRT:
      RaiseException(EXCEPTION_BREAKPOINT, EXCEPTION_NONCONTINUABLE, 0, NULL);
      break;
#else
    default:
      raise(signum);
      break;
#endif
  }

  return EXIT_SUCCESS;
}
