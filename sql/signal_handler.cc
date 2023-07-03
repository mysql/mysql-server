/* Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_config.h"

#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <atomic>

#include "lex_string.h"
#include "my_inttypes.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "my_macros.h"
#include "my_stacktrace.h"
#include "my_sys.h"
#include "my_time.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"

#ifdef _WIN32
#include <crtdbg.h>

#define SIGNAL_FMT "exception 0x%x"
#else
#define SIGNAL_FMT "signal %d"
#endif

/*
  We are handling signals in this file.
  Any global variables we read should be 'volatile sig_atomic_t' or lock-free
  std::atomic.
 */

/**
  This is used to check if the signal handler is not called again while already
  handling the previous signal, as may happen when either another thread
  triggers it, or a bug in handling causes abort again.
*/
static std::atomic<bool> s_handler_being_processed{false};
/**
  Used to remember if the fatal info was already printed. The info can be
  printed from user threads, but in the fatal signal handler we want to print it
  if and only if the info was not yet printed. User threads after printing the
  info will call abort which will call the handler.
*/
static std::atomic<bool> s_fatal_info_printed{false};

/**
  This function will try to dump relevant debugging information to stderr and
  dump a core image.

  It may be called as part of the signal handler. This fact limits library calls
  that we can perform and much more, @see handle_fatal_signal

  @param sig Signal number
*/
void print_fatal_signal(int sig) {
  s_fatal_info_printed = true;
#ifdef _WIN32
  SYSTEMTIME utc_time;
  GetSystemTime(&utc_time);

  const long year = utc_time.wYear;
  const long month = utc_time.wMonth;
  const long day = utc_time.wDay;

  const long hrs = utc_time.wHour;
  const long mins = utc_time.wMinute;
  const long secs = utc_time.wSecond;
#else
  /* Using time() instead of my_time() to avoid looping */
  const time_t curr_time = time(nullptr);

  // Offset for the UNIX epoch.
  const ulong days_at_timestart = 719528;

  /* Calculate time of day */
  const long total_mins = curr_time / 60;
  const long total_hrs = total_mins / 60;
  const long total_days = (total_hrs / 24) + days_at_timestart;

  const long hrs = total_hrs % 24;
  const long mins = total_mins % 60;
  const long secs = curr_time % 60;

  uint year, month, day;

  get_date_from_daynr(total_days, &year, &month, &day);
#endif

  char hrs_buf[3] = "00";
  char mins_buf[3] = "00";
  char secs_buf[3] = "00";
  my_safe_itoa(10, hrs, &hrs_buf[2]);
  my_safe_itoa(10, mins, &mins_buf[2]);
  my_safe_itoa(10, secs, &secs_buf[2]);

  char year_buf[5] = "0000";
  char month_buf[3] = "00";
  char day_buf[3] = "00";
  my_safe_itoa(10, year, &year_buf[4]);
  my_safe_itoa(10, month, &month_buf[2]);
  my_safe_itoa(10, day, &day_buf[2]);

  my_safe_printf_stderr(
      "%s-%s-%sT%s:%s:%sZ UTC - mysqld got " SIGNAL_FMT " ;\n", year_buf,
      month_buf, day_buf, hrs_buf, mins_buf, secs_buf, sig);

  my_safe_printf_stderr(
      "%s",
      "Most likely, you have hit a bug, but this error can also "
      "be caused by malfunctioning hardware.\n");

#if defined(HAVE_BUILD_ID_SUPPORT)
  my_safe_printf_stderr("BuildID[sha1]=%s\n", server_build_id);
#endif

#ifdef HAVE_STACKTRACE
  THD *thd = current_thd;

  if (!(test_flags & TEST_NO_STACKTRACE)) {
    my_safe_printf_stderr("Thread pointer: 0x%p\n", thd);
    my_safe_printf_stderr(
        "%s",
        "Attempting backtrace. You can use the following "
        "information to find out\n"
        "where mysqld died. If you see no messages after this, something went\n"
        "terribly wrong...\n");
    my_print_stacktrace(
        thd ? pointer_cast<const uchar *>(thd->thread_stack) : nullptr,
        my_thread_stack_size);
  }
  if (thd) {
    const char *kreason = "UNKNOWN";
    switch (thd->killed.load()) {
      case THD::NOT_KILLED:
        kreason = "NOT_KILLED";
        break;
      case THD::KILL_CONNECTION:
        kreason = "KILL_CONNECTION";
        break;
      case THD::KILL_QUERY:
        kreason = "KILL_QUERY";
        break;
      case THD::KILL_TIMEOUT:
        kreason = "KILL_TIMEOUT";
        break;
      case THD::KILLED_NO_VALUE:
        kreason = "KILLED_NO_VALUE";
        break;
    }
    my_safe_printf_stderr(
        "%s",
        "\n"
        "Trying to get some variables.\n"
        "Some pointers may be invalid and cause the dump to abort.\n");

    my_safe_printf_stderr("Query (%p): ", thd->query().str);
    my_safe_puts_stderr(thd->query().str,
                        std::min(size_t{1024}, thd->query().length));
    my_safe_printf_stderr("Connection ID (thread ID): %u\n", thd->thread_id());
    my_safe_printf_stderr("Status: %s\n\n", kreason);
  }
  my_safe_printf_stderr(
      "%s",
      "The manual page at "
      "http://dev.mysql.com/doc/mysql/en/crashing.html contains\n"
      "information that should help you find out what is causing the crash.\n");

#endif /* HAVE_STACKTRACE */
}

/**
  Handler for fatal signals

  Fatal events (seg.fault, bus error etc.) will trigger this signal handler. The
  handler will try to dump relevant debugging information to stderr and dump a
  core image.

  Signal handlers can only use a set of 'safe' system calls and library
  functions.

  - A list of safe calls in POSIX systems are available at:
  http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
  - For MS Windows, guidelines are available in documentation of the `signal()`
  function:
  https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/signal?view=msvc-160

  @param sig Signal number
*/
extern "C" void handle_fatal_signal(int sig) {
  if (s_handler_being_processed) {
    my_safe_printf_stderr("Fatal " SIGNAL_FMT " while backtracing\n", sig);
    _exit(MYSQLD_FAILURE_EXIT); /* Quit without running destructors */
  }

  s_handler_being_processed = true;

  if (!s_fatal_info_printed) {
    print_fatal_signal(sig);
  }

  if ((test_flags & TEST_CORE_ON_SIGNAL) != 0) {
    my_safe_printf_stderr("%s", "Writing a core file\n");
    my_write_core(sig);
  }

#ifndef _WIN32
  /*
     Quit, without running destructors (etc.)
     On Windows, do not terminate, but pass control to exception filter.
  */
  _exit(MYSQLD_FAILURE_EXIT);  // Using _exit(), since exit() is not async
                               // signal safe
#endif
}

/**
  This is a wrapper around abort() which ensures that abort() will be called
  exactly once, as calling it more than once might cause following problems:
  When original abort() is called there is a signal processing triggered, but
  only the first abort() causes the signal handler to be called, all other
  abort()s called by the other threads will cause immediate exit() call, which
  will also terminate the first abort() processing within the signal handler,
  aborting stacktrace printing, core writeout or any other processing.
*/
void my_server_abort() {
  static std::atomic_int aborts_pending{0};
  static std::atomic_bool abort_processing{false};
  /* Broadcast that this thread wants to print the signal info. */
  aborts_pending++;
  /*
    Wait for the exclusive right to print the signal info. This assures the
    output is not interleaved.
  */
  while (abort_processing.exchange(true)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  /*
    This actually takes some time, some or many other threads may call
    my_server_abort in meantime.
  */
  print_fatal_signal(SIGABRT);
  abort_processing = false;
  /*
    If there are no other threads pending abort then we call real abort as the
    last aborting thread. If that succeeds, we are left with a positive
    `aborts_pending`, and it will never go down to zero again. This effectively
    prevents any other thread from calling real `abort`.
  */
  auto left = --aborts_pending;
  if (!left && aborts_pending.compare_exchange_strong(left, 1)) {
    /*
      Wait again for the exclusive right to print the signal info by calling
      the real `abort`. This assures the output is not interleaved with any
      printing from a few lines above, that could start in the meantime.
    */
    while (abort_processing.exchange(true)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    abort();
  }
  /*
    Abort can't return, we will sleep here forever - the algorithm above
    assures exactly one thread, eventually, will call `abort()` and terminate
    the whole program.
  */
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
